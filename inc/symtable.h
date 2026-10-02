#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdbool.h>

typedef enum { SYM_VAR, SYM_FUNC } sym_kind_t;

typedef enum { TYPE_NULL, TYPE_INT, TYPE_FLOAT, TYPE_STRING, TYPE_BOOL, TYPE_UNKNOWN } data_type_t;

typedef enum { ST_OK, ST_EXISTS, ST_NOMEM } st_result_t;

typedef struct {
    sym_kind_t kind;
    data_type_t type;       // variable type / function return type
    bool defined;           // function: body seen?  var: assigned?
    bool used;              // unused-variable checks
    bool is_const;
    bool builtin;
    // function only:
    int param_count;
    data_type_t *param_types;
    char **param_names;
} symbol_t;

typedef struct bst_node {
    char *key;              // identifier name
    symbol_t data;
    int height;             // for balancing (AVL)
    struct bst_node *left, *right;
} bst_node_t;

typedef struct scope {
    bst_node_t *root;
    struct scope *parent;   // enclosing scope (stack via linked list)
    int depth;
} scope_t;

// Used as a stack
typedef struct {
    scope_t *top;           // stack pointer
    bst_node_t *global;     // functions
} symtable_t;

/* lifetime */
bool      symtable_init(symtable_t *st); // pushes scope 0
void      symtable_free(symtable_t *st);

/* scopes */
bool      symtable_push_scope(symtable_t *st);
void      symtable_pop_scope(symtable_t *st);

/* variables */
st_result_t symtable_insert_var(symtable_t *st, const char *name, data_type_t type, symbol_t **out);
symbol_t *symtable_lookup_var(symtable_t *st, const char *name);       // all scopes, inner outer
symbol_t *symtable_lookup_local(symtable_t *st, const char *name);     // current scope only

/* functions */
st_result_t symtable_insert_func(symtable_t *st, const char *name, symbol_t **out);
symbol_t *symtable_lookup_func(symtable_t *st, const char *name);
bool symtable_func_add_param(symbol_t *func, const char *name, data_type_t type);

typedef void (*symtable_visit_fn)(const char *name, symbol_t *sym, void *ctx);
void symtable_foreach_func(symtable_t *st, symtable_visit_fn fn, void *ctx);
void symtable_foreach_local(symtable_t *st, symtable_visit_fn fn, void *ctx); // current scope

int  symtable_depth(symtable_t *st);   // current nesting level, 0 = global
symbol_t *symtable_lookup_var_ex(symtable_t *st, const char *name, int *found_depth);


#endif
