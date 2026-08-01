#ifndef ALKYL_OPS_TD
#define ALKYL_OPS_TD

include "AlkylDialect.td"
include "mlir/Interfaces/SideEffectInterfaces.td"

class Alkyl_Op<string mnemonic, list<Trait> traits = []> :
    Op<Alkyl_Dialect, mnemonic, traits>;

// 1. Pure Call Op
def Alkyl_PureCallOp : Alkyl_Op<"call_pure", [Pure]> {
    let summary = "Calls a pure Alkyl function";
    let description = [{
        This operation represents a call to a function marked as `pure`.
        Because it has no side effects, MLIR can automatically fold it, 
        memoize it, or perform dead code elimination on it.
    }];
    let arguments = (ins SymbolRefAttr:$callee, Variadic<AnyType>:$operands);
    let results = (outs AnyType);
}

// 2. Untaint Op
def Alkyl_UntaintOp : Alkyl_Op<"untaint", [Pure]> {
    let summary = "Safely converts tainted data to pristine data";
    let description = [{
        This operation acts as a barrier or check that converts a tainted 
        value into a pristine value.
    }];
    let arguments = (ins AnyType:$tainted_val);
    let results = (outs AnyType:$pristine_val);
}

#endif // ALKYL_OPS_TD
