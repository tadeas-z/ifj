// Implementation of the IFJ26 imperative language compiler
// astree.h
// Author(s): Adam Zatloukal (xzatloa01)

#ifndef ASTREE_H
#define ASTREE_H

#include <stdio.h>
#include <stdint.h>
#include "symtable.h"

typedef struct ast_node ast_node_t;

typedef enum {
    // Program structure
    AST_PROGRAM, // The entire program (list of functions)
    AST_FUNC_DEF, // def name(params): Type = body
    AST_BLOCK, // Indented sequence of statements

    // Statements
    AST_VAR_DEF, // val/var x [: Type] = init
    AST_ASSIGN, // x = value
    AST_RETURN, // return [value]
    AST_IF, // if cond then ... else ...
    AST_WHILE, // while cond do ...
    AST_EXPR_STMT, // Expression or call used as a statement

    // Expressions
    AST_INT_LIT, // 1_000 stored as number
    AST_DOUBLE_LIT, // 1.01e-3 stored as double
    AST_STRING_LIT, // "a\n"
    AST_UNIT_LIT, // () nothing literal
    AST_VAR_REF, // Existing variable reference
    AST_UNARY, // Unary operators -x
    AST_BINARY, // Binary operators a + b, ...
    AST_CALL, // f(a, 1)
    AST_CONVERT // Int -> Double (Only inserted by the semantic analyzer)
} ast_node_kind_t;

typedef struct
{
    ast_node_t** items;
    size_t count, capacity;
} ast_list_t;

typedef struct
{
    char* name;         // P
    type_t* type;       // P
    int id;             // S
}ast_param_t;

typedef enum
{
    AST_OP_NEG
}ast_unary_op_t;

typedef enum {
    AST_OP_ADD,            // +
    AST_OP_SUB,            // -
    AST_OP_MUL,            // *
    AST_OP_DIV,            // /
    AST_OP_LT,             // <
    AST_OP_LE,             // <=
    AST_OP_GT,             // >
    AST_OP_GE,             // >=
    AST_OP_EQ,             // ==
    AST_OP_NE              // !=
} ast_binary_op_t;

typedef struct
{
    ast_list_t func_list;      // (P) list of functions (AST_FUNC_DEF) nodes in source order
} ast_program_t;

typedef struct
{
    char* name;             // (P)
    bool is_main;           // (P) semantics must check there is only one function with @main
    ast_param_t *params;    // (P) array of function params + count
    int param_count;
    type_t *return_type;    // (P, then S) NULL when not written. Semantics replaces it with the inferred type
    ast_node_t* body;       // (P) AST_BLOCK node.
    ast_list_t locals;      // (S) every VAR_DEF in the body - pointers are borrowed
} ast_func_def_t;

typedef struct
{
    ast_list_t statements;      // (P) statements of the block in source order
} ast_block_t;

typedef struct
{
    char* name;             // (P) identifier
    bool is_const;          // (P) var - false, val - true
    type_t* declared_type;  // (P) written ": Type" or NULL when omitted
    ast_node_t* init;       // (P) initializer expression or call - always present
    int id;                 // (S) unique id of this variable (used for shadowing)
} ast_var_def_t;

typedef struct
{
    char* name;             // (P) variable being assigned
    ast_node_t* value;      // (P) the new value (expression or call)
    int id;                 // (S) id of the variable this assignment refers to
} ast_assign_t;

typedef struct
{
    ast_node_t* value;      // (P) returned expression or call, NULL for a bare return
} ast_return_t;

typedef struct
{
    ast_node_t* cond;           // (P) the condition - a relational expression
    ast_node_t* then_block;     // (P) AST_BLOCK run when cond is true
    ast_node_t* else_block;     // (P) AST_BLOCK run otherwise
} ast_if_t;

typedef struct
{
    ast_node_t* cond;       // (P) loop condition (relational expression)
    ast_node_t* body;       // (P) AST_BLOCK run while cond is true
} ast_while_t;

typedef struct
{
    ast_node_t* expr;       // (P) the expression or call
} ast_expr_stmt_t;

typedef struct
{
    int64_t value;       // (P) value of the literal - separators are already removed
} ast_int_lit_t;

typedef struct
{
    double value;       // (P) value of the literal
} ast_double_lit_t;

typedef struct
{
    char* value;        // (P) the decoded contents - without quotes, escapes replaced
    size_t length;      // (P) in bytes
} ast_string_lit_t;

typedef struct
{
    char* name;         // (P) the identifier
    int id;             // (S) id of the variable it refers to
} ast_var_ref_t;

typedef struct
{
    ast_unary_op_t op;         // (P) only negation (without extensions)
    ast_node_t* operand;       // (P)
} ast_unary_t;

typedef struct
{
    ast_binary_op_t op;         // (P) + - * / < <= > >= == !=
    ast_node_t* left;           // (P)
    ast_node_t* right;          // (P)
} ast_binary_t;

typedef struct
{
    char* name;                 // (P) the called functions identifier
    ast_list_t args;            // (P) argument expressions in order
    bool is_builtin;            // (S) print, readInt, ...
} ast_call_t;

typedef struct
{
    ast_node_t* operand;        // (S) the Int expression being converted
} ast_convert_t;

struct ast_node {
    ast_node_kind_t kind;
    size_t line, column;
    type_t* type;
    union {
        ast_program_t program;
        ast_func_def_t func_def;
        ast_block_t block;
        ast_var_def_t var_def;
        ast_assign_t assign;
        ast_return_t return_stmt;
        ast_if_t if_stmt;
        ast_while_t while_stmt;
        ast_expr_stmt_t expr_stmt;
        ast_int_lit_t int_lit;
        ast_double_lit_t double_lit;
        ast_string_lit_t string_lit;
        // AST_UNIT_LIT has no data
        ast_var_ref_t var_ref;
        ast_unary_t unary;
        ast_binary_t binary;
        ast_call_t call;
        ast_convert_t convert;
    };
};

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Can you insert the constructor function declarations into this
//          header file?"
// Scope: declarations of the ast_new_* constructors, ast_func_def_add_param
//        and ast_list_append only

// Program structure
ast_node_t* ast_new_program(void);
// Creates a function with no params, return_type = NULL and body = NULL.
// The parser adds params with ast_func_def_add_param and then sets
// func_def.return_type and func_def.body directly.
ast_node_t* ast_new_func_def(const char* name, bool is_main, size_t line, size_t column);
bool ast_func_def_add_param(ast_node_t* func_def, const char* name, type_t* type);
// Creates an empty block, statements are added with ast_list_append.
ast_node_t* ast_new_block(size_t line, size_t column);

// Statements
// declared_type is NULL when ": Type" is omitted.
ast_node_t* ast_new_var_def(const char* name, bool is_const, type_t* declared_type, ast_node_t* init, size_t line, size_t column);
ast_node_t* ast_new_assign(const char* name, ast_node_t* value, size_t line, size_t column);
// value is NULL for a bare return.
ast_node_t* ast_new_return(ast_node_t* value, size_t line, size_t column);
ast_node_t* ast_new_if(ast_node_t* cond, ast_node_t* then_block, ast_node_t* else_block, size_t line, size_t column);
ast_node_t* ast_new_while(ast_node_t* cond, ast_node_t* body, size_t line, size_t column);
ast_node_t* ast_new_expr_stmt(ast_node_t* expr, size_t line, size_t column);

// Expressions
ast_node_t* ast_new_int_lit(int64_t value, size_t line, size_t column);
ast_node_t* ast_new_double_lit(double value, size_t line, size_t column);
// value must already be decoded, length is in bytes.
ast_node_t* ast_new_string_lit(const char* value, size_t length, size_t line, size_t column);
ast_node_t* ast_new_unit_lit(size_t line, size_t column);
ast_node_t* ast_new_var_ref(const char* name, size_t line, size_t column);
ast_node_t* ast_new_unary(ast_unary_op_t op, ast_node_t* operand, size_t line, size_t column);
ast_node_t* ast_new_binary(ast_binary_op_t op, ast_node_t* left, ast_node_t* right, size_t line, size_t column);
// Creates a call with no arguments, arguments are added with ast_list_append.
ast_node_t* ast_new_call(const char* name, size_t line, size_t column);
// Used only by semantics. Takes line and column from operand, type is TYPE_DOUBLE.
ast_node_t* ast_new_convert(ast_node_t* operand);

// Appends node to the end of list, growing it when full. Returns false if
// allocation fails, the node is then still owned by the caller.
bool ast_list_append(ast_list_t* list, ast_node_t* node);



// Frees node, all its children, lists and strings. NULL is allowed.
// Nodes in func_def.locals are borrowed, only the list's array is freed.
void ast_free(ast_node_t* node);

// Debug output: prints the tree one node per line, children indented.
void ast_print(const ast_node_t* node, FILE* out);

#endif
