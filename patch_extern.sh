sed -i 's/if f.block_count == 0 {/if f.is_extern != 0 {/g' src/codegen_cranelift/rust_backend/src/lib.rs
