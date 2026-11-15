// Implementation of the IFJ26 imperative language compiler
// codegen_ast.h
// Author(s): Marek Zachensky (xzachem00)

#ifndef CODEGEN_AST_H
#define CODEGEN_AST_H

#include "ast.h"
#include "codegen.h"

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Write the Doxygen description of the expression codegen
//          function in this header."
// Scope: doc comment of codegen_expr only

/**
 * @brief Writes the code of an expression, its value is left on the data stack.
 *
 * Works for literals, variables, unary minus, binary operators and the
 * Int -> Double conversion. Needs the types filled in by semantics
 * (expr->type) for unary minus and binary operators.
 *
 * @param codegen Generator created by codegen_init.
 * @param expr Expression node from the AST.
 * @note String + uses the temp variables GF@tmp$1 and GF@tmp$2, they are
 *       defined in codegen_program_begin.
 */
void codegen_expr(codegen_t* codegen, ast_node_t* expr);

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Write the Doxygen description of the statement codegen
//          function in this header."
// Scope: doc comment of codegen_stmt only

/**
 * @brief Writes the code of a statement.
 *
 * Works for blocks, variable definitions, assignments, if and while.
 * Values are stored with POPS into LF@name$id.
 *
 * @param codegen Generator created by codegen_init.
 * @param stmt Statement node from the AST.
 * @note The variables are not defined here, DEFVAR of all locals must be
 *       written at the start of the function (so while does not define
 *       them again).
 */
void codegen_stmt(codegen_t* codegen, ast_node_t* stmt);

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Write the Doxygen description of the function codegen_func
//          in this header."
// Scope: doc comment of codegen_func only

/**
 * @brief Writes the code of a function definition.
 *
 * Params are taken from the data stack (last arg is on top), all locals
 * are defined at the start, then the body is written. The result is left
 * on the data stack, a Unit function always returns ().
 *
 * @param codegen Generator created by codegen_init.
 * @param func AST_FUNC_DEF node with return_type and locals filled in
 *             by semantics.
 */
void codegen_func(codegen_t* codegen, ast_node_t* func);

#endif
