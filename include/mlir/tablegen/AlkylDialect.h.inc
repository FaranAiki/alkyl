/*===- TableGen'erated file -------------------------------------*- C++ -*-===*\
|*                                                                            *|
|* Dialect Declarations                                                       *|
|*                                                                            *|
|* Automatically generated file, do not edit!                                 *|
|* From: AlkylDialect.td                                                      *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

namespace alkyl_mlir {

/// A custom dialect for the Alkyl programming language
/// The Alkyl dialect provides operations and types that natively model
///         Alkyl's specific semantics, such as pristine/tainted data flows,
///         pure/impure functions, and advanced macro expansions.
class AlkylDialect : public ::mlir::Dialect {
  explicit AlkylDialect(::mlir::MLIRContext *context);

  void initialize();
  friend class ::mlir::MLIRContext;
public:
  ~AlkylDialect() override;
  static constexpr ::llvm::StringLiteral getDialectNamespace() {
    return ::llvm::StringLiteral("alkyl");
  }
};

} // namespace alkyl_mlir
MLIR_DECLARE_EXPLICIT_TYPE_ID(::alkyl_mlir::AlkylDialect)
