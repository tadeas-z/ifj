// Implementation of the IFJ26 imperative language compiler
// parser_expression.h
// Author(s): Dusan Popovic (xpopovd00)

// Precedence parser for expressions using a stack and a precedence table.
// Called from the recursive descend parser, shares its current token and returns
/// the expressions AST subtree. Also converts literals to values

#ifndef IFJ_PARSER_EXPRESSION_H
#define IFJ_PARSER_EXPRESSION_H

#include "ast.h"
#include "comp_error.h"
#include "lexer.h"

// AI-generated using: Gemini
//                model: Gemini 4 HYDROGEN
// Prompt (summary): "Generate Doxygen comments for parser_expression() and
//                    parser_expression_push_back()."
// Scope: parser_expression, parser_expression_push_back

/**
 * @brief Parses an expression using the shift-reduce precedence algorithm.
 *
 * Enters with `current_token` positioned on the first token of the expression,
 * and exits with `current_token` positioned on the first token following the expression.
 *
 * @param lexer Pointer to the lexer instance used for retrieving tokens.
 * @param current_token Pointer to the shared current token between parser and expression parser.
 * @param out_node Pointer to an AST node pointer where the root of the generated tree will be stored.
 * @return ERR_OK (0) on success, or an error code (ERR_LEXICAL, ERR_SYNTAX, ERR_INTERNAL).
 */
error_code_t parser_expression(lexer_t* lexer, token_t* current_token, ast_node_t** out_node);

/**
 * @brief Stores a lookahead token to be consumed on the next expression token advance.
 *
 * Moves the given token into an internal pending buffer and zeroes out the caller's
 * token structure so ownership is transferred without double-disposal.
 *
 * @param token Pointer to the lookahead token to push back (ownership is transferred).
 */
void parser_expression_push_back(token_t* token);

#endif
