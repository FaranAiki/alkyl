import re

with open('src/codegen_cranelift/rust_backend/src/lib.rs', 'r') as f:
    content = f.read()

# 1. Replace val_map with var_map + get_or_create_var + use Variable import
content = content.replace(
    'use cranelift_module::{Module, default_libcall_names};',
    'use cranelift_module::{Module, default_libcall_names};\nuse cranelift::frontend::Variable;'
)

content = content.replace(
    'let mut val_map = std::collections::HashMap::<usize, cranelift::codegen::ir::Value>::new();',
    '''let mut var_map = std::collections::HashMap::<usize, Variable>::new();
            let mut next_var_id = 0;'''
)

# 2. Update get_val signature and logic
old_get_val = '''                    let get_val = |v_ptr: *mut AlirValue, bld: &mut cranelift::frontend::FunctionBuilder<'_>, map: &std::collections::HashMap<usize, cranelift::codegen::ir::Value>, module: &mut ObjectModule, global_map: &std::collections::HashMap<String, cranelift_module::DataId>| -> cranelift::codegen::ir::Value {
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

new_get_val = '''                    let mut get_or_create_var = |ptr: usize, bld: &mut cranelift::frontend::FunctionBuilder| -> Variable {
                        *var_map.entry(ptr).or_insert_with(|| {
                            let var = Variable::new(next_var_id);
                            next_var_id += 1;
                            bld.declare_var(var, cranelift::codegen::ir::types::I64);
                            var
                        })
                    };

                    let mut get_val = |
                        v_ptr: *mut AlirValue, 
                        bld: &mut cranelift::frontend::FunctionBuilder<'_>, 
                        module: &mut ObjectModule, 
                        global_map: &std::collections::HashMap<String, cranelift_module::DataId>
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
                            let var = get_or_create_var(v_ptr as usize, bld);
                            bld.use_var(var)
                        }
                    };'''
content = content.replace(old_get_val, new_get_val)

# 3. Fix instruction calls to get_val without map
content = content.replace('&val_map, ', '')

# 4. Fix assignment (def_var instead of val_map.insert)
content = re.sub(
    r'val_map\.insert\(inst\.dest as usize, (.*?)\);',
    r'if !inst.dest.is_null() { let dest_var = get_or_create_var(inst.dest as usize, &mut builder); builder.def_var(dest_var, \1); }',
    content
)

# 5. Fix icmp (uext to I64 so it can be compared later)
content = re.sub(
    r'let res = builder\.ins\(\)\.icmp\((.*?), lhs, rhs\);',
    r'let b1_res = builder.ins().icmp(\1, lhs, rhs); let res = builder.ins().uext(cranelift::codegen::ir::types::I64, b1_res);',
    content
)

# 6. Break after terminators (Jump, Condi, Ret)
content = content.replace('terminated = true;', 'terminated = true; break;')

# 7. Add null check for args in Call
old_call_args = '''                                        for i in 0..inst.arg_count {
                                            let arg_v = unsafe { *(inst.args.offset(i as isize)) };
                                            call_args.push(get_val(arg_v, &mut builder, &mut module, &global_map));
                                        }'''
new_call_args = '''                                        if !inst.args.is_null() {
                                            for i in 0..inst.arg_count {
                                                let arg_v = unsafe { *(inst.args.offset(i as isize)) };
                                                call_args.push(get_val(arg_v, &mut builder, &mut module, &global_map));
                                            }
                                        }'''
content = content.replace(old_call_args, new_call_args)

# 8. Fix FAdd, FSub, FMul, FDiv casting to f64
for fop in ['fadd', 'fsub', 'fmul', 'fdiv']:
    old_f = f'let res = builder.ins().{fop}(lhs, rhs);'
    new_f = f'let lhs_f = builder.ins().bitcast(cranelift::codegen::ir::types::F64, lhs); let rhs_f = builder.ins().bitcast(cranelift::codegen::ir::types::F64, rhs); let res_f = builder.ins().{fop}(lhs_f, rhs_f); let res = builder.ins().bitcast(cranelift::codegen::ir::types::I64, res_f);'
    content = content.replace(old_f, new_f)

with open('src/codegen_cranelift/rust_backend/src/lib.rs', 'w') as f:
    f.write(content)

