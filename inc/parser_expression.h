#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "lexer.h"
#include "astree.h"
#include "parser_stack.h"

/**
 * @brief Parses an expression using the shift-reduce precedence algorithm.
 * 
 * @param lexer Pointer to the lexer instance used for retrieving tokens.
 * @param out_node Pointer to an AST node pointer where the root of the generated tree will be stored.
 * @return int 0 on success, syntax error code (e.g., 2) on failure.
 */
int parse_expression(lexer_t *lexer, ast_node_t **out_node);

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
static bool reduce_rules(tstack_t *stack);

/**
 * @brief Safely fetches the next token from the lexer stream.
 * 
 * @param lexer Pointer to the lexer structure.
 * @return token_t The next token retrieved from the input.
 */
static token_t get_next_token(lexer_t *lexer);

#endif
