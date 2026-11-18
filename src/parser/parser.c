// Implementation of the IFJ26 imperative language compiler
// parser.c
// Author(s): Dušan Popović (xpopovd00)

#include "parser.h"
#include <stdlib.h>
#include <string.h>

#include "comp_error.h" //compiler exit
#include "parser_expression.h"
#include "parser_stack.h"
#include "types.h"

typedef struct {
    lexer_t* lexer;
    token_t input_token;
    token_t peek_token;
    bool has_peek;
    size_t current_indent;
} parser_t;

// AI-generated using: Gemini
//                model: Gemini 4 HYDROGEN
// Prompt (summary): "Generate comments for the helper functions used in
//                    syntactic analysis (token retrieval, and lexeme copying)."
// Scope: get_next_token, copy_lexeme

static int get_next_token(parser_t* parser)
{
    token_dispose(&parser->input_token);
    if (parser->has_peek) {
        parser->input_token = parser->peek_token;
        memset(&parser->peek_token, 0, sizeof(token_t));
        parser->has_peek = false;
        return ERR_OK;
    }
    int status = lexer_next(parser->lexer, &parser->input_token);
    if (status != ERR_OK)
        return status;
    return ERR_OK;
}

static bool is_expected(parser_t* parser, token_type_t token)
{
    if (parser->input_token.type == token)
        return true;
    else
        return false;
}

/*
---------------------------------------------------------------------------------
|  Chacking syntax of VAR/VAL declaration                                        |
---------------------------------------------------------------------------------
*/

static char* copy_lexeme(char* src)
{
    if (src == NULL)
        return NULL;

    size_t len = strlen(src);
    char* dst = (char*)malloc(len + 1);
    if (dst != NULL)
        memcpy(dst, src, len + 1);
    return dst;
}

static int expect_statement_end(parser_t* parser)
{
    if (is_expected(parser, TOKEN_SEMICOLON))
        return get_next_token(parser);
    if (is_expected(parser, TOKEN_NEWLINE) || is_expected(parser, TOKEN_EOF))
        return ERR_OK;
    return ERR_SYNTAX;
}

static int parser_expr_shared(parser_t* parser, ast_node_t** out_expr)
{
    return parser_expression(parser->lexer, &parser->input_token, out_expr);
}

static int parser_block(parser_t* parser, ast_node_t** out_node);
static int parser_statement(parser_t* parser, ast_node_t** out_node);
static int parser_return(parser_t* parser, ast_node_t** out_node);
static int expect_statement_end(parser_t* parser);

/*
---------------------------------------------------------------------------------
|  Chacking syntax of VAR/VAL declaration                                        |
|  (syntax:  var/val temp [: Data_Type] = <value>) ;                             |
---------------------------------------------------------------------------------
*/

static int parser_var_declaration(parser_t* parser, ast_node_t** out_node)
{
    bool is_const = is_expected(parser, TOKEN_VAL);
    size_t line = parser->input_token.line;
    size_t col = parser->input_token.column;
    int err = get_next_token(parser);
    if (err != ERR_OK)
        return err;

    // Variable name identifier
    if (!is_expected(parser, TOKEN_IDENTIFIER))
        return ERR_SYNTAX;

    char* var_name = copy_lexeme(parser->input_token.lexeme);
    if (var_name == NULL)
        return ERR_INTERNAL;

    err = get_next_token(parser);
    if (err != ERR_OK) {
        free(var_name);
        return err;
    }

    // Parse optional explicit type annotation
    type_t* var_type = NULL;
    if (is_expected(parser, TOKEN_COLON)) {
        err = get_next_token(parser);
        if (is_expected(parser, TOKEN_TYPE_INT))
            var_type = TYPE_INT;
        else if (is_expected(parser, TOKEN_TYPE_DOUBLE))
            var_type = TYPE_DOUBLE;
        else if (is_expected(parser, TOKEN_TYPE_STRING))
            var_type = TYPE_STRING;
        else if (is_expected(parser, TOKEN_TYPE_UNIT))
            var_type = TYPE_UNIT;
        else {
            free(var_name);
            return ERR_SYNTAX;
        }
        err = get_next_token(parser);
        if (err != ERR_OK) {
            free(var_name);
            return err;
        }
    }

    if (!is_expected(parser, TOKEN_ASSIGN)) {
        free(var_name);
        return ERR_SYNTAX;
    }

    // Advance past the type keyword
    err = get_next_token(parser);
    if (err != ERR_OK) {
        free(var_name);
        return err;
    }

    // Parse initializer expression
    ast_node_t* init_expr = NULL;
    err = parser_expr_shared(parser, &init_expr);
    if (err != ERR_OK) {
        free(var_name);
        ast_free(init_expr);
        return err;
    }
    // Build variable definition AST node
    ast_node_t* var_node = ast_new_var_def(var_name, is_const, var_type, init_expr, line, col);
    free(var_name);
    if (var_node == NULL) {
        ast_free(init_expr);
        return ERR_INTERNAL;
    }

    *out_node = var_node;
    return ERR_OK;
}

/*
---------------------------------------------------------------------------------
|  Assigning a value to a variable                                              |
|  (syntax:   temp = <expr>;  || fun(<arg>);                                 |
---------------------------------------------------------------------------------
*/

static int parser_assignment(parser_t* parser, ast_node_t** out_node)
{
    size_t line = parser->input_token.line;
    size_t col = parser->input_token.column;

    // Store target variable name and consume identifier token
    char* var_name = copy_lexeme(parser->input_token.lexeme);
    if (var_name == NULL)
        return ERR_INTERNAL;

    int err = get_next_token(parser);
    if (err != ERR_OK) {
        free(var_name);
        return err;
    }

    // temp = <expr> ;
    if (!is_expected(parser, TOKEN_ASSIGN)) {
        free(var_name);
        return ERR_SYNTAX;
    }

    err = get_next_token(parser);
    if (err != ERR_OK) {
        free(var_name);
        return err;
    }

    // Parse right-hand side expression
    ast_node_t* var_expr = NULL;
    err = parser_expr_shared(parser, &var_expr);
    if (err != ERR_OK) {
        free(var_name);
        return err;
    }

    // Verify statement termination
    err = expect_statement_end(parser);
    if (err != ERR_OK) {
        free(var_name);
        ast_free(var_expr);
        return err;
    }

    // Create assignment AST node
    ast_node_t* assign_node = ast_new_assign(var_name, var_expr, line, col);
    free(var_name);
    if (assign_node == NULL) {
        ast_free(var_expr);
        return ERR_INTERNAL;
    }

    *out_node = assign_node;
    return ERR_OK;
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

static int parser_if_statement(parser_t* parser, ast_node_t** out_node)
{
    size_t line = parser->input_token.line;
    size_t col = parser->input_token.column;
    // Record 'if' indentation to match a corresponding 'else' at the same level
    size_t if_ident = parser->input_token.indentation;

    int err = get_next_token(parser);
    if (err != ERR_OK)
        return err;

    // condition expression
    ast_node_t* cond_expr = NULL;
    err = parser_expr_shared(parser, &cond_expr);
    if (err != ERR_OK)
        return err;

    if (!is_expected(parser, TOKEN_THEN)) {
        ast_free(cond_expr);
        return ERR_SYNTAX;
    }

    err = get_next_token(parser);
    if (err != ERR_OK) {
        ast_free(cond_expr);
        return err;
    }

    // Parse indented 'if <expr> then' body block
    ast_node_t* if_block = NULL;
    err = parser_block(parser, &if_block);
    if (err != ERR_OK) {
        ast_free(cond_expr);
        return err;
    }

    // skip new lines before else
    while (is_expected(parser, TOKEN_NEWLINE)) {
        err = get_next_token(parser);
        if (err != ERR_OK) {
            ast_free(cond_expr);
            ast_free(if_block);
            return err;
        }
    }

    // else_block
    ast_node_t* else_block = NULL;
    if (is_expected(parser, TOKEN_ELSE) && parser->input_token.starts_line && parser->input_token.indentation == if_ident) {
        size_t else_line = parser->input_token.line;
        size_t else_col = parser->input_token.column;

        err = get_next_token(parser);
        if (err != ERR_OK) {
            ast_free(cond_expr);
            ast_free(if_block);
            return err;
        }

        // else if
        if (is_expected(parser, TOKEN_IF)) {
            ast_node_t* else_if = NULL;
            err = parser_if_statement(parser, &else_if);
            if (err != ERR_OK) {
                ast_free(cond_expr);
                ast_free(if_block);
                return err;
            }

            else_block = ast_new_block(else_line, else_col);
            if (else_block == NULL || !dyn_arr_append(&else_block->block.statements, else_if)) {
                ast_free(else_if);
                ast_free(else_block);
                ast_free(cond_expr);
                ast_free(if_block);
                return ERR_INTERNAL;
            }
        } else {
            err = parser_block(parser, &else_block);
            if (err != ERR_OK) {
                ast_free(cond_expr);
                ast_free(if_block);
                return err;
            }
        }
    }

    // Construct final conditional AST node
    ast_node_t* if_node = ast_new_if(cond_expr, if_block, else_block, line, col);
    if (if_node == NULL) {
        ast_free(cond_expr);
        ast_free(if_block);
        ast_free(else_block);
        return ERR_INTERNAL;
    }

    *out_node = if_node;
    return ERR_OK;
}

/*
---------------------------------------------------------------------------------
|  Check syntax of WHILE loop                                                    |
|  (syntax:  while <expr> do                                                |
|                      <stmts>                                                   |
---------------------------------------------------------------------------------
*/

static int parser_while_statement(parser_t* parser, ast_node_t** out_node)
{
    size_t line = parser->input_token.line;
    size_t col = parser->input_token.column;

    // Skip 'while' keyword
    int err = get_next_token(parser);
    if (err != ERR_OK)
        return err;

    ast_node_t* cond_expr = NULL;
    err = parser_expr_shared(parser, &cond_expr);
    if (err != ERR_OK)
        return err;

    if (!(is_expected(parser, TOKEN_DO))) {
        ast_free(cond_expr);
        return ERR_SYNTAX;
    }

    err = get_next_token(parser);
    if (err != ERR_OK) {
        ast_free(cond_expr);
        return err;
    }
    // while_block, parse indented loop body block
    ast_node_t* while_block = NULL;
    err = parser_block(parser, &while_block);
    if (err != ERR_OK) {
        ast_free(cond_expr);
        return err;
    }

    // Construct while-loop AST node
    ast_node_t* while_node = ast_new_while(cond_expr, while_block, line, col);
    if (while_node == NULL) {
        ast_free(cond_expr);
        ast_free(while_block);
        return ERR_INTERNAL;
    }

    *out_node = while_node;
    return ERR_OK;
}

/*
---------------------------------------------------------------------------------
|  Function Declaration Syntax Analysis                                         |
|  Syntax: def id (param_list) [: type] = <expr_or_block>                       |
---------------------------------------------------------------------------------
*/

static int parser_func_declaration(parser_t* parser, ast_node_t** out_node, bool is_main)
{
    size_t line = parser->input_token.line;
    size_t col = parser->input_token.column;

    // Check 'def' keyword
    if (!(is_expected(parser, TOKEN_DEF)))
        return ERR_SYNTAX;

    int err = get_next_token(parser);
    if (err != ERR_OK)
        return err;

    // Function name is required
    if (!is_expected(parser, TOKEN_IDENTIFIER))
        return ERR_SYNTAX;

    ast_node_t* func_node = ast_new_func_def(parser->input_token.lexeme, is_main, line, col);
    if (func_node == NULL)
        return ERR_INTERNAL;

    err = get_next_token(parser);
    if (err != ERR_OK) {
        ast_free(func_node);
        return err;
    }

    if (!is_expected(parser, TOKEN_LPAREN)) {
        ast_free(func_node);
        return ERR_SYNTAX;
    }

    err = get_next_token(parser);
    if (err != ERR_OK) {
        ast_free(func_node);
        return err;
    }

    if (!is_expected(parser, TOKEN_RPAREN)) {
        while (1) {
            if (!is_expected(parser, TOKEN_IDENTIFIER)) {
                ast_free(func_node);
                return ERR_SYNTAX;
            }

            char* param_name = copy_lexeme(parser->input_token.lexeme);
            if (param_name == NULL) {
                ast_free(func_node);
                return ERR_INTERNAL;
            }

            err = get_next_token(parser);
            if (err != ERR_OK) {
                free(param_name);
                ast_free(func_node);
                return err;
            }

            if (!is_expected(parser, TOKEN_COLON)) {
                free(param_name);
                ast_free(func_node);
                return ERR_SYNTAX;
            }

            err = get_next_token(parser);
            if (err != ERR_OK) {
                free(param_name);
                ast_free(func_node);
                return err;
            }

            type_t* param_type = NULL;
            if (is_expected(parser, TOKEN_TYPE_INT))
                param_type = TYPE_INT;
            else if (is_expected(parser, TOKEN_TYPE_DOUBLE))
                param_type = TYPE_DOUBLE;
            else if (is_expected(parser, TOKEN_TYPE_STRING))
                param_type = TYPE_STRING;
            else if (is_expected(parser, TOKEN_TYPE_UNIT))
                param_type = TYPE_UNIT;
            else {
                free(param_name);
                ast_free(func_node);
                return ERR_SYNTAX;
            }

            // Register parameter in the function AST node
            bool added = ast_func_def_add_param(func_node, param_name, param_type);
            free(param_name);
            if (!added) {
                ast_free(func_node);
                return ERR_INTERNAL;
            }

            err = get_next_token(parser);
            if (err != ERR_OK) {
                ast_free(func_node);
                return err;
            }

            // Continue parsing if a comma separates additional parameters
            if (is_expected(parser, TOKEN_COMMA)) {
                err = get_next_token(parser);
                if (err != ERR_OK) {
                    ast_free(func_node);
                    return err;
                }

                bool had_newline = false;
                // Allow line breaks after a comma in multi-line parameter lists
                while (is_expected(parser, TOKEN_NEWLINE)) {
                    had_newline = true;
                    err = get_next_token(parser);
                    if (err != ERR_OK) {
                        ast_free(func_node);
                        return err;
                    }
                }
                if (is_expected(parser, TOKEN_RPAREN)) {
                    if (!had_newline) {
                        ast_free(func_node);
                        return ERR_SYNTAX;
                    }
                    break;
                }

                continue;
            }
            break;
        }
    }

    // Expect closing ')' of parameter list
    if (!is_expected(parser, TOKEN_RPAREN)) {
        ast_free(func_node);
        return ERR_SYNTAX;
    }

    err = get_next_token(parser);
    if (err != ERR_OK) {
        ast_free(func_node);
        return err;
    }

    if (is_expected(parser, TOKEN_COLON)) {
        err = get_next_token(parser);
        if (err != ERR_OK) {
            ast_free(func_node);
            return err;
        }

        if (is_expected(parser, TOKEN_TYPE_INT))
            func_node->func_def.return_type = TYPE_INT;
        else if (is_expected(parser, TOKEN_TYPE_DOUBLE))
            func_node->func_def.return_type = TYPE_DOUBLE;
        else if (is_expected(parser, TOKEN_TYPE_STRING))
            func_node->func_def.return_type = TYPE_STRING;
        else if (is_expected(parser, TOKEN_TYPE_UNIT))
            func_node->func_def.return_type = TYPE_UNIT;
        else {
            ast_free(func_node);
            return ERR_SYNTAX;
        }

        err = get_next_token(parser);
        if (err != ERR_OK) {
            ast_free(func_node);
            return err;
        }
    }

    // Expect '=' separating function from its body
    if (!is_expected(parser, TOKEN_ASSIGN)) {
        ast_free(func_node);
        return ERR_SYNTAX;
    }

    err = get_next_token(parser);
    if (err != ERR_OK) {
        ast_free(func_node);
        return err;
    }

    // multi-line block
    if (is_expected(parser, TOKEN_NEWLINE)) {
        ast_node_t* body_block = NULL;
        err = parser_block(parser, &body_block);
        if (err != ERR_OK) {
            ast_free(func_node);
            return err;
        }
        func_node->func_def.body = body_block;
    }

    // single-line block
    else {
        size_t expr_line = parser->input_token.line;
        size_t expr_col = parser->input_token.column;

        ast_node_t* body_expr = NULL;
        err = parser_expr_shared(parser, &body_expr);
        if (err != ERR_OK) {
            ast_free(func_node);
            return err;
        }

        err = expect_statement_end(parser);
        if (err != ERR_OK) {
            ast_free(body_expr);
            ast_free(func_node);
            return err;
        }
        ast_node_t* expr_stmt = ast_new_expr_stmt(body_expr, expr_line, expr_col);
        ast_node_t* body_block = ast_new_block(expr_line, expr_col);
        if (expr_stmt == NULL || body_block == NULL || !dyn_arr_append(&body_block->block.statements, expr_stmt)) {
            ast_free(body_block);
            ast_free(expr_stmt);
            ast_free(func_node);
            return ERR_INTERNAL;
        }
        func_node->func_def.body = body_block;
    }

    *out_node = func_node;
    return ERR_OK;
}

/*
---------------------------------------------------------------------------------
|  Helper function for parsing indented block syntax                            |
---------------------------------------------------------------------------------
*/

static int parser_block(parser_t* parser, ast_node_t** out_node)
{
    size_t line = parser->input_token.line;
    size_t col = parser->input_token.column;

    // Skip leading blank lines before the first statement of the block
    while (is_expected(parser, TOKEN_NEWLINE)) {
        int err = get_next_token(parser);
        if (err != ERR_OK)
            return err;
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

    ast_node_t* block = ast_new_block(line, col);
    if (block == NULL)
        return ERR_INTERNAL;

    size_t old_indent = parser->current_indent;
    parser->current_indent = block_indent;

    while (!is_expected(parser, TOKEN_EOF)) {
        if (is_expected(parser, TOKEN_NEWLINE)) {
            int err = get_next_token(parser);
            if (err != ERR_OK) {
                ast_free(block);
                return err;
            }
            continue;
        }

        if (parser->input_token.starts_line) {
            // If indent is smaller then block, that means end of block
            if (parser->input_token.indentation < block_indent)
                break;

            if (parser->input_token.indentation != block_indent) {
                ast_free(block);
                return ERR_SYNTAX;
            }
        }

        ast_node_t* stmt = NULL;
        int err = parser_statement(parser, &stmt);
        if (err != ERR_OK) {
            ast_free(block);
            return err;
        }

        if (stmt != NULL) {
            if (!dyn_arr_append(&block->block.statements, stmt)) {
                ast_free(stmt);
                ast_free(block);
                return ERR_INTERNAL;
            }
        }
    }

    // block must not be empity
    if (block->block.statements.count == 0) {
        ast_free(block);
        return ERR_SYNTAX;
    }

    parser->current_indent = old_indent;
    *out_node = block;
    return ERR_OK;
}

/*
---------------------------------------------------------------------------------
|  Statement (Router)                                                           |
|  Routes execution to the appropriate rule based on the starting token         |
---------------------------------------------------------------------------------
*/

static int parser_expression_statement(parser_t* parser, ast_node_t** out_node)
{
    size_t line = parser->input_token.line;
    size_t col = parser->input_token.column;

    // Return the peeked token to the expression parser so it isn't skipped
    if (parser->has_peek) {
        parser_expression_push_back(&parser->peek_token);
        parser->has_peek = false;
    }

    ast_node_t* expr = NULL;
    int err = parser_expr_shared(parser, &expr);
    if (err != ERR_OK)
        return err;

    err = expect_statement_end(parser);
    if (err != ERR_OK) {
        ast_free(expr);
        return err;
    }

    ast_node_t* stmt = ast_new_expr_stmt(expr, line, col);
    if (stmt == NULL) {
        ast_free(expr);
        return ERR_INTERNAL;
    }

    *out_node = stmt;
    return ERR_OK;
}

static int parser_statement(parser_t* parser, ast_node_t** out_node)
{

    // Var or const declaration
    if (is_expected(parser, TOKEN_VAR) || is_expected(parser, TOKEN_VAL))
        return parser_var_declaration(parser, out_node);

    // Conditional statement
    if (is_expected(parser, TOKEN_IF))
        return parser_if_statement(parser, out_node);

    // While loop statement
    if (is_expected(parser, TOKEN_WHILE))
        return parser_while_statement(parser, out_node);

    // Return statement
    if (is_expected(parser, TOKEN_RETURN))
        return parser_return(parser, out_node);

    // Empty statement
    if (is_expected(parser, TOKEN_SEMICOLON)) {
        *out_node = NULL;
        return get_next_token(parser);
    }

    // Disambiguate assignment vs expression statement using 1-token peek
    if (is_expected(parser, TOKEN_IDENTIFIER)) {
        if (!parser->has_peek) {
            int err = lexer_next(parser->lexer, &parser->peek_token);
            if (err != ERR_OK)
                return err;
            parser->has_peek = true;
        }

        if (parser->peek_token.type == TOKEN_ASSIGN)
            return parser_assignment(parser, out_node);

        return parser_expression_statement(parser, out_node);
    }

    // Expression starting with a literal, unary minus, or parenthesis
    if (is_expected(parser, TOKEN_LPAREN) || is_expected(parser, TOKEN_INTEGER) || is_expected(parser, TOKEN_DOUBLE) || is_expected(parser, TOKEN_STRING) || is_expected(parser, TOKEN_MINUS))
        return parser_expression_statement(parser, out_node);

    return ERR_SYNTAX;
}

/*
---------------------------------------------------------------------------------
|  Return                                                                        |
|  Syntax: return <expr> ;                                                       |
---------------------------------------------------------------------------------
*/

static int parser_return(parser_t* parser, ast_node_t** out_node)
{
    size_t line = parser->input_token.line;
    size_t col = parser->input_token.column;

    int err = get_next_token(parser);
    if (err != ERR_OK)
        return err;

    // Return without a value expression
    if (is_expected(parser, TOKEN_NEWLINE) || is_expected(parser, TOKEN_SEMICOLON) || is_expected(parser, TOKEN_EOF)) {
        err = expect_statement_end(parser);
        if (err != ERR_OK)
            return err;

        ast_node_t* ret_node = ast_new_return(NULL, line, col);
        if (ret_node == NULL)
            return ERR_INTERNAL;
        *out_node = ret_node;
        return ERR_OK;
    }

    // Parse return value expression
    ast_node_t* return_expr = NULL;
    err = parser_expr_shared(parser, &return_expr);
    if (err != ERR_OK) {
        ast_free(return_expr);
        return err;
    }

    // Construct return AST node
    ast_node_t* ret_node = ast_new_return(return_expr, line, col);
    if (ret_node == NULL) {
        ast_free(return_expr);
        return ERR_INTERNAL;
    }

    *out_node = ret_node;
    return ERR_OK;
}

/*
---------------------------------------------------------------------------------
|  Main Program Entry Point                                                     |
---------------------------------------------------------------------------------
*/

int parser_main(lexer_t* lexer, ast_node_t** out_ast)
{
    if (lexer == NULL || out_ast == NULL)
        return ERR_INTERNAL;

    *out_ast = NULL;

    // Initialize parser state and root program AST node
    parser_t parser;
    memset(&parser, 0, sizeof(parser_t));
    parser.lexer = lexer;

    ast_node_t* program = ast_new_program();
    if (program == NULL)
        return ERR_INTERNAL;

    int err = get_next_token(&parser);
    if (err != ERR_OK) {
        ast_free(program);
        return err;
    }

    // Skip newlines before the mandatory import prolog
    while (is_expected(&parser, TOKEN_NEWLINE)) {
        err = get_next_token(&parser);
        if (err != ERR_OK) {
            ast_free(program);
            token_dispose(&parser.input_token);
            return err;
        }
    }

    // prolog: import ifj26.*
    if (!is_expected(&parser, TOKEN_IMPORT)) {
        ast_free(program);
        token_dispose(&parser.input_token);
        return ERR_SYNTAX;
    }

    err = get_next_token(&parser);
    if (err != ERR_OK) {
        ast_free(program);
        return err;
    }

    if (!(is_expected(&parser, TOKEN_IDENTIFIER)) || strcmp(parser.input_token.lexeme, "ifj26") != 0 || parser.input_token.lexeme == NULL) {
        ast_free(program);
        token_dispose(&parser.input_token);
        return ERR_SYNTAX;
    }

    err = get_next_token(&parser);
    if (err != ERR_OK) {
        ast_free(program);
        return err;
    }

    if (!is_expected(&parser, TOKEN_DOT)) {
        ast_free(program);
        token_dispose(&parser.input_token);
        return ERR_SYNTAX;
    }

    err = get_next_token(&parser);
    if (err != ERR_OK) {
        ast_free(program);
        return err;
    }

    if (!is_expected(&parser, TOKEN_STAR)) {
        ast_free(program);
        token_dispose(&parser.input_token);
        return ERR_SYNTAX;
    }

    err = get_next_token(&parser);
    if (err != ERR_OK) {
        ast_free(program);
        return err;
    }

    // Prolog must be followed by a newline or EOF
    if (!is_expected(&parser, TOKEN_NEWLINE) && !is_expected(&parser, TOKEN_EOF)) {
        ast_free(program);
        token_dispose(&parser.input_token);
        return ERR_SYNTAX;
    }

    while (!is_expected(&parser, TOKEN_EOF)) {
        if (is_expected(&parser, TOKEN_NEWLINE)) {
            err = get_next_token(&parser);
            if (err != ERR_OK) {
                ast_free(program);
                token_dispose(&parser.input_token);
                return err;
            }
            continue;
        }

        // Check for optional 'main' modifier preceding 'def'
        bool is_main = false;
        if (is_expected(&parser, TOKEN_MAIN)) {
            is_main = true;
            err = get_next_token(&parser);
            if (err != ERR_OK) {
                ast_free(program);
                token_dispose(&parser.input_token);
                return err;
            }
            while (is_expected(&parser, TOKEN_NEWLINE)) {
                err = get_next_token(&parser);
                if (err != ERR_OK) {
                    ast_free(program);
                    token_dispose(&parser.input_token);
                    return err;
                }
            }
        }

        // Parse function declaration and append it to the program AST
        if (is_expected(&parser, TOKEN_DEF)) {
            ast_node_t* func_node = NULL;
            err = parser_func_declaration(&parser, &func_node, is_main);
            if (err != ERR_OK) {
                ast_free(program);
                token_dispose(&parser.input_token);
                return err;
            }
            if (!dyn_arr_append(&program->program.func_list, func_node)) {
                ast_free(func_node);
                ast_free(program);
                token_dispose(&parser.input_token);
                return ERR_INTERNAL;
            }
        } else {
            ast_free(program);
            token_dispose(&parser.input_token);
            return ERR_SYNTAX;
        }
    }

    // Clean up input token and return AST
    token_dispose(&parser.input_token);
    *out_ast = program;
    return ERR_OK;
}
