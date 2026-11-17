// Implementation of the IFJ26 imperative language compiler
// parser_expression.h
// Author(s): Dusan Popovic (xpopovd00)

#ifndef IFJ_PARSER_EXPRESSION_H
#define IFJ_PARSER_EXPRESSION_H

#include <stdbool.h>
#include "parser_expression.h"
#include "parser_stack.h"
#include "ast.h"
#include "lexer.h"
#include "comp_error.h"

// AI-generated using: Gemini
//               model: Gemini 4 hydrogen
// Prompt (summary): "Generate comments of parse_expression() function for 
//                    shift-reduce precedence algorithm, and comment for 
//                    parser_expression_push_back."
// Scope: parser_expression, get_precedence_index, reduce_rules, get_next_token

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

int parser_expression(lexer_t *lexer, token_t *current_token, ast_node_t **out_node);

/**
 * @brief Pushes a new symbol/token onto the back of the expression stack.
 *
 * Allocates and appends an element (terminal or non-terminal) to the end of
 * the precedence parsing stack/list used during bottom-up expression analysis.
 *
 * @param stack Pointer to the expression stack structure.
 * @param symbol Symbol or token to be pushed onto the stack.
 * @return True if the element was successfully pushed, false on memory allocation failure.
 */

void parser_expression_push_back(token_t *token);

#endif
