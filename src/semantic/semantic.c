#include "semantic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

void scan_declarations(SemCtx *ctx, ASTNode *node, const char *prefix) {
    while(node) {
        if (node->type == NODE_FUNC_DEF) {
            FuncDefNode *fd = (FuncDefNode*)node;
            char *name = fd->name;
            char *qualified = NULL;
            if (prefix) {
                int len = strlen(prefix) + strlen(name) + 2;
                qualified = malloc(len);
                snprintf(qualified, len, "%s.%s", prefix, name);
                name = qualified;
            }
            
            // Mangling
            char *mangled = mangle_function(name, fd->params);
            fd->mangled_name = strdup(mangled);
            
            // Collect param types for resolution
            int pcount = 0;
            Parameter *p = fd->params;
            while(p) { pcount++; p = p->next; }
            
            VarType *ptypes = malloc(sizeof(VarType) * pcount);
            p = fd->params;
            int i = 0;
            while(p) { ptypes[i++] = p->type; p = p->next; }
            
            // Check redefinition (checking mangled name for overloads)
            SemFunc *exist = ctx->functions;
            while(exist) {
                if (strcmp(exist->mangled_name, mangled) == 0) {
                     sem_error(ctx, node, "Redefinition of function '%s' with same signature", name);
                }
                exist = exist->next;
            }
            
            add_func(ctx, name, mangled, fd->ret_type, ptypes, pcount);
            
            free(mangled);
            if (qualified) free(qualified);
        } 
        else if (node->type == NODE_CLASS) {
            ClassNode *cn = (ClassNode*)node;
            char *name = cn->name;
            char *qualified = NULL;
            if (prefix) {
                int len = strlen(prefix) + strlen(name) + 2;
                qualified = malloc(len);
                snprintf(qualified, len, "%s.%s", prefix, name);
                name = qualified;
            }
            
            add_class(ctx, name, cn->parent_name, cn->traits.names, cn->traits.count);
            
            SemClass *cls = find_sem_class(ctx, name);
            if (cls) {
                ASTNode *mem = cn->members;
                while(mem) {
                    if (mem->type == NODE_VAR_DECL) {
                        VarDeclNode *vd = (VarDeclNode*)mem;
                        SemSymbol *s = malloc(sizeof(SemSymbol));
                        s->name = strdup(vd->name);
                        s->type = vd->var_type;
                        s->is_mutable = vd->is_mutable;
                        s->is_array = vd->is_array;
                        s->next = cls->members;
                        cls->members = s;
                    }
                    mem = mem->next;
                }
            }

            scan_declarations(ctx, cn->members, name);

            if (qualified) free(qualified);
        }
        else if (node->type == NODE_NAMESPACE) {
             NamespaceNode *ns = (NamespaceNode*)node;
             char *new_prefix = ns->name;
             char *qualified = NULL;
             if (prefix) {
                 int len = strlen(prefix) + strlen(ns->name) + 2;
                 qualified = malloc(len);
                 snprintf(qualified, len, "%s.%s", prefix, ns->name);
                 new_prefix = qualified;
             }
             scan_declarations(ctx, ns->body, new_prefix);
             if (qualified) free(qualified);
        }
        else if (node->type == NODE_ENUM) {
            EnumNode *en = (EnumNode*)node;
            char *name = en->name;
            
            SemEnum *se = malloc(sizeof(SemEnum));
            se->name = strdup(name);
            se->members = NULL;
            se->next = ctx->enums;
            ctx->enums = se;

            EnumEntry *ent = en->entries;
            struct SemEnumMember **tail = &se->members;
            
            while(ent) {
                struct SemEnumMember *m = malloc(sizeof(struct SemEnumMember));
                m->name = strdup(ent->name);
                m->next = NULL;
                *tail = m;
                tail = &m->next;

                VarType vt = {TYPE_INT, 0, NULL};
                add_symbol_semantic(ctx, ent->name, vt, 0, 0, 0, en->base.line, en->base.col);
                ent = ent->next;
            }
        }

        node = node->next;
    }
}

void check_program(SemCtx *ctx, ASTNode *node) {
    while(node) {
        if (node->type == NODE_FUNC_DEF) {
            FuncDefNode *fd = (FuncDefNode*)node;
            ctx->current_func_ret_type = fd->ret_type;
            enter_scope(ctx);
            
            Parameter *p = fd->params;
            while(p) {
                add_symbol_semantic(ctx, p->name, p->type, 1, 0, 0, 0, 0); 
                p = p->next;
            }
            
            if (fd->class_name) {
                 ctx->current_class = fd->class_name;
            }

            check_stmt(ctx, fd->body);
            
            ctx->current_class = NULL;
            exit_scope(ctx);
        }
        else if (node->type == NODE_VAR_DECL) {
             check_stmt(ctx, node); 
        }
        else if (node->type == NODE_NAMESPACE) {
             check_program(ctx, ((NamespaceNode*)node)->body);
        }
        else if (node->type == NODE_CLASS) {
             // Basic class check (scan_declarations already handled sigs)
        }
        else {
            check_stmt(ctx, node);
        }
        node = node->next;
    }
}

int semantic_analysis(ASTNode *root, const char *source, const char *filename) {
    SemCtx ctx;
    ctx.current_scope = NULL;
    ctx.functions = NULL;
    ctx.classes = NULL;
    ctx.enums = NULL; 
    ctx.error_count = 0;
    ctx.in_loop = 0;
    ctx.current_class = NULL;
    ctx.source_code = source;
    ctx.filename = filename;
    
    enter_scope(&ctx);
    
    scan_declarations(&ctx, root, NULL);
    check_program(&ctx, root);
    
    exit_scope(&ctx);
    
    return ctx.error_count;
}
