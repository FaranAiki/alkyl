import sys

with open('src/codegen_cranelift/rust_backend/src/lib.rs', 'r') as f:
    content = f.read()

# 1. Update is_extern check
old_extern = '''        let curr_block = f.blocks as *const AlirBlock;
        if curr_block.is_null() {
            // External function
            curr_func = f.next;
            continue;
        }'''
new_extern = '''        if f.is_extern != 0 {
            println!("{} is external (skipped)", fname);
            curr_func = f.next;
            continue;
        }'''
content = content.replace(old_extern, new_extern)

# 2. Add catch_unwind wrapping
content = content.replace(
    'pub extern "C" fn alkyl_backend_run_cranelift(alir_ptr: *const c_void, basename_ptr: *const c_char) -> i32 {',
    'pub extern "C" fn alkyl_backend_run_cranelift(alir_ptr: *const c_void, basename_ptr: *const c_char) -> i32 {\n    let result = std::panic::catch_unwind(|| {'
)

# 3. Add catch_unwind trailing bracket and match
content = content.replace(
    '    0\n}',
    '    0\n    });\n\n    match result {\n        Ok(code) => code,\n        Err(_) => {\n            println!("[Cranelift Rust Backend] Fatal compiler panic caught!");\n            -1\n        }\n    }\n}'
)

# 4. Update ALIR_OP_CALL to use direct call
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
                                        call_args.push(get_val(arg_v, &mut builder, &val_map, &mut module, &global_map));
                                    }

                                    let call_inst = builder.ins().call_indirect(sig_ref, func_ptr, &call_args);'''

new_call = '''                                    let mut call_args = Vec::new();
                                    for i in 0..inst.arg_count {
                                        let arg_v = unsafe { *(inst.args.offset(i as isize)) };
                                        call_args.push(get_val(arg_v, &mut builder, &val_map, &mut module, &global_map));
                                    }

                                    let callee_id = func_map.get(&func_name).copied().unwrap_or_else(|| {
                                        module.declare_function(&func_name, cranelift_module::Linkage::Import, &sig).unwrap()
                                    });
                                    let local_callee = module.declare_func_in_func(callee_id, &mut builder.func);
                                    let call_inst = builder.ins().call(local_callee, &call_args);'''

content = content.replace(old_call, new_call)

with open('src/codegen_cranelift/rust_backend/src/lib.rs', 'w') as f:
    f.write(content)
