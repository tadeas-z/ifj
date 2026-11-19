// Implementation of the IFJ26 imperative language compiler
// parser.h
// Author(s): Dušan Popović (xpopovd00)

#ifndef IFJ_PARSER_H
#define IFJ_PARSER_H

#include "ast.h"
#include "comp_error.h"
#include "lexer.h"

// AI-generated using: Gemini
//                model: Gemini 4 HYDROGEN
// Prompt (summary): "Generate Doxygen comments for parser_main() and
//                    parser_if_expression() in parser.h."
// Scope: parser_main, parser_if_expression

/**
 * @brief Main entry point for syntactic analysis of an IFJ26 source program.
 *
 * Validates the mandatory program prolog (`import ifj26.*`), parses all top-level
 * function definitions (including the `main` entry function), enforces indentation-based
 * block structures, and constructs the abstract syntax tree (AST).
 *
 * @param lexer Pointer to an initialized lexer instance providing input tokens.
 * @param out_ast Output pointer where the root of the generated AST will be stored.
 * @return ERR_OK (0) on successful parsing, or a corresponding error code
 *         (ERR_LEXICAL, ERR_SYNTAX, ERR_INTERNAL).
 */
error_code_t parser_main(lexer_t *lexer, ast_node_t **out_ast);

/**
 * @brief Parses an `if` expression (`if <cond> then <expr> else <expr>`) for CONTROL.
 *
 * Called by the precedence expression parser when `if` appears in operand position.
 *
 * @param lexer Pointer to the lexer instance used for retrieving tokens.
 * @param current_token Pointer to the shared current token between expression and statement parsers.
 * @param out_node Output pointer where the resulting `AST_IF` node will be stored.
 * @return ERR_OK (0) on success, or an error code (ERR_LEXICAL, ERR_SYNTAX, ERR_INTERNAL).
 */
int parser_if_expression(lexer_t *lexer, token_t *current_token, ast_node_t **out_node);

#endif
