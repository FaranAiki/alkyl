import re

with open('src/codegen_cranelift/rust_backend/src/lib.rs', 'r') as f:
    code = f.read()

# 1. Fix bitcast
code = code.replace(
    'builder.ins().bitcast(cranelift::codegen::ir::types::F64, lhs)',
    'builder.ins().bitcast(cranelift::codegen::ir::types::F64, cranelift::codegen::ir::MemFlags::new(), lhs)'
)
code = code.replace(
    'builder.ins().bitcast(cranelift::codegen::ir::types::F64, rhs)',
    'builder.ins().bitcast(cranelift::codegen::ir::types::F64, cranelift::codegen::ir::MemFlags::new(), rhs)'
)
code = code.replace(
    'builder.ins().bitcast(cranelift::codegen::ir::types::I64, f_res)',
    'builder.ins().bitcast(cranelift::codegen::ir::types::I64, cranelift::codegen::ir::MemFlags::new(), f_res)'
)

# 2. Fix uext -> uextend
code = code.replace('builder.ins().uext(', 'builder.ins().uextend(')

# 3. Fix shadowed ptr name
code = code.replace('let ptr = inst.dest as usize;', 'let dest_ptr_addr = inst.dest as usize;')
code = code.replace('var_map.entry(ptr)', 'var_map.entry(dest_ptr_addr)')

with open('src/codegen_cranelift/rust_backend/src/lib.rs', 'w') as f:
    f.write(code)
