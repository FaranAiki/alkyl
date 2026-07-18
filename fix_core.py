import re

content = """1: #include "parser_internal.h"
2: #include <string.h>
3: #include <stdio.h>
4: #include <stdlib.h>
5: 
6: void parser_init(Parser *p, Lexer *l, ParserSettings *settings) {
7:     p->l = l;
8:     p->ctx = l->ctx;
9:     p->has_error = 0;
10:     p->macro_head = NULL;
11:     p->type_head = NULL;
12:     p->alias_head = NULL;
13:     p->expansion_head = NULL;
14:     p->disable_macro_expansion = 0;
15:     
16:     p->tokens = NULL;
17:     p->token_count = 0;
18:     p->token_capacity = 0;
19:     p->token_pos = 0;
20:     
21:     if (p->ctx && p->ctx->arena) {
22:         hashmap_init(&p->types_map, p->ctx->arena, 64);
23:     } else {
24:         hashmap_init(&p->types_map, NULL, 64);
25:     }
26:     
27:     p->pending_cconv = NULL;
28:     
29:     if (settings) {
30:         p->settings = *settings;
31:     } else {
32:         // Defaults
33:         p->settings.require_parens_for_conditions = 0;
34:         p->settings.allow_implicit_return = 0;
35:         p->settings.allow_postfix_types = 0;
36:         p->settings.strict_boolean_conditions = 0;
37:     }
38:     
39:     if (l) {
40:         p->current_token.type = TOKEN_UNKNOWN;
41:     }
42: }
43: 
44: void* parser_alloc(Parser *p, size_t size) {
45:     if (!p || !p->ctx || !p->ctx->arena) return calloc(1, size);
46:     void *ptr = arena_alloc(p->ctx->arena, size);
47:     if (ptr) memset(ptr, 0, size);
48:     return ptr;
49: }
50: 
51: char* parser_strdup(Parser *p, const char *str) {
52:     if (!str) return NULL;
53:     if (!p || !p->ctx || !p->ctx->arena) return strdup(str); 
54:     return arena_strdup(p->ctx->arena, str);
55: }
56: 
57: void register_typename(Parser *p, const char *name, int is_enum) {
58:     hashmap_put(&p->types_map, name, (void*)(intptr_t)(is_enum ? 2 : 1));
59: 
60:     const char *current_ns = diag_get_namespace(p->ctx);
61:     if (current_ns && strlen(current_ns) > 0 && strcmp(current_ns, "main") != 0) {
62:         char full_name[512];
63:         snprintf(full_name, sizeof(full_name), "%s.%s", current_ns, name);
64:         hashmap_put(&p->types_map, parser_strdup(p, full_name), (void*)(intptr_t)(is_enum ? 2 : 1));
65:     }
66: }
67: 
68: int is_typename(Parser *p, const char *name) {
69:     return hashmap_has(&p->types_map, name);
70: }
71: 
72: int is_type_start(Parser *p) {
73:     TokenType ct = p->current_token.type;
74:     if (ct == TOKEN_KW_INT || ct == TOKEN_KW_SHORT || ct == TOKEN_KW_LONG || 
75:         ct == TOKEN_KW_DOUBLE || ct == TOKEN_KW_SINGLE || ct == TOKEN_KW_CHAR || 
76:         ct == TOKEN_KW_VOID || ct == TOKEN_KW_BOOL) {
77:         return 1;
78:     }
79:     if (ct == TOKEN_IDENTIFIER && is_typename(p, p->current_token.text)) {
80:         return 1;
81:     }
82:     return 0;
83: }
84: 
85: static int get_typename_kind(Parser *p, const char *name) {
86:     if (hashmap_has(&p->types_map, name)) {
87:         return (int)(intptr_t)hashmap_get(&p->types_map, name);
88:     }
89:     return 0;
90: }
91: 
92: void register_alias(Parser *p, const char *name, VarType target) {
93:     TypeAlias *curr = p->alias_head;
94:     while(curr) {
95:         if (strcmp(curr->name, name) == 0) {
96:             curr->target = target;
97:             return;
98:         }
99:         curr = curr->next;
100:     }
101: 
102:     TypeAlias *a = parser_alloc(p, sizeof(TypeAlias));
103:     a->name = parser_strdup(p, name);
104:     a->target = target;
105:     if (target.class_name) a->target.class_name = parser_strdup(p, target.class_name);
106:     
107:     a->next = p->alias_head;
108:     p->alias_head = a;
109: }
110: 
111: VarType* get_alias(Parser *p, const char *name) {
112:     TypeAlias *curr = p->alias_head;
113:     while(curr) {
114:         if (strcmp(curr->name, name) == 0) return &curr->target;
115:         curr = curr->next;
116:     }
117:     return NULL;
118: }
119: 
120: Token token_clone(Parser *p, Token t) {
121:     Token new_t = t;
122:     if (t.text) new_t.text = parser_strdup(p, t.text);
123:     return new_t;
124: }
125: 
126: void register_macro(Parser *p, const char *name, char **params, int param_count, Token *body, int body_len) {
127:     Macro *m = parser_alloc(p, sizeof(Macro));
128:     m->name = parser_strdup(p, name);
129:     m->params = params; 
130:     m->param_count = param_count;
131:     m->body = parser_alloc(p, sizeof(Token) * body_len);
132:     for (int i=0; i<body_len; i++) {
133:         m->body[i] = token_clone(p, body[i]);
134:     }
135:     m->body_len = body_len;
136:     m->next = p->macro_head;
137:     p->macro_head = m;
138:     if (p->ctx) p->ctx->macro_head = m;
139: }
140: 
141: static Macro* find_macro(Parser *p, const char *name) {
142:     Macro *curr = p->macro_head;
143:     while(curr) {
144:         if (strcmp(curr->name, name) == 0) return curr;
145:         curr = curr->next;
146:     }
147:     return NULL;
148: }
149: 
150: Token lexer_next_raw(Parser *p) {
151:     if (p->tokens && p->token_pos < p->token_count) {
152:         return p->tokens[p->token_pos++];
153:     }
154:     Token eof;
155:     memset(&eof, 0, sizeof(Token));
156:     eof.type = TOKEN_EOF;
157:     return eof;
158: }
159: 
160: Token get_next_token_expanded(Parser *p) {
161:     if (p->expansion_head) {
162:         if (p->expansion_head->pos < p->expansion_head->count) {
163:             return token_clone(p, p->expansion_head->tokens[p->expansion_head->pos++]);
164:         } else {
165:             p->expansion_head = p->expansion_head->next;
166:             return get_next_token_expanded(p);
167:         }
168:     }
169:     return lexer_next(p->l);
170: }
171: 
172: static Token fetch_safe(Parser *p) { return get_next_token_expanded(p); }
173: 
174: void parser_fail_at(Parser *p, Token t, const char *msg) {
175:     report_error(p->l, t, msg); 
176:     if (p->ctx) p->ctx->error_count++;
177:     p->has_error = 1;
178: }
179: 
180: void parser_fail(Parser *p, const char *msg) {
181:     parser_fail_at(p, p->current_token, msg);
182: }
183: 
184: void parser_sync(Parser *p) {
185:     while (p->current_token.type != TOKEN_EOF) {
186:         if (p->current_token.type == TOKEN_SEMICOLON) {
187:             eat(p, TOKEN_SEMICOLON);
188:     if (p->has_error) return;
189:             return;
190:         }
191:         if (p->current_token.type == TOKEN_RBRACE) {
192:             eat(p, TOKEN_RBRACE);
193:     if (p->has_error) return;
194:             return;
195:         }
196:         switch (p->current_token.type) {
197:             case TOKEN_CLASS:
198:             case TOKEN_STRUCT:
199:             case TOKEN_UNION: 
200:             case TOKEN_NAMESPACE:
201:             case TOKEN_KW_INT:
202:             case TOKEN_KW_VOID:
203:             case TOKEN_KW_CHAR:
204:             case TOKEN_KW_BOOL:
205:             case TOKEN_IF:
206:             case TOKEN_WHILE:
207:             case TOKEN_LOOP:
208:             case TOKEN_RETURN:
209:             case TOKEN_KW_LET:
210:             case TOKEN_DEFINE:
211:                 return;
212:             default:
213:                 eat(p, p->current_token.type); 
214:     if (p->has_error) return;
215:         }
216:     }
217: }
218: 
219: void eat(Parser *p, TokenType type) {
220:   if (p->has_error) return;
221:   if (p->current_token.type == type) {
222:     Token t = fetch_safe(p);
223:     
224:     while (!p->disable_macro_expansion && t.type == TOKEN_IDENTIFIER) {
225:         Macro *m = find_macro(p, t.text);
226:         if (!m) break; 
227:         
228:         Token **args = NULL;
229:         int *arg_lens = NULL;
230:         
231:         if (m->param_count > 0) {
232:             Token peek = fetch_safe(p);
233:             if (peek.type != TOKEN_LPAREN) {
234:                 parser_fail(p, "Function-like macro requires arguments list '('.");
235:             }
236: 
237:             args = parser_alloc(p, sizeof(Token*) * m->param_count);
238:             arg_lens = parser_alloc(p, m->param_count * sizeof(int));
239:             
240:             for(int i=0; i<m->param_count; i++) {
241:                 int cap = 8; int len = 0;
242:                 args[i] = parser_alloc(p, sizeof(Token) * cap);
243:                 int depth = 0;
244:                 while(1) {
245:                     Token arg_t = fetch_safe(p);
246:                     if (arg_t.type == TOKEN_EOF) parser_fail(p, "Unexpected EOF in macro arguments");
247:                     
248:                     if (arg_t.type == TOKEN_LPAREN) depth++;
249:                     else if (arg_t.type == TOKEN_RPAREN) {
250:                         if (depth == 0) {
251:                             if (i == m->param_count - 1) break; 
252:                             depth--; 
253:                         } else depth--;
254:                     }
255:                     else if (arg_t.type == TOKEN_COMMA) {
256:                         if (depth == 0) {
257:                             if (i < m->param_count - 1) break;
258:                         }
259:                     }
260:                     
261:                     if (len >= cap) { 
262:                         cap *= 2; 
263:                         Token *new_arr = parser_alloc(p, sizeof(Token)*cap);
264:                         memcpy(new_arr, args[i], sizeof(Token)*len);
265:                         args[i] = new_arr;
266:                     }
267:                     args[i][len++] = arg_t;
268:                 }
269:                 arg_lens[i] = len;
270:             }
271:         }
272:         
273:         int res_cap = m->body_len * 2 + 16;
274:         int res_len = 0;
275:         Token *res = parser_alloc(p, sizeof(Token) * res_cap);
276:         
277:         for(int i=0; i<m->body_len; i++) {
278:             Token bt = m->body[i];
279:             int p_idx = -1;
280:             if (bt.type == TOKEN_IDENTIFIER && m->param_count > 0) {
281:                 for(int k=0; k<m->param_count; k++) {
282:                     if (strcmp(bt.text, m->params[k]) == 0) { p_idx = k; break; }
283:                 }
284:             }
285:             
286:             if (p_idx != -1) {
287:                 for(int k=0; k<arg_lens[p_idx]; k++) {
288:                     if (res_len >= res_cap) { 
289:                         res_cap *= 2; 
290:                         Token *new_res = parser_alloc(p, sizeof(Token)*res_cap);
291:                         memcpy(new_res, res, sizeof(Token)*res_len);
292:                         res = new_res;
293:                     }
294:                     res[res_len++] = token_clone(p, args[p_idx][k]);
295:                 }
296:             } else {
297:                 if (res_len >= res_cap) { 
298:                     res_cap *= 2; 
299:                     Token *new_res = parser_alloc(p, sizeof(Token)*res_cap);
300:                     memcpy(new_res, res, sizeof(Token)*res_len);
301:                     res = new_res;
302:                 }
303:                 res[res_len++] = token_clone(p, bt);
304:             }
305:         }
306:         
307:         Expansion *ex = parser_alloc(p, sizeof(Expansion));
308:         ex->tokens = res;
309:         ex->count = res_len;
310:         ex->pos = 0;
311:         ex->next = p->expansion_head;
312:         p->expansion_head = ex;
313:         
314:         t = fetch_safe(p);
315:     }
316:     
317:     p->current_token = t;
318: 
319:   } else {
320:     char msg[256];
321:     const char *expected = get_token_description(type);
322:     const char *found = p->current_token.type == TOKEN_EOF ? "end of file" : 
323:                         (p->current_token.text ? p->current_token.text : token_type_to_string(p->current_token.type));
324:     
325:     snprintf(msg, sizeof(msg), "Expected '%s' but found '%s'", expected, found);
326:     parser_fail(p, msg);
327:   }
328: }
329: 
330: // Composite type parsing helper
331: VarType parse_type(Parser *p) {
332:   VarType t = {0}; 
333:   t.base = TYPE_UNKNOWN;
334: 
335: 
336: 
337:   if (p->current_token.type == TOKEN_KW_UNSIGNED) {
338:       t.is_unsigned = 1;
339:       eat(p, TOKEN_KW_UNSIGNED);
340:     if (p->has_error) return (VarType){0};
341:   }
342: 
343:   if (p->current_token.type == TOKEN_IDENTIFIER) {
344:       VarType *alias = get_alias(p, p->current_token.text);
345:       if (alias) {
346:           t.base = alias->base;
347:           t.ptr_depth += alias->ptr_depth; 
348:           t.vector_depth += alias->vector_depth;
349:           t.array_size = alias->array_size;
350:           if (alias->class_name) t.class_name = parser_strdup(p, alias->class_name);
351:           eat(p, TOKEN_IDENTIFIER);
352:     if (p->has_error) return (VarType){0};
353:       }
354:       else {
355:           int saved_pos = p->token_pos;
356:           Token saved_tok = p->current_token;
357: 
358:           char full_type_name[512];
359:           snprintf(full_type_name, sizeof(full_type_name), "%s", p->current_token.text);
360:           eat(p, TOKEN_IDENTIFIER);
361:     if (p->has_error) return (VarType){0};
362:           
363:           while (p->current_token.type == TOKEN_DOT) {
364:               eat(p, TOKEN_DOT);
365:     if (p->has_error) return (VarType){0};
366:               strcat(full_type_name, ".");
367:               if (p->current_token.type == TOKEN_IDENTIFIER) {
368:                   strcat(full_type_name, p->current_token.text);
369:                   eat(p, TOKEN_IDENTIFIER);
370:     if (p->has_error) return (VarType){0};
371:               } else {
372:                   break;
373:               }
374:           }
375: 
376:           int kind = get_typename_kind(p, full_type_name);
377:           if (kind != 0) {
378:               if (kind == 2) { 
379:                   t.base = TYPE_ENUM;
380:                   t.class_name = parser_strdup(p, full_type_name);
381:               } else {
382:                   t.base = TYPE_CLASS;
383:                   char base_name[512];
384:                   snprintf(base_name, sizeof(base_name), "%s", full_type_name);
385:                   
386:                   if (p->current_token.type == TOKEN_LBRACKET) {
387:                       char full_name[1024];
388:                       snprintf(full_name, sizeof(full_name), "%s", base_name);
389:                       
390:                       eat(p, TOKEN_LBRACKET);
391:     if (p->has_error) return (VarType){0};
392:                       strcat(full_name, "[");
393:                       
394:                       while (p->current_token.type != TOKEN_RBRACKET && p->current_token.type != TOKEN_EOF) {
395:                           if (p->current_token.text) {
396:                               strcat(full_name, p->current_token.text);
397:                           } else {
398:                               strcat(full_name, token_type_to_string(p->current_token.type));
399:                           }
400:                           eat(p, p->current_token.type);
401:     if (p->has_error) return (VarType){0};
402:                       }
403:                       eat(p, TOKEN_RBRACKET);
404:     if (p->has_error) return (VarType){0};
405:                       strcat(full_name, "]");
406:                       t.class_name = parser_strdup(p, full_name);
407:                   } else {
408:                       t.class_name = parser_strdup(p, base_name);
409:                   }
410:               }
411:           } else {
412:               p->token_pos = saved_pos;
413:               p->current_token = saved_tok;
414:               if (t.is_unsigned) t.base = TYPE_INT;
415:               return t; 
416:           }
417:       }
418:   } else {
419:       TokenType ct = p->current_token.type;
420:       if (ct == TOKEN_KW_INT) { t.base = TYPE_INT; eat(p, TOKEN_KW_INT); }
421:       else if (ct == TOKEN_KW_SHORT) { t.base = TYPE_SHORT; eat(p, TOKEN_KW_SHORT); }
422:       else if (ct == TOKEN_KW_LONG) {
423:           eat(p, TOKEN_KW_LONG);
424:     if (p->has_error) return (VarType){0};
425:           if (p->current_token.type == TOKEN_KW_LONG) {
426:               eat(p, TOKEN_KW_LONG);
427:     if (p->has_error) return (VarType){0};
428:               if (p->current_token.type == TOKEN_KW_DOUBLE) {
429:                   eat(p, TOKEN_KW_DOUBLE);
430:     if (p->has_error) return (VarType){0};
431:                   t.base = TYPE_LONG_DOUBLE;
432:               } else {
433:                   t.base = TYPE_LONG_LONG;
434:               }
435:           } else if (p->current_token.type == TOKEN_KW_DOUBLE) {
436:               eat(p, TOKEN_KW_DOUBLE);
437:     if (p->has_error) return (VarType){0};
438:               t.base = TYPE_LONG_DOUBLE;
439:           } else if (p->current_token.type == TOKEN_KW_INT) {
440:               eat(p, TOKEN_KW_INT);
441:     if (p->has_error) return (VarType){0};
442:               t.base = TYPE_LONG;
443:           } else {
444:               t.base = TYPE_LONG;
445:           }
446:       }
447:       else if (ct == TOKEN_KW_DOUBLE) {
448:           eat(p, TOKEN_KW_DOUBLE);
449:     if (p->has_error) return (VarType){0};
450:           if (p->current_token.type == TOKEN_KW_LONG) {
451:               eat(p, TOKEN_KW_LONG);
452:     if (p->has_error) return (VarType){0};
453:               if (p->current_token.type == TOKEN_KW_LONG) eat(p, TOKEN_KW_LONG); 
454:               t.base = TYPE_LONG_DOUBLE;
455:           } else {
456:               t.base = TYPE_DOUBLE;
457:           }
458:       }
459:       else if (ct == TOKEN_KW_CHAR) { t.base = TYPE_CHAR; eat(p, TOKEN_KW_CHAR); }
460:       else if (ct == TOKEN_KW_BOOL) { t.base = TYPE_BOOL; eat(p, TOKEN_KW_BOOL); }
461:       else if (ct == TOKEN_KW_SINGLE) { t.base = TYPE_FLOAT; eat(p, TOKEN_KW_SINGLE); }
462: 
463:       else if (ct == TOKEN_KW_VOID) { t.base = TYPE_VOID; eat(p, TOKEN_KW_VOID); }
464:       else if (ct == TOKEN_KW_LET) { t.base = TYPE_AUTO; eat(p, TOKEN_KW_LET); }
465:       else {
466:           if (t.is_unsigned) t.base = TYPE_INT; 
467:           else return t; 
468:       }
469:   }
470: 
471:   while (p->current_token.type == TOKEN_STAR) {
472:     t.ptr_depth++;
473:     eat(p, TOKEN_STAR);
474:     if (p->has_error) return (VarType){0};
475:   }
476:   
477:   if (p->current_token.type == TOKEN_LPAREN) {
478:       Token next = parser_peek_token(p);
479:       if (next.type == TOKEN_STAR) {
480:           return parse_func_ptr_decl(p, t, NULL);
481:     if (p->has_error) return (VarType){0};
482:       }
483:   }
484:   
485:   if (p->current_token.type == TOKEN_QUESTION) {
486:       t.is_tainted = 1;
487:       eat(p, TOKEN_QUESTION);
488:     if (p->has_error) return (VarType){0};
489:   }
490: 
491:   return t;
492: }
493: 
494: // TODO understand what the fuck is this
495: // This is for varshit idk wtf
496: VarType parse_func_ptr_decl(Parser *p, VarType ret_type, char **out_name) {
497:     VarType vt = {0};
498:     vt.is_func_ptr = 1;
499:     vt.fp_ret_type = parser_alloc(p, sizeof(VarType));
500:     *vt.fp_ret_type = ret_type;
501:     
502:     eat(p, TOKEN_LPAREN);
503:     if (p->has_error) return (VarType){0};
504:     if (p->current_token.type == TOKEN_STAR) {
505:         eat(p, TOKEN_STAR);
506:     if (p->has_error) return (VarType){0};
507:         
508:         if (p->current_token.type == TOKEN_IDENTIFIER) {
509:             if (out_name) *out_name = parser_strdup(p, p->current_token.text);
510:             eat(p, TOKEN_IDENTIFIER);
511:     if (p->has_error) return (VarType){0};
512:         } else if (out_name) {
513:             *out_name = NULL;
514:         }
515:         
516:         eat(p, TOKEN_RPAREN);
517:     if (p->has_error) return (VarType){0};
518:         eat(p, TOKEN_LPAREN);
519:     if (p->has_error) return (VarType){0};
520:     } else {
521:         if (out_name) *out_name = NULL;
522:     }
523:     
524:     int cap = 4;
525:     vt.fp_param_types = parser_alloc(p, sizeof(VarType) * cap);
526:     vt.fp_param_count = 0;
527:     
528:     if (p->current_token.type != TOKEN_RPAREN) {
529:         while(1) {
530:             if (p->current_token.type == TOKEN_ELLIPSIS) {
531:                 vt.fp_is_varargs = 1;
532:                 eat(p, TOKEN_ELLIPSIS);
533:     if (p->has_error) return (VarType){0};
534:                 break;
535:             }
536:             
537:             int pmods = parse_modifiers(p);
538:     if (p->has_error) return (VarType){0};
539:             (void)pmods; // unused in func ptr types for now
540:             VarType pt = parse_type(p);
541:     if (p->has_error) return (VarType){0};
542:             if (pt.base == TYPE_UNKNOWN) parser_fail(p, "Expected type in function pointer params");
543:             
544:             if (p->current_token.type == TOKEN_IDENTIFIER) {
545:                 eat(p, TOKEN_IDENTIFIER); 
546:     if (p->has_error) return (VarType){0};
547:             }
548:             
549:              if (p->current_token.type == TOKEN_LBRACKET) {
550:                 eat(p, TOKEN_LBRACKET);
551:     if (p->has_error) return (VarType){0};
552:                 if (p->current_token.type != TOKEN_RBRACKET) {
553:                      ASTNode* tmp = parse_expression(p);
554:     if (p->has_error) return (VarType){0};
555:                      (void)tmp;
556:                 }
557:                 eat(p, TOKEN_RBRACKET);
558:     if (p->has_error) return (VarType){0};
559:                 pt.ptr_depth++;
560:             }
561:             
562:             if (vt.fp_param_count >= cap) {
563:                 cap *= 2;
564:                 VarType *new_params = parser_alloc(p, sizeof(VarType) * cap);
565:                 memcpy(new_params, vt.fp_param_types, sizeof(VarType) * vt.fp_param_count);
566:                 vt.fp_param_types = new_params;
567:             }
568:             vt.fp_param_types[vt.fp_param_count++] = pt;
569:             
570:             if (p->current_token.type == TOKEN_COMMA) eat(p, TOKEN_COMMA);
571:             else break;
572:         }
573:     }
574:     eat(p, TOKEN_RPAREN);
575:     if (p->has_error) return (VarType){0};
576:     
577:     return vt;
578: }
579: 
580: static char* read_file_content(Parser *p, const char* path) {
581:     FILE* f = fopen(path, "rb");
582:     if (!f) return NULL;
583:     fseek(f, 0, SEEK_END);
584:     long len = ftell(f);
585:     fseek(f, 0, SEEK_SET);
586:     char* buf = parser_alloc(p, len + 1);
587:     if(buf) { fread(buf, 1, len, f); buf[len] = 0; }
588:     fclose(f);
589:     return buf;
590: }
591: 
592: char* read_import_file(Parser *p, const char* filename) {
593:   const char* paths[] = { "", "lib/" };
594:   const char* exts[] = { ".aky", ".hky", ".alk", ".alky", ".alkyl", "" };
595:   char path[1024];
596:   
597:   for (unsigned long i = 0; i < sizeof(paths)/sizeof(*paths); i++) {
598:       for (unsigned long j = 0; j < sizeof(exts)/sizeof(*exts); j++) {
599:           snprintf(path, sizeof(path), "%s%s%s", paths[i], filename, exts[j]);
600:           char *content = read_file_content(p, path);
601:           if (content) return content;
602:       }
603:   }
604:   return NULL;
605: }
606: 
607: Token parser_peek_token(Parser *p) {
608:     if (p->expansion_head) {
609:         if (p->expansion_head->pos < p->expansion_head->count) {
610:             return p->expansion_head->tokens[p->expansion_head->pos];
611:         }
612:     }
613:     if (p->tokens && p->token_pos < p->token_count) {
614:         return p->tokens[p->token_pos];
615:     }
616:     Token eof;
617:     memset(&eof, 0, sizeof(Token));
618:     eof.type = TOKEN_EOF;
619:     return eof;
620: }
621: 
622: void parser_prescan(Parser *p) {
623:     int saved_pos = p->token_pos;
624:     while (p->token_pos < p->token_count) {
625:         Token t = lexer_next_raw(p);
626:         if (t.type == TOKEN_EOF) break;
627:         if (t.type == TOKEN_CLASS || t.type == TOKEN_STRUCT || t.type == TOKEN_UNION || t.type == TOKEN_ENUM) {
628:             Token name = lexer_next_raw(p);
629:             if (name.type == TOKEN_IDENTIFIER) {
630:                 register_typename(p, name.text, (t.type == TOKEN_ENUM));
631:             }
632:         }
633:     }
634:     p->token_pos = saved_pos;
635: }
636: 
637: ASTNode* parse_program(Parser *p) {
638:   if (p->l) {
639:       p->token_capacity = 1024;
640:       p->tokens = parser_alloc(p, sizeof(Token) * p->token_capacity);
641:       p->token_count = 0;
642:       p->token_pos = 0;
643:       while (1) {
644:           Token t = lexer_next(p->l);
645:           if (p->token_count >= p->token_capacity) {
646:               int new_cap = p->token_capacity * 2;
647:               Token *new_tokens = parser_alloc(p, sizeof(Token) * new_cap);
648:               memcpy(new_tokens, p->tokens, sizeof(Token) * p->token_count);
649:               p->tokens = new_tokens;
650:               p->token_capacity = new_cap;
651:           }
652:           p->tokens[p->token_count++] = t;
653:           if (t.type == TOKEN_EOF) break;
654:       }
655:   }
656: 
657:   parser_prescan(p);
658:   p->current_token = lexer_next_raw(p);
659:   
660:   ASTNode *head = NULL;
661:   ASTNode **current = &head;
662:   
663:   while (p->current_token.type != TOKEN_EOF) {
664:     if (p->has_error) {
665:         p->has_error = 0;
666:         parser_sync(p);
667:         if (p->current_token.type == TOKEN_EOF) break;
668:     }
669:    
670:     ASTNode *node = parse_top_level(p);
671:     if (node) {
672:         if (!*current) *current = node; 
673:         
674:         ASTNode *iter = node;
675:         while (iter->next) iter = iter->next;
676:         current = &iter->next;
677:     }
678:   }
679:   
680:   *current = NULL;
681:   return head;
682: }"""

lines = []
for line in content.split('\n'):
    if re.match(r'^\d+:\s', line):
        lines.append(line.split(':', 1)[1][1:])
    else:
        lines.append(line)

with open('src/parser/core.c', 'w') as f:
    f.write('\n'.join(lines))

