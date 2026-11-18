// Implementation of the IFJ26 imperative language compiler
// parser_expression.c
// Author(s): Dusan Popovic (xpopovd00)
// run: gcc -std=c11 -Wall -Wextra -I. test_parser_expression.c mock_astree.c -o test_app

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_expression.h"
#include "lexer.h"
#include "astree.h"


// AI-generated using: Gemini
//               model: Gemini 3.1 pro
// Prompt (summary): "Implement test for parse_expression()."
// Scope: lexer_next, print_ast, run_test

// -------------------------------------------------------------
// 1. MOCK LEXER (fake lexer)
// -------------------------------------------------------------

static token_t *mock_tokens = NULL;
static int mock_token_index = 0;

lexer_status_t lexer_next(lexer_t *lexer, token_t *out_token) {
    (void)lexer; // Ne koristimo stvarni lexer
    
    if (mock_tokens != NULL) {
        *out_token = mock_tokens[mock_token_index++];
        if (out_token->type == TOKEN_EOF) {
            return EOF;
        }
        return LEXER_OK;
    }
    
    out_token->type = TOKEN_EOF;
    return EOF;
}

#include "parser_expression.c"

// -------------------------------------------------------------
// 2. HELP FUNCTION FOR PRINTF AST TREE
// -------------------------------------------------------------
void print_ast(ast_node_t *node, int depth) {
    if (node == NULL) return;
    
    for (int i = 0; i < depth; i++) printf("  ");
    
    switch (node->kind) {
        case AST_INT_LIT:
            printf("Int: %lld\n", node->int_lit.value);
            break;
        case AST_DOUBLE_LIT:
            printf("Double: %f\n", node->double_lit.value);
            break;
        case AST_STRING_LIT:
            printf("String: \"%s\"\n", node->string_lit.value);
            break;
        case AST_VAR_REF:
            printf("Var: %s\n", node->var_ref.name);
            break;
        case AST_BINARY:
            printf("Binary OP: ");
            switch(node->binary.op) {
                case AST_OP_ADD: printf("+\n"); break;
                case AST_OP_SUB: printf("-\n"); break;
                case AST_OP_MUL: printf("*\n"); break;
                case AST_OP_DIV: printf("/\n"); break;
                case AST_OP_EQ:  printf("==\n"); break;
                case AST_OP_NE:  printf("!=\n"); break;
                case AST_OP_LT:  printf("<\n"); break;
                case AST_OP_LE:  printf("<=\n"); break;
                case AST_OP_GT:  printf(">\n"); break;
                case AST_OP_GE: printf(">=\n"); break;
                default: printf("Other\n"); break;
            }
            print_ast(node->binary.left, depth + 1);
            print_ast(node->binary.right, depth + 1);
            break;
        default:
            printf("Unknown node (%d)\n", node->kind);
    }
}

// -------------------------------------------------------------
// 3. RUN_TEST function
// -------------------------------------------------------------
void run_test(const char* test_name, token_t tokens[], int expected_result) {
    printf("========== TEST: %s ==========\n", test_name);
    
    mock_tokens = tokens;
    mock_token_index = 0;
    
    ast_node_t *result_tree = NULL;
    
    // Poziv funkcije za parsiranje izraza
    int result_code = parse_expression(NULL, &result_tree);
    
    printf("Expected: %d | Got: %d\n", expected_result, result_code);
    if (result_code == expected_result) {
        printf("Test Passed\n");
        if (result_code == 0 && result_tree != NULL) {
            printf("AST Tree:\n");
            print_ast(result_tree, 1);
        }
    } else {
        printf("Test Failed\n");
    }
    
    // Očisti memoriju stabla
    if (result_tree != NULL) {
        ast_free(result_tree);
    }
    printf("\n");
}

int main() {

    token_t t1[] = {
        {.type = TOKEN_INTEGER, .lexeme = "42"},
        {.type = TOKEN_EOF, .lexeme = NULL}
    };
    run_test("Single Integer Literal", t1, 0);

    // TEST 2: a + b * 3 (provera prioriteta operatora)
    token_t t2[] = {
        {.type = TOKEN_IDENTIFIER, .lexeme = "a"},
        {.type = TOKEN_PLUS, .lexeme = NULL},
        {.type = TOKEN_IDENTIFIER, .lexeme = "b"},
        {.type = TOKEN_STAR, .lexeme = NULL},
        {.type = TOKEN_INTEGER, .lexeme = "3"},
        {.type = TOKEN_EOF, .lexeme = NULL}
    };
    run_test("Precedence: a + b * 3", t2, 0);

    // TEST 3: ( a + b ) * 3 
    token_t t3[] = {
        {.type = TOKEN_LPAREN, .lexeme = NULL},
        {.type = TOKEN_IDENTIFIER, .lexeme = "a"},
        {.type = TOKEN_PLUS, .lexeme = NULL},
        {.type = TOKEN_IDENTIFIER, .lexeme = "b"},
        {.type = TOKEN_RPAREN, .lexeme = NULL},
        {.type = TOKEN_STAR, .lexeme = NULL},
        {.type = TOKEN_DOUBLE, .lexeme = "3.0"},
        {.type = TOKEN_EOF, .lexeme = NULL}
    };
    run_test("Parentheses: (a + b) * 3.0", t3, 0);

    // TEST 4: Sintatic error: a + * b (expected code 2)
    token_t t4[] = {
        {.type = TOKEN_IDENTIFIER, .lexeme = "a"},
        {.type = TOKEN_PLUS, .lexeme = NULL},
        {.type = TOKEN_STAR, .lexeme = NULL},
        {.type = TOKEN_IDENTIFIER, .lexeme = "b"},
        {.type = TOKEN_EOF, .lexeme = NULL}
    };
    run_test("Syntax Error: a + * b", t4, 2);

    // TEST 5:  brojac < 10
    token_t t5[] = {
        {.type = TOKEN_IDENTIFIER, .lexeme = "counter"},
        {.type = TOKEN_LESS, .lexeme = NULL},
        {.type = TOKEN_INTEGER, .lexeme = "10"},
        {.type = TOKEN_EOF, .lexeme = NULL}
    };
    run_test("Relational: counter < 10", t5, 0);

    // TEST 6: a - b - c 
    token_t t6[] = {
        {.type = TOKEN_IDENTIFIER, .lexeme = "a"},
        {.type = TOKEN_MINUS,      .lexeme = NULL},
        {.type = TOKEN_IDENTIFIER, .lexeme = "b"},
        {.type = TOKEN_MINUS,      .lexeme = NULL},
        {.type = TOKEN_IDENTIFIER, .lexeme = "c"},
        {.type = TOKEN_EOF,        .lexeme = NULL}
    };
    run_test("Left Associativity: a - b - c", t6, 0);

    // TEST 7: a * 2 <= b + 3
    token_t t7[] = {
        {.type = TOKEN_IDENTIFIER,  .lexeme = "a"},
        {.type = TOKEN_STAR,        .lexeme = NULL},
        {.type = TOKEN_INTEGER,     .lexeme = "2"},
        {.type = TOKEN_LESS_EQUAL,  .lexeme = NULL},
        {.type = TOKEN_IDENTIFIER,  .lexeme = "b"},
        {.type = TOKEN_PLUS,        .lexeme = NULL},
        {.type = TOKEN_INTEGER,     .lexeme = "3"},
        {.type = TOKEN_EOF,         .lexeme = NULL}
    };
    run_test("Mixed Relational/Math: a * 2 <= b + 3", t7, 0);

    // TEST 8: ( a + b           (miss r_paren)
    token_t t8[] = {
        {.type = TOKEN_LPAREN,     .lexeme = NULL},
        {.type = TOKEN_IDENTIFIER, .lexeme = "a"},
        {.type = TOKEN_PLUS,       .lexeme = NULL},
        {.type = TOKEN_IDENTIFIER, .lexeme = "b"},
        {.type = TOKEN_EOF,        .lexeme = NULL}
    };
    run_test("Missing Parenthesis: ( a + b", t8, 2);

    return 0;
}
