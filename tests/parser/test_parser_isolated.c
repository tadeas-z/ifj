// Implementation of the IFJ26 imperative language compiler
// test_parser_isolated.c            // isolated from lexer, inter lexer
// Author(s): Dušan Popović (xpopovd00)

// AI-generated using: Gemini
//                model: Gemini 4 HYDROGEN
// Prompt (summary): "Generate unit tests for the IFJ26 expression parser covering
//                    operator precedence, function calls, mock lexer, and trailing commas."
// Scope: test_parser_isolated.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "comp_error.h"
#include "lexer.h"
#include "parser.h"

/* ---- Mock lekser ------------------------------------------------------- */

static const token_t* mock_tokens = NULL;
static size_t mock_count = 0;
static size_t mock_pos = 0;

static char* c11_strdup(const char* s)
{
    if (!s)
        return NULL;
    size_t len = strlen(s) + 1;
    char* copy = malloc(len);
    if (copy)
        memcpy(copy, s, len);
    return copy;
}

void token_dispose(token_t* token)
{
    if (token) {
        free(token->lexeme);
        *token = (token_t) { 0 };
    }
}

lexer_status_t lexer_next(lexer_t* lexer, token_t* output)
{
    if (!output)
        return ERR_INTERNAL;
    *output = (token_t) { 0 };
    if (!lexer)
        return ERR_INTERNAL;

    if (mock_pos < mock_count) {
        *output = mock_tokens[mock_pos++];
        if (output->lexeme) {
            output->lexeme = c11_strdup(output->lexeme);
            if (!output->lexeme) {
                lexer->status = ERR_INTERNAL;
                return ERR_INTERNAL;
            }
        }
    } else {
        output->type = TOKEN_EOF;
        output->line = 999;
        output->column = 1;
    }
    return ERR_OK;
}

/* Pomoćni makroi za čitljivo pisanje niza tokena */
#define TOK(t, text, indent, starts)                                                                                                        \
    {                                                                                                                                       \
        .type = (t), .lexeme = (text), .length = sizeof(text) - 1, .line = 1, .column = 1, .indentation = (indent), .starts_line = (starts) \
    }

#define NL(indent)                                                                                                            \
    {                                                                                                                         \
        .type = TOKEN_NEWLINE, .lexeme = "\n", .length = 1, .line = 1, .column = 1, .indentation = (indent), .starts_line = 0 \
    }

#define EOF_TOK(indent)                                                                                                   \
    {                                                                                                                     \
        .type = TOKEN_EOF, .lexeme = NULL, .length = 0, .line = 1, .column = 1, .indentation = (indent), .starts_line = 0 \
    }

/* ---- Test runner ------------------------------------------------------- */

static int passed = 0;
static int failed = 0;

static void run_parser_test(const char* name, const token_t* tokens, size_t count, error_code_t expected)
{
    mock_tokens = tokens;
    mock_count = count;
    mock_pos = 0;

    lexer_t dummy_lexer = { 0 };
    ast_node_t* ast = NULL;
    error_code_t got = parser_main(&dummy_lexer, &ast);

    if (ast) {
        ast_free(ast);
    }

    if (got == expected) {
        printf("[PASS] %s\n", name);
        passed++;
    } else {
        const char* last = (mock_pos > 0 && mock_tokens[mock_pos - 1].lexeme)
            ? mock_tokens[mock_pos - 1].lexeme
            : "EOF";
        printf("[FAIL] %s -> got %d, expected %d (at token #%zu: '%s')\n",
            name, got, expected, mock_pos, last);
        failed++;
    }
}

int main(void)
{
    /* 1. Kompleksan ispravan program (funkcija, if-izraz, val/var, &&, ||, !, pozivi) */
    {
        const token_t tokens[] = {
            /* import ifj26.* */
            TOK(TOKEN_IMPORT, "import", 0, 1),
            TOK(TOKEN_IDENTIFIER, "ifj26", 0, 0),
            TOK(TOKEN_DOT, ".", 0, 0),
            TOK(TOKEN_STAR, "*", 0, 0),
            NL(0),

            /* def calc(a: Int, b: Boolean): Int = */
            TOK(TOKEN_DEF, "def", 0, 1),
            TOK(TOKEN_IDENTIFIER, "calc", 0, 0),
            TOK(TOKEN_LPAREN, "(", 0, 0),
            TOK(TOKEN_IDENTIFIER, "a", 0, 0),
            TOK(TOKEN_COLON, ":", 0, 0),
            TOK(TOKEN_TYPE_INT, "Int", 0, 0),
            TOK(TOKEN_COMMA, ",", 0, 0),
            TOK(TOKEN_IDENTIFIER, "b", 0, 0),
            TOK(TOKEN_COLON, ":", 0, 0),
            TOK(TOKEN_TYPE_BOOLEAN, "Boolean", 0, 0),
            TOK(TOKEN_RPAREN, ")", 0, 0),
            TOK(TOKEN_COLON, ":", 0, 0),
            TOK(TOKEN_TYPE_INT, "Int", 0, 0),
            TOK(TOKEN_ASSIGN, "=", 0, 0),
            NL(0),

            /*     if !b && (a > 0 || a == 1) then a else 0 */
            TOK(TOKEN_IF, "if", 4, 1),
            TOK(TOKEN_NOT, "!", 4, 0),
            TOK(TOKEN_IDENTIFIER, "b", 4, 0),
            TOK(TOKEN_AND, "&&", 4, 0),
            TOK(TOKEN_LPAREN, "(", 4, 0),
            TOK(TOKEN_IDENTIFIER, "a", 4, 0),
            TOK(TOKEN_GREATER, ">", 4, 0),
            TOK(TOKEN_INTEGER, "0", 4, 0),
            TOK(TOKEN_OR, "||", 4, 0),
            TOK(TOKEN_IDENTIFIER, "a", 4, 0),
            TOK(TOKEN_EQUAL, "==", 4, 0),
            TOK(TOKEN_INTEGER, "1", 4, 0),
            TOK(TOKEN_RPAREN, ")", 4, 0),
            TOK(TOKEN_THEN, "then", 4, 0),
            TOK(TOKEN_IDENTIFIER, "a", 4, 0),
            TOK(TOKEN_ELSE, "else", 4, 0),
            TOK(TOKEN_INTEGER, "0", 4, 0),
            NL(4),

            /* @main */
            TOK(TOKEN_MAIN, "@main", 0, 1),
            NL(0),

            /* def main(): Unit = */
            TOK(TOKEN_DEF, "def", 0, 1),
            TOK(TOKEN_IDENTIFIER, "main", 0, 0),
            TOK(TOKEN_LPAREN, "(", 0, 0),
            TOK(TOKEN_RPAREN, ")", 0, 0),
            TOK(TOKEN_COLON, ":", 0, 0),
            TOK(TOKEN_TYPE_UNIT, "Unit", 0, 0),
            TOK(TOKEN_ASSIGN, "=", 0, 0),
            NL(0),

            /*     val res = calc(5, false) */
            TOK(TOKEN_VAL, "val", 4, 1),
            TOK(TOKEN_IDENTIFIER, "res", 4, 0),
            TOK(TOKEN_ASSIGN, "=", 4, 0),
            TOK(TOKEN_IDENTIFIER, "calc", 4, 0),
            TOK(TOKEN_LPAREN, "(", 4, 0),
            TOK(TOKEN_INTEGER, "5", 4, 0),
            TOK(TOKEN_COMMA, ",", 4, 0),
            TOK(TOKEN_FALSE, "false", 4, 0),
            TOK(TOKEN_RPAREN, ")", 4, 0),
            NL(4),

            /*     var total = res + 10 * 2 */
            TOK(TOKEN_VAR, "var", 4, 1),
            TOK(TOKEN_IDENTIFIER, "total", 4, 0),
            TOK(TOKEN_ASSIGN, "=", 4, 0),
            TOK(TOKEN_IDENTIFIER, "res", 4, 0),
            TOK(TOKEN_PLUS, "+", 4, 0),
            TOK(TOKEN_INTEGER, "10", 4, 0),
            TOK(TOKEN_STAR, "*", 4, 0),
            TOK(TOKEN_INTEGER, "2", 4, 0),
            NL(4),

            /*     print(total) */
            TOK(TOKEN_IDENTIFIER, "print", 4, 1),
            TOK(TOKEN_LPAREN, "(", 4, 0),
            TOK(TOKEN_IDENTIFIER, "total", 4, 0),
            TOK(TOKEN_RPAREN, ")", 4, 0),
            NL(4),
            EOF_TOK(0)
        };
        run_parser_test("Valid full AST with all extensions", tokens, sizeof(tokens) / sizeof(tokens[0]), ERR_OK);
    }

    /* 2. Višelinijski poziv funkcije */
    {
        const token_t tokens[] = {
            TOK(TOKEN_IMPORT, "import", 0, 1),
            TOK(TOKEN_IDENTIFIER, "ifj26", 0, 0),
            TOK(TOKEN_DOT, ".", 0, 0),
            TOK(TOKEN_STAR, "*", 0, 0),
            NL(0),
            TOK(TOKEN_MAIN, "@main", 0, 1),
            NL(0),
            TOK(TOKEN_DEF, "def", 0, 1),
            TOK(TOKEN_IDENTIFIER, "main", 0, 0),
            TOK(TOKEN_LPAREN, "(", 0, 0),
            TOK(TOKEN_RPAREN, ")", 0, 0),
            TOK(TOKEN_COLON, ":", 0, 0),
            TOK(TOKEN_TYPE_UNIT, "Unit", 0, 0),
            TOK(TOKEN_ASSIGN, "=", 0, 0),
            NL(0),
            TOK(TOKEN_IDENTIFIER, "print", 4, 1),
            TOK(TOKEN_LPAREN, "(", 4, 0),
            TOK(TOKEN_INTEGER, "1", 8, 1),
            TOK(TOKEN_RPAREN, ")", 4, 1),
            NL(4),
            EOF_TOK(0)
        };
        run_parser_test("Valid multiline call", tokens, sizeof(tokens) / sizeof(tokens[0]), ERR_OK);
    }

    /* 3. Sintaksna greška: završni zarez u istom redu */
    {
        const token_t tokens[] = {
            TOK(TOKEN_IMPORT, "import", 0, 1),
            TOK(TOKEN_IDENTIFIER, "ifj26", 0, 0),
            TOK(TOKEN_DOT, ".", 0, 0),
            TOK(TOKEN_STAR, "*", 0, 0),
            NL(0),
            TOK(TOKEN_MAIN, "@main", 0, 1),
            NL(0),
            TOK(TOKEN_DEF, "def", 0, 1),
            TOK(TOKEN_IDENTIFIER, "main", 0, 0),
            TOK(TOKEN_LPAREN, "(", 0, 0),
            TOK(TOKEN_RPAREN, ")", 0, 0),
            TOK(TOKEN_COLON, ":", 0, 0),
            TOK(TOKEN_TYPE_UNIT, "Unit", 0, 0),
            TOK(TOKEN_ASSIGN, "=", 0, 0),
            NL(0),
            TOK(TOKEN_IDENTIFIER, "print", 4, 1),
            TOK(TOKEN_LPAREN, "(", 4, 0),
            TOK(TOKEN_INTEGER, "1", 4, 0),
            TOK(TOKEN_COMMA, ",", 4, 0),
            TOK(TOKEN_RPAREN, ")", 4, 0),
            NL(4),
            EOF_TOK(0)
        };
        run_parser_test("Syntax error: trailing comma on same line", tokens, sizeof(tokens) / sizeof(tokens[0]), ERR_SYNTAX);
    }

    /* 4. Sintaksna greška: novi tokeni iz koleginog PR-a (_, |, [, {) moraju vratiti 2 */
    {
        token_type_t bad_tokens[] = {
            TOKEN_UNDERSCORE,
            TOKEN_PIPE,
            TOKEN_LBRACKET,
            TOKEN_LBRACE
        };
        for (size_t i = 0; i < sizeof(bad_tokens) / sizeof(bad_tokens[0]); i++) {
            const token_t tokens[] = {
                TOK(TOKEN_IMPORT, "import", 0, 1),
                TOK(TOKEN_IDENTIFIER, "ifj26", 0, 0),
                TOK(TOKEN_DOT, ".", 0, 0),
                TOK(TOKEN_STAR, "*", 0, 0),
                NL(0),
                TOK(TOKEN_MAIN, "@main", 0, 1),
                NL(0),
                TOK(TOKEN_DEF, "def", 0, 1),
                TOK(TOKEN_IDENTIFIER, "main", 0, 0),
                TOK(TOKEN_LPAREN, "(", 0, 0),
                TOK(TOKEN_RPAREN, ")", 0, 0),
                TOK(TOKEN_COLON, ":", 0, 0),
                TOK(TOKEN_TYPE_UNIT, "Unit", 0, 0),
                TOK(TOKEN_ASSIGN, "=", 0, 0),
                NL(0),
                TOK(TOKEN_VAL, "val", 4, 1),
                TOK(TOKEN_IDENTIFIER, "x", 4, 0),
                TOK(TOKEN_ASSIGN, "=", 4, 0),
                TOK(bad_tokens[i], "?", 4, 0),
                NL(4),
                EOF_TOK(0)
            };
            run_parser_test("Syntax error: unsupported extension token", tokens, sizeof(tokens) / sizeof(tokens[0]), ERR_SYNTAX);
        }
    }

    /* 5. Sintaksna greška: pogrešno uvlačenje tela funkcije */
    {
        const token_t tokens[] = {
            TOK(TOKEN_IMPORT, "import", 0, 1),
            TOK(TOKEN_IDENTIFIER, "ifj26", 0, 0),
            TOK(TOKEN_DOT, ".", 0, 0),
            TOK(TOKEN_STAR, "*", 0, 0),
            NL(0),
            TOK(TOKEN_MAIN, "@main", 0, 1),
            NL(0),
            TOK(TOKEN_DEF, "def", 0, 1),
            TOK(TOKEN_IDENTIFIER, "main", 0, 0),
            TOK(TOKEN_LPAREN, "(", 0, 0),
            TOK(TOKEN_RPAREN, ")", 0, 0),
            TOK(TOKEN_COLON, ":", 0, 0),
            TOK(TOKEN_TYPE_UNIT, "Unit", 0, 0),
            TOK(TOKEN_ASSIGN, "=", 0, 0),
            NL(0),
            TOK(TOKEN_VAL, "val", 0, 1),
            TOK(TOKEN_IDENTIFIER, "x", 0, 0),
            TOK(TOKEN_ASSIGN, "=", 0, 0),
            TOK(TOKEN_INTEGER, "1", 0, 0),
            NL(0),
            EOF_TOK(0)
        };
        run_parser_test("Syntax error: unindented function body", tokens, sizeof(tokens) / sizeof(tokens[0]), ERR_SYNTAX);
    }

    /* 6. Sintaksna greška: dva binarna operatora zaredom (1 + * 2) */
    {
        const token_t tokens[] = {
            TOK(TOKEN_IMPORT, "import", 0, 1),
            TOK(TOKEN_IDENTIFIER, "ifj26", 0, 0),
            TOK(TOKEN_DOT, ".", 0, 0),
            TOK(TOKEN_STAR, "*", 0, 0),
            NL(0),
            TOK(TOKEN_MAIN, "@main", 0, 1),
            NL(0),
            TOK(TOKEN_DEF, "def", 0, 1),
            TOK(TOKEN_IDENTIFIER, "main", 0, 0),
            TOK(TOKEN_LPAREN, "(", 0, 0),
            TOK(TOKEN_RPAREN, ")", 0, 0),
            TOK(TOKEN_COLON, ":", 0, 0),
            TOK(TOKEN_TYPE_UNIT, "Unit", 0, 0),
            TOK(TOKEN_ASSIGN, "=", 0, 0),
            NL(0),
            TOK(TOKEN_VAL, "val", 4, 1),
            TOK(TOKEN_IDENTIFIER, "x", 4, 0),
            TOK(TOKEN_ASSIGN, "=", 4, 0),
            TOK(TOKEN_INTEGER, "1", 4, 0),
            TOK(TOKEN_PLUS, "+", 4, 0),
            TOK(TOKEN_STAR, "*", 4, 0),
            TOK(TOKEN_INTEGER, "2", 4, 0),
            NL(4),
            EOF_TOK(0)
        };
        run_parser_test("Syntax error: consecutive binary operators", tokens, sizeof(tokens) / sizeof(tokens[0]), ERR_SYNTAX);
    }

    printf("\nIsolated parser results: %d / %d passed.\n", passed, passed + failed);
    return failed == 0 ? 0 : 1;
}
