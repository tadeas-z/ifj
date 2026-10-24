// Implementation of the IFJ26 imperative language compiler
// symtable.c
// Author(s): Adam Zatloukal (xzatloa01)

#include "symtable.h"
#include <stdlib.h>
#include <string.h>

typedef struct bst_node {
    char* key; // identifier name
    symbol_t data;
    int height; // for balancing (AVL)
    struct bst_node *left, *right;
} bst_node_t;

typedef struct scope {
    bst_node_t* root;
    struct scope* parent; // enclosing scope (stack via linked list)
    int depth;
} scope_t;

// Used as a stack
struct symtable {
    scope_t* top; // stack pointer
    bst_node_t* global; // functions
    int next_id; // counter for symbol_t.id
};

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Create macros for initialising the symbot_t struct
//          with default values"

#define SYMBOL_INIT_VAR(st, t, c) \
    ((symbol_t) {                 \
        .id = (st)->next_id++,    \
        .kind = (SYM_VAR),        \
        .type = (t),              \
        .is_const = (c) })

#define SYMBOL_INIT_FUNC(st)   \
    ((symbol_t) {              \
        .id = (st)->next_id++, \
        .kind = (SYM_FUNC),    \
        .type = NULL,          \
        .is_const = true,      \
        .ret_inferred = false, \
        .param_count = 0,      \
        .param_types = NULL,   \
        .param_names = NULL })

#define CHECK_MEM_ERR(ptr)   \
    do {                     \
        if ((ptr) == NULL)   \
            return ST_NOMEM; \
    } while (0)

static int get_height(bst_node_t* ptr);
static int update_height(bst_node_t* ptr);
static int balance_factor(bst_node_t* ptr);

static bst_node_t* rotate_left(bst_node_t* old_root);
static bst_node_t* rotate_right(bst_node_t* old_root);
static bst_node_t* rebalance(bst_node_t* node);

static bst_node_t* bst_insert(bst_node_t* node, const char* key, symbol_t symbol, bst_node_t** out_node, st_result_t* result);
static bst_node_t* bst_find(const char* key, bst_node_t* node);
static void bst_free(bst_node_t* root);

static bst_node_t* left_left_case(bst_node_t* node);
static bst_node_t* left_right_case(bst_node_t* node);
static bst_node_t* right_left_case(bst_node_t* node);
static bst_node_t* right_right_case(bst_node_t* node);

static int max(int n1, int n2);
static bst_node_t* allocate_node(const char* key, symbol_t symbol);

static void in_order(bst_node_t* node, symtable_visit_fn fn, void* ctx);

static type_t INT_INST = { .kind = T_INT };
static type_t DOUBLE_INST = { .kind = T_DOUBLE };
static type_t STRING_INST = { .kind = T_STRING };
static type_t UNIT_INST = { .kind = T_UNIT };
static type_t BOOL_INST = { .kind = T_BOOL };
static type_t ANY_INST = { .kind = T_ANY };

/* CAN be accessed outside of this file */
type_t* const TYPE_INT = &INT_INST;
type_t* const TYPE_DOUBLE = &DOUBLE_INST;
type_t* const TYPE_STRING = &STRING_INST;
type_t* const TYPE_UNIT = &UNIT_INST;
type_t* const TYPE_BOOL = &BOOL_INST;
type_t* const TYPE_ANY = &ANY_INST;

// ========================================
//  Type checks
// ========================================

bool type_equal(const type_t* a, const type_t* b)
{
    if (a == NULL || b == NULL)
        return false;

    return a->kind == b->kind; // Might need to be expanded as type_t struct grows with extensions
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Implement the type_compatible function. return true only when:
//          type is T_ANY OR both types match OR we are assigning T_INT to
//          T_DOUBLE. Also count in any other edge cases"

bool type_compatible(const type_t* target, const type_t* source)
{
    if (target == NULL || source == NULL)
        return false;
    if (target->kind == T_ANY)
        return true;
    if (type_equal(target, source))
        return true;
    if (target->kind == T_DOUBLE && source->kind == T_INT)
        return true;
    return false;
}

// ========================================
//  Binary search tree (AVL)
// ========================================

static int max(int n1, int n2)
{
    return n1 > n2 ? n1 : n2;
}

static int get_height(bst_node_t* ptr)
{
    return ptr == NULL ? 0 : ptr->height;
}

static int update_height(bst_node_t* ptr)
{
    if (ptr == NULL)
        return 0;
    ptr->height = max(get_height(ptr->left), get_height(ptr->right)) + 1;
    return ptr->height;
}

// ----------------------------------------
//  Tree operations
// ----------------------------------------

/**
 * @brief inserts node into BST
 * @param node root of the bst
 * @param key identifier of the variable/function
 * @param symbol variable/function metadata
 * @param out_node address which points at the address of the inserted node THIS IS THE OUTPUT
 * @param result THIS IS ALSO AN OUTPUT. ST_OK when insertion was successful, ST_EXISTS when key
 *          matches with an already existing node, ST_NOMEM when memory allocation fails.
 * @note This function is recursive and its return value is used to update root pointer after subtree
 *          rebalancing, thus out_node has to be used as the real return value
 */
static bst_node_t* bst_insert(bst_node_t* node, const char* key, symbol_t symbol, bst_node_t** out_node, st_result_t* result)
{
    if (node == NULL) {
        bst_node_t* new_node = allocate_node(key, symbol);
        if (new_node == NULL)
            *result = ST_NOMEM;
        else
            *out_node = new_node;
        return new_node;
    }

    int cmp = strcmp(key, node->key);

    // Go left (new node to be inserted sorts lower than node)
    if (cmp < 0)
        node->left = bst_insert(node->left, key, symbol, out_node, result);
    // Go right (new node to be inserted sorts higher than node)
    else if (cmp > 0)
        node->right = bst_insert(node->right, key, symbol, out_node, result);
    else {
        *out_node = node;
        *result = ST_EXISTS;
        return node; // Already present
    }

    return rebalance(node); // Also updates height
}

static bst_node_t* allocate_node(const char* key, symbol_t symbol)
{
    bst_node_t* node = calloc(1, sizeof(bst_node_t));
    char* key_copy = malloc((strlen(key) + 1) * sizeof(char));

    if (node == NULL || key_copy == NULL) {
        free(node);
        free(key_copy);
        return NULL;
    }
    strcpy(key_copy, key);

    // Insert symbol data into newly allocated node
    node->key = key_copy;
    node->data = symbol;
    node->height = 1;

    return node;
}

static bst_node_t* bst_find(const char* key, bst_node_t* node)
{
    while (node != NULL) {
        int cmp = strcmp(key, node->key);
        if (cmp == 0)
            return node;
        if (cmp < 0)
            node = node->left;
        else
            node = node->right;
    }
    return NULL;
}

static void bst_free(bst_node_t* node)
{
    if (node != NULL) {
        bst_free(node->left);
        bst_free(node->right);

        for (int i = 0; i < node->data.param_count; i++)
            free(node->data.param_names[i]);
        free(node->data.param_names);
        free(node->data.param_types);

        free(node->key);
        free(node);
    }
}

// ----------------------------------------
//  Rotations
// ----------------------------------------

bst_node_t* rotate_left(bst_node_t* old_root)
{
    // If rotation cannot be performed return the old tree
    if (old_root == NULL || old_root->right == NULL)
        return old_root;

    bst_node_t* new_root = old_root->right;
    bst_node_t* old_left = old_root->right->left;

    new_root->left = old_root;
    old_root->right = old_left;

    update_height(old_root);
    update_height(new_root);

    return new_root;
}

bst_node_t* rotate_right(bst_node_t* old_root)
{
    // If rotation cannot be performed return the old tree
    if (old_root == NULL || old_root->left == NULL)
        return old_root;

    bst_node_t* new_root = old_root->left;
    bst_node_t* old_right = old_root->left->right;

    new_root->right = old_root;
    old_root->left = old_right;

    update_height(old_root);
    update_height(new_root);

    return new_root;
}

// ----------------------------------------
//  Rebalancing
// ----------------------------------------

int balance_factor(bst_node_t* ptr)
{
    return (get_height(ptr->right) - get_height(ptr->left));
}

static bst_node_t* rebalance(bst_node_t* node)
{
    update_height(node);
    // Left heavy
    if (balance_factor(node) == -2) {
        if (balance_factor(node->left) <= 0) {
            return left_left_case(node);
        } else {
            return left_right_case(node);
        }
    }
    // Right heavy
    if (balance_factor(node) == 2) {
        if (balance_factor(node->right) >= 0) {
            return right_right_case(node);
        } else {
            return right_left_case(node);
        }
    }
    return node; // When rebalancing is not needed
}

static bst_node_t* left_left_case(bst_node_t* node)
{
    return rotate_right(node);
}

static bst_node_t* left_right_case(bst_node_t* node)
{
    node->left = rotate_left(node->left);
    return rotate_right(node);
}

static bst_node_t* right_left_case(bst_node_t* node)
{
    node->right = rotate_right(node->right);
    return rotate_left(node);
}

static bst_node_t* right_right_case(bst_node_t* node)
{
    return rotate_left(node);
}

// ========================================
//  Symbol table
// ========================================
// Variables live in a stack of scopes, where each scope is one BST
// Functions are in a global BST

// ----------------------------------------
//  Stack
// ----------------------------------------

st_result_t symtable_push_scope(symtable_t* st)
{
    if (st == NULL)
        return ST_INVALID;

    scope_t* new_scope = malloc(sizeof(scope_t));
    CHECK_MEM_ERR(new_scope);

    // Scope on top of the stack has the highest depth
    // Top of the stack is the innermost block of code the
    // parser is currently inside.
    int depth;
    if (st->top == NULL)
        depth = 1;
    else
        depth = st->top->depth + 1;

    new_scope->root = NULL;
    new_scope->depth = depth;

    // Insert the new scope to the top of the stack
    scope_t* old_top = st->top;
    new_scope->parent = old_top;
    st->top = new_scope;

    return ST_OK;
}

st_result_t symtable_pop_scope(symtable_t* st)
{
    if (st == NULL || st->top == NULL)
        return ST_INVALID;

    scope_t* new_top = st->top->parent;
    bst_free(st->top->root);
    free(st->top);
    st->top = new_top;

    return ST_OK;
}

// ----------------------------------------
//  Symbol table creation & deletion
// ----------------------------------------

symtable_t* symtable_new(void)
{
    symtable_t* st = malloc(sizeof(symtable_t));
    if (st == NULL)
        return NULL;

    st->next_id = 0;
    st->top = NULL;
    st->global = NULL;
    return st;
}

st_result_t symtable_free(symtable_t* st)
{
    if (st == NULL)
        return ST_INVALID;

    bst_free(st->global);

    while (st->top != NULL)
        symtable_pop_scope(st);
    free(st);

    return ST_OK;
}

// ----------------------------------------
//  Insertion & lookup
// ----------------------------------------

// Variables

st_result_t symtable_insert_var(symtable_t* st, const char* name, type_t* type, bool is_const, symbol_t** out)
{
    if (out != NULL)
        *out = NULL;

    if (st == NULL || st->top == NULL || name == NULL)
        return ST_INVALID;

    bst_node_t* tree_root = st->top->root;
    symbol_t symbol = SYMBOL_INIT_VAR(st, type, is_const); // zero-initialises the rest of the fields.

    st_result_t res = ST_OK;
    // This stores the address of the node that is about to be inserted
    bst_node_t* inserted_node = NULL;

    // If we were to pass inserted_node as is, the function would get a copy of the address
    // stored in inserted_node (NULL). In the function it would change that address. But then we would
    // have no way of reading that changed address.
    // By passing the pointer to the address ot copies that, inside it dereferences it and changed inserted_node,
    // which we can access. (return value is used to return the new root of the tree - because
    // it could have been rotated)
    st->top->root = bst_insert(tree_root, name, symbol, &inserted_node, &res);
    if (res == ST_NOMEM)
        return res;

    if (out != NULL)
        *out = &(inserted_node->data);
    return res;
}

symbol_t* symtable_lookup_var(symtable_t* st, const char* name)
{
    return symtable_lookup_var_ex(st, name, NULL);
}

symbol_t* symtable_lookup_var_ex(symtable_t* st, const char* name, int* found_depth)
{
    // Default value when not found
    if (found_depth != NULL)
        *found_depth = -1;

    if (st == NULL || st->top == NULL || name == NULL)
        return NULL;

    scope_t* scope = st->top;
    // Go over every scope on the stack and find the node
    while (scope != NULL) {
        bst_node_t* node = bst_find(name, scope->root);
        if (node != NULL) {
            if (found_depth != NULL)
                *found_depth = scope->depth;
            return &(node->data);
        }
        scope = scope->parent;
    }
    return NULL;
}

symbol_t* symtable_lookup_local(symtable_t* st, const char* name)
{
    if (st == NULL || st->top == NULL || name == NULL)
        return NULL;

    bst_node_t* node = bst_find(name, st->top->root);
    if (node == NULL)
        return NULL;
    return &(node->data);
}

// Functions

st_result_t symtable_insert_func(symtable_t* st, const char* name, symbol_t** out)
{
    if (out != NULL)
        *out = NULL;

    if (st == NULL || name == NULL)
        return ST_INVALID;

    bst_node_t* tree_root = st->global; // Functions can only be inserted into the global scope
    symbol_t symbol = SYMBOL_INIT_FUNC(st); // Most values are NULL - it is the callers job to fill them

    st_result_t res = ST_OK;
    bst_node_t* inserted_node = NULL;
    st->global = bst_insert(tree_root, name, symbol, &inserted_node, &res); // Return value is the new root
    if (res == ST_NOMEM)
        return res;

    if (out != NULL)
        *out = &(inserted_node->data);
    return res;
}

symbol_t* symtable_lookup_func(symtable_t* st, const char* name)
{
    if (st == NULL || name == NULL)
        return NULL;

    bst_node_t* node = bst_find(name, st->global);
    if (node == NULL)
        return NULL;
    return &(node->data);
}

st_result_t symtable_func_add_param(symbol_t* func, const char* name, type_t* type)
{
    // Check input
    if (func == NULL || name == NULL || type == NULL)
        return ST_INVALID;

    // Check if param isnt already present
    for (int i = 0; i < func->param_count; i++) {
        if (strcmp(func->param_names[i], name) == 0)
            return ST_EXISTS;
    }

    int new_count = func->param_count + 1;
    type_t** new_param_types = (realloc(func->param_types, new_count * sizeof(type_t*)));
    CHECK_MEM_ERR(new_param_types);
    func->param_types = new_param_types;

    char** new_param_names = realloc(func->param_names, new_count * sizeof(char*));
    CHECK_MEM_ERR(new_param_names);
    func->param_names = new_param_names;

    // Copy and set the name
    char* name_copy = malloc(strlen(name) + 1);
    CHECK_MEM_ERR(name_copy);

    strcpy(name_copy, name);

    func->param_types[func->param_count] = type;
    func->param_names[func->param_count] = name_copy;
    func->param_count = new_count;

    return ST_OK;
}

// ----------------------------------------
//  For each functions
// ----------------------------------------

void symtable_foreach_func(symtable_t* st, symtable_visit_fn fn, void* ctx)
{
    if (st == NULL || fn == NULL)
        return;

    in_order(st->global, fn, ctx);
}

static void in_order(bst_node_t* node, symtable_visit_fn fn, void* ctx)
{
    if (node == NULL)
        return;

    in_order(node->left, fn, ctx);

    fn(node->key, &(node->data), ctx);

    in_order(node->right, fn, ctx);
}

void symtable_foreach_local(symtable_t* st, symtable_visit_fn fn, void* ctx)
{
    if (st == NULL || fn == NULL || st->top == NULL)
        return;

    in_order(st->top->root, fn, ctx);
}

// ----------------------------------------
//  Properties
// ----------------------------------------

int symtable_depth(symtable_t* st)
{
    if (st == NULL || st->top == NULL)
        return 0;
    return st->top->depth;
}
