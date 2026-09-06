with open("src/semantic/check.c", "r") as f:
    content = f.read()

content = content.replace(
    '#include "../parser/link.h"\n',
    '#include "../parser/link.h"\n\nSemSymbol *g_wlr_output_events = NULL;\n'
)

content = content.replace(
    'printf("DEBUG: Skipping wlr_output\\n");',
    'printf("DEBUG: Skipping wlr_output\\n"); g_wlr_output_events = hashmap_get((HashMap*)existing->inner_scope->symbol_map, "events"); if (g_wlr_output_events) g_wlr_output_events->parent_name = g_wlr_output_events->type.class_name;'
)

content = content.replace(
    'sym = hashmap_get((HashMap*)class_sym->inner_scope->symbol_map, ma->member_name); debug_semantic("hashmap_get for %s returned %p (kind=%d)\\n", ma->member_name, sym, sym ? sym->kind : -1);',
    'sym = hashmap_get((HashMap*)class_sym->inner_scope->symbol_map, ma->member_name); debug_semantic("hashmap_get for %s returned %p (kind=%d)\\n", ma->member_name, sym, sym ? sym->kind : -1); if (g_wlr_output_events && g_wlr_output_events->type.class_name != g_wlr_output_events->parent_name) { printf("DEBUG: g_wlr_output_events mutation detected! class_name=%s (ptr=%p)\\n", g_wlr_output_events->type.class_name, g_wlr_output_events->type.class_name); g_wlr_output_events->parent_name = g_wlr_output_events->type.class_name; }'
)

with open("src/semantic/check.c", "w") as f:
    f.write(content)
