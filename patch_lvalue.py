import re
with open("src/alir/lvalue.c", "r") as f:
    text = f.read()

being_func = """
AlirValue* alir_gen_being(AlirCtx *ctx, BeingNode *bn) {
    AlirValue *op_val = alir_gen_expr(ctx, bn->operand);
    AlirValue *res = new_temp(ctx, bn->var_type);
    
    AlirInst *inst = mk_inst(ctx->module, ALIR_OP_BITCAST, res, op_val, NULL);
    inst->custom_flag = bn->endian;
    emit(ctx, inst);
    
    return res;
}
"""

text = text.replace("AlirValue* alir_gen_cast(AlirCtx *ctx, CastNode *cn) {", being_func + "\nAlirValue* alir_gen_cast(AlirCtx *ctx, CastNode *cn) {")
text = text.replace("case NODE_CAST: return alir_gen_cast(ctx, (CastNode*)node);", "case NODE_CAST: return alir_gen_cast(ctx, (CastNode*)node);\n        case NODE_BEING: return alir_gen_being(ctx, (BeingNode*)node);")

with open("src/alir/lvalue.c", "w") as f:
    f.write(text)
