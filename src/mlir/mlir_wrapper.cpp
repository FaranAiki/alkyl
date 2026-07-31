#include "mlir/mlir_wrapper.h"

#ifdef __has_include
#if __has_include(<mlir/IR/MLIRContext.h>)
#include <mlir/IR/MLIRContext.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/Arith/IR/Arith.h>
#include <mlir/Dialect/MemRef/IR/MemRef.h>
#define HAS_MLIR 1
#endif
#endif

extern "C" {

AlkylMlirContext alkyl_mlir_create_context() {
#ifdef HAS_MLIR
    mlir::MLIRContext* ctx = new mlir::MLIRContext();
    ctx->loadDialect<mlir::func::FuncDialect, mlir::arith::ArithDialect, mlir::memref::MemRefDialect>();
    return static_cast<void*>(ctx);
#else
    return nullptr;
#endif
}

AlkylMlirModule alkyl_mlir_create_module(AlkylMlirContext c_ctx, const char* name) {
#ifdef HAS_MLIR
    auto* ctx = static_cast<mlir::MLIRContext*>(c_ctx);
    mlir::OpBuilder builder(ctx);
    mlir::ModuleOp module = mlir::ModuleOp::create(builder.getUnknownLoc(), name);
    return static_cast<void*>(module.getOperation());
#else
    (void)c_ctx; (void)name;
    return nullptr;
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

// --- BUILDER WRAPPERS --- //

// We store a global builder for simplicity in this example, 
// though a real compiler would attach it to a custom struct.
#ifdef HAS_MLIR
static mlir::OpBuilder* global_builder = nullptr;
#endif

AlkylMlirFunc alkyl_mlir_add_function(AlkylMlirContext c_ctx, AlkylMlirModule mod, const char* name) {
#ifdef HAS_MLIR
    auto* ctx = static_cast<mlir::MLIRContext*>(c_ctx);
    auto* op = static_cast<mlir::Operation*>(mod);
    auto moduleOp = mlir::cast<mlir::ModuleOp>(op);
    
    if (!global_builder) {
        global_builder = new mlir::OpBuilder(ctx);
    }
    
    global_builder->setInsertionPointToEnd(moduleOp.getBody());
    
    // Creating a func returning void, taking no arguments (just as a mock)
    auto funcType = mlir::FunctionType::get(ctx, std::nullopt, std::nullopt);
    auto funcOp = global_builder->create<mlir::func::FuncOp>(global_builder->getUnknownLoc(), name, funcType);
    
    return static_cast<void*>(funcOp.getOperation());
#else
    (void)c_ctx; (void)mod; (void)name;
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
        if (val) {
            auto v = static_cast<mlir::Value>(reinterpret_cast<mlir::detail::ValueImpl*>(val));
            global_builder->create<mlir::func::ReturnOp>(global_builder->getUnknownLoc(), v);
        } else {
            global_builder->create<mlir::func::ReturnOp>(global_builder->getUnknownLoc());
        }
    }
#else
    (void)c_ctx; (void)val;
#endif
}

AlkylMlirValue alkyl_mlir_build_alloca(AlkylMlirContext c_ctx, const char* name) {
#ifdef HAS_MLIR
    if (!global_builder) return nullptr;
    auto* ctx = static_cast<mlir::MLIRContext*>(c_ctx);
    auto type = mlir::IntegerType::get(ctx, 32); // Mock: Always int32
    auto memrefType = mlir::MemRefType::get({}, type);
    auto alloca = global_builder->create<mlir::memref::AllocaOp>(global_builder->getUnknownLoc(), memrefType);
    return reinterpret_cast<void*>(alloca.getResult().getImpl());
#else
    (void)c_ctx; (void)name; return nullptr;
#endif
}

void alkyl_mlir_build_store(AlkylMlirContext c_ctx, AlkylMlirValue val, AlkylMlirValue ptr) {
#ifdef HAS_MLIR
    if (!global_builder) return;
    auto v = static_cast<mlir::Value>(reinterpret_cast<mlir::detail::ValueImpl*>(val));
    auto p = static_cast<mlir::Value>(reinterpret_cast<mlir::detail::ValueImpl*>(ptr));
    global_builder->create<mlir::memref::StoreOp>(global_builder->getUnknownLoc(), v, p, mlir::ValueRange{});
#else
    (void)c_ctx; (void)val; (void)ptr;
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
GEN_BINOP(alkyl_mlir_build_shl, mlir::arith::ShLIOp)
GEN_BINOP(alkyl_mlir_build_shr, mlir::arith::ShRSIOp)

void alkyl_mlir_build_scf_if(AlkylMlirContext c_ctx, AlkylMlirValue cond) {
#ifdef HAS_MLIR
    if (!global_builder) return;
    // Just a placeholder, as full SCF IF requires regions and yield ops
    // auto condition = static_cast<mlir::Value>(reinterpret_cast<mlir::detail::ValueImpl*>(cond));
    // global_builder->create<mlir::scf::IfOp>(...);
#else
    (void)c_ctx; (void)cond;
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

}
