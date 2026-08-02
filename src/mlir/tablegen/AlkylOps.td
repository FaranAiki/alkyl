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

// 2. Untaint / Pristine Op
def Alkyl_PristineOp : Alkyl_Op<"pristine", [Pure]> {
    let summary = "Safely converts tainted data to pristine data";
    let description = [{
        This operation acts as a barrier or check that converts a tainted 
        value into a pristine value.
    }];
    let arguments = (ins AnyType:$tainted_val);
    let results = (outs AnyType:$pristine_val);
}

// 3. Taint Op
def Alkyl_TaintOp : Alkyl_Op<"taint", [Pure]> {
    let summary = "Marks pristine data as tainted";
    let description = [{
        Explicitly marks a value as tainted, restricting how it can be used
        downstream without going through a pristine check.
    }];
    let arguments = (ins AnyType:$pristine_val);
    let results = (outs AnyType:$tainted_val);
}

// 4. Enum Match Op
def Alkyl_EnumMatchOp : Alkyl_Op<"enum_match", [Pure]> {
    let summary = "Matches an enum variant";
    let description = [{
        Checks if the given enum instance matches a specific variant tag.
    }];
    let arguments = (ins AnyType:$enum_val, I32Attr:$variant_tag);
    let results = (outs I1:$is_match);
}

// 5. Class Alloc Op
def Alkyl_ClassAllocOp : Alkyl_Op<"class_alloc"> {
    let summary = "Allocates a new instance of a class";
    let description = [{
        Allocates memory for a class instance and returns a reference.
    }];
    let arguments = (ins StrAttr:$class_name);
    let results = (outs AnyType:$instance);
}

#endif // ALKYL_OPS_TD
