// Implementation of the IFJ26 imperative language compiler
// parser.h
// Author(s): Dušan Popović (xpopovd00)

#ifndef IFJ_PARSER_H
#define IFJ_PARSER_H

#include "comp_error.h"      //compiler exit
#include "parser_expression.h"
#include "types.h"

// AI-generated using: Gemini
//               model: Gemini 4 hydrogen
// Prompt (summary): "Generate comment of parse_main() function for 
//                    syntactic analysis of an IFJ26 source program."
// Scope: parser_main

/**

 * @brief Main entry point for syntactic analysis of an IFJ26 source program.
 *
 * Validates the mandatory program prolog (`import ifj26.*`), parses all top-level
 * function definitions (including the `@main` entry function), enforces indentation-based
 * block structures, and constructs the abstract syntax tree (AST).
 *
 * @param lexer Pointer to an initialized lexer instance providing input tokens.
 * @param out_ast Output pointer where the root of the generated AST will be stored.
 * @return ERR_OK (0) on successful parsing, or a corresponding error code
 *         (ERR_LEXICAL, ERR_SYNTAX).
 */
int parser_main (lexer_t *lexer, ast_node_t **out_ast);

#endif
