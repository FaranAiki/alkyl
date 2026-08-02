#include "mlir/mlir_wrapper.h"

#ifdef ALKYL_ENABLE_MLIR
#include <mlir/IR/MLIRContext.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/Arith/IR/Arith.h>
#include <mlir/Dialect/MemRef/IR/MemRef.h>
#include <mlir/Dialect/SCF/IR/SCF.h>
#include <mlir/Dialect/ControlFlow/IR/ControlFlowOps.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#define HAS_MLIR 1
#include <vector>
#include <cstdint>
struct SwitchState {
    mlir::Value cond;
    mlir::Block* current_cond_block;
    mlir::Block* merge_block;
    mlir::Block* leak_source = nullptr;
    bool has_default_case = false;
};
#endif

#include "common/debug.h"

#ifdef HAS_MLIR
#include "mlir/tablegen/AlkylDialect.h.inc"

#define GET_OP_CLASSES
#include "mlir/tablegen/AlkylOps.h.inc"

#define GET_DIALECT_CLASSES
#include "mlir/tablegen/AlkylDialect.cpp.inc"

#define GET_OP_CLASSES
#include "mlir/tablegen/AlkylOps.cpp.inc"

namespace alkyl_mlir {
  void AlkylDialect::initialize() {
    addOperations<
#define GET_OP_LIST
#include "mlir/tablegen/AlkylOps.cpp.inc"
    >();
  }
}
#endif

extern "C" {

AlkylMlirContext alkyl_mlir_create_context() {
#ifdef HAS_MLIR
    mlir::MLIRContext* ctx = new mlir::MLIRContext();
    ctx->loadDialect<mlir::func::FuncDialect, mlir::arith::ArithDialect, mlir::memref::MemRefDialect, mlir::scf::SCFDialect, mlir::LLVM::LLVMDialect, mlir::cf::ControlFlowDialect>();
    ctx->getOrLoadDialect<alkyl_mlir::AlkylDialect>();
    return static_cast<void*>(ctx);
#else
    return reinterpret_cast<void*>(1);
#endif
}

AlkylMlirModule alkyl_mlir_create_module(AlkylMlirContext c_ctx, const char* name) {
#ifdef HAS_MLIR
    auto* ctx = static_cast<mlir::MLIRContext*>(c_ctx);
    mlir::OpBuilder builder(ctx);
    (void)name;
    mlir::ModuleOp module = mlir::ModuleOp::create(builder.getUnknownLoc());
    return static_cast<void*>(module.getOperation());
#else
    (void)c_ctx; (void)name;
    return reinterpret_cast<void*>(1);
#endif
}

void alkyl_mlir_destroy_context(AlkylMlirContext c_ctx) {
#ifdef HAS_MLIR
    if (c_ctx) {
        auto* ctx = static_cast<mlir::MLIRContext*>(c_ctx);
        delete ctx;
    }
#else
    (void)c_ctx;
#endif
}

void alkyl_mlir_dump_module(AlkylMlirModule c_mod, const char* filename) {
#ifdef HAS_MLIR
    if (!c_mod) return;
    auto op = static_cast<mlir::Operation*>(c_mod);
    auto moduleOp = mlir::cast<mlir::ModuleOp>(op);
    
    std::error_code ec;
    llvm::raw_fd_ostream dest(filename, ec, llvm::sys::fs::OF_None);
    if (ec) {
        llvm::errs() << "Could not open file: " << ec.message() << "\n";
        return;
    }
    moduleOp.print(dest);
#else
    (void)c_mod; (void)filename;
#endif
}

// --- BUILDER WRAPPERS --- //

// We store a global builder for simplicity in this example, 
// though a real compiler would attach it to a custom struct.
#ifdef HAS_MLIR
static mlir::OpBuilder* global_builder = nullptr;
#endif

AlkylMlirFunc alkyl_mlir_add_function(AlkylMlirContext c_ctx, AlkylMlirModule mod, const char* name, int is_extern, int num_args) {
#ifdef HAS_MLIR
    auto* ctx = static_cast<mlir::MLIRContext*>(c_ctx);
    auto* op = static_cast<mlir::Operation*>(mod);
    auto moduleOp = mlir::cast<mlir::ModuleOp>(op);
    
    auto existing = moduleOp.lookupSymbol<mlir::func::FuncOp>(name);
    if (existing) {
        return static_cast<void*>(existing.getOperation());
    }
    
    if (!global_builder) {
        global_builder = new mlir::OpBuilder(ctx);
    }
    
    global_builder->setInsertionPointToEnd(moduleOp.getBody());
    
    if (is_extern) {
        return nullptr;
    }
    
    // Creating a func returning i64, taking num_args arguments
    auto i64Type = mlir::IntegerType::get(ctx, 64);
    std::vector<mlir::Type> argTypes(num_args, i64Type);
    auto funcType = mlir::FunctionType::get(ctx, argTypes, mlir::TypeRange{i64Type});
    auto funcOp = global_builder->create<mlir::func::FuncOp>(global_builder->getUnknownLoc(), name, funcType);
    
    return static_cast<void*>(funcOp.getOperation());
#else
    (void)c_ctx; (void)mod; (void)name; (void)is_extern; (void)num_args;
    return nullptr;
#endif
}

AlkylMlirBlock alkyl_mlir_add_block(AlkylMlirFunc func) {
#ifdef HAS_MLIR
    auto* op = static_cast<mlir::Operation*>(func);
    auto funcOp = mlir::cast<mlir::func::FuncOp>(op);
    mlir::Block* block = funcOp.addEntryBlock();
    return static_cast<void*>(block);
#else
    (void)func;
    return nullptr;
#endif
}

AlkylMlirValue alkyl_mlir_get_arg(AlkylMlirFunc func, int index) {
#ifdef HAS_MLIR
    auto* op = static_cast<mlir::Operation*>(func);
    auto funcOp = mlir::cast<mlir::func::FuncOp>(op);
    if (index >= (int)funcOp.getNumArguments()) return nullptr;
    return reinterpret_cast<void*>(funcOp.getArgument(index).getAsOpaquePointer());
#else
    (void)func; (void)index; return nullptr;
#endif
}

void alkyl_mlir_set_insertion_point_to_end(AlkylMlirContext c_ctx, AlkylMlirBlock block) {
#ifdef HAS_MLIR
    if (global_builder) {
        global_builder->setInsertionPointToEnd(static_cast<mlir::Block*>(block));
    }
#else
    (void)c_ctx; (void)block;
#endif
}

void alkyl_mlir_build_return(AlkylMlirContext c_ctx, AlkylMlirValue val) {
#ifdef HAS_MLIR
    if (global_builder) {
        mlir::Block *block = global_builder->getBlock();
        if (block && !block->empty() && block->back().hasTrait<mlir::OpTrait::IsTerminator>()) {
            return;
        }
        if (val) {
            auto v = static_cast<mlir::Value>(reinterpret_cast<mlir::detail::ValueImpl*>(val));
            if (mlir::isa<mlir::MemRefType>(v.getType())) {
                auto extractOp = global_builder->create<mlir::memref::ExtractAlignedPointerAsIndexOp>(global_builder->getUnknownLoc(), v);
                v = global_builder->create<mlir::arith::IndexCastOp>(global_builder->getUnknownLoc(), global_builder->getI64Type(), extractOp.getResult());
            } else if (mlir::isa<mlir::LLVM::LLVMPointerType>(v.getType())) {
                v = global_builder->create<mlir::LLVM::PtrToIntOp>(global_builder->getUnknownLoc(), global_builder->getI64Type(), v);
            } else if (v.getType().isIntOrIndex()) {
                if (v.getType().getIntOrFloatBitWidth() < 64) {
                    v = global_builder->create<mlir::arith::ExtSIOp>(global_builder->getUnknownLoc(), global_builder->getI64Type(), v);
                }
            }
            global_builder->create<mlir::func::ReturnOp>(global_builder->getUnknownLoc(), v);
        } else {
            global_builder->create<mlir::func::ReturnOp>(global_builder->getUnknownLoc());
        }
    }
#else
    (void)c_ctx; (void)val;
#endif
}

int alkyl_mlir_is_terminated(AlkylMlirContext c_ctx) {
#ifdef HAS_MLIR
    if (!global_builder) return 0;
    mlir::Block *block = global_builder->getBlock();
    return (block && !block->empty() && block->back().hasTrait<mlir::OpTrait::IsTerminator>()) ? 1 : 0;
#else
    (void)c_ctx;
    return 0;
#endif
}

int alkyl_mlir_block_has_terminator(AlkylMlirBlock block) {
#ifdef HAS_MLIR
    if (!block) return 0;
    auto b = static_cast<mlir::Block*>(block);
    if (b->empty()) return 0;
    return b->back().hasTrait<mlir::OpTrait::IsTerminator>() ? 1 : 0;
#else
    (void)block;
    return 0;
#endif
}

AlkylMlirValue alkyl_mlir_build_alloca(AlkylMlirContext c_ctx, const char* name) {
#ifdef HAS_MLIR
    if (!global_builder) return nullptr;
    auto* ctx = static_cast<mlir::MLIRContext*>(c_ctx);
    mlir::Type type = mlir::IntegerType::get(ctx, 64); // Use i64 to hold pointers safely
    auto memrefType = mlir::MemRefType::get({}, type);
    auto alloca = global_builder->create<mlir::memref::AllocaOp>(global_builder->getUnknownLoc(), memrefType);
    return reinterpret_cast<void*>(alloca.getResult().getImpl());
#else
    (void)c_ctx; (void)name; return nullptr;
#endif
}

AlkylMlirValue alkyl_mlir_build_alloc_object(AlkylMlirContext c_ctx, int num_fields) {
#ifdef HAS_MLIR
    if (!global_builder) return nullptr;
    auto* ctx = static_cast<mlir::MLIRContext*>(c_ctx);
    auto type = mlir::IntegerType::get(ctx, 64);
    auto memrefType = mlir::MemRefType::get({num_fields}, type);
    auto alloca = global_builder->create<mlir::memref::AllocaOp>(global_builder->getUnknownLoc(), memrefType);
    return reinterpret_cast<void*>(alloca.getResult().getImpl());
#else
    (void)c_ctx; (void)num_fields; return nullptr;
#endif
}

void alkyl_mlir_build_store(AlkylMlirContext c_ctx, AlkylMlirValue val, AlkylMlirValue ptr) {
#ifdef HAS_MLIR
    if (!global_builder || !val || !ptr) return;
    auto v = static_cast<mlir::Value>(reinterpret_cast<mlir::detail::ValueImpl*>(val));
    auto p = static_cast<mlir::Value>(reinterpret_cast<mlir::detail::ValueImpl*>(ptr));
    if (!mlir::isa<mlir::MemRefType>(p.getType())) return;
    auto memrefType = mlir::cast<mlir::MemRefType>(p.getType());
    
    // If storing a memref (e.g. object), extract pointer as i64
    if (mlir::isa<mlir::MemRefType>(v.getType())) {
        auto extractOp = global_builder->create<mlir::memref::ExtractAlignedPointerAsIndexOp>(global_builder->getUnknownLoc(), v);
        v = global_builder->create<mlir::arith::IndexCastOp>(global_builder->getUnknownLoc(), global_builder->getI64Type(), extractOp.getResult());
    } else if (mlir::isa<mlir::LLVM::LLVMPointerType>(v.getType())) {
        v = global_builder->create<mlir::LLVM::PtrToIntOp>(global_builder->getUnknownLoc(), global_builder->getI64Type(), v);
    }
    
    // cast v to match elementType if they are both integers but different widths
    if (v.getType().isIntOrIndex() && memrefType.getElementType().isIntOrIndex()) {
        unsigned v_width = v.getType().getIntOrFloatBitWidth();
        unsigned p_width = memrefType.getElementType().getIntOrFloatBitWidth();
        if (v_width < p_width) {
            v = global_builder->create<mlir::arith::ExtSIOp>(global_builder->getUnknownLoc(), memrefType.getElementType(), v);
        } else if (v_width > p_width) {
            v = global_builder->create<mlir::arith::TruncIOp>(global_builder->getUnknownLoc(), memrefType.getElementType(), v);
        }
    }
    
    if (v.getType() != memrefType.getElementType()) return; // Ignore invalid store
    global_builder->create<mlir::memref::StoreOp>(global_builder->getUnknownLoc(), v, p, mlir::ValueRange{});
#else
    (void)c_ctx; (void)val; (void)ptr;
#endif
}

void alkyl_mlir_build_store_field(AlkylMlirContext c_ctx, AlkylMlirValue val, AlkylMlirValue ptr, int index) {
#ifdef HAS_MLIR
    if (!global_builder || !val || !ptr) return;
    auto* ctx = static_cast<mlir::MLIRContext*>(c_ctx);
    auto v = static_cast<mlir::Value>(reinterpret_cast<mlir::detail::ValueImpl*>(val));
    auto p = static_cast<mlir::Value>(reinterpret_cast<mlir::detail::ValueImpl*>(ptr));
    
    if (!mlir::isa<mlir::MemRefType>(p.getType())) {
        return; // Ignore invalid store to non-memref
    }
    
    auto index_type = mlir::IndexType::get(ctx);
    auto idx = global_builder->create<mlir::arith::ConstantOp>(global_builder->getUnknownLoc(), index_type, global_builder->getIntegerAttr(index_type, index));
    
    // cast v to i64 if needed
    if (!v.getType().isInteger(64) && !mlir::isa<mlir::LLVM::LLVMPointerType>(v.getType())) {
        v = global_builder->create<mlir::arith::ExtSIOp>(global_builder->getUnknownLoc(), mlir::IntegerType::get(ctx, 64), v);
    }
    
    // If v is a pointer, cast it to i64 so we can store it in memref<i64>
    if (mlir::isa<mlir::LLVM::LLVMPointerType>(v.getType())) {
        v = global_builder->create<mlir::LLVM::PtrToIntOp>(global_builder->getUnknownLoc(), mlir::IntegerType::get(ctx, 64), v);
    }

    global_builder->create<mlir::memref::StoreOp>(global_builder->getUnknownLoc(), v, p, mlir::ValueRange{idx.getResult()});
#else
    (void)c_ctx; (void)val; (void)ptr; (void)index;
#endif
}

AlkylMlirValue alkyl_mlir_build_load_field(AlkylMlirContext c_ctx, AlkylMlirValue ptr, int index, int is_string) {
#ifdef HAS_MLIR
    if (!global_builder || !ptr) return nullptr;
    auto* ctx = static_cast<mlir::MLIRContext*>(c_ctx);
    auto p = static_cast<mlir::Value>(reinterpret_cast<mlir::detail::ValueImpl*>(ptr));
    
    mlir::Value res;
    if (mlir::isa<mlir::MemRefType>(p.getType())) {
        auto index_type = mlir::IndexType::get(ctx);
        auto idx = global_builder->create<mlir::arith::ConstantOp>(global_builder->getUnknownLoc(), index_type, global_builder->getIntegerAttr(index_type, index));
        auto loadOp = global_builder->create<mlir::memref::LoadOp>(global_builder->getUnknownLoc(), p, mlir::ValueRange{idx.getResult()});
        res = loadOp.getResult();
    } else {
        // Assume p is i64
        auto i64Type = global_builder->getI64Type();
        if (p.getType() != i64Type) {
            p = global_builder->create<mlir::arith::ExtSIOp>(global_builder->getUnknownLoc(), i64Type, p);
        }
        auto ptrType = mlir::LLVM::LLVMPointerType::get(ctx);
        auto rawPtr = global_builder->create<mlir::LLVM::IntToPtrOp>(global_builder->getUnknownLoc(), ptrType, p);
        
        auto idx = global_builder->create<mlir::arith::ConstantOp>(global_builder->getUnknownLoc(), global_builder->getI32Type(), global_builder->getI32IntegerAttr(index));
        auto gep = global_builder->create<mlir::LLVM::GEPOp>(global_builder->getUnknownLoc(), ptrType, i64Type, rawPtr, mlir::ValueRange{idx.getResult()});
        auto loadOp = global_builder->create<mlir::LLVM::LoadOp>(global_builder->getUnknownLoc(), i64Type, gep);
        res = loadOp.getResult();
    }
    
    if (is_string) {
        res = global_builder->create<mlir::LLVM::IntToPtrOp>(global_builder->getUnknownLoc(), mlir::LLVM::LLVMPointerType::get(ctx), res);
    }
    
    return reinterpret_cast<void*>(res.getImpl());
#else
    (void)c_ctx; (void)ptr; (void)index; (void)is_string; return nullptr;
#endif
}

AlkylMlirValue alkyl_mlir_build_int_constant(AlkylMlirContext c_ctx, int val) {
#ifdef HAS_MLIR
    if (!global_builder) return nullptr;
    auto* ctx = static_cast<mlir::MLIRContext*>(c_ctx);
    auto type = mlir::IntegerType::get(ctx, 32);
    auto cst = global_builder->create<mlir::arith::ConstantOp>(global_builder->getUnknownLoc(), type, global_builder->getIntegerAttr(type, val));
    return reinterpret_cast<void*>(cst.getResult().getImpl());
#else
    (void)c_ctx; (void)val; return nullptr;
#endif
}
AlkylMlirValue alkyl_mlir_build_string_constant(AlkylMlirContext c_ctx, AlkylMlirModule mod, const char* str) {
#ifdef HAS_MLIR
    if (!global_builder) return nullptr;
    auto* ctx = static_cast<mlir::MLIRContext*>(c_ctx);
    auto* op = static_cast<mlir::Operation*>(mod);
    auto moduleOp = mlir::cast<mlir::ModuleOp>(op);

    static int str_counter = 0;
    std::string sym_name = "str" + std::to_string(str_counter++);
    
    auto ip = global_builder->saveInsertionPoint();
    global_builder->setInsertionPointToStart(moduleOp.getBody());
    
    llvm::StringRef strRef(str);
    auto i8Type = mlir::IntegerType::get(ctx, 8);
    auto arrayType = mlir::LLVM::LLVMArrayType::get(i8Type, strRef.size() + 1);
    
    // Create global string
    auto globalOp = global_builder->create<mlir::LLVM::GlobalOp>(
        global_builder->getUnknownLoc(),
        arrayType,
        true, // isConstant
        mlir::LLVM::Linkage::Internal,
        sym_name,
        mlir::StringAttr::get(ctx, llvm::StringRef(str, strRef.size() + 1)),
        0 // alignment
    );
    
    global_builder->restoreInsertionPoint(ip);
    
    auto addr = global_builder->create<mlir::LLVM::AddressOfOp>(
        global_builder->getUnknownLoc(),
        mlir::LLVM::LLVMPointerType::get(ctx),
        mlir::SymbolRefAttr::get(ctx, sym_name)
    );
    
    return reinterpret_cast<void*>(addr.getResult().getImpl());
#else
    (void)c_ctx; (void)mod; (void)str; return nullptr;
#endif
}

#ifdef HAS_MLIR
#define GEN_BINOP(func_name, op_class) \
    AlkylMlirValue func_name(AlkylMlirContext c_ctx, AlkylMlirValue lhs, AlkylMlirValue rhs) { \
        if (!global_builder) return nullptr; \
        auto l = static_cast<mlir::Value>(reinterpret_cast<mlir::detail::ValueImpl*>(lhs)); \
        auto r = static_cast<mlir::Value>(reinterpret_cast<mlir::detail::ValueImpl*>(rhs)); \
        auto op = global_builder->create<op_class>(global_builder->getUnknownLoc(), l, r); \
        return reinterpret_cast<void*>(op.getResult().getImpl()); \
    }
#else
#define GEN_BINOP(func_name, op_class) \
    AlkylMlirValue func_name(AlkylMlirContext c_ctx, AlkylMlirValue lhs, AlkylMlirValue rhs) { \
        (void)c_ctx; (void)lhs; (void)rhs; return nullptr; \
    }
#endif

GEN_BINOP(alkyl_mlir_build_add, mlir::arith::AddIOp)
GEN_BINOP(alkyl_mlir_build_sub, mlir::arith::SubIOp)
GEN_BINOP(alkyl_mlir_build_mul, mlir::arith::MulIOp)
GEN_BINOP(alkyl_mlir_build_div, mlir::arith::DivSIOp)
GEN_BINOP(alkyl_mlir_build_mod, mlir::arith::RemSIOp)
GEN_BINOP(alkyl_mlir_build_shl, mlir::arith::ShLIOp)
GEN_BINOP(alkyl_mlir_build_shr, mlir::arith::ShRSIOp)
GEN_BINOP(alkyl_mlir_build_and, mlir::arith::AndIOp)
GEN_BINOP(alkyl_mlir_build_or, mlir::arith::OrIOp)
GEN_BINOP(alkyl_mlir_build_xor, mlir::arith::XOrIOp)

AlkylMlirValue alkyl_mlir_build_load(AlkylMlirContext c_ctx, AlkylMlirValue ptr) {
#ifdef HAS_MLIR
    if (!global_builder) return nullptr;
    auto p = static_cast<mlir::Value>(reinterpret_cast<mlir::detail::ValueImpl*>(ptr));
    auto load = global_builder->create<mlir::memref::LoadOp>(global_builder->getUnknownLoc(), p, mlir::ValueRange{});
    return reinterpret_cast<void*>(load.getResult().getImpl());
#else
    (void)c_ctx; (void)ptr; return nullptr;
#endif
}

AlkylMlirValue alkyl_mlir_build_call(AlkylMlirContext c_ctx, const char* name, AlkylMlirValue* args, int num_args) {
#ifdef HAS_MLIR
    if (!global_builder) return nullptr;
    auto* ctx = static_cast<mlir::MLIRContext*>(c_ctx);
    
    std::vector<mlir::Value> operands;
    for (int i = 0; i < num_args; i++) {
        auto val = static_cast<mlir::Value>(reinterpret_cast<mlir::detail::ValueImpl*>(args[i]));
        if (mlir::isa<mlir::MemRefType>(val.getType())) {
            auto extractOp = global_builder->create<mlir::memref::ExtractAlignedPointerAsIndexOp>(global_builder->getUnknownLoc(), val);
            val = global_builder->create<mlir::arith::IndexCastOp>(global_builder->getUnknownLoc(), global_builder->getI64Type(), extractOp.getResult());
        } else if (mlir::isa<mlir::LLVM::LLVMPointerType>(val.getType())) {
            val = global_builder->create<mlir::LLVM::PtrToIntOp>(global_builder->getUnknownLoc(), global_builder->getI64Type(), val);
        } else if (val.getType().isIntOrIndex()) {
            if (val.getType().getIntOrFloatBitWidth() < 64) {
                val = global_builder->create<mlir::arith::ExtSIOp>(global_builder->getUnknownLoc(), global_builder->getI64Type(), val);
            }
        }
        operands.push_back(val);
    }
    
    // For now, assume returning i64 and all args i64
    auto type = mlir::IntegerType::get(ctx, 64);
    
    // Auto-declare if missing or redefine if arg count mismatch is expected
    if (auto block = global_builder->getBlock()) {
        if (auto parent = block->getParentOp()) {
            if (auto moduleOp = parent->getParentOfType<mlir::ModuleOp>()) {
                auto existingFunc = moduleOp.lookupSymbol<mlir::func::FuncOp>(name);
                auto existingLLVM = moduleOp.lookupSymbol<mlir::LLVM::LLVMFuncOp>(name);
                
                if (!existingFunc && !existingLLVM) {
                    auto ip = global_builder->saveInsertionPoint();
                    global_builder->setInsertionPointToEnd(moduleOp.getBody());
                    
                    auto llvmFuncType = mlir::LLVM::LLVMFunctionType::get(type, llvm::ArrayRef<mlir::Type>{}, /*isVarArg=*/true);
                    global_builder->create<mlir::LLVM::LLVMFuncOp>(global_builder->getUnknownLoc(), name, llvmFuncType);
                    
                    global_builder->restoreInsertionPoint(ip);
                }
                
                if (!existingFunc) {
                    auto call = global_builder->create<mlir::LLVM::CallOp>(
                        global_builder->getUnknownLoc(), mlir::TypeRange{type}, mlir::SymbolRefAttr::get(ctx, name), operands);
                    auto llvmFuncType = mlir::LLVM::LLVMFunctionType::get(type, llvm::ArrayRef<mlir::Type>{}, /*isVarArg=*/true);
                    call->setAttr("var_callee_type", mlir::TypeAttr::get(llvmFuncType));
                    mlir::Value v = call.getResult();
                    return static_cast<void*>(v.getAsOpaquePointer());
                }
            }
        }
    }
    
    auto call = global_builder->create<mlir::func::CallOp>(
        global_builder->getUnknownLoc(), name, mlir::TypeRange{type}, operands);
        
    if (call.getNumResults() > 0)
        return reinterpret_cast<void*>(call.getResult(0).getImpl());
    return nullptr;
#else
    (void)c_ctx; (void)name; (void)args; (void)num_args; return nullptr;
#endif
}

void* alkyl_mlir_build_scf_if_start(AlkylMlirContext c_ctx, AlkylMlirValue cond, int has_else) {
#ifdef HAS_MLIR
    if (!global_builder) return nullptr;
    auto condition = static_cast<mlir::Value>(reinterpret_cast<mlir::detail::ValueImpl*>(cond));
    
    auto bool_cond = condition;
    if (!condition.getType().isInteger(1)) {
        auto zero = global_builder->create<mlir::arith::ConstantIntOp>(global_builder->getUnknownLoc(), condition.getType(), 0);
        bool_cond = global_builder->create<mlir::arith::CmpIOp>(global_builder->getUnknownLoc(), mlir::arith::CmpIPredicate::ne, condition, zero);
    }
    
    auto ifOp = global_builder->create<mlir::scf::IfOp>(global_builder->getUnknownLoc(), bool_cond, has_else != 0);
    auto &then_block = ifOp.getThenRegion().front();
    
    if (!then_block.empty()) {
        global_builder->setInsertionPoint(&then_block, std::prev(then_block.end()));
    } else {
        global_builder->setInsertionPointToStart(&then_block);
    }
    
    if (has_else) {
        auto &else_block = ifOp.getElseRegion().front();
        if (!else_block.empty()) {
            global_builder->setInsertionPoint(&else_block, std::prev(else_block.end()));
        } else {
            global_builder->setInsertionPointToStart(&else_block);
        }
    }
    
    return reinterpret_cast<void*>(ifOp.getOperation());
#else
    (void)c_ctx; (void)cond; return reinterpret_cast<void*>(1);
#endif
}

void alkyl_mlir_build_scf_if_else(AlkylMlirContext c_ctx, void* if_op_ptr) {
#ifdef HAS_MLIR
    if (!global_builder || !if_op_ptr) return;
    auto op = static_cast<mlir::Operation*>(if_op_ptr);
    auto ifOp = mlir::cast<mlir::scf::IfOp>(op);
    
    auto &else_block = ifOp.getElseRegion().front();
    if (!else_block.empty()) {
        global_builder->setInsertionPoint(&else_block, std::prev(else_block.end()));
    } else {
        global_builder->setInsertionPointToStart(&else_block);
    }
#else
    (void)c_ctx; (void)if_op_ptr;
#endif
}

void alkyl_mlir_build_scf_if_end(AlkylMlirContext c_ctx, void* if_op_ptr) {
#ifdef HAS_MLIR
    if (!global_builder || !if_op_ptr) return;
    auto op = static_cast<mlir::Operation*>(if_op_ptr);
    auto ifOp = mlir::cast<mlir::scf::IfOp>(op);
    global_builder->setInsertionPointAfter(ifOp);
#else
    (void)c_ctx; (void)if_op_ptr;
#endif
}

void alkyl_mlir_build_scf_while(AlkylMlirContext c_ctx, AlkylMlirValue cond) {
#ifdef HAS_MLIR
    if (!global_builder) return;
    // Placeholder for SCF While
#else
    (void)c_ctx; (void)cond;
#endif
}

void* alkyl_mlir_build_switch_start(AlkylMlirContext c_ctx, AlkylMlirValue cond, int num_cases) {
#ifdef HAS_MLIR
    if (!global_builder) return nullptr;
    auto state = new SwitchState();
    state->cond = static_cast<mlir::Value>(reinterpret_cast<mlir::detail::ValueImpl*>(cond));
    
    mlir::Block* currentBlock = global_builder->getBlock();
    mlir::Region* parentRegion = currentBlock->getParent();
    state->merge_block = new mlir::Block();
    parentRegion->push_back(state->merge_block);
    state->current_cond_block = currentBlock;
    
    return state;
#else
    (void)c_ctx; (void)cond; (void)num_cases; return reinterpret_cast<void*>(1);
#endif
}

void alkyl_mlir_build_switch_set_cond_insertion(AlkylMlirContext c_ctx, void* switch_op_ptr) {
#ifdef HAS_MLIR
    if (!global_builder || !switch_op_ptr) return;
    auto state = static_cast<SwitchState*>(switch_op_ptr);
    global_builder->setInsertionPointToEnd(state->current_cond_block);
#else
    (void)c_ctx; (void)switch_op_ptr;
#endif
}

void alkyl_mlir_build_switch_case_start(AlkylMlirContext c_ctx, void* switch_op_ptr, AlkylMlirValue val, int is_leak) {
#ifdef HAS_MLIR
    if (!global_builder || !switch_op_ptr) return;
    auto state = static_cast<SwitchState*>(switch_op_ptr);
    auto case_val = static_cast<mlir::Value>(reinterpret_cast<mlir::detail::ValueImpl*>(val));
    mlir::Region* parentRegion = state->current_cond_block->getParent();
    
    mlir::Block* case_block = new mlir::Block();
    mlir::Block* next_cond_block = new mlir::Block();
    
    // Insert before merge_block
    parentRegion->getBlocks().insert(mlir::Region::iterator(state->merge_block), case_block);
    parentRegion->getBlocks().insert(mlir::Region::iterator(state->merge_block), next_cond_block);
    
    global_builder->setInsertionPointToEnd(state->current_cond_block);
    
    // Ensure same types for cmpi
    if (state->cond.getType() != case_val.getType()) {
        auto cond_type = state->cond.getType();
        auto case_type = case_val.getType();
        if (cond_type.isIntOrIndex() && case_type.isIntOrIndex()) {
            unsigned cond_width = cond_type.getIntOrFloatBitWidth();
            unsigned case_width = case_type.getIntOrFloatBitWidth();
            if (cond_width > case_width) {
                case_val = global_builder->create<mlir::arith::ExtSIOp>(global_builder->getUnknownLoc(), cond_type, case_val);
            } else if (cond_width < case_width) {
                case_val = global_builder->create<mlir::arith::TruncIOp>(global_builder->getUnknownLoc(), cond_type, case_val);
            }
        }
    }
    
    auto cmp = global_builder->create<mlir::arith::CmpIOp>(global_builder->getUnknownLoc(), mlir::arith::CmpIPredicate::eq, state->cond, case_val);
    global_builder->create<mlir::cf::CondBranchOp>(global_builder->getUnknownLoc(), cmp, case_block, next_cond_block);
    
    if (state->leak_source) {
        global_builder->setInsertionPointToEnd(state->leak_source);
        global_builder->create<mlir::cf::BranchOp>(global_builder->getUnknownLoc(), case_block);
        state->leak_source = nullptr;
    }
    
    global_builder->setInsertionPointToEnd(case_block);
    state->current_cond_block = next_cond_block;
#else
    (void)c_ctx; (void)switch_op_ptr; (void)val; (void)is_leak;
#endif
}

void alkyl_mlir_build_switch_case_end(AlkylMlirContext c_ctx, void* switch_op_ptr, int is_leak) {
#ifdef HAS_MLIR
    if (!global_builder || !switch_op_ptr) return;
    auto state = static_cast<SwitchState*>(switch_op_ptr);
    if (!is_leak) {
        global_builder->create<mlir::cf::BranchOp>(global_builder->getUnknownLoc(), state->merge_block);
    } else {
        state->leak_source = global_builder->getBlock();
    }
#else
    (void)c_ctx; (void)switch_op_ptr; (void)is_leak;
#endif
}

void alkyl_mlir_build_switch_default_start(AlkylMlirContext c_ctx, void* switch_op_ptr) {
#ifdef HAS_MLIR
    if (!global_builder || !switch_op_ptr) return;
    auto state = static_cast<SwitchState*>(switch_op_ptr);
    
    // Default block is just the current next_cond_block
    // but actually, we should just let it be a new block, or jump from current_cond_block directly.
    mlir::Block* default_block = new mlir::Block();
    mlir::Region* parentRegion = state->current_cond_block->getParent();
    parentRegion->getBlocks().insert(mlir::Region::iterator(state->merge_block), default_block);
    
    global_builder->setInsertionPointToEnd(state->current_cond_block);
    global_builder->create<mlir::cf::BranchOp>(global_builder->getUnknownLoc(), default_block);
    state->has_default_case = true;
    
    if (state->leak_source) {
        global_builder->setInsertionPointToEnd(state->leak_source);
        global_builder->create<mlir::cf::BranchOp>(global_builder->getUnknownLoc(), default_block);
        state->leak_source = nullptr;
    }
    
    global_builder->setInsertionPointToEnd(default_block);
#else
    (void)c_ctx; (void)switch_op_ptr;
#endif
}

void alkyl_mlir_build_switch_default_end(AlkylMlirContext c_ctx, void* switch_op_ptr) {
#ifdef HAS_MLIR
    if (!global_builder || !switch_op_ptr) return;
    auto state = static_cast<SwitchState*>(switch_op_ptr);
    global_builder->create<mlir::cf::BranchOp>(global_builder->getUnknownLoc(), state->merge_block);
#else
    (void)c_ctx; (void)switch_op_ptr;
#endif
}

void alkyl_mlir_build_switch_end(AlkylMlirContext c_ctx, void* switch_op_ptr) {
#ifdef HAS_MLIR
    if (!global_builder || !switch_op_ptr) return;
    auto state = static_cast<SwitchState*>(switch_op_ptr);
    
    if (!state->has_default_case) {
        global_builder->setInsertionPointToEnd(state->current_cond_block);
        global_builder->create<mlir::cf::BranchOp>(global_builder->getUnknownLoc(), state->merge_block);
    }
    
    if (state->leak_source) {
        global_builder->setInsertionPointToEnd(state->leak_source);
        global_builder->create<mlir::cf::BranchOp>(global_builder->getUnknownLoc(), state->merge_block);
        state->leak_source = nullptr;
    }
    
    global_builder->setInsertionPointToEnd(state->merge_block);
    delete state;
#else
    (void)c_ctx; (void)switch_op_ptr;
#endif
}

}

