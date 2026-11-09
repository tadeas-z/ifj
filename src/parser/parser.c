#include <stdio.h>
#include <stdlib.h>
#include <strlen.h>

#include "parser.h"

typedef struct {
  lexer_t *lexer;
  token_t input_token;
  symtable_t *symtable;
}parser_t;

static int get_next_token(parser_t *parser){
  lexer_status_t status = lexer_next(parser->lexer, &parser->input_token);
  if(status == ERR_LEXICAL)
    return ERR_LEXICAL;
  return ERR_OK;
}

static bool is_expected (parser_t *parser, token_type_t token){
  if(parser->input_token.type == token)
    return true;
  else
    return false;
}
static int parser_var_declaration (parser_t *parser, ast_node_t **out_node){
  bool is_const = is_expected(parser, TOKEN_VAR);
  get_next_token (parser);

  if(!(is_expected(parser, TOKEN_IDENTIFIER)))
      return ERR_SYNTAX;

  char var_name = parser->input_token.lexeme;
  get_next_token (parser);
  
}
