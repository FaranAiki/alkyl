import sys

with open('src/codegen_cranelift/rust_backend/src/lib.rs', 'r') as f:
    content = f.read()

content = content.replace(
    'println!("Compiling function: {:?}", fname);',
    'println!("Compiling function: {:?}, blocks: {}, extern: {}, varargs: {}", fname, f.block_count, f.is_extern, f.is_varargs);'
)

with open('src/codegen_cranelift/rust_backend/src/lib.rs', 'w') as f:
    f.write(content)
