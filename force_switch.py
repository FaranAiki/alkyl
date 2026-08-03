import re

with open('src/codegen_cranelift/rust_backend/src/lib.rs', 'r') as f:
    code = f.read()

code = code.replace(
    'builder.append_block_params_for_function_params(entry_block);\\n                \\n                // Bind function arguments to var_map'.replace('\\n', '\n'),
    'builder.append_block_params_for_function_params(entry_block);\\n                builder.switch_to_block(entry_block);\\n                \\n                // Bind function arguments to var_map'.replace('\\n', '\n')
)

with open('src/codegen_cranelift/rust_backend/src/lib.rs', 'w') as f:
    f.write(code)
