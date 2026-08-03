import re

with open('src/codegen_cranelift/rust_backend/src/lib.rs', 'r') as f:
    code = f.read()

# 1. Change var_map signature
code = code.replace(
    'let mut var_map = std::collections::HashMap::<usize, Variable>::new();',
    'let mut var_map = std::collections::HashMap::<String, Variable>::new();'
)
code = code.replace(
    'var_map: &mut std::collections::HashMap<usize, Variable>',
    'var_map: &mut std::collections::HashMap<String, Variable>'
)

# 2. Change get_val to use String key
old_get_val_end = '''                    } else {
                        let dest_ptr_addr = v_ptr as usize;
                        let var = *var_map.entry(dest_ptr_addr).or_insert_with(|| {
                            let new_var = Variable::new(*next_var_id);
                            *next_var_id += 1;
                            bld.declare_var(new_var, cranelift::codegen::ir::types::I64);
                            new_var
                        });
                        bld.use_var(var)
                    }'''
new_get_val_end = '''                    } else {
                        let key = if v.kind == 4 {
                            unsafe { CStr::from_ptr(v.val as *const c_char) }.to_string_lossy().into_owned()
                        } else {
                            format!("ptr_{:x}", v_ptr as usize)
                        };
                        let var = *var_map.entry(key).or_insert_with(|| {
                            let new_var = Variable::new(*next_var_id);
                            *next_var_id += 1;
                            bld.declare_var(new_var, cranelift::codegen::ir::types::I64);
                            new_var
                        });
                        bld.use_var(var)
                    }'''
code = code.replace(old_get_val_end, new_get_val_end)

# 3. Change dest assignment to use String key
old_dest = '''if !inst.dest.is_null() {
                                let dest_ptr_addr = inst.dest as usize;
                                let var = *var_map.entry(dest_ptr_addr).or_insert_with(|| {'''
new_dest = '''if !inst.dest.is_null() {
                                let v_dest = unsafe { &*inst.dest };
                                let key = if v_dest.kind == 4 {
                                    unsafe { CStr::from_ptr(v_dest.val as *const c_char) }.to_string_lossy().into_owned()
                                } else {
                                    format!("ptr_{:x}", inst.dest as usize)
                                };
                                let var = *var_map.entry(key).or_insert_with(|| {'''
code = code.replace(old_dest, new_dest)

# 4. Now, before the instruction loop, bind the function arguments!
# After: builder.append_block_params_for_function_params(entry_block);
old_bind = 'builder.append_block_params_for_function_params(entry_block);'
new_bind = '''builder.append_block_params_for_function_params(entry_block);
                
                // Bind function arguments to var_map
                for i in 0..f.param_count {
                    let pname = format!("p{}", i);
                    let var = *var_map.entry(pname).or_insert_with(|| {
                        let new_var = Variable::new(next_var_id);
                        next_var_id += 1;
                        builder.declare_var(new_var, cranelift::codegen::ir::types::I64);
                        new_var
                    });
                    let param_val = builder.block_params(entry_block)[i as usize];
                    builder.def_var(var, param_val);
                }'''
code = code.replace(old_bind, new_bind)

with open('src/codegen_cranelift/rust_backend/src/lib.rs', 'w') as f:
    f.write(code)
