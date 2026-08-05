sed -i 's/hashmap_put(&ctx->symbol_map, s->name, s);/hashmap_put(\&ctx->symbol_map, s->name, s); printf("debug: alir: added symbol %s\\n", s->name);/' src/alir/utils.c
