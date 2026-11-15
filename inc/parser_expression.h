// Implementation of the IFJ26 imperative language compiler
// parser_expression.h
// Author(s): Dusan Popovic (xpopovd00)

#ifndef IFJ_PARSER_EXPRESSION_H
#define IFJ_PARSER_EXPRESSION_H

#include <stdbool.h>
#include "lexer.h"
#include "astree.h"
#include "parser_stack.h"

// AI-generated using: Gemini
//               model: Gemini 4 hydrogen
// Prompt (summary): "Generate comments of parse_expression() function for 
//                    shift-reduce precedence algorithm."
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

/* --- Helper Functions --- */

/**
 * @brief Maps a token type to its corresponding row/column index in the precedence table.
 * 
 * @param t The token type to map.
 * @return int The precedence matrix index, or -1 if the token type is invalid in expressions.
 */
static int get_precedence_index(token_type_t t);

/**
 * @brief Reduces grammar rules on the stack when a reduction opportunity ('>') is encountered.
 * 
 * @param stack Pointer to the precedence parser stack.
 * @return true If a rule was successfully reduced and pushed back to the stack.
 * @return false If no matching grammar rule was found or stack error occurred.
 */
static int reduce_rules(tstack_t *stack);

/**
 * @brief Safely fetches the next token from the lexer stream.
 * 
 * @param lexer Pointer to the lexer structure.
 * @return token_t The next token retrieved from the input.
 */
static token_t get_next_token(lexer_t *lexer);

#endif
