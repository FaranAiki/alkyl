import re

with open('src/codegen_cranelift/rust_backend/src/lib.rs', 'r') as f:
    code = f.read()

# Add Sizeof, Shr, Alignof ops
old_op = '''                                AlirOpcode::Mul => {
                                    let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                                    let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                                    let res = builder.ins().imul(lhs, rhs);
                                    
                                    if !inst.dest.is_null() {
                                        let v_dest = unsafe { &*inst.dest };
                                        let key = if v_dest.kind == 4 {
                                            unsafe { CStr::from_ptr(v_dest.val as *const c_char) }.to_string_lossy().into_owned()
                                        } else {
                                            format!("ptr_{:x}", inst.dest as usize)
                                        };
                                        let var = *var_map.entry(key).or_insert_with(|| {
                                            let new_var = Variable::new(next_var_id);
                                            next_var_id += 1;
                                            builder.declare_var(new_var, cranelift::codegen::ir::types::I64);
                                            new_var
                                        });
                                        builder.def_var(var, res);
                                    }
                                },'''

new_op = old_op + '''
                                AlirOpcode::Shr => {
                                    let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                                    let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                                    let res = builder.ins().ushr(lhs, rhs);
                                    
                                    if !inst.dest.is_null() {
                                        let v_dest = unsafe { &*inst.dest };
                                        let key = if v_dest.kind == 4 {
                                            unsafe { CStr::from_ptr(v_dest.val as *const c_char) }.to_string_lossy().into_owned()
                                        } else {
                                            format!("ptr_{:x}", inst.dest as usize)
                                        };
                                        let var = *var_map.entry(key).or_insert_with(|| {
                                            let new_var = Variable::new(next_var_id);
                                            next_var_id += 1;
                                            builder.declare_var(new_var, cranelift::codegen::ir::types::I64);
                                            new_var
                                        });
                                        builder.def_var(var, res);
                                    }
                                },
                                AlirOpcode::Shl => {
                                    let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                                    let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                                    let res = builder.ins().ishl(lhs, rhs);
                                    
                                    if !inst.dest.is_null() {
                                        let v_dest = unsafe { &*inst.dest };
                                        let key = if v_dest.kind == 4 {
                                            unsafe { CStr::from_ptr(v_dest.val as *const c_char) }.to_string_lossy().into_owned()
                                        } else {
                                            format!("ptr_{:x}", inst.dest as usize)
                                        };
                                        let var = *var_map.entry(key).or_insert_with(|| {
                                            let new_var = Variable::new(next_var_id);
                                            next_var_id += 1;
                                            builder.declare_var(new_var, cranelift::codegen::ir::types::I64);
                                            new_var
                                        });
                                        builder.def_var(var, res);
                                    }
                                },
                                AlirOpcode::Sizeof | AlirOpcode::Alignof => {
                                    let v_op = unsafe { &*inst.op1 };
                                    let size_val = if v_op.kind == 7 { v_op.val } else { 8 }; // Fallback
                                    let res = builder.ins().iconst(cranelift::codegen::ir::types::I64, size_val as i64);
                                    if !inst.dest.is_null() {
                                        let v_dest = unsafe { &*inst.dest };
                                        let key = if v_dest.kind == 4 {
                                            unsafe { CStr::from_ptr(v_dest.val as *const c_char) }.to_string_lossy().into_owned()
                                        } else {
                                            format!("ptr_{:x}", inst.dest as usize)
                                        };
                                        let var = *var_map.entry(key).or_insert_with(|| {
                                            let new_var = Variable::new(next_var_id);
                                            next_var_id += 1;
                                            builder.declare_var(new_var, cranelift::codegen::ir::types::I64);
                                            new_var
                                        });
                                        builder.def_var(var, res);
                                    }
                                },'''

code = code.replace(old_op, new_op)

with open('src/codegen_cranelift/rust_backend/src/lib.rs', 'w') as f:
    f.write(code)
