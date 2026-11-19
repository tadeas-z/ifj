// Implementation of the IFJ26 imperative language compiler
// symtable.h
// Author(s): Adam Zatloukal (xzatloa01)

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include "types.h"
#include <stdbool.h>

struct ast_node;

typedef enum {
    SYM_VAR,
    SYM_FUNC
} sym_kind_t;

typedef enum {
    ST_OK,
    ST_EXISTS,
    ST_NOMEM,
    ST_INVALID
} st_result_t;

typedef enum {
    SEMA_UNCHECKED,
    SEMA_IN_PROGRESS,
    SEMA_DONE
} marker_t;

typedef struct
{
    sym_kind_t kind;
    type_t* type; // variable type / function return type
    bool is_const; // val = True, var = False
    int id;
    // function only:
    bool builtin;
    bool ret_inferred;
    bool is_main;
    int param_count;
    type_t** param_types; // Dynamic array of param types
    char** param_names; // Dynamic array of param names
    marker_t sema_marker;
    struct ast_node* decl; // Declaration of a function - used & set by semantics
} symbol_t;

// implementation is private
typedef struct symtable symtable_t;

// ========================================
//  Symbol table
// ========================================
// Variables live in a stack of scopes, where each scope is one BST
// Functions are in a global BST

// ----------------------------------------
//  Stack
// ----------------------------------------

/**
 * @brief Push scope to the top of the stack
 * @param st symbol table handle
 * @return ST_INVALID when symtable handle is invalid
 *         ST_NOMEM when memory allocation fails
 *         ST_OK on successful push
 */
st_result_t symtable_push_scope(symtable_t* st);

/**
 * @brief Pop the scope that is on top of the stack
 * @param st symbol table handle
 * @return ST_INVALID when symtable handle or stack pointer is invalid
 *         ST_OK on successful pop
 */
st_result_t symtable_pop_scope(symtable_t* st);

// ----------------------------------------
//  Symbol table creation & deletion
// ----------------------------------------

/**
 * @brief Allocates memory for a symtable instance and sets default values
 * @returns handle to the newly allocated symbol table
 */
symtable_t* symtable_new(void);

/**
 * @brief frees memory allocated to passed symtable. It frees every BST and
 *          every scope, afterwards it frees itself
 * @param st symbol table handle
 * @returns ST_INVALID on invalid symbol table handle, else ST_OK
 */
st_result_t symtable_free(symtable_t* st);

// ----------------------------------------
//  Insertion & lookup
// ----------------------------------------

/* Variables */

/**
 * @brief inserts variable to scope where stack pointer points
 * @param st symbol table handle
 * @param variable name
 * @param type type_t struct (in case of future expansions) with appropriate type
 * @param is_const in case variable is decalred as "val"
 * @param out pointer to the address of the inserted symbol within the symbol table THIS IS AN OUTPUT
 * @return ST_INVALID on invalid handle, invalid stack, invalid name.
 *         ST_NOMEM in case of failed memory allocation within allocate_node.
 *         ST_EXISTS when node with mathing name is already inserted in the top scope IN THIS CASE out IS SET TO THE ALREADY INSERTED NODE.
 *         ST_OK on successful insertion, in this case out points to the newly inserted node
 */
st_result_t symtable_insert_var(symtable_t* st, const char* name, type_t* type, bool is_const, symbol_t** out);

/**
 * @brief Searches the provided lookup table for a node with mathing name/key IN EVERY SCOPE
 * @param st symbol table handle
 * @param name name/key of the node (symbol) you want to find
 * @return NULL on invalid symbol table handle, invalid stack pointer, name OR when the node wasnt found
 *         symbol_t* when the node with matching name was successfuly found
 * @return Since this function searches every scope it returns the first match
 */
symbol_t* symtable_lookup_var(symtable_t* st, const char* name);

/**
 * @brief Searches the provided lookup table for a node with mathing name/key IN TOP SCOPE ONLY
 * @param st symbol table handle
 * @param name name/key of the node (symbol) you want to find
 * @return NULL on invalid symbol table handle, invalid stack pointer, name OR when the node wasnt found
 *         symbol_t* when the node with matching name was successfuly found
 */
symbol_t* symtable_lookup_local(symtable_t* st, const char* name);

/**
 * @brief Exactly the same as symtable_lookup-var, except it also  returns found depth
 *        which is passed as a pointer
 * @note this wont be used unless LAMBDA extension is added
 */
symbol_t* symtable_lookup_var_ex(symtable_t* st, const char* name, int* found_depth);

/* Functions */

/**
 * @brief Inserts function into the global scope of the symbol table
 * @param st symbol table handle
 * @param name function identifier
 * @param out THIS IS AN OUTPUT pointer to the symbol which was inserted
 * @return ST_INVALID when name or st is invalid
 *         ST_NOMEM when memory allocation fails
 *         ST_EXISTS when function with the provided identifier is already inserted
 *         ST_OK otherwise
 * @note out returns the inserted symbol and it is the callers job to set the symbols properties
 *       like the return type etc.
 */
st_result_t symtable_insert_func(symtable_t* st, const char* name, symbol_t** out);

/**
 * @brief Finds symbol whose nodes name (key) matches with the given name
 * @param st symbol table handle
 * @name function identifier for which youre looking
 * @return symbol which was connected to the node where a match was found
 *         NULL on invalid input (st handle or name) OR when no match was found
 */
symbol_t* symtable_lookup_func(symtable_t* st, const char* name);

/**
 * @brief Appends function type and name to dynamic arrays inside of the provided symbol
 * @param func pointer to the symbol where the param should be added
 * @param name new param name
 * @param type pointer a type struct
 * @return ST_INVALID on invalid func, name, type.
 *         ST_EXISTS when param with a matching name is already in func->param_names.
 *         ST_NOMEM on memory allocation failure
 *         ST_OK on successful param insertion
 */
st_result_t symtable_func_add_param(symbol_t* func, const char* name, type_t* type);

// ----------------------------------------
//  For each functions
// ----------------------------------------

typedef void (*symtable_visit_fn)(const char* name, symbol_t* sym, void* ctx);

/**
 * @brief Traverses the BST in the global scope using in order
 * @param st symbol table handle
 * @param fn pointer to a function (see symtable_visit_fn declaration)
 * @param ctx additional params that can be provided to fn
 */
void symtable_foreach_func(symtable_t* st, symtable_visit_fn fn, void* ctx);

/**
 * @brief Traverses the BST inside of a scope that is on top of the stack
 * @param st symbol table handle
 * @param fn pointer to a function (see symtable_visit_fn declaration)
 * @param ctx additional params that can be provided to fn
 */
void symtable_foreach_local(symtable_t* st, symtable_visit_fn fn, void* ctx);

// ----------------------------------------
//  Properties
// ----------------------------------------

/**
 * @brief returns the number of scopes on the stack
 * @param st symbol table handle
 * @note the scope on the bottom of the stack has depth 1, each
 *      scope on top of that has its depth as last_scope_depth+1.
 *      The global scope for functions has a depth of 0
 */
int symtable_depth(symtable_t* st);

#endif
