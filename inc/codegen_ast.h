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
// Prompt: "Update the description for BOOLTHEN (true, false, !, && and ||)"
// Prompt: "Add a note that an unknown node ends with exit code 99"
// Prompt: "Update the description for VALMEM members"
// Prompt: "Update the description for if as expression (CONTROL)"
// Scope: doc comment of codegen_expr only

/**
 * @brief Writes the code of an expression, its value is left on the data stack.
 *
 * Works for literals (also true and false), variables, unary minus, !,
 * binary operators and the Int -> Double conversion. && and || are short
 * circuit, the right side runs only when it is needed. A member (x.length,
 * x.toString, x.toInt, x.floor) is a call of a built-in with x as the
 * argument. An if as expression leaves the value of the last statement of
 * the branch that runs. Needs the types filled in by semantics (expr->type)
 * for unary minus and binary operators.
 *
 * @param codegen Generator created by codegen_init.
 * @param expr Expression node from the AST.
 * @note String + uses the temp variables GF@tmp$1 and GF@tmp$2, they are
 *       defined in codegen_program_begin.
 * @note Unknown node kind is an internal error, the program ends with 99.
 */
void codegen_expr(codegen_t* codegen, ast_node_t* expr);

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Write the Doxygen description of the statement codegen
//          function in this header."
// Prompt: "Update the description for if without else (BOOLTHEN)"
// Prompt: "Add a note that an unknown node ends with exit code 99"
// Prompt: "Update the description for for loops (CONTROL)"
// Scope: doc comment of codegen_stmt only

/**
 * @brief Writes the code of a statement.
 *
 * Works for blocks, variable definitions, assignments, if (else can be
 * missing), while and for. In for the start, end and step are computed
 * once, a minus step goes down, a zero step ends with error 40 and a
 * false guard skips the body.
 * Values are stored with POPS into LF@name$id.
 *
 * @param codegen Generator created by codegen_init.
 * @param stmt Statement node from the AST.
 * @note The variables are not defined here, DEFVAR of all locals must be
 *       written at the start of the function (so while does not define
 *       them again).
 * @note Unknown node kind is an internal error, the program ends with 99.
 */
void codegen_stmt(codegen_t* codegen, ast_node_t* stmt);

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Write the Doxygen description of the function codegen_func
//          in this header."
// Prompt: "Update the description for locals of for and a body that ends
//          with an if (CONTROL)"
// Scope: doc comment of codegen_func only

/**
 * @brief Writes the code of a function definition.
 *
 * Params are taken from the data stack (last arg is on top), all locals
 * (also the variable, end and step of every for) are defined at the start,
 * then the body is written. The result is the value of the last statement
 * (also an if) left on the data stack, a Unit function always returns ().
 *
 * @param codegen Generator created by codegen_init.
 * @param func AST_FUNC_DEF node with return_type and locals filled in
 *             by semantics.
 */
void codegen_func(codegen_t* codegen, ast_node_t* func);

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Write the Doxygen description of the function codegen_program
//          in this header."
// Scope: doc comment of codegen_program only

/**
 * @brief Writes the code of the whole program.
 *
 * Writes the start of the program, all functions in source order, the
 * built in functions and the end of the program.
 *
 * @param codegen Generator created by codegen_init.
 * @param program AST_PROGRAM node checked by semantics.
 */
void codegen_program(codegen_t* codegen, ast_node_t* program);

#endif
