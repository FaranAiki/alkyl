import re
with open("src/codegen_llvm/translate/expr.c", "r") as f:
    text = f.read()

text = text.replace("LLVMTypeKind src_kind = LLVMGetTypeKind(LLVMTypeOf(op1));", "LLVMTypeKind src_kind = LLVMGetTypeKind(LLVMTypeOf(op1));\n                printf(\"DEBUG: src_kind=%d, dst_kind=%d\\n\", src_kind, dst_kind);")

with open("src/codegen_llvm/translate/expr.c", "w") as f:
    f.write(text)
