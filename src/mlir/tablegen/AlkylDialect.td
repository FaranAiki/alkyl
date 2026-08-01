#ifndef ALKYL_DIALECT_TD
#define ALKYL_DIALECT_TD

include "mlir/IR/OpBase.td"

def Alkyl_Dialect : Dialect {
    let name = "alkyl";
    let summary = "A custom dialect for the Alkyl programming language";
    let description = [{
        The Alkyl dialect provides operations and types that natively model
        Alkyl's specific semantics, such as pristine/tainted data flows,
        pure/impure functions, and advanced macro expansions.
    }];
    let cppNamespace = "::alkyl_mlir";
}

#endif // ALKYL_DIALECT_TD
