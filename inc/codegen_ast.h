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

#endif
