// Implementation of the IFJ26 imperative language compiler
// test_parser_expression.c
// Author(s): Dušan Popović (xpopovd00)

// AI-generated using: Gemini
//                model: Gemini 4 HYDROGEN
// Prompt (summary): "Generate unit tests for the IFJ26 expression parser covering
//                    operator precedence, function calls, and trailing commas."
// Scope: test_parser_expression.c

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "comp_error.h"
#include "lexer.h"
#include "parser_expression.h"

static int total_tests = 0;
static int passed_tests = 0;

/* Adjust these lines if your lexer initialization function has a different signature */
static FILE* open_mem_file(const char* src, lexer_t* lexer)
{
    FILE* f = tmpfile();
    assert(f != NULL);
    fputs(src, f);
    rewind(f);
    lexer_init(lexer, f);
    return f;
}

static int parse_expr_str(const char* src, ast_node_t** out_ast)
{
    lexer_t lexer;
    FILE* f = open_mem_file(src, &lexer);

    token_t tok;
    memset(&tok, 0, sizeof(token_t));

    int err = lexer_next(&lexer, &tok);
    if (err == ERR_OK) {
        err = parser_expression(&lexer, &tok, out_ast);
    }

    token_dispose(&tok);
    fclose(f);
    return err;
}

static void check_expr(const char* name, const char* src, int expected_err)
{
    total_tests++;
    ast_node_t* ast = NULL;
    int actual = parse_expr_str(src, &ast);

    if (actual == expected_err) {
        if (expected_err == ERR_OK) {
            assert(ast != NULL);
        }
        passed_tests++;
        printf("[PASS] %s\n", name);
    } else {
        printf("[FAIL] %s -> got %d, expected %d (input: `%s`)\n",
            name, actual, expected_err, src);
    }

    if (ast != NULL) {
        ast_free(ast);
    }
}

int main(void)
{
    /* --- Valid expressions (ERR_OK) --- */
    check_expr("Simple integer literal", "42", ERR_OK);
    check_expr("Hexadecimal and underscore literals", "0xFF + 1_000", ERR_OK);
    check_expr("Double and string with escape sequence", "3.14 + \"hello\\n\"", ERR_OK);
    check_expr("Arithmetic operator precedence", "1 + 2 * 3 - 4 / 2", ERR_OK);
    check_expr("Parentheses override precedence", "(1 + 2) * (3 - 4)", ERR_OK);
    check_expr("Unary minus and double unary minus", "- -5 + -(2 * -3)", ERR_OK);
    check_expr("Relational and equality operators", "3 + 2 <= 10 / 2 == (1 != 0)", ERR_OK);
    check_expr("Unit literal and comparison", "() == ()", ERR_OK);
    check_expr("Function call without arguments", "foo()", ERR_OK);
    check_expr("Nested function calls with expressions", "foo((1 + 2) * 3, bar(4, 5 + 6))", ERR_OK);
    check_expr("Multi-line call with trailing comma",
        "foo(\n"
        "    1,\n"
        "    2 + 3,\n"
        "    )",
        ERR_OK);

    /* --- Invalid expressions (ERR_SYNTAX) --- */
    check_expr("Trailing comma on the same line", "foo(1,)", ERR_SYNTAX);
    check_expr("Trailing comma without newline before closing paren", "foo(1, 2, )", ERR_SYNTAX);
    check_expr("Chained relational operators (R_ERR)", "1 < 2 < 3", ERR_SYNTAX);
    check_expr("Chained equality operators (R_ERR)", "1 == 2 == 3", ERR_SYNTAX);
    check_expr("Incomplete binary expression", "1 +", ERR_SYNTAX);
    check_expr("Two consecutive binary operators", "1 + * 2", ERR_SYNTAX);
    check_expr("Unclosed parenthesis", "(1 + 2", ERR_SYNTAX);
    check_expr("Extra closing parenthesis in call", "foo(1, 2))", ERR_SYNTAX);
    check_expr("Empty argument between commas", "foo(1, , 2)", ERR_SYNTAX);

    printf("\nExpression test results: %d / %d passed.\n", passed_tests, total_tests);
    return (passed_tests == total_tests) ? 0 : 1;
}
