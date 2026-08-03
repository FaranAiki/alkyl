import re

with open('src/codegen_cranelift/rust_backend/src/lib.rs', 'r') as f:
    code = f.read()

# 1. Imports
code = code.replace(
    'use cranelift_module::{Module, default_libcall_names};',
    'use cranelift_module::{Module, default_libcall_names};\nuse cranelift::frontend::Variable;'
)

# 2. Add catch_unwind
code = code.replace(
    'pub extern "C" fn alkyl_backend_run_cranelift(alir_ptr: *const c_void, basename_ptr: *const c_char) -> i32 {',
    'pub extern "C" fn alkyl_backend_run_cranelift(alir_ptr: *const c_void, basename_ptr: *const c_char) -> i32 {\n    let result = std::panic::catch_unwind(|| {'
)
code = code.replace(
    '    0\n}',
    '    0\n    });\n    match result {\n        Ok(code) => code,\n        Err(_) => -1,\n    }\n}'
)

# 3. is_extern fix
code = code.replace(
    'let curr_block = f.blocks as *const AlirBlock;\n        if curr_block.is_null() {',
    'if f.is_extern != 0 {\n            curr_func = f.next;\n            continue;\n        }\n        let curr_block = f.blocks as *const AlirBlock;\n        if curr_block.is_null() {'
)

# 4. var_map instead of val_map
code = code.replace(
    'let mut val_map = std::collections::HashMap::<usize, cranelift::codegen::ir::Value>::new();',
    'let mut var_map = std::collections::HashMap::<usize, Variable>::new();\n        let mut next_var_id = 0;'
)

# 5. The instruction parsing logic
# We need to replace get_val definition
old_get_val = '''                let get_val = |v_ptr: *mut AlirValue, bld: &mut cranelift::frontend::FunctionBuilder<'_>, map: &std::collections::HashMap<usize, cranelift::codegen::ir::Value>, module: &mut ObjectModule, global_map: &std::collections::HashMap<String, cranelift_module::DataId>| -> cranelift::codegen::ir::Value {
                    if v_ptr.is_null() {
                        return bld.ins().iconst(cranelift::codegen::ir::types::I64, 0);
                    }
                    let v = unsafe { &*v_ptr };
                    if v.kind == 7 { // ALIR_VAL_CONST
                        bld.ins().iconst(cranelift::codegen::ir::types::I64, v.val as i64)
                    } else if v.kind == 9 { // ALIR_VAL_GLOBAL
                        let gname = unsafe { CStr::from_ptr(v.val as *const c_char) }.to_string_lossy().into_owned();
                        if let Some(data_id) = global_map.get(&gname) {
                            let local_id = module.declare_data_in_func(*data_id, &mut bld.func);
                            bld.ins().symbol_value(cranelift::codegen::ir::types::I64, local_id)
                        } else {
                            println!("WARNING: Global not found in get_val: {}", gname);
                            bld.ins().iconst(cranelift::codegen::ir::types::I64, 0)
                        }
                    } else if let Some(val) = map.get(&(v_ptr as usize)) {
                        *val
                    } else {
                        bld.ins().iconst(cranelift::codegen::ir::types::I64, 0)
                    }
                };'''

new_get_val = '''                let mut get_val = |
                    v_ptr: *mut AlirValue, 
                    bld: &mut cranelift::frontend::FunctionBuilder<'_>, 
                    module: &mut ObjectModule, 
                    global_map: &std::collections::HashMap<String, cranelift_module::DataId>,
                    var_map: &mut std::collections::HashMap<usize, Variable>,
                    next_var_id: &mut usize
                | -> cranelift::codegen::ir::Value {
                    if v_ptr.is_null() {
                        return bld.ins().iconst(cranelift::codegen::ir::types::I64, 0);
                    }
                    let v = unsafe { &*v_ptr };
                    if v.kind == 7 { // ALIR_VAL_CONST
                        bld.ins().iconst(cranelift::codegen::ir::types::I64, v.val as i64)
                    } else if v.kind == 9 { // ALIR_VAL_GLOBAL
                        let gname = unsafe { CStr::from_ptr(v.val as *const c_char) }.to_string_lossy().into_owned();
                        if let Some(data_id) = global_map.get(&gname) {
                            let local_id = module.declare_data_in_func(*data_id, &mut bld.func);
                            bld.ins().symbol_value(cranelift::codegen::ir::types::I64, local_id)
                        } else {
                            bld.ins().iconst(cranelift::codegen::ir::types::I64, 0)
                        }
                    } else {
                        let ptr = v_ptr as usize;
                        let var = *var_map.entry(ptr).or_insert_with(|| {
                            let new_var = Variable::new(*next_var_id);
                            *next_var_id += 1;
                            bld.declare_var(new_var, cranelift::codegen::ir::types::I64);
                            new_var
                        });
                        bld.use_var(var)
                    }
                };'''

code = code.replace(old_get_val, new_get_val)

# Fix invocations to get_val
code = code.replace('&val_map, ', '')
code = code.replace('&global_map)', '&global_map, &mut var_map, &mut next_var_id)')

# Replace val_map.insert(inst.dest as usize, res) with def_var
def replace_dest(match):
    val = match.group(1)
    return f'''if !inst.dest.is_null() {{
                                let ptr = inst.dest as usize;
                                let var = *var_map.entry(ptr).or_insert_with(|| {{
                                    let new_var = Variable::new(next_var_id);
                                    next_var_id += 1;
                                    builder.declare_var(new_var, cranelift::codegen::ir::types::I64);
                                    new_var
                                }});
                                builder.def_var(var, {val});
                            }}'''

code = re.sub(r'val_map\.insert\(inst\.dest as usize, (.*?)\);', replace_dest, code)

# Break loop after terminators
code = code.replace('terminated = true;', 'terminated = true; break;')

# Fix icmp
code = re.sub(
    r'let res = builder\.ins\(\)\.icmp\((.*?), lhs, rhs\);',
    r'let b1_res = builder.ins().icmp(\1, lhs, rhs);\n                                let res = builder.ins().uext(cranelift::codegen::ir::types::I64, b1_res);',
    code
)

# Fix fadd/fsub/fmul/fdiv
for op in ['fadd', 'fsub', 'fmul', 'fdiv']:
    code = code.replace(
        f'let res = builder.ins().{op}(lhs, rhs);',
        f'let f_lhs = builder.ins().bitcast(cranelift::codegen::ir::types::F64, lhs);\n                                let f_rhs = builder.ins().bitcast(cranelift::codegen::ir::types::F64, rhs);\n                                let f_res = builder.ins().{op}(f_lhs, f_rhs);\n                                let res = builder.ins().bitcast(cranelift::codegen::ir::types::I64, f_res);'
    )

# Fix direct call
old_call = '''                                    let func_ptr = if let Some(id) = func_map.get(&func_name) {
                                        let local_callee = module.declare_func_in_func(*id, &mut builder.func);
                                        builder.ins().func_addr(cranelift::codegen::ir::types::I64, local_callee)
                                    } else {
                                        let callee_data_id = module.declare_data(&func_name, cranelift_module::Linkage::Import, false, false).unwrap();
                                        let global_val = module.declare_data_in_func(callee_data_id, &mut builder.func);
                                        builder.ins().symbol_value(cranelift::codegen::ir::types::I64, global_val)
                                    };

                                    let mut call_args = Vec::new();
                                    for i in 0..inst.arg_count {
                                        let arg_v = unsafe { *(inst.args.offset(i as isize)) };
                                        call_args.push(get_val(arg_v, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id));
                                    }

                                    let call_inst = builder.ins().call_indirect(sig_ref, func_ptr, &call_args);'''
new_call = '''                                    let mut call_args = Vec::new();
                                    if !inst.args.is_null() {
                                        for i in 0..inst.arg_count {
                                            let arg_v = unsafe { *(inst.args.offset(i as isize)) };
                                            call_args.push(get_val(arg_v, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id));
                                        }
                                    }

                                    let callee_id = func_map.get(&func_name).copied().unwrap_or_else(|| {
                                        module.declare_function(&func_name, cranelift_module::Linkage::Import, &sig).unwrap()
                                    });
                                    let local_callee = module.declare_func_in_func(callee_id, &mut builder.func);
                                    let call_inst = builder.ins().call(local_callee, &call_args);'''
code = code.replace(old_call, new_call)


with open('src/codegen_cranelift/rust_backend/src/lib.rs', 'w') as f:
    f.write(code)
