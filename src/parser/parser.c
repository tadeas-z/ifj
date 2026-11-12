#include "parser.h"

typedef struct {
  lexer_t *lexer;
  token_t input_token;
  symtable_t *symtable;
  size_t current_indent;
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


static int parser_block (parser_t *parser, ast_node_t **out_node);
static int parser_statement (parser_t *parser, ast_node_t **out_node);

/*
---------------------------------------------------------------------------------
|  Chacking syntax of VAR/VAL declaration                                           |
|  (for example:  var temp: Int = 10)                                     |
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
  if (is_expected(parser, TOKEN_COLON)){
    get_next_token (parser);
    if (is_expected(parser, TOKEN_TYPE_INT))
      var_type = T_INT;
    else if (is_expected(parser,TOKEN_TYPE_DOUBLE))
      var_type = T_DOUBLE;
    else if (is_expected(parser, TOKEN_TYPE_STRING))
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
|  (for example:   temp = a + 5;  || fun(a, b);                                 |
---------------------------------------------------------------------------------
*/

static int parser_assignment_or_funcall_assignment (parser_t *parser, ast_node_t **out_node){
  char *var_name = parser->input_token.lexeme;
  get_next_token (parser);

  // temp = <expr> ;
  if(is_expected (parser, TOKEN_ASSIGN)){
    get_next_token (parser);
    
    ast_node_t *var_expression = NULL;
    int expr_result = parser_expression (parser->lexer, &var_expression);
    if (expr_result != ERR_OK)
      return expr_result;

    get_next_token (parser);
    
    if (!is_expected(parser, TOKEN_SEMICOLON))
      return ERR_SYNTAX; 
    return ERR_OK;
  }
    // func(<arg>) ;
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

/*
---------------------------------------------------------------------------------
|  Check syntax of IF conditional                                                |
|  (for example:   if <expr> then                                                |
|                      <stmts>                                                   |
|                  else                                                          |
|                      <stmts>                                                   |
---------------------------------------------------------------------------------
*/

static int parser_if_statement (parser_t *parser, ast_node_t **out_node){
  if(is_expected(parser, TOKEN_IF)){
    get_next_token(parser);

    ast_node_t *cond_expr = NULL;                                          //condition expression
    int expr_result = parser_expression (parser->lexer, &cond_expr);
    if(expr_result != ERR_OK)
      return expr_result;

    get_next_token(parser);
    
    if(!(is_expected(parser, TOKEN_THEN))){
      ast_free(cond_expr);
      return ERR_SYNTAX;
    }
    
    get_next_token(parser);
    //if_block
    ast_node_t *if_block = NULL;
    int block_syntax = parser_block(parser, &if_block); 
    if (block_syntax != ERR_OK){ 
      ast_free(cond_expr);
      return block_syntax;
    }
    
    //skip new line before else_condition
    while (is_expected(parser, TOKEN_NEWLINE)) {
        get_next_token(parser);
    }
    
    //else_condition
    if (!(is_expected(parser, TOKEN_ELSE))) 
        return ERR_SYNTAX;
    
    get_next_token(parser);
    //else_block
    ast_node_t *else_block = NULL;
    block_syntax = parser_block(parser, &else_block);
    return block_syntax;
  }   
  return ERR_SYNTAX;
}

/*
---------------------------------------------------------------------------------
|  Check syntax of WHILE loop                                                    |
|  (for example:  while <expr> do                                                |
|                      <stmts>                                                   |
---------------------------------------------------------------------------------
*/

static int parser_while_statement (parser_t *parser, ast_node_t **out_node){
  if(is_expected(parser, TOKEN_WHILE)){
    
    get_next_token(parser);

    ast_node_t *cond_expr = NULL;
    int expr_result = parser_expression (parser->lexer, &cond_expr);
    if(expr_result != ERR_OK)
      return expr_result;

    get_next_token(parser);

    if(!(is_expected(parser, TOKEN_DO))){
      ast_free(cond_expr);
      return ERR_SYNTAX;
    }

    get_next_token(parser);
    //while_block
    ast_node_t *while_block = NULL;
    int block_syntax = parser_block(parser, &while_block); 
    if (block_syntax != ERR_OK){ 
      ast_free(cond_expr);
      return block_syntax;
    }
    return ERR_OK;
  }
  return ERR_SYNTAX;
}

/*
---------------------------------------------------------------------------------
|  Helper function for parsing indented block syntax                            |
---------------------------------------------------------------------------------
*/

static int parser_block(parser_t *parser, ast_node_t **out_node) {
    (void)out_node;

    while (is_expected(parser, TOKEN_NEWLINE)) {
        int block_syntax = get_next_token(parser);
        if (block_syntax != ERR_OK) 
          return block_syntax;
    }

    // Indention must starts on the new line
    if (!parser->input_token.starts_line) {
        return ERR_SYNTAX;
    }

    size_t parent_indent = parser->current_indent;
    size_t block_indent = parser->input_token.indentation;

    // Indention of new block must be bigger then indention of parent block
    if (block_indent <= parent_indent) {
        return ERR_SYNTAX; 
    }

    size_t old_indent = parser->current_indent;
    parser->current_indent = block_indent;

    if (symtable_push_scope(parser->symtable) != ST_OK) {
        return ERR_INTERNAL;
    }

    while (!is_expected(parser, TOKEN_EOF)) {
        if (is_expected(parser, TOKEN_NEWLINE)) {
          get_next_token(parser);
          continue;
        }

        if (parser->input_token.starts_line) {
            // If indent of smaller then block, that means end of block
            if (parser->input_token.indentation < block_indent) {
                break; 
            }

            if (parser->input_token.indentation != block_indent) {
                symtable_pop_scope(parser->symtable);
                return ERR_SYNTAX; 
            }
        }

        int statement_syntax = parser_statement(parser, out_node);
        if (statement_syntax != ERR_OK) {
            symtable_pop_scope(parser->symtable);
            return statement_syntax;
        }
    }

    //end of domen and return old indent
    symtable_pop_scope(parser->symtable);
    parser->current_indent = old_indent;

    return ERR_OK;
}

/*
---------------------------------------------------------------------------------
|  Statement (Router)                                                           |
|  Routes execution to the appropriate rule based on the starting token         |
---------------------------------------------------------------------------------
*/

static int parser_statement (parser_t *parser, ast_node_t **out_node){
  
  if(is_expected(parser, TOKEN_VAR) || is_expected(parser, TOKEN_VAL))
    return parser_var_declaration (parser, out_node);
    
  else if(is_expected(parser, TOKEN_IDENTIFIER))
    return parser_assignment_or_funcall_assignment (parser, out_node);

  else if(is_expected(parser, TOKEN_IF))
    return parser_if_statement (parser, out_node);

  else if (is_expected(parser, TOKEN_WHILE)) 
    return parser_while_statement(parser, out_node);

  else if(is_expected(parser, TOKEN_SEMICOLON)){
    get_next_token(parser);
    return ERR_OK;
  }
  
  return ERR_SYNTAX;
}

