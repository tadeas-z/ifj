// Implementation of the IFJ26 imperative language compiler
// ast.h
// Author(s): Adam Zatloukal (xzatloa01)

// Contains every AST node declaration
// and constructor declarations

#ifndef ASTREE_H
#define ASTREE_H

#include "dynamic_array.h"
#include "types.h"
#include <stdint.h>
#include <stdio.h>

// ----------------------------------------
//  AST node types(kinds)
// ----------------------------------------

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
    AST_FOR, // for i <- 1 <to/until> 10 <by> 2 <guards> do ...
    AST_EXPR_STMT, // Expression or call used as a statement

    // Expressions
    AST_INT_LIT, // 1_000 stored as number
    AST_DOUBLE_LIT, // 1.01e-3 stored as double
    AST_STRING_LIT, // "a\n"
    AST_UNIT_LIT, // () nothing literal
    AST_BOOL_LIT, // Boolthen extension
    AST_VAR_REF, // Existing variable reference
    AST_UNARY, // Unary operators -x
    AST_BINARY, // Binary operators a + b, ...
    AST_CALL, // f(a, 1)
    AST_CONVERT, // Int -> Double (Only inserted by the semantic analyzer)

    AST_MEMBER // VALMEM
} ast_node_kind_t;

typedef struct
{
    char* name; // (P)
    type_t* type; // (P)
    int id; // (S)
} ast_param_t;

typedef enum {
    AST_OP_NEG,
    AST_OP_NOT
} ast_unary_op_t;

typedef enum {
    AST_OP_ADD, // +
    AST_OP_SUB, // -
    AST_OP_MUL, // *
    AST_OP_DIV, // /
    AST_OP_LT, // <
    AST_OP_LE, // <=
    AST_OP_GT, // >
    AST_OP_GE, // >=
    AST_OP_EQ, // ==
    AST_OP_NE, // !=
    AST_OP_AND, // &&
    AST_OP_OR // ||
} ast_binary_op_t;

typedef struct
{
    dyn_arr_t func_list; // (P) list of functions (AST_FUNC_DEF) node pointers in source order
} ast_program_t;

typedef struct
{
    char* name; // (P)
    bool is_main; // (P) semantics must check there is only one function with @main
    dyn_arr_t params; // (P) array of ast_param_t* - also hold the count
    type_t* return_type; // (P, then S) NULL when not written. Semantics replaces it with the inferred type
    ast_node_t* body; // (P) AST_BLOCK node.
    dyn_arr_t locals; // (S) every (VAR_DEF or AST_FOR) in the body - pointers are borrowed
} ast_func_def_t;

// ast_node_t
// ┌──────────────────────────────┐
// │ kind   = AST_BLOCK           │
// │ line, column                 │
// │ type   = NULL / TYPE_*       │   (semantics may set it to the block's result type)
// │ block.statements:            │
// │   items ─────────────────────────► [ ptr | ptr | ptr | ... ]   capacity slots
// │   count    = 3               │        │     │     │
// │   capacity = 32              │        ▼     ▼     ▼
// └──────────────────────────────┘     VAR_DEF WHILE EXPR_STMT
typedef struct
{
    dyn_arr_t statements; // (P) statements of the block in source order (item is ast_node_t)
} ast_block_t;

typedef struct
{
    char* name; // (P) identifier
    bool is_const; // (P) var - false, val - true
    type_t* declared_type; // (P) written ": Type" or NULL when omitted
    ast_node_t* init; // (P) initializer expression or call - always present (val x: Int = a + 1 -> a+1 is the init)
    int id; // (S) unique id of this variable (used for shadowing)
} ast_var_def_t;

typedef struct
{
    char* name; // (P) variable being assigned
    ast_node_t* value; // (P) the new value (expression or call)
    int id; // (S) id of the variable this assignment refers to
} ast_assign_t;

typedef struct
{
    ast_node_t* value; // (P) returned expression or call, NULL for a bare return
} ast_return_t;

typedef struct
{
    ast_node_t* cond; // (P) the condition - any boolean
    ast_node_t* then_block; // (P) AST_BLOCK run when cond is true
    ast_node_t* else_block; // (P) AST_BLOCK run otherwise
} ast_if_t;

typedef struct
{
    ast_node_t* cond; // (P) loop condition (relational expression)
    ast_node_t* body; // (P) AST_BLOCK run while cond is true
} ast_while_t;

typedef struct
{
    char* var_name; // (P) loop variable, copied to the heap
    int var_id; // (S) filled by semantics (-1 when constructed)
    int end_id; // (S) evaluated end value id - used by codegen
    int step_id; // (S) evaluated step value id - used by codegen
    ast_node_t* from; // (P) start expression
    ast_node_t* to; // (P) end expression
    ast_node_t* step; // (P) NULL when there is no by
    bool inclusive; // (P) to = true, until = false
    dyn_arr_t guards; // (P) arr of ast_node_t* (array of conditions in source order)
    ast_node_t* body; // (P) Always AST_BLOCK node
} ast_for_t;

typedef struct
{
    ast_node_t* expr; // (P) the expression or call
} ast_expr_stmt_t;

typedef struct
{
    int64_t value; // (P) value of the literal - separators are already removed
} ast_int_lit_t;

typedef struct
{
    double value; // (P) value of the literal
} ast_double_lit_t;

typedef struct
{
    char* value; // (P) the decoded contents - without quotes, escapes replaced
    size_t length; // (P) in bytes
} ast_string_lit_t;

typedef struct
{
    bool value;
} ast_bool_lit_t;

typedef struct
{
    char* name; // (P) the identifier
    int id; // (S) id of the variable it refers to
} ast_var_ref_t;

typedef struct
{
    ast_unary_op_t op; // (P) NEG or NOT
    ast_node_t* operand; // (P)
} ast_unary_t;

typedef struct
{
    ast_binary_op_t op; // (P) + - * / < <= > >= == != && ||
    ast_node_t* left; // (P)
    ast_node_t* right; // (P)
} ast_binary_t;

typedef struct
{
    char* name; // (P) the called functions identifier
    dyn_arr_t args; // (P) argument expressions in order (ast_node_t*)
    bool is_builtin; // (S) print, readInt, ...
} ast_call_t;

typedef struct
{
    ast_node_t* operand; // (S) the Int expression being converted
} ast_convert_t;

// x.floor.toInt    MEMBER toInt -> MEMBER floor -> VAR_REF x
typedef struct
{
    ast_node_t* receiver; // (P) the expression before the "."
    char* name; // (P)
} ast_member_t;

// ----------------------------------------
//  AST node
// ----------------------------------------

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
        ast_for_t for_stmt;
        ast_expr_stmt_t expr_stmt;
        ast_int_lit_t int_lit;
        ast_double_lit_t double_lit;
        ast_string_lit_t string_lit;
        // AST_UNIT_LIT has no data
        ast_bool_lit_t bool_lit;
        ast_var_ref_t var_ref;
        ast_unary_t unary;
        ast_binary_t binary;
        ast_call_t call;
        ast_convert_t convert;
        ast_member_t member;
    };
};

// ========================================
//  Constructors
// ========================================

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Can you insert the constructor function declarations into this
//          header file?"
// Scope: declarations of the ast_new_* constructors, ast_func_def_add_param

// ----------------------------------------
//  Program
// ----------------------------------------

/**
 * @brief Consturcts a node which acts as the root of the tree (program)
 * @return NULL on memory allocation failure, else the constructed node
 */
ast_node_t* ast_new_program(void);

// ----------------------------------------
//  Functions
// ----------------------------------------

/**
 * @brief Constructs a node that represents a function definiton and fills it with default values
 * @param name function identifier
 * @param is_main true if this function was marked with @main
 * @param line line of the definition
 * @param column column of the function deifiniton
 * @return NULL on memory allocation failure, else the new node
 * @note functions params can be added using ast_func_def_add_param and
 *      return_type and the body is added directly by the parser
 */
ast_node_t* ast_new_func_def(const char* name, bool is_main, size_t line, size_t column);

/**
 * @brief Add a praam to an already existing function definition node
 * @param func_def_node function definition node
 * @param name name of the param you want to insert (copied to the heap by the function)
 * @param type type struct fot the new param
 * @return false on invalid input parameters or memory allocation failure, else true
 * @note sets the parameters (ast_param_t) id to -1
 */
bool ast_func_def_add_param(ast_node_t* func_def_node, const char* name, type_t* type);

/**
 * @brief Creates a ast block node and initializes its statement pointer array
 * @param line line where the block starts
 * @param column column where the block starts
 * @note statements can be added to this block using dyn_arr_append
 */
ast_node_t* ast_new_block(size_t line, size_t column);

// ----------------------------------------
//  Statements
// ----------------------------------------

/**
 * @brief Constructs a variable definition node and sets its properties
 * @param name name of the variable is copied and lives on the heap (is freed by ast_free)
 * @param is_const is this value declared as val or var
 * @param declared_type type struct pointer. Can be NULL when type is omitted
 * @param init expression or call (val x: Int = a + 1 -> a+1 is the init)
 * @param line line of the definition
 * @param column column where the definition starts
 * @return NULL on invalid params or memory allocation failure, node on success
 */
ast_node_t* ast_new_var_def(const char* name, bool is_const, type_t* declared_type, ast_node_t* init, size_t line, size_t column);

/**
 * @brief Construct an assignment node
 * @param name name of the variable being assigned
 * @param value right hand side of the assignment
 * @param line of the assignment
 * @param column columnt of the assignment
 * @return NULL on invalid params or allocation failure, else new node
 */
ast_node_t* ast_new_assign(const char* name, ast_node_t* value, size_t line, size_t column);

/**
 * @brief Constructs a return node and fills its return value
 * @param value can be any statement or expression or value
 * @param line line of the return
 * @param column column where the return starts
 * @return NULL on allocation failure, else new node
 * @note NULL is a valid value. This can happen when no expression or value or statement is returned
 */
ast_node_t* ast_new_return(ast_node_t* value, size_t line, size_t column);

/**
 * @brief Constructs a new if node
 * @param cond condition of the if statement - REQUIRED
 * @param then_block block of code after the then keyword - REQUIRED
 * @param else_block block of code after the else keyword - NOT REQUIRED IF BOOLTHEN IS IMPLEMENTED
 * @param line line of the if statement
 * @param column column of the if statement
 * @return NULL on memory allocation failure, else node
 */
ast_node_t* ast_new_if(ast_node_t* cond, ast_node_t* then_block, ast_node_t* else_block, size_t line, size_t column);

/**
 * @brief Constructs a new while node
 * @param cond condition of the while loop - REQUIRED
 * @param body block of code after the condition - REQUIRED
 * @param line line of the while loop
 * @param column column of the while loop
 * @return NULL on memory allocation failure, else node
 */
ast_node_t* ast_new_while(ast_node_t* cond, ast_node_t* body, size_t line, size_t column);

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "add docs to ast new for and check the consturctor"
// Scope: declaration and doc comment of ast_new_for only

/**
 * @brief Constructs a for loop node (CONTROL extension) - for name <- from to/until to [by step]
 * @param name name of the loop variable - REQUIRED. Copied to the heap (freed by ast_free)
 * @param from start of the range - REQUIRED
 * @param to end of the range - REQUIRED
 * @param step step expression after the by keyword, NULL when by is omitted (step 1)
 * @param inclusive true for "to" (end included), false for "until" (end excluded)
 * @param line line of the for keyword
 * @param column column of the for keyword
 * @return NULL on invalid params or memory allocation failure, else new node
 * @note on failure the caller still owns from, to and step.
 *       Guards are added in order with dyn_arr_append(&node->for_stmt.guards, cond),
 *       body (AST_BLOCK) is set directly by the parser.
 *       var_id, end_id and step_id are set to -1, semantics fills them
 */
ast_node_t* ast_new_for(const char* name, ast_node_t* from, ast_node_t* to,
    ast_node_t* step, bool inclusive, size_t line, size_t column);

/**
 * @brief Constructs an expression statement node - wraps an expression or call
 *        so it can be stored in a block (print(x) or the last value of a function)
 * @param expr the wrapped expression or call - REQUIRED
 * @param line line of the statement
 * @param column column where the statement starts
 * @return NULL on invalid params or memory allocation failure, else new node
 * @note the value of expr is discarded, unless this is the last statement
 *       of a function body - then it is the function's return value
 */
ast_node_t* ast_new_expr_stmt(ast_node_t* expr, size_t line, size_t column);

// ----------------------------------------
//  Expressions
// ----------------------------------------

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "update the comments above funtions for the literals according to the others"
// Scope: doc comments of the four literal constructors only

/**
 * @brief Constructs an Int literal node
 * @param value value of the literal - separators (1_000) are already removed by the parser
 * @param line line of the literal
 * @param column column where the literal starts
 * @return NULL on memory allocation failure, else new node
 */
ast_node_t* ast_new_int_lit(int64_t value, size_t line, size_t column);

/**
 * @brief Constructs a Double literal node
 * @param value value of the literal (1.5, 1e-3, ...)
 * @param line line of the literal
 * @param column column where the literal starts
 * @return NULL on memory allocation failure, else new node
 */
ast_node_t* ast_new_double_lit(double value, size_t line, size_t column);

/**
 * @brief Constructs a String literal node
 * @param value contents of the literal - REQUIRED. Must already be decoded: without
 *        quotes and with escape sequences (\n, \t, ...) replaced by the real characters.
 *        Copied to the heap (freed by ast_free)
 * @param line line of the literal
 * @param column column where the literal starts
 * @return NULL on invalid params or memory allocation failure, else new node
 * @note the length in bytes is computed by the constructor
 */
ast_node_t* ast_new_string_lit(const char* value, size_t line, size_t column);

/**
 * @brief Constructs a Unit literal node - the () value, has no data
 * @param line line of the literal
 * @param column column where the literal starts
 * @return NULL on memory allocation failure, else new node
 */
ast_node_t* ast_new_unit_lit(size_t line, size_t column);

/**
 * @brief Constructs a Boolean literal node
 * @param value value of the literal
 * @param line line of the literal
 * @param column column where the literal starts
 * @return NULL on memory allocation failure, else new node
 */
ast_node_t* ast_new_bool_lit(bool value, size_t line, size_t column);

/**
 * @brief Constructs a variable reference node - a use of an existing variable in an expression
 * @param name name of the variable - REQUIRED. Copied to the heap (freed by ast_free)
 * @param line line of the reference
 * @param column column where the reference starts
 * @return NULL on invalid params or memory allocation failure, else new node
 * @note sets the id to -1, semantics fills it with the id of the referenced variable
 */
ast_node_t* ast_new_var_ref(const char* name, size_t line, size_t column);

/**
 * @brief Constructs a unary operation node (-x)
 * @param op the operator - AST_OP_NEG or AST_OP_NOT
 * @param operand the expression the operator is applied to - REQUIRED
 * @param line line of the operator
 * @param column column of the operator
 * @return NULL on invalid params or memory allocation failure, else new node
 */
ast_node_t* ast_new_unary(ast_unary_op_t op, ast_node_t* operand, size_t line, size_t column);

/**
 * @brief Constructs a binary operation node (a + b, a < b, ...)
 * @param op the operator
 * @param left left operand - REQUIRED
 * @param right right operand - REQUIRED
 * @param line line of the operator
 * @param column column of the operator
 * @return NULL on invalid params or memory allocation failure, else new node
 */
ast_node_t* ast_new_binary(ast_binary_op_t op, ast_node_t* left, ast_node_t* right, size_t line, size_t column);

/**
 * @brief Constructs a function call node with no arguments
 * @param name name of the called function - REQUIRED. Copied to the heap (freed by ast_free)
 * @param line line of the call
 * @param column column where the call starts
 * @return NULL on invalid params or memory allocation failure, else new node
 * @note arguments are added in order with dyn_arr_append(&node->call.args, arg),
 *       is_builtin is set later by semantics
 */
ast_node_t* ast_new_call(const char* name, size_t line, size_t column);

/**
 * @brief Constructs an Int -> Double conversion node. Used only by semantics
 * @param operand the Int expression being converted - REQUIRED
 * @return NULL on invalid params or memory allocation failure, else new node
 * @note line and column are taken from operand, the node's type is TYPE_DOUBLE
 */
ast_node_t* ast_new_convert(ast_node_t* operand);

/**
 * @brief Constructs a member access node (VALMEM extension): receiver.name
 * @param name member name as written after the "." (toString, toInt, floor, length),
 *        the string is copied - REQUIRED. Semantics checks that the member exists
 * @param receiver the expression before the "." - REQUIRED
 * @param line line of the member access
 * @param column column where the member access starts
 * @return NULL on invalid params or memory allocation failure, else new node
 * @note the new node owns receiver (freed together with it). On failure nothing is
 *       taken over, receiver still belongs to the caller
 * @note chaining x.floor.toInt is built from the inside out: the receiver of the
 *       toInt node is the floor node
 */
ast_node_t* ast_new_member(const char* name, ast_node_t* receiver, size_t line, size_t column);

// ----------------------------------------
//  Utility
// ----------------------------------------

/**
 * @brief Recursively frees the entire abstract syntax tree
 * @param node pointer to the root of the AST
 * @note this function is implemented as a recursive switch statement
 */
void ast_free(ast_node_t* node);

// ========================================
//  Debug
// ========================================

/**
 * @brief Debug output - prints the tree one node per line, children indented
 * @param node root of the (sub)tree to print, NULL prints "(null)"
 * @param out where to print - use stderr or a file, never stdout (stdout is the generated code)
 */
void ast_print(const ast_node_t* node, FILE* out);

#endif
