#include <mlir/IR/MLIRContext.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
int main() {
    mlir::MLIRContext ctx;
    ctx.loadDialect<mlir::LLVM::LLVMDialect>();
    return 0;
}
