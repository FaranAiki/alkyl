import re

with open('src/codegen_cranelift/rust_backend/src/lib.rs', 'r') as f:
    code = f.read()

old_block = '''                builder.append_block_params_for_function_params(entry_block);
                
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
                }
                
                builder.switch_to_block(entry_block);'''

new_block = '''                builder.append_block_params_for_function_params(entry_block);
                builder.switch_to_block(entry_block);
                
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

code = code.replace(old_block, new_block)

with open('src/codegen_cranelift/rust_backend/src/lib.rs', 'w') as f:
    f.write(code)
