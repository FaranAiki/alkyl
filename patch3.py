with open("src/semantic/table.c", "r") as f:
    content = f.read()

content = content.replace(
    'hashmap_put((HashMap*)ctx->current_scope->symbol_map, sym->name, sym);',
    'hashmap_put((HashMap*)ctx->current_scope->symbol_map, sym->name, sym); if (strcmp(sym->name, "events") == 0) printf("DEBUG: INSERTED events into scope %p with type %s (sym=%p)\\n", ctx->current_scope, sym->type.class_name, sym);'
)

with open("src/semantic/table.c", "w") as f:
    f.write(content)
