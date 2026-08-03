import re

with open('src/codegen_cranelift/rust_backend/src/lib.rs', 'r') as f:
    code = f.read()

code = code.replace(
    'let ptr = v_ptr as usize;\n                        let var = *var_map.entry(dest_ptr_addr).or_insert_with(|| {',
    'let dest_ptr_addr = v_ptr as usize;\n                        let var = *var_map.entry(dest_ptr_addr).or_insert_with(|| {'
)

with open('src/codegen_cranelift/rust_backend/src/lib.rs', 'w') as f:
    f.write(code)
