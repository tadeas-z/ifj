
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
/*
---------------------------------------------------------------------------------
|  Chacking syntax of var declaration                                           |
|  (for example:  var temp: Int = [0 - 9]*)                                     |
---------------------------------------------------------------------------------
*/
static int parser_var_declaration (parser_t *parser, ast_node_t **out_node){
  bool is_const = is_expected(parser, TOKEN_VAL);
  get_next_token (parser);

  if(!(is_expected(parser, TOKEN_IDENTIFIER)))
      return ERR_SYNTAX;

  char *var_name = parser->input_token.lexeme;
  get_next_token (parser);

  type_t *var_type;
  if (is_expected(lexer->input_token.type, TOKEN_COLON)){
    get_next_token (parser);
    if (is_expected(parser, TOKEN_INTEGER))
      var_type = T_INT;
    else if (is_expected(parser,TOKEN_DOUBLE))
      var_type = T_DOUBLE;
    else if (is_expected(parser, TOKEN_STRING))
      var_type = T_STRING;
    else
      return ERR_SYNTAX;
    get_next_token(parser);
  }

  symbol_t *sym_var = NULL;
  st_result_t st_result = symtable_insert_var(parser->symtable, var_name, var_type, is_const, &sym_var);
  if(st_result == ST_NOMEM)
    return ERR_INTERNAL;

  if(is_expected(parser, TOKEN_ASSIGN)){
    get_next_token(parser);  
  }
  else
    return ERR_SYNTAX;

  ast_node_t *new_expression = NULL;
  int expr_result = parser_expression (parser->lexer, &new_expression);
  if (expr_result != ERR_OK)
    return expr_result;

  get_next_token (parser);
  if(!(is_expected(parser, TOKEN_SEMICOLON)))
    return ERR_SYNTAX;
  return ERR_OK;
}
/*
---------------------------------------------------------------------------------
|  Assigning a value to a variable                                              |
|  (for example:   temp = [0 - 9]* || '[a-z]*' || '[A-Z]*' || fun(a, b...)      |
---------------------------------------------------------------------------------
*/

static int parser_assignment_or_fun_assignment (parser_t *parser, ast_node_t **out_node){
  char *var_name = parser->input_token.lexeme;
  get_next_token (parser);

  if(is_expected (parser, TOKEN_ASSIGN)){
    get_next_token (parser);
    
    // temp = expr ;
    ast_node_t *var_expression = NULL;
    int expr_result = parser_expression (parser->lexer, &var_expression);
    if (expr_result != ERR_OK)
      return expr_result;

    get_next_token (parser);
    
    if (!is_expected(parser, TOKEN_SEMICOLON))
      return ERR_SYNTAX; 
    return ERR_OK;
  }
      //temp = fun() ;
  else if(is_expected(parser, TOKEN_LPAREN)){
    get_next_token(parser); 

    if(!(is_expected (parser, TOKEN_RPAREN))){
      while(1){
        ast_node_t *arg_expr = NULL;                                          //argument expression
        int expr_result = parser_expression (parser->lexer, &arg_expr);
        if(expr_result != ERR_OK)
          return expr_result;
  
        get_next_token(parser);
  
        if(is_expected(parser, TOKEN_COMMA)){
          get_next_token(parser);
          continue;
        }
        break;
      }
    }
    
    if (!is_expected(parser, TOKEN_RPAREN)) 
        return ERR_SYNTAX;

    get_next_token (parser);
    if (!is_expected(parser, TOKEN_SEMICOLON)) 
        return ERR_SYNTAX;
    return ERR_OK;
  }
  return ERR_SYNTAX;
}

static int if_statement (parser_t parser, ast_node_t **out_node){
  if(is_expected(parser, TOKEN_IF)){
    get_next_token(parser);

    int err = expect(parser, TOKEN_LPAREN);
    if (err != ERR_OK) 
      return err;
    
  }
    
}
