import re

with open('src/codegen_cranelift/rust_backend/src/lib.rs', 'r') as f:
    code = f.read()

def replace_key(m):
    return '''let key = if v.kind == 4 {
                            unsafe { CStr::from_ptr(v.val as *const c_char) }.to_string_lossy().into_owned()
                        } else if v.kind == 5 {
                            format!("%t{}", v.temp_id)
                        } else {
                            format!("ptr_{:x}", v_ptr as usize)
                        };'''

code = re.sub(r'let key = if v\.kind == 4 \{.*?else \{.*?format!\("ptr_\{:x\}", v_ptr as usize\)\n\s*\};', replace_key, code, flags=re.DOTALL)

def replace_dest_key(m):
    return '''let key = if v_dest.kind == 4 {
                                    unsafe { CStr::from_ptr(v_dest.val as *const c_char) }.to_string_lossy().into_owned()
                                } else if v_dest.kind == 5 {
                                    format!("%t{}", v_dest.temp_id)
                                } else {
                                    format!("ptr_{:x}", inst.dest as usize)
                                };'''

code = re.sub(r'let key = if v_dest\.kind == 4 \{.*?else \{.*?format!\("ptr_\{:x\}", inst\.dest as usize\)\n\s*\};', replace_dest_key, code, flags=re.DOTALL)


with open('src/codegen_cranelift/rust_backend/src/lib.rs', 'w') as f:
    f.write(code)
