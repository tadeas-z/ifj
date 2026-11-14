// Implementation of the IFJ26 imperative language compiler
// parser.c
// Author(s): Dušan Popović (xpopovd00)

#include "parser.h"
#include <string.h>

typedef struct {
  lexer_t *lexer;
  token_t input_token;
  symtable_t *symtable;
  size_t current_indent;
}parser_t;

static int get_next_token(parser_t *parser){
  lexer_status_t status = lexer_next(parser->lexer, &parser->input_token);        //lexer_status_t is same like error_code_t
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
static int parser_return(parser_t *parser, ast_node_t **out_node);
static int statement_end(parser_t *parser);

/*
---------------------------------------------------------------------------------
|  Chacking syntax of VAR/VAL declaration                                           |
|  (syntax:  var/val temp: Int = <value>) ;                                    |
---------------------------------------------------------------------------------
*/

static int parser_var_declaration (parser_t *parser, ast_node_t **out_node){
  bool is_const = is_expected(parser, TOKEN_VAL);
  get_next_token (parser);

  if(!(is_expected(parser, TOKEN_IDENTIFIER)))
      return ERR_SYNTAX;

  char *var_name = parser->input_token.lexeme;
  get_next_token (parser);

  type_t *var_type = T_ANY;
  if (is_expected(parser, TOKEN_COLON)){
    get_next_token (parser);
    if (is_expected(parser, TOKEN_TYPE_INT))
      var_type = T_INT;
    else if (is_expected(parser,TOKEN_TYPE_DOUBLE))
      var_type = T_DOUBLE;
    else if (is_expected(parser, TOKEN_TYPE_STRING))
      var_type = T_STRING;
    else if (is_expected(parser, TOKEN_TYPE_UNIT))
      var_type = T_UNIT;
    else
      return ERR_SYNTAX;
    get_next_token(parser);
  }

  symbol_t *sym_var = NULL;
  st_result_t st_result = symtable_insert_var(parser->symtable, var_name, var_type, is_const, &sym_var);
  if(st_result == ST_NOMEM)
    return ERR_INTERNAL;

  if(!(is_expected(parser, TOKEN_ASSIGN)))  
    return ERR_SYNTAX;

  get_next_token(parser);

  ast_node_t *new_expression = NULL;
  int expr_result = parser_expression (parser->lexer, &new_expression);
  if (expr_result != ERR_OK)
    return expr_result;

  get_next_token (parser);
  
  return expect_statement_end(parser);
}

/*
---------------------------------------------------------------------------------
|  Assigning a value to a variable                                              |
|  (syntax:   temp = <expr>;  || fun(<arg>);                                 |
---------------------------------------------------------------------------------
*/

static int parser_assignment_or_funcall (parser_t *parser, ast_node_t **out_node){
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
    
    return expect_statement_end(parser);
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

          while (is_expected(parser, TOKEN_NEWLINE)) 
            get_next_token(parser);

          if (is_expected(parser, TOKEN_RPAREN))
            break;
          
          continue;
        }
        break;
      }
    }
    
    if (!is_expected(parser, TOKEN_RPAREN)) 
        return ERR_SYNTAX;

    get_next_token (parser);
    
    return expect_statement_end(parser);
  }
  return ERR_SYNTAX;
}

/*
---------------------------------------------------------------------------------
|  Check syntax of IF conditional                                                |
|  (syntax:   if <expr> then                                                     |
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
    
    //skip new lines before else
    while (is_expected(parser, TOKEN_NEWLINE)) {
        get_next_token(parser);
    }
    
    //else_condition
    if (!(is_expected(parser, TOKEN_ELSE))){
      ast_free(cond_expr);  
      return ERR_SYNTAX;
    }
    
    get_next_token(parser);
    //else_block
    ast_node_t *else_block = NULL;
    block_syntax = parser_block(parser, &else_block);
    ast_free(cond_expr);
    return block_syntax;
  }   
  return ERR_SYNTAX;
}

/*
---------------------------------------------------------------------------------
|  Check syntax of WHILE loop                                                    |
|  (syntax:  while <expr> do                                                |
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
    ast_free(cond_expr);
    return block_syntax;
  }
  return ERR_SYNTAX;
}

/*
---------------------------------------------------------------------------------
|  Function Declaration Syntax Analysis                                         |
|  Syntax: def id (param_list) [: type] = <expr_or_block>                       |
---------------------------------------------------------------------------------
*/
static int parser_func_declaration (parser_t *parser, ast_node_t **out_node, bool is_main){
  if(is_expected(parser, TOKEN_DEF)){
    get_next_toke(parser);

    if(!(is_expected(parser, TOKEN_IDENTIFIER)))
      return ERR_SYNTAX;

    char *func_name = parser->input_token.lexeme;

    symbol_t *func_sym = NULL;
    if(symtable_insert_func(parser->symtable, func_name, &func_sym) != ST_OK)
      return ERR_INTERNAL;

    func_sym->is_main = is_main;
    get_next_token(parser);

    if(!(is_expected(parser, TOKEN_LPAREN)))
      return ERR_SYNTAX;

    get_next_token(parser);
    
    while(!(is_expected(parser, TOKEN_RPAREN))){
      if(!(is_expected(parser, TOKEN_IDENTIFIER)))
        return ERR_SYNTAX;

      char *param_name = parser->input_token.lexeme;
      
      get_next_token(parser);

      if(!(is_expected(parser, TOKEN_COLON)))
        return ERR_SYNTAX;

      type_t *param_type = T_ANY;
      if(is_expected(parser, TOKEN_TYPE_INT))
        param_type = T_INT;
      else if(is_expected(parser, TOKEN_TYPE_DOUBLE))
        param_type = T_DOUBLE;
      else if(is_expeceted(parser, TOKEN_TYPE_STRING))
        param_type = T_STRING;
      else if(is_expected(parser, TOKEN_TYPE_UNIT))
        param_type = T_UNIT;
      else
        return ERR_SYNTAX;

      if(symtable_func_add_param(func_sym, param_name, param_type) != ST_OK)
        return ERR_INTERNAL;

      get_next_token(parser);

      if(is_expeceted(parser, TOKEN_COMMA))
        get_next_token(parser);
        while (is_expected(parser, TOKEN_NEWLINE)) 
          get_next_token(parser);
        
        if (is_expected(parser, TOKEN_RPAREN))
          break;
        continue;
      }
      break;
    }
    
    if(!(is_expected(parser, TOKEN_RPAREN)))
      return ERR_SYNTAX;

    get_next_token(parser);

    if(is_expected(parser, TOKEN_COLON)){
      get_next_token(parser);
      if(is_expeceted(parser, TOKEN_TYPE_INT))
        func_sym->type = T_INT;
      else if(is_expected(parser, TOKEN_TYPE_DOUBLE))
        func_sym->type = T_DOUBLE;
      else if(is_expected(parser, TOKEN_TYPE_STRING))
        func_sym->type = T_STRING;
      else if(is_expected(parser, TOKEN_TYPE_UNIT))
        func_sym->type = T_UNIT;
      else
        return ERR_SYNTAX;
      get_next_token(parser);
    }

    if(!(is_expected(parser, TOKEN_ASSIGN)))
      return ERR_SYNTAX;

    get_next_token(parser);

    int body_result;
    // multi-line block
    if(is_expected(parser, TOKEN_NEWLINE)){
      ast_node_t *body_block = NULL;
      body_result = parser_block(parser, &body_block);
    }
      
    // single-line block  
    else{
      ast_node_t *body_expr = NULL;
      body_result = parser_expression(parser->lexer, &body_expr);
      if(body_result == ERR_OK){
        get_next_token(parser);
        body_result = statement_end(parser);
      }
    }
    return body_result;
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
        if (parser->input_token.indentation < block_indent) 
          break; 
  
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
    return parser_assignment_or_funcall (parser, out_node);

  else if(is_expected(parser, TOKEN_IF))
    return parser_if_statement (parser, out_node);

  else if (is_expected(parser, TOKEN_WHILE)) 
    return parser_while_statement(parser, out_node);

  else if (is_expected(parser, TOKEN_RETURN))
    return parser_return(parser, out_node);
    
  else if(is_expected(parser, TOKEN_SEMICOLON)){
    get_next_token(parser);
    return ERR_OK;
  }

  else if(is_expected(parser, TOKEN_RETURN))
    return parser_return(parser, out_node);
  
  return ERR_SYNTAX;
}

/*
---------------------------------------------------------------------------------
|  Return                                                                        |
|  Syntax: return <expr> ;                                                       |
---------------------------------------------------------------------------------
*/

static int parser_return(parser_t *parser, ast_node_t **out_node){
  get_next_token(parser);

   if (is_expected(parser, TOKEN_NEWLINE) || 
       is_expected(parser, TOKEN_SEMICOLON) ||
       is_expected(parser, TOKEN_EOF)) {
    return statement_end(parser);
  }
  
  ast_node_t *return_expr = NULL;
  int expr_result = parser_expression(parser->lexer, &return_expr);
  if(expr_result != ERR_OK)
    return expr_result;

  get_next_token(parser);

  return expect_statement_end(parser);
}

static int statement_end(parser_t *parser) {
    if (is_expected(parser, TOKEN_SEMICOLON)) {
      return get_next_token(parser);
    }
    if (is_expected(parser, TOKEN_NEWLINE) || is_expected(parser, TOKEN_EOF)) {
      return ERR_OK;
    }
    return ERR_SYNTAX;
}

int parser_main(lexer_t *lexer, ast_node_t **out_ast){
  parser_t parser;
  parser.lexer = lexer;
  parser.current_indent = 0;

  parser.symtable = symtable_new ();
  if(parser.symtable == NULL)
    return ERR_INTERNAL;

  if(get_next_token(&parser) != ERR_OK){
    symtable_free(parser.symtable);
    return ERR_LEXICAL;
  }

  // prolog: import ifj26.*
  if(!(is_expected(&parser, TOKEN_IMPORT))){
    symtable_free(parser.symtable);
    return ERR_SYNTAX;
  }

  get_next_token(&parser);
  
  if(!(is_expected(&parser, TOKEN_IDENTIFIER)) || strcmp(parser.input_token.lexeme, "ifj26") != 0){
    symtable_free(parser.symtable);
    return ERR_SYNTAX;
  }

  get_next_token(&parser);
  
  if (!is_expected(&parser, TOKEN_DOT)){
    symtable_free(parser.symtable);
    return ERR_SYNTAX;
  }
  
  get_next_token(&parser);

  if (!is_expected(&parser, TOKEN_STAR)){
    symtable_free(parser.symtable);
    return ERR_SYNTAX;
  }
  
  get_next_token(&parser);

  if (!is_expected(&parser, TOKEN_NEWLINE) && !is_expected(&parser, TOKEN_EOF)){
    symtable_free(parser.symtable);
    return ERR_SYNTAX;
  }
  
  while (!is_expected(&parser, TOKEN_EOF)){
    if (is_expected(&parser, TOKEN_NEWLINE)){
      get_next_token(&parser);
      continue;
    }

    bool is_main = false;
    if (is_expected(&parser, TOKEN_MAIN)) {
      is_main = true;
      get_next_token(&parser);
      while (is_expected(&parser, TOKEN_NEWLINE)) 
        get_next_token(&parser);
    }
  
    if (is_expected(&parser, TOKEN_DEF)) {
      int err = parser_func_declaration(&parser, out_ast, is_main);
      if (err != ERR_OK) {
          symtable_free(parser.symtable);
          return err;
      }
    } 
      
    else {
         symtable_free(parser.symtable);
         return ERR_SYNTAX;
    }
  }
  
  symtable_free(parser.symtable);
  return ERR_OK;
    
}
