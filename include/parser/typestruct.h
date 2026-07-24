#ifndef PARSER_TYPESTRUCT_H
#define PARSER_TYPESTRUCT_H

#include "common/aliases.h"

typedef enum {
  NODE_ROOT,
  NODE_FUNC_DEF,  
  NODE_CALL,    
  NODE_RETURN,  
  NODE_BREAK,
  NODE_CONTINUE,
  NODE_LOOP,    
  NODE_WHILE,   
  NODE_IF,
  NODE_SWITCH,
  NODE_CASE,
  NODE_VAR_DECL,  
  NODE_ASSIGN,  
  NODE_VAR_REF,
  NODE_BINARY_OP,
  NODE_UNARY_OP, 
  NODE_LITERAL,
  NODE_ARRAY_LIT, 
  NODE_INDEX_ACCESS, 
  NODE_VECTOR_ACCESS, 
  NODE_INC_DEC, 
  NODE_LINK,
  NODE_CLASS,
  NODE_STRUCT,
  NODE_NAMESPACE, 
  NODE_ENUM, 
  NODE_ERRNUM,
  NODE_MEMBER_ACCESS,
  NODE_METHOD_CALL, 
  NODE_TYPEOF,
  NODE_HAS_METHOD,    
  NODE_HAS_ATTRIBUTE,  
  NODE_CAST,
  NODE_EMIT,
  NODE_FOR_IN,
  NODE_WASH, 
  NODE_CLEAN,
  NODE_UNTAINT,
  NODE_SIZEOF,
  NODE_ALIGNOF,
  NODE_DEFER,
  NODE_DEFINED,
  NODE_ISCOMPATIBLE,
  NODE_META,
  NODE_POSTMETA,
  NODE_PURGE,
  NODE_COMPOUND,
  NODE_TEMPLATE_INSTANTIATION,
  NODE_NAMED_ARG
} NodeType;

typedef enum {
  TYPE_VOID,
  TYPE_INT,
  TYPE_UNSIGNED_INT,
  TYPE_SHORT,
  TYPE_LONG,
  TYPE_LONG_LONG,
  TYPE_UNSIGNED_LONG,
  TYPE_UNSIGNED_LONG_LONG,
  TYPE_CHAR,
  TYPE_UNSIGNED_CHAR,
  TYPE_BOOL,
  TYPE_SINGLE,
  TYPE_DOUBLE,
  TYPE_LONG_DOUBLE,
  TYPE_ARRAY,

  TYPE_AUTO,
  TYPE_CLASS, 
  TYPE_ENUM, 
  TYPE_NAMESPACE,
  TYPE_ERROR,
  TYPE_UNKNOWN,
} BaseType;

typedef enum {
  IS_A_NONE,
  IS_A_NAKED,
  IS_A_FINAL,
} IsASemantic; 

typedef enum {
  HAS_A_NONE,
  HAS_A_REACTIVE, 
  HAS_A_INERT, 
} HasASemantic; 

typedef struct VarType {
  BaseType base;
  int ptr_depth;
  char *class_name;
  int array_size;
  int array_depth;
  
  struct VarType *fp_ret_type;   
  struct VarType *fp_param_types; 
  int fp_param_count;

  bool is_unsigned : 1; 
  bool is_func_ptr : 1;
  bool fp_is_varargs : 1;
  bool is_tainted : 1;
  bool is_pristine : 1;
} VarType;

typedef struct ASTNode {
  NodeType type;
  struct ASTNode *next; 
  int line;
  int col;
  char *reason;
  VarType sem_type;
} ASTNode;

typedef struct Parameter {
  VarType type;
  char *name;
  
  bool is_pure : 1;
  bool has_explicit_pure : 1;
  bool is_pristine : 1;
  bool has_explicit_pristine : 1;

  struct Parameter *next;
  ASTNode *default_value;
} Parameter;

typedef struct {
  ASTNode base;
  char *name;
  char *mangled_name; 
  VarType ret_type;
  Parameter *params;
  ASTNode *body; 
  char *class_name; 

  IsASemantic is_is_a;
  HasASemantic is_has_a;

  bool is_varargs : 1; 
  bool is_macro : 1;
  bool is_public : 1;
  bool is_open : 1;
  bool is_static : 1;
  bool is_virtual : 1;
  bool is_abstract : 1;
  bool is_flux : 1;
  bool is_pure : 1;
  bool has_explicit_pure : 1;
  bool is_pristine : 1;
  bool has_explicit_pristine : 1;
  bool is_extern : 1;
  bool has_body : 1;
  bool is_covalent : 1;
  char *cconv;
  char *extern_name;
  char **err_names;       // error set attached via `errnum [...]`
  int num_err;
  bool has_errnum : 1;
} FuncDefNode;

typedef struct {
  ASTNode base;
  char *name;
  char *parent_name; 
  struct {
      char **names;
      int count;
  } traits; 
  ASTNode *members; 
  
  IsASemantic is_is_a;
  HasASemantic is_has_a;

  bool is_open : 1; 
  bool is_public : 1; 
  bool is_extern : 1;
  bool has_body : 1; 
  bool is_union : 1;
  bool is_static : 1; 
  bool is_abstract : 1;
  bool is_exact : 1;
  bool is_pragma : 1;
  bool is_method_class : 1;
  bool is_container : 1;
  bool is_frame : 1;
  bool is_pure : 1;
  bool has_explicit_pure : 1;
  bool is_tainted : 1;
} ClassNode;

typedef struct {
  ASTNode base;
  char *name;
  char *parent_name; 
  struct {
      char **names;
      int count;
  } traits; 
  ASTNode *members; 
  
  IsASemantic is_is_a;
  HasASemantic is_has_a;

  bool is_open : 1; 
  bool is_public : 1; 
  bool is_extern : 1;
  bool has_body : 1; 
  bool is_union : 1;
  bool is_static : 1; 
  bool is_abstract : 1;
  bool is_pure : 1;
} StructNode;


typedef struct EnumEntry {
    char *name;
    int value;
    struct EnumEntry *next;
} EnumEntry;

typedef struct {
    ASTNode base;
    char *name;
    EnumEntry *entries;
} EnumNode;

typedef struct {
    ASTNode base;
    EnumEntry *entries;
} ErrNumNode;

// A single residue case inside an untaint/clean statement:
//   [ErrA, ErrB] { ... }   (err_names == NULL means the default catch-all case)
typedef struct ResidueCase {
    char **err_names;     // NULL for the default catch-all case
    int num_err;
    ASTNode *body;
    bool is_default : 1;  // catch-all case (no bracket)
    struct ResidueCase *next;
} ResidueCase;



typedef struct {
  ASTNode base;
  char **type_params;
  VarType **allowed_types;
  int *num_allowed;
  int num_type_params;
  ASTNode *body;
} CompoundNode;

typedef struct {
  ASTNode base;
  ASTNode *target;
  VarType *template_types;
  int num_template_types;
} TemplateInstNode;

typedef struct {
  ASTNode base;
  char *name;
  ASTNode *body; 
} NamespaceNode;

typedef struct {
  ASTNode base;
  ASTNode *object; 
  char *member_name; 
} MemberAccessNode;

typedef struct {
  ASTNode base;
  ASTNode *object;
  char *method_name;
  ASTNode *args;
  char *mangled_name; 
  char *owner_class;  
  bool is_static : 1;      
} MethodCallNode;

typedef struct {
    ASTNode base;
    VarType var_type;
    ASTNode *operand;
    char *custom_cast_method;
} CastNode;

typedef struct {
  ASTNode base;
  char *name;
  char *mangled_name; 
  ASTNode *args; 
  ASTNode *target; // For complex calls like template instantiations or function pointers
} CallNode;

typedef struct {
  ASTNode base;
  ASTNode *msg;
} PurgeNode;

typedef struct {
  ASTNode base;
  ASTNode *value;
} ReturnNode;

typedef struct {
    ASTNode base;
    ASTNode *value; 
    ASTNode *body;  
    bool is_leak : 1;    
} CaseNode;

typedef struct {
    ASTNode base;
    ASTNode *condition;
    ASTNode *cases; 
    ASTNode *default_case; 
} SwitchNode;

typedef struct {
    ASTNode base;
    char *var_name;
    char *pristine_var_name;
    ASTNode *body;
    char *err_var_name;
    ResidueCase *residue_cases;  // list of [Err...] { ... } cases (replaces residue_body)
    ASTNode *residue_body;       // kept for backward compat (single residue)
} CleanNode;

typedef struct {
    ASTNode base;
    char *var_name;
    char *err_var_name;
    ResidueCase *residue_cases;  // list of [Err...] { ... } cases
    ASTNode *residue_body;       // kept for backward compat
} UntaintNode;

typedef struct {
  ASTNode base;
  VarType var_type;
  char *name;
  ASTNode *initializer;
  ASTNode *array_size; 

  IsASemantic is_is_a;
  HasASemantic is_has_a;

  bool is_array : 1;   
  bool is_open : 1;
  bool is_public : 1;
  bool is_static : 1;
  bool is_const : 1;
  bool is_mutable : 1; 
  bool is_pure : 1;
  bool has_explicit_pure : 1;
  bool is_pristine : 1;
  bool has_explicit_pristine : 1;
} VarDeclNode;

typedef struct {
  ASTNode base;
  char *name;
  ASTNode *value;
  ASTNode *index; 
  ASTNode *target; 
  int op; 
  bool is_implicit_let : 1;
  char *overloaded_func_name;
} AssignNode; 

typedef struct {
  ASTNode base;
  char *name;
  ASTNode *index; 
  ASTNode *target; 
  int op;
  bool is_prefix : 1; 
  char *overloaded_func_name;
} IncDecNode;

typedef struct {
  ASTNode base;
  char *name;
  char *mangled_name;
  int error_id;
  bool is_class_member : 1; 
  bool is_error_id : 1;
} VarRefNode;

typedef struct {
  ASTNode base;
  ASTNode *target; 
  ASTNode *index;
} IndexAccessNode;

typedef struct {
  ASTNode base;
  ASTNode *elements; 
  bool is_vector;
} ArrayLitNode;

typedef struct {
  ASTNode base;
  ASTNode *target; 
  ASTNode *index;
} VectorAccessNode;


typedef struct {
  ASTNode base;
  char *lib_name;
} LinkNode;

typedef struct {
  ASTNode base;
  int op; 
  ASTNode *left;
  ASTNode *right;
  char *overloaded_func_name;
  char *fallback_err_name; // For ?[...] operator (first/legacy case)
  char *err_var_name;     // bound error variable for `? [Err] v`
  // List of cases for the `?`/untaint/clean residue matching.
  // Each case has a set of error names (err_names == NULL means default).
  ResidueCase *cases;
  int exhaustiveness_checked : 1;
} BinaryOpNode;

typedef struct {
  ASTNode base;
  int op; 
  ASTNode *operand;
  int is_suffix;
  char *overloaded_func_name;
} UnaryOpNode;

typedef struct {
    ASTNode base;
    ASTNode *value;
} EmitNode;

typedef struct {
  ASTNode base;
} BreakNode;

typedef struct {
  ASTNode base;
} ContinueNode;

typedef struct {
  ASTNode base;
  ASTNode *iterations;
  ASTNode *body;
} LoopNode;

typedef struct {
  ASTNode base;
  ASTNode *condition;
  ASTNode *body;
  bool is_do_while : 1; 
} WhileNode;

typedef struct {
    ASTNode base;
    char *var_name;
    ASTNode *collection;
    ASTNode *body;
    VarType iter_type; 
} ForInNode;

typedef struct {
  ASTNode base;
  ASTNode *condition;
  ASTNode *then_body;
  ASTNode *else_body;
} IfNode;

/* TODO use this correctly */
// see var_type
typedef struct {
  ASTNode base;
  VarType var_type;
  Value val; 
} LiteralNode;

typedef struct {
  ASTNode base;
  VarType target_type;
  ASTNode *operand;
} SizeOfNode;

typedef struct {
  ASTNode base;
  VarType target_type;
  VarType target_type2;
} IsCompatibleNode;

typedef struct {
  ASTNode base;
  ASTNode *body;
} DeferNode;

typedef struct {
  ASTNode base;
  bool is_post;
  ASTNode *body;
} MetaNode;

typedef struct {
  ASTNode base;
  char *name;
  ASTNode *value;
} NamedArgNode;

#endif // PARSER_TYPESTRUCT_H
