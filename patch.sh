sed -i 's/hashmap_init(&ctx->string_pool, 1024);/hashmap_init(\&ctx->string_pool, 1024);\n    hashmap_init(\&ctx->error_table, 64);\n    ctx->settings.no_purge = false;/g' src/common/context.c
sed -i 's/case TYPE_VOID: base = "void"; break;/case TYPE_VOID: base = "void"; break;\n        case TYPE_ERROR: base = "error"; break;/g' src/semantic/table.c
sed -i 's/case TYPE_VOID: sb_append_fmt(sb, "void"); break;/case TYPE_VOID: sb_append_fmt(sb, "void"); break;\n        case TYPE_ERROR: sb_append_fmt(sb, "error"); break;/g' src/semantic/emitter.c
sed -i 's/case TYPE_VOID: sb_append(sb, "void"); break;/case TYPE_VOID: sb_append(sb, "void"); break;\n        case TYPE_ERROR: sb_append(sb, "error"); break;/g' src/parser/emitter.c
