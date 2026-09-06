with open("src/semantic/modifier/class.c", "r") as f:
    content = f.read()

content = content.replace(
    'sem_symbolic_var_decl(ctx, mem);',
    'sem_symbolic_var_decl(ctx, mem); extern SemSymbol *g_wlr_output_events; if (g_wlr_output_events && g_wlr_output_events->type.class_name != g_wlr_output_events->parent_name) { printf("DEBUG: MUTATION FOUND AFTER sem_symbolic_var_decl in class %s\\n", class_sym->name); g_wlr_output_events->parent_name = g_wlr_output_events->type.class_name; }'
)

with open("src/semantic/modifier/class.c", "w") as f:
    f.write(content)
