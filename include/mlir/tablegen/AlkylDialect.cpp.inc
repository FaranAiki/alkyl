/*===- TableGen'erated file -------------------------------------*- C++ -*-===*\
|*                                                                            *|
|* Dialect Definitions                                                        *|
|*                                                                            *|
|* Automatically generated file, do not edit!                                 *|
|* From: AlkylDialect.td                                                      *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

MLIR_DEFINE_EXPLICIT_TYPE_ID(::alkyl_mlir::AlkylDialect)
namespace alkyl_mlir {


AlkylDialect::AlkylDialect(::mlir::MLIRContext *context)
    : ::mlir::Dialect(getDialectNamespace(), context, ::mlir::TypeID::get<AlkylDialect>())
    
     {
  
  initialize();
}

AlkylDialect::~AlkylDialect() = default;


} // namespace alkyl_mlir
