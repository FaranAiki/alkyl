module {
  func.func private @printf() -> i32
  func.func @main() -> i32 {
    %0 = arith.constant 0 : i32
    return %0 : i32
  }
}
