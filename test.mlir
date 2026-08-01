module {
  llvm.mlir.global internal constant @str0("Ayam is 0\nBurger is 1\nAyam ato ngak daging!\nanjay aman bosku!\n\00")
  llvm.func @printf(!llvm.ptr, ...) -> i32
  llvm.func @main() -> i32 {
    %0 = llvm.mlir.addressof @str0 : !llvm.ptr
    %1 = llvm.call @printf(%0) vararg(!llvm.func<i32 (!llvm.ptr, ...)>) : (!llvm.ptr) -> i32
    %2 = llvm.mlir.constant(0 : i32) : i32
    llvm.return %2 : i32
  }
}
