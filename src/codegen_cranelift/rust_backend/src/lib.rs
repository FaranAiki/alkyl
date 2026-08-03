use std::ffi::{c_char, c_void};
use std::ffi::CStr;
use cranelift::prelude::*;
use cranelift_object::{ObjectBuilder, ObjectModule};
use cranelift_module::{Module, default_libcall_names};
use cranelift::frontend::Variable;
use std::fs::File;
use std::io::Write;

// .. skipped enum definition

#[repr(C)]
#[derive(Debug, PartialEq, Eq)]
pub enum AlirOpcode {
    Alloca = 0,
    FreeStack,
    Store,
    Load,
    GetPtr,
    Bitcast,

    Add, Sub, Mul, Div, Mod,
    FAdd, FSub, FMul, FDiv,

    And, Or, Xor, Not,
    Shl, Shr, Rotr, Rotl,

    Lt, Gt, Lte, Gte, Eq, Neq,

    Jump,
    Condi,
    Call,
    Ret,
    Panic,
    Fallback,

    Cast,
    Sizeof,
    Alignof,
}

#[repr(C)]
pub struct AlirValue {
    pub kind: std::ffi::c_int,
    pub _pad1: [u8; 4],
    pub type_pad: [u8; 48],
    pub temp_id: std::ffi::c_int,
    pub _pad2: [u8; 4],
    pub val: u64,
}

#[repr(C)]
pub struct AlirInst {
    pub op: AlirOpcode,
    pub dest: *mut AlirValue,
    pub op1: *mut AlirValue,
    pub op2: *mut AlirValue,
    pub args: *mut *mut AlirValue,
    pub arg_count: std::ffi::c_int,
    pub _pad: [u8; 4],
    pub next: *mut AlirInst,
    pub line: std::ffi::c_int,
    pub col: std::ffi::c_int,
}

#[repr(C)]
pub struct AlirBlock {
    pub id: std::ffi::c_int,
    pub label: *mut c_char,
    pub head: *mut AlirInst,
    pub tail: *mut AlirInst,
    pub next: *mut AlirBlock,
    pub pred: *mut c_void,
    pub succ: *mut c_void,
}

#[repr(C)]
pub struct AlirFunction {
    pub name: *mut c_char,
    pub ret_type_pad: [u8; 48],
    pub params: *mut c_void,
    pub param_count: std::ffi::c_int,
    pub blocks: *mut AlirBlock,
    pub block_count: std::ffi::c_int,
    pub is_flux: std::ffi::c_int,
    pub is_varargs: std::ffi::c_int,
    pub is_extern: std::ffi::c_int,
    pub is_pure: std::ffi::c_int,
    pub reason: *mut c_char,
    pub cconv: *mut c_char,
    pub next: *mut AlirFunction,

}
// TODO implement this
// VarType in C is a struct, not a pointer. It's too big to guess.
// We will just read `functions` using a raw pointer approach if we need to.

#[repr(C)]
pub struct AlirGlobal {
    pub name: *const c_char,
    pub string_content: *const c_char,
    pub type_padding: [u8; 48],
    pub next: *mut AlirGlobal,
}

#[repr(C)]
pub struct AlirModule {
    pub name: *mut c_char,
    pub globals: *mut AlirGlobal,
    pub functions: *mut c_void, // AlirFunction pointer
}

#[no_mangle]
pub extern "C" fn alkyl_backend_run_cranelift(alir_ptr: *const c_void, basename_ptr: *const c_char) -> i32 {
    let result = std::panic::catch_unwind(|| {
    if alir_ptr.is_null() || basename_ptr.is_null() {
        return -1;
    }

    let basename = unsafe { CStr::from_ptr(basename_ptr) }.to_string_lossy().into_owned();

    // Use ObjectBuilder instead of JITBuilder
    let isa = cranelift_native::builder().unwrap().finish(cranelift::codegen::settings::Flags::new(cranelift::codegen::settings::builder())).unwrap();
    let builder = ObjectBuilder::new(isa, basename.clone() + ".o", cranelift_module::default_libcall_names()).unwrap();
    let mut module = ObjectModule::new(builder);
    let mut ctx = module.make_context();
    let mut func_ctx = FunctionBuilderContext::new();

    let alir = unsafe { &*(alir_ptr as *const AlirModule) };
    if !alir.name.is_null() {
        let name = unsafe { CStr::from_ptr(alir.name) };
        println!("[Cranelift Rust Backend] Compiling module: {:?}", name);
    }

    // Traverse functions
    let mut data_desc = cranelift_module::DataDescription::new();
    let mut global_map = std::collections::HashMap::new();
    let mut func_map = std::collections::HashMap::new();

    // Iterate over globals
    let mut curr_glob = alir.globals;
    while !curr_glob.is_null() {
        let g = unsafe { &*curr_glob };
        if !g.name.is_null() && !g.string_content.is_null() {
            let gname = unsafe { CStr::from_ptr(g.name) }.to_string_lossy().into_owned();
            let gcontent = unsafe { CStr::from_ptr(g.string_content) };
            let data_id = module.declare_data(&gname, cranelift_module::Linkage::Export, true, false).unwrap();

            data_desc.define(gcontent.to_bytes_with_nul().to_vec().into_boxed_slice());
            module.define_data(data_id, &data_desc).unwrap();
            data_desc.clear();

            global_map.insert(gname, data_id);
        }
        curr_glob = g.next;
    }

    let mut curr_func = alir.functions as *const AlirFunction;
    while !curr_func.is_null() {
        let f = unsafe { &*curr_func };

        let fname = if !f.name.is_null() {
            unsafe { CStr::from_ptr(f.name) }.to_string_lossy().into_owned()
        } else {
            "unknown_func".to_string()
        };
        println!("Compiling function: {:?}", fname);

        // We would set up Cranelift function signature here
        let mut sig = module.make_signature();
        for _ in 0..f.param_count {
            sig.params.push(cranelift::codegen::ir::AbiParam::new(cranelift::codegen::ir::types::I64));
        }
        sig.returns.push(cranelift::codegen::ir::AbiParam::new(cranelift::codegen::ir::types::I64)); // example

        let func_id = module.declare_function(&fname, cranelift_module::Linkage::Export, &sig).unwrap();
        func_map.insert(fname.clone(), func_id);
        ctx.func.signature = sig;
        ctx.func.name = cranelift::codegen::ir::UserFuncName::user(0, func_id.as_u32());

        let mut curr_block = f.blocks as *const AlirBlock;
        if curr_block.is_null() {
            // External function
            curr_func = f.next;
            continue;
        }

        let mut builder = cranelift::frontend::FunctionBuilder::new(&mut ctx.func, &mut func_ctx);
        let mut var_map = std::collections::HashMap::<String, Variable>::new();
        let mut next_var_id = 0;
        let mut block_map = std::collections::HashMap::<String, cranelift::codegen::ir::Block>::new();

        // Pre-create all blocks
        let mut curr_block = f.blocks as *const AlirBlock;
        while !curr_block.is_null() {
            let b = unsafe { &*curr_block };
            if !b.label.is_null() {
                let lbl = unsafe { CStr::from_ptr(b.label) }.to_string_lossy().into_owned();
                block_map.insert(lbl, builder.create_block());
            }
            curr_block = b.next;
        }

        curr_block = f.blocks as *const AlirBlock;
        if !curr_block.is_null() {
            let b = unsafe { &*curr_block };
            if !b.label.is_null() {
                let lbl = unsafe { CStr::from_ptr(b.label) }.to_string_lossy().into_owned();
                let entry_block = *block_map.get(&lbl).unwrap();
                builder.append_block_params_for_function_params(entry_block);
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
                    println!("DEBUG: binding arg {} to var {:?}, param_val {:?}", i, var, param_val);
                    builder.def_var(var, param_val);
                    println!("DEBUG: bound arg {}", i);
                }
            }
        }

        while !curr_block.is_null() {
            let b = unsafe { &*curr_block };
            if b.label.is_null() {
                curr_block = b.next;
                continue;
            }
            let lbl = unsafe { CStr::from_ptr(b.label) }.to_string_lossy().into_owned();
            let cl_block = *block_map.get(&lbl).unwrap();
    builder.switch_to_block(cl_block);


            let mut curr_inst = b.head as *const AlirInst;
            let mut terminated = false;
            println!("Function: {}", fname);
            while !curr_inst.is_null() {
                let inst = unsafe { &*curr_inst };
                println!("  Opcode: {:?}", inst.op);

                let mut get_val = |
                    v_ptr: *mut AlirValue, 
                    bld: &mut cranelift::frontend::FunctionBuilder<'_>, 
                    module: &mut ObjectModule, 
                    global_map: &std::collections::HashMap<String, cranelift_module::DataId>,
                    var_map: &mut std::collections::HashMap<String, Variable>,
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
                    }
                };

                if !terminated {
                    match inst.op {
                        AlirOpcode::Add => {
                            let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let res = builder.ins().iadd(lhs, rhs);
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
                        AlirOpcode::Sub => {
                            let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let res = builder.ins().isub(lhs, rhs);
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
                        AlirOpcode::Mul => {
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
                        },
                        AlirOpcode::Div => {
                            let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let res = builder.ins().sdiv(lhs, rhs);
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
                        AlirOpcode::Mod => {
                            let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let res = builder.ins().srem(lhs, rhs);
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
                        AlirOpcode::FAdd => {
                            let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let f_lhs = builder.ins().bitcast(cranelift::codegen::ir::types::F64, cranelift::codegen::ir::MemFlags::new(), lhs);
                                let f_rhs = builder.ins().bitcast(cranelift::codegen::ir::types::F64, cranelift::codegen::ir::MemFlags::new(), rhs);
                                let f_res = builder.ins().fadd(f_lhs, f_rhs);
                                let res = builder.ins().bitcast(cranelift::codegen::ir::types::I64, cranelift::codegen::ir::MemFlags::new(), f_res);
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
                        AlirOpcode::FSub => {
                            let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let f_lhs = builder.ins().bitcast(cranelift::codegen::ir::types::F64, cranelift::codegen::ir::MemFlags::new(), lhs);
                                let f_rhs = builder.ins().bitcast(cranelift::codegen::ir::types::F64, cranelift::codegen::ir::MemFlags::new(), rhs);
                                let f_res = builder.ins().fsub(f_lhs, f_rhs);
                                let res = builder.ins().bitcast(cranelift::codegen::ir::types::I64, cranelift::codegen::ir::MemFlags::new(), f_res);
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
                        AlirOpcode::FMul => {
                            let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let f_lhs = builder.ins().bitcast(cranelift::codegen::ir::types::F64, cranelift::codegen::ir::MemFlags::new(), lhs);
                                let f_rhs = builder.ins().bitcast(cranelift::codegen::ir::types::F64, cranelift::codegen::ir::MemFlags::new(), rhs);
                                let f_res = builder.ins().fmul(f_lhs, f_rhs);
                                let res = builder.ins().bitcast(cranelift::codegen::ir::types::I64, cranelift::codegen::ir::MemFlags::new(), f_res);
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
                        AlirOpcode::FDiv => {
                            let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let f_lhs = builder.ins().bitcast(cranelift::codegen::ir::types::F64, cranelift::codegen::ir::MemFlags::new(), lhs);
                                let f_rhs = builder.ins().bitcast(cranelift::codegen::ir::types::F64, cranelift::codegen::ir::MemFlags::new(), rhs);
                                let f_res = builder.ins().fdiv(f_lhs, f_rhs);
                                let res = builder.ins().bitcast(cranelift::codegen::ir::types::I64, cranelift::codegen::ir::MemFlags::new(), f_res);
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
                        AlirOpcode::And => {
                            let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let res = builder.ins().band(lhs, rhs);
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
                        AlirOpcode::Or => {
                            let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let res = builder.ins().bor(lhs, rhs);
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
                        AlirOpcode::Xor => {
                            let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let res = builder.ins().bxor(lhs, rhs);
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
                        AlirOpcode::Not => {
                            let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let res = builder.ins().bnot(lhs);
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
                        AlirOpcode::Shr => {
                            let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let res = builder.ins().sshr(lhs, rhs);
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
                        AlirOpcode::Rotl => {
                            let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let res = builder.ins().rotl(lhs, rhs);
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
                        AlirOpcode::Rotr => {
                            let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let res = builder.ins().rotr(lhs, rhs);
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
                        AlirOpcode::Eq => {
                            let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let b1_res = builder.ins().icmp(cranelift::codegen::ir::condcodes::IntCC::Equal, lhs, rhs);
                                let res = builder.ins().uextend(cranelift::codegen::ir::types::I64, b1_res);
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
                        AlirOpcode::Neq => {
                            let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let b1_res = builder.ins().icmp(cranelift::codegen::ir::condcodes::IntCC::NotEqual, lhs, rhs);
                                let res = builder.ins().uextend(cranelift::codegen::ir::types::I64, b1_res);
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
                        AlirOpcode::Lt => {
                            let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let b1_res = builder.ins().icmp(cranelift::codegen::ir::condcodes::IntCC::SignedLessThan, lhs, rhs);
                                let res = builder.ins().uextend(cranelift::codegen::ir::types::I64, b1_res);
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
                        AlirOpcode::Lte => {
                            let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let b1_res = builder.ins().icmp(cranelift::codegen::ir::condcodes::IntCC::SignedLessThanOrEqual, lhs, rhs);
                                let res = builder.ins().uextend(cranelift::codegen::ir::types::I64, b1_res);
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
                        AlirOpcode::Gt => {
                            let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let b1_res = builder.ins().icmp(cranelift::codegen::ir::condcodes::IntCC::SignedGreaterThan, lhs, rhs);
                                let res = builder.ins().uextend(cranelift::codegen::ir::types::I64, b1_res);
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
                        AlirOpcode::Gte => {
                            let lhs = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let rhs = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                            let b1_res = builder.ins().icmp(cranelift::codegen::ir::condcodes::IntCC::SignedGreaterThanOrEqual, lhs, rhs);
                                let res = builder.ins().uextend(cranelift::codegen::ir::types::I64, b1_res);
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
                        AlirOpcode::Ret => {
                            if !inst.op1.is_null() {
                                let v = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                                builder.ins().return_(&[v]);
                            } else {
                                let v = builder.ins().iconst(cranelift::codegen::ir::types::I64, 0);
                                builder.ins().return_(&[v]);
                            }
                            terminated = true; break;
                        },
                        AlirOpcode::Jump => {
                            if !inst.op1.is_null() {
                                let v1 = unsafe { &*inst.op1 };
                                if v1.kind == 6 && v1.val != 0 { // ALIR_VAL_LABEL
                                    let lbl = unsafe { CStr::from_ptr(v1.val as *const c_char) }.to_string_lossy().into_owned();
                                    if let Some(dest_block) = block_map.get(&lbl) {
                                        builder.ins().jump(*dest_block, &[]);
                                        terminated = true; break;
                                    }
                                }
                            }
                        },
                        AlirOpcode::Condi => {
                            if !inst.op1.is_null() && !inst.op2.is_null() && inst.arg_count > 0 {
                                let cond = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                                let v2 = unsafe { &*inst.op2 };
                                let v3 = unsafe { &*(*inst.args.offset(0)) };
                                if v2.kind == 6 && v3.kind == 6 && v2.val != 0 && v3.val != 0 {
                                    let then_lbl = unsafe { CStr::from_ptr(v2.val as *const c_char) }.to_string_lossy().into_owned();
                                    let else_lbl = unsafe { CStr::from_ptr(v3.val as *const c_char) }.to_string_lossy().into_owned();

                                    if let (Some(then_block), Some(else_block)) = (block_map.get(&then_lbl), block_map.get(&else_lbl)) {
                                        let c = builder.ins().icmp_imm(cranelift::codegen::ir::condcodes::IntCC::NotEqual, cond, 0);
                                        builder.ins().brif(c, *then_block, &[], *else_block, &[]);
                                        terminated = true; break;
                                    }
                                }
                            }
                        },
                        AlirOpcode::Alloca => {
                            let slot = builder.create_sized_stack_slot(cranelift::codegen::ir::StackSlotData::new(
                                cranelift::codegen::ir::StackSlotKind::ExplicitSlot,
                                1024 // Big enough for structs in Cranelift for now
                            ));
                            let ptr = builder.ins().stack_addr(cranelift::codegen::ir::types::I64, slot, 0);
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
                                builder.def_var(var, ptr);
                            }
                        },
                        AlirOpcode::Store => {
                            if !inst.op1.is_null() && !inst.op2.is_null() {
                                let val = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                                let ptr = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                                builder.ins().store(cranelift::codegen::ir::MemFlags::new(), val, ptr, 0);
                            }
                        },
                        AlirOpcode::Load => {
                            if !inst.op1.is_null() {
                                let ptr = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                                let val = builder.ins().load(cranelift::codegen::ir::types::I64, cranelift::codegen::ir::MemFlags::new(), ptr, 0);
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
                                builder.def_var(var, val);
                            }
                            }
                        },
                        AlirOpcode::GetPtr => {
                            if !inst.op1.is_null() {
                                let mut ptr = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                                if !inst.op2.is_null() {
                                    let idx = get_val(inst.op2, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
                                    let offset = builder.ins().imul_imm(idx, 8); // Assuming 8-byte elements for now
                                    ptr = builder.ins().iadd(ptr, offset);
                                }
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
                                builder.def_var(var, ptr);
                            }
                            }
                        },
                        AlirOpcode::Call => {
                            if !inst.op1.is_null() {
                                let v1 = unsafe { &*inst.op1 };

                                let mut func_name_opt = None;
                                if (v1.kind == 4 || v1.kind == 9) && v1.val != 0 { // ALIR_VAL_VAR or ALIR_VAL_GLOBAL
                                    func_name_opt = Some(unsafe { CStr::from_ptr(v1.val as *const c_char) }.to_string_lossy().into_owned());
                                }
                                println!("DEBUG: ALIR_OP_CALL func_name={:?} v1.kind={}", func_name_opt, v1.kind);

                                if let Some(func_name) = func_name_opt {

                                    let mut sig = module.make_signature();
                                    for _ in 0..inst.arg_count {
                                        sig.params.push(cranelift::codegen::ir::AbiParam::new(cranelift::codegen::ir::types::I64));
                                    }
                                    sig.returns.push(cranelift::codegen::ir::AbiParam::new(cranelift::codegen::ir::types::I64));

                                    let sig_ref = builder.import_signature(sig.clone());

                                    let callee_id = if let Some(id) = func_map.get(&func_name) {
                                        *id
                                    } else {
                                        module.declare_function(&func_name, cranelift_module::Linkage::Import, &sig).unwrap()
                                    };
                                    let local_callee = module.declare_func_in_func(callee_id, &mut builder.func);
                                    let func_ptr = builder.ins().func_addr(cranelift::codegen::ir::types::I64, local_callee);

                                    let mut call_args = Vec::new();
                                    for i in 0..inst.arg_count {
                                        let arg_v = unsafe { *(inst.args.offset(i as isize)) };
                                        call_args.push(get_val(arg_v, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id));
                                    }

                                    let call_inst = builder.ins().call_indirect(sig_ref, func_ptr, &call_args);
                                    let res = builder.inst_results(call_inst)[0];
                                    if !inst.dest.is_null() {
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
                                    }
                                }
                            }
                        },
                        AlirOpcode::Cast => {
                            if !inst.op1.is_null() && !inst.dest.is_null() {
                                let val = get_val(inst.op1, &mut builder, &mut module, &global_map, &mut var_map, &mut next_var_id);
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
                                builder.def_var(var, val);
                            }
                            }
                        },
                        _ => {}
                    }
                }

                curr_inst = inst.next;
            }

            // Fallback terminator if not explicitly terminated
            if !terminated {
                let v = builder.ins().iconst(cranelift::codegen::ir::types::I64, 0);
                builder.ins().return_(&[v]);
            }

            curr_block = b.next;
        }

        builder.seal_all_blocks();
        builder.finalize();

        module.define_function(func_id, &mut ctx).unwrap();
        module.clear_context(&mut ctx);

        curr_func = f.next;
    }

    // Write object file to disk
    let obj = module.finish();
    let obj_path = format!("{}.o", basename);
    if let Ok(mut file) = File::create(&obj_path) {
        let _ = file.write_all(&obj.emit().unwrap());
        println!("[Cranelift Rust Backend] Successfully wrote {}", obj_path);
    } else {
        println!("[Cranelift Rust Backend] Failed to write {}", obj_path);
        return -1;
    }

    0
    });
    match result {
        Ok(code) => code,
        Err(_) => -1,
    }
}

