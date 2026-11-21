// Implementation of the IFJ26 imperative language compiler
// test_lexer.c
// Author(s): Tadeáš Zonyga (xzonygt00)

/* Regression tests for the IFJ26 lexer. */

#include "lexer.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* Optional fault injection; enabled only by the standalone test command. */
#ifdef LEXER_TEST_ALLOC_FAILURES
static size_t fail_allocation, allocation_calls;
void* __real_malloc(size_t size);
void* __real_realloc(void* ptr, size_t size);

void* __wrap_malloc(size_t size)
{
    if (fail_allocation && ++allocation_calls == fail_allocation)
        return NULL;
    return __real_malloc(size);
}

void* __wrap_realloc(void* ptr, size_t size)
{
    if (fail_allocation && ++allocation_calls == fail_allocation)
        return NULL;
    return __real_realloc(ptr, size);
}
#endif

typedef struct {
    token_type_t type;
    const char* lexeme;
    size_t line, column;
} expected_t;

static FILE* source(const char* text)
{
    FILE* input = tmpfile();
    assert(input);
    assert(fwrite(text, 1, strlen(text), input) == strlen(text));
    rewind(input);
    return input;
}

static void sequence(const char* text, const expected_t* expected, size_t count)
{
    FILE* input = source(text);
    lexer_t lexer;
    token_t token = { 0 };
    assert(lexer_init(&lexer, input) == ERR_OK);
    for (size_t i = 0; i < count; ++i) {
        assert(lexer_next(&lexer, &token) == ERR_OK);
        assert(token.type == expected[i].type);
        assert(token.line == expected[i].line);
        assert(token.column == expected[i].column);
        if (expected[i].lexeme) {
            assert(token.lexeme);
            assert(strcmp(token.lexeme, expected[i].lexeme) == 0);
            assert(token.length == strlen(expected[i].lexeme));
        } else {
            assert(token.lexeme == NULL);
            assert(token.length == 0);
        }
        token_dispose(&token);
        token_dispose(&token);
    }
    assert(lexer_next(&lexer, &token) == ERR_OK);
    assert(token.type == TOKEN_EOF);
    size_t line = token.line, column = token.column;
    assert(lexer_next(&lexer, &token) == ERR_OK);
    assert(token.type == TOKEN_EOF && token.line == line && token.column == column);
    token_dispose(&token);
    fclose(input);
}

static void basic_program(void)
{
    const expected_t expected[] = {
        { TOKEN_IMPORT, "import", 1, 1 }, { TOKEN_IDENTIFIER, "ifj26", 1, 8 },
        { TOKEN_DOT, ".", 1, 13 }, { TOKEN_STAR, "*", 1, 14 },
        { TOKEN_NEWLINE, "\n", 1, 15 },
        { TOKEN_MAIN, "@main", 2, 1 }, { TOKEN_DEF, "def", 2, 7 },
        { TOKEN_IDENTIFIER, "run", 2, 11 }, { TOKEN_LPAREN, "(", 2, 14 },
        { TOKEN_RPAREN, ")", 2, 15 }, { TOKEN_COLON, ":", 2, 16 },
        { TOKEN_TYPE_UNIT, "Unit", 2, 18 }, { TOKEN_ASSIGN, "=", 2, 23 },
        { TOKEN_NEWLINE, "\n", 2, 24 },
        { TOKEN_VAL, "val", 3, 3 }, { TOKEN_IDENTIFIER, "x", 3, 7 },
        { TOKEN_ASSIGN, "=", 3, 9 }, { TOKEN_MINUS, "-", 3, 11 },
        { TOKEN_INTEGER, "1_000", 3, 12 }, { TOKEN_PLUS, "+", 3, 18 },
        { TOKEN_DOUBLE, ".5e+2D", 3, 20 }, { TOKEN_EOF, NULL, 3, 26 }
    };
    sequence("import ifj26.*\n@main def run(): Unit =\n  val x = -1_000 + .5e+2D",
        expected, sizeof expected / sizeof expected[0]);
}

static void operators(void)
{
    const expected_t expected[] = {
        { TOKEN_LPAREN, "(", 1, 1 }, { TOKEN_RPAREN, ")", 1, 2 },
        { TOKEN_COLON, ":", 1, 3 }, { TOKEN_COMMA, ",", 1, 4 },
        { TOKEN_SEMICOLON, ";", 1, 5 }, { TOKEN_DOT, ".", 1, 6 },
        { TOKEN_PLUS, "+", 1, 7 }, { TOKEN_MINUS, "-", 1, 8 },
        { TOKEN_STAR, "*", 1, 9 }, { TOKEN_SLASH, "/", 1, 10 },
        { TOKEN_ASSIGN, "=", 1, 12 }, { TOKEN_EQUAL, "==", 1, 14 },
        { TOKEN_NOT_EQUAL, "!=", 1, 17 }, { TOKEN_LESS, "<", 1, 20 },
        { TOKEN_LESS_EQUAL, "<=", 1, 22 }, { TOKEN_GREATER, ">", 1, 25 },
        { TOKEN_GREATER_EQUAL, ">=", 1, 27 }, { TOKEN_EOF, NULL, 1, 29 }
    };
    sequence("():,;.+-*/ = == != < <= > >=", expected,
        sizeof expected / sizeof expected[0]);
}

static void words(void)
{
    const char* names[] = {
        "def", "do", "Double", "else", "if", "import", "Int", "return",
        "String", "then", "Unit", "val", "var", "while", "Boolean", "case",
        "class", "extension", "for", "List", "match", "type", "main",
        "IF", "int", "val1", "_name", "a_b2", "readInt"
    };
    const token_type_t types[] = {
        TOKEN_DEF, TOKEN_DO, TOKEN_TYPE_DOUBLE, TOKEN_ELSE, TOKEN_IF,
        TOKEN_IMPORT, TOKEN_TYPE_INT, TOKEN_RETURN, TOKEN_TYPE_STRING,
        TOKEN_THEN, TOKEN_TYPE_UNIT, TOKEN_VAL, TOKEN_VAR, TOKEN_WHILE,
        TOKEN_TYPE_BOOLEAN, TOKEN_RESERVED_KEYWORD, TOKEN_RESERVED_KEYWORD,
        TOKEN_RESERVED_KEYWORD, TOKEN_FOR, TOKEN_RESERVED_KEYWORD,
        TOKEN_RESERVED_KEYWORD, TOKEN_RESERVED_KEYWORD, TOKEN_IDENTIFIER,
        TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_IDENTIFIER,
        TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_IDENTIFIER
    };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i) {
        const expected_t expected[] = { { types[i], names[i], 1, 1 } };
        sequence(names[i], expected, 1);
    }
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt (summary): "Update the number tests for repeated underscores:
//          1__000 and similar are valid, a trailing run of '_' or a run
//          before e/d/an identifier character is lexical error 1."
// Scope: integers/doubles arrays in numbers(), bad array in errors().

static void numbers(void)
{
    const char* integers[] = { "0", "00", "0_0", "0__0", "42", "1_000_000", "1__000", "1___0",
        "2147483648" };
    const char* doubles[] = {
        ".5", "0.5", "01.5", "1.0d", "1.0D", "1e2", "1E-2", "1e+2D",
        "1_000.2_5e1_0d", ".5e-2", "0e0", "1.7976931348623157e308",
        "1e-310", "0e-9999", "1.2__5", "1e1__0", "1__0.5__0e1__0d"
    };
    for (size_t i = 0; i < sizeof integers / sizeof integers[0]; ++i) {
        const expected_t expected[] = { { TOKEN_INTEGER, integers[i], 1, 1 } };
        sequence(integers[i], expected, 1);
    }
    for (size_t i = 0; i < sizeof doubles / sizeof doubles[0]; ++i) {
        const expected_t expected[] = { { TOKEN_DOUBLE, doubles[i], 1, 1 } };
        sequence(doubles[i], expected, 1);
    }
    const expected_t adjacent[] = {
        { TOKEN_INTEGER, "1", 1, 1 }, { TOKEN_DOT, ".", 1, 2 },
        { TOKEN_IDENTIFIER, "foo", 1, 3 }, { TOKEN_MINUS, "-", 1, 6 },
        { TOKEN_INTEGER, "2", 1, 7 }
    };
    sequence("1.foo-2", adjacent, sizeof adjacent / sizeof adjacent[0]);
}

static void expect_error(const char* text);

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt (summary): "Rewrite the numeric boundary tests after the code
//          review: a number glued to an identifier character is lexical
//          error 1, a number followed by '.', an operator or a string still
//          ends normally."
// Scope: number_boundaries().
/* Spec 1.1 and 6: unsupported bases and suffixes (hex, L, f, d on an
 * integer) and any letter right after a number form one malformed lexeme. */
static void number_boundaries(void)
{
    const char* glued[] = {
        "10abc\"y\"", "12abc", "0x12", "0x1F", "1L", "1.0f", "1.5F",
        "1.0Dfoo", "1.0dd", "1d", "1e2x", "1_0.5_0dx", ".5abc", "1_0x", "x = 1abc"
    };
    for (size_t i = 0; i < sizeof glued / sizeof glued[0]; ++i)
        expect_error(glued[i]);

    const expected_t member[] = {
        { TOKEN_INTEGER, "1", 1, 1 }, { TOKEN_DOT, ".", 1, 2 },
        { TOKEN_IDENTIFIER, "toString", 1, 3 }
    };
    sequence("1.toString", member, 3);
    const expected_t field[] = {
        { TOKEN_IDENTIFIER, "x", 1, 1 }, { TOKEN_DOT, ".", 1, 2 },
        { TOKEN_IDENTIFIER, "y", 1, 3 }
    };
    sequence("x.y", field, 3);
    const expected_t ends[] = {
        { TOKEN_INTEGER, "10", 1, 1 }, { TOKEN_STRING, "\"y\"", 1, 3 },
        { TOKEN_DOUBLE, "1e5", 1, 6 }, { TOKEN_RPAREN, ")", 1, 9 },
        { TOKEN_INTEGER, "2", 1, 10 }, { TOKEN_PLUS, "+", 1, 11 },
        { TOKEN_DOUBLE, ".5", 1, 12 }
    };
    sequence("10\"y\"1e5)2+.5", ends, 7);
}

static void errors(void)
{
    const char* bad[] = {
        "01", "0_1", "0__1", "1_", "1__", "1__x", "1e", "1e+", "1e_2", "1e2_",
        "1e2__", "1.0__", "1.0__d",
        "1.0_", "1ex", "1e+x", "1_x", "1.5_d", "01abc", "00_1", "1e2_x",
        "@", "@ main", "@other", "@mainly", "&", "%",
        "`name`", "\x80",
        "1e309", "1e-9999"
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        FILE* input = source(bad[i]);
        lexer_t lexer;
        token_t token = { 0 };
        assert(lexer_init(&lexer, input) == ERR_OK);
        lexer_status_t status;
        do {
            status = lexer_next(&lexer, &token);
            if (status == ERR_OK)
                assert(token.type != TOKEN_EOF);
            token_dispose(&token);
        } while (status == ERR_OK);
        assert(status == ERR_LEXICAL);
        assert(lexer_next(&lexer, &token) == ERR_LEXICAL);
        assert(token.lexeme == NULL);
        fclose(input);
    }
    lexer_t lexer;
    token_t token = { 0 };
    assert(lexer_init(NULL, NULL) == ERR_INTERNAL);
    assert(lexer_init(&lexer, NULL) == ERR_INTERNAL);
    assert(lexer_next(&lexer, &token) == ERR_INTERNAL);
    assert(lexer_next(NULL, &token) == ERR_INTERNAL);
    assert(lexer_next(NULL, NULL) == ERR_INTERNAL);
}

static void positions_and_ownership(void)
{
    const expected_t expected[] = {
        { TOKEN_IDENTIFIER, "a", 1, 3 }, { TOKEN_NEWLINE, "\n", 1, 5 },
        { TOKEN_IDENTIFIER, "b", 3, 3 },
        { TOKEN_EOF, NULL, 3, 4 }
    };
    sequence("  a\r\n\n  b", expected, sizeof expected / sizeof expected[0]);
    sequence("", NULL, 0);
    sequence(" \t\r", NULL, 0);
    FILE* input = source("first second");
    lexer_t lexer;
    token_t first, second;
    assert(lexer_init(&lexer, input) == ERR_OK);
    assert(lexer_next(&lexer, &first) == ERR_OK);
    assert(lexer_next(&lexer, &second) == ERR_OK);
    assert(strcmp(first.lexeme, "first") == 0);
    assert(strcmp(second.lexeme, "second") == 0);
    token_dispose(&second);
    assert(strcmp(first.lexeme, "first") == 0);
    token_dispose(&first);
    fclose(input);

    char long_name[8193];
    memset(long_name, 'a', sizeof long_name - 1);
    long_name[sizeof long_name - 1] = '\0';
    const expected_t long_expected[] = { { TOKEN_IDENTIFIER, long_name, 1, 1 } };
    sequence(long_name, long_expected, 1);
}

// AI-generated using: Codex CLI
//              model: GPT-6 Astra - Medium
// Prompt (summary): "Extend the existing lexer test suite against the
//          IFJ26 specification: strings, comments, error handling, logical
//          lines, token metadata, memory safety, independent lexers and full
//          programs. Reuse the test helpers, change the lexer only for a
//          confirmed specification violation, then build and run all tests."
// Scope: string_literals, comments, the error group for strings, comments
//        and layout, logical_lines, indentation_metadata, full_program,
//        independent_lexers, allocation_failures.

static void string_literals(void)
{
    const char* valid[] = {
        "\"\"", "\"text\"", "\"a b\"", "\"\\\\\\\"\\n\\r\\t\"",
        "\"// not a comment /* either */\"",
        "\"\"\"\"\"\"", "\"\"\"first\nsecond\"\"\"",
        "\"\"\"\\q\\n/*raw*/\"\"\"", "\"\"\"a\"b\"\"c\"\"\""
    };
    for (size_t i = 0; i < sizeof valid / sizeof valid[0]; ++i) {
        const expected_t expected[] = { { TOKEN_STRING, valid[i], 1, 1 } };
        sequence(valid[i], expected, 1);
    }
    const expected_t adjacent[] = {
        { TOKEN_STRING, "\"a\"", 1, 1 }, { TOKEN_STRING, "\"b\"", 1, 4 },
        { TOKEN_IDENTIFIER, "tail", 1, 8 }
    };
    sequence("\"a\"\"b\" tail", adjacent, 3);
    const expected_t multiline[] = {
        { TOKEN_STRING, "\"\"\"a\nb\"\"\"", 1, 1 },
        { TOKEN_IDENTIFIER, "x", 2, 6 }, { TOKEN_NEWLINE, "\n", 2, 7 },
        { TOKEN_IDENTIFIER, "y", 3, 1 }
    };
    sequence("\"\"\"a\nb\"\"\" x\ny", multiline, 4);

    /* Dynamic buffers must grow without losing raw text or its length. */
    char* long_string = malloc(16387);
    assert(long_string);
    long_string[0] = '"';
    memset(long_string + 1, 'x', 16384);
    long_string[16385] = '"';
    long_string[16386] = '\0';
    const expected_t long_expected[] = { { TOKEN_STRING, long_string, 1, 1 } };
    sequence(long_string, long_expected, 1);
    free(long_string);
}

static void comments(void)
{
    sequence("// comment", NULL, 0);
    sequence("/* comment */", NULL, 0);
    sequence("/* outer /* nested */ outer */", NULL, 0);
    sequence("// x\n \n/* one\ntwo */\n// final", NULL, 0);
    const expected_t separated[] = {
        { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_IDENTIFIER, "b", 1, 12 },
        { TOKEN_SLASH, "/", 1, 14 }, { TOKEN_INTEGER, "2", 1, 16 }
    };
    sequence("a/* gap */ b / 2", separated, 4);
    const expected_t line_comment[] = {
        { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_NEWLINE, "\n", 1, 10 },
        { TOKEN_IDENTIFIER, "b", 3, 1 }
    };
    sequence("a // tail\n// ignored\nb", line_comment, 3);
    const expected_t eof_comment[] = {
        { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_NEWLINE, "\n", 1, 9 }
    };
    sequence("a // eof", eof_comment, 2);
    const expected_t block_newline[] = {
        { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_NEWLINE, "\n", 1, 5 },
        { TOKEN_IDENTIFIER, "b", 3, 1 }
    };
    sequence("a /*\n*/\nb", block_newline, 3);
    const expected_t block_eof[] = {
        { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_NEWLINE, "\n", 1, 5 }
    };
    sequence("a /*\n*/", block_eof, 2);
    const expected_t nested[] = {
        { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_IDENTIFIER, "b", 1, 20 }
    };
    sequence("a/* x /* y */ z */ b", nested, 2);
}

static void expect_error(const char* text)
{
    FILE* input = source(text);
    lexer_t lexer;
    token_t token = { 0 };
    assert(lexer_init(&lexer, input) == ERR_OK);
    lexer_status_t status;
    size_t count = 0;
    do {
        status = lexer_next(&lexer, &token);
        if (status == ERR_OK && token.type == TOKEN_EOF) {
            fprintf(stderr, "Expected lexical error for: %s\n", text);
            assert(0);
        }
        token_dispose(&token);
        assert(++count <= strlen(text) + 2);
    } while (status == ERR_OK);
    assert(status == ERR_LEXICAL);
    assert(lexer_next(&lexer, &token) == ERR_LEXICAL);
    assert(token.lexeme == NULL);
    fclose(input);
}

static void layout_and_literal_errors(void)
{
    const char* bad[] = {
        "\"unfinished", "\"line\nbreak\"", "\"\\q\"", "\"\\0\"",
        "\"\\x41\"", "\"\\u0041\"", "\"trailing\\", "\"\\\n\"",
        "\"\x80\"", "\"\"\"\x80\"\"\"", "\"\"\"unfinished", "\"\"\"end\"\"",
        "/*", "/* outer /* inner */", "/* outer /* inner",
        "a /*\n*/ b", "/*\n*/ 1", "f( /*\n*/ x)",
        "\tval x = 1", " \tval x = 1", "a\n\tb",
        "f(\nx)", "  f(\n  x)", "  f(\n x)", "f(\n\tx)",
        "f(\n  x\n)", "f(\n  x,\n// ignored\ny)"
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i)
        expect_error(bad[i]);
}

static void logical_lines(void)
{
    const expected_t expected[] = {
        { TOKEN_IDENTIFIER, "f", 1, 1 }, { TOKEN_LPAREN, "(", 1, 2 },
        { TOKEN_INTEGER, "1", 2, 3 }, { TOKEN_COMMA, ",", 2, 4 },
        { TOKEN_INTEGER, "2", 4, 5 }, { TOKEN_COMMA, ",", 4, 6 },
        { TOKEN_RPAREN, ")", 5, 3 }, { TOKEN_NEWLINE, "\n", 5, 4 },
        { TOKEN_IDENTIFIER, "x", 7, 1 }
    };
    sequence("f(\n  1,\n// ignored\n    2,\n  )\n\nx", expected, 9);
    const expected_t inline_block[] = {
        { TOKEN_IDENTIFIER, "f", 1, 1 }, { TOKEN_LPAREN, "(", 1, 2 },
        { TOKEN_IDENTIFIER, "x", 3, 3 }, { TOKEN_RPAREN, ")", 3, 4 }
    };
    sequence("f(/*\n*/\n  x)", inline_block, 4);
    const expected_t semicolons[] = {
        { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_SEMICOLON, ";", 1, 2 },
        { TOKEN_IDENTIFIER, "b", 1, 4 }, { TOKEN_NEWLINE, "\n", 1, 5 },
        { TOKEN_IDENTIFIER, "c", 2, 1 }
    };
    sequence("a; b\nc", semicolons, 5);
}

static void indentation_metadata(void)
{
    const char* text = "a\n  b(c,\n    d)\n\n // ignored\n  /* gap */ e\nf";
    const token_type_t types[] = {
        TOKEN_IDENTIFIER, TOKEN_NEWLINE,
        TOKEN_IDENTIFIER, TOKEN_LPAREN, TOKEN_IDENTIFIER, TOKEN_COMMA,
        TOKEN_IDENTIFIER, TOKEN_RPAREN, TOKEN_NEWLINE,
        TOKEN_IDENTIFIER, TOKEN_NEWLINE, TOKEN_IDENTIFIER, TOKEN_EOF
    };
    const size_t indentation[] = { 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0 };
    const int starts[] = { 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0 };
    FILE* input = source(text);
    lexer_t lexer;
    token_t token = { 0 };
    assert(lexer_init(&lexer, input) == ERR_OK);
    for (size_t i = 0; i < sizeof types / sizeof types[0]; ++i) {
        assert(lexer_next(&lexer, &token) == ERR_OK);
        assert(token.type == types[i]);
        assert(token.indentation == indentation[i]);
        assert(token.starts_line == starts[i]);
        token_dispose(&token);
    }
    fclose(input);
    sequence(" \t\n  \t", NULL, 0); // Whitespace-only lines do not create blocks.
}

static void full_program(void)
{
    const char* text = "import ifj26.*\n// example\n@main def run(): Unit =\n"
                       "  val s = \"hello\\n\"\n"
                       "  /* nested /* comment */ ok */\n"
                       "  print(\n    s\n    )\n"
                       "  val raw = \"\"\"first\nsecond\"\"\"\n";
    const token_type_t expected[] = {
        TOKEN_IMPORT, TOKEN_IDENTIFIER, TOKEN_DOT, TOKEN_STAR, TOKEN_NEWLINE,
        TOKEN_MAIN, TOKEN_DEF, TOKEN_IDENTIFIER, TOKEN_LPAREN, TOKEN_RPAREN,
        TOKEN_COLON, TOKEN_TYPE_UNIT, TOKEN_ASSIGN, TOKEN_NEWLINE,
        TOKEN_VAL, TOKEN_IDENTIFIER, TOKEN_ASSIGN, TOKEN_STRING, TOKEN_NEWLINE,
        TOKEN_IDENTIFIER, TOKEN_LPAREN, TOKEN_IDENTIFIER, TOKEN_RPAREN, TOKEN_NEWLINE,
        TOKEN_VAL, TOKEN_IDENTIFIER, TOKEN_ASSIGN, TOKEN_STRING, TOKEN_NEWLINE, TOKEN_EOF
    };
    FILE* input = source(text);
    lexer_t lexer;
    token_t token = { 0 };
    assert(lexer_init(&lexer, input) == ERR_OK);
    for (size_t i = 0; i < sizeof expected / sizeof expected[0]; ++i) {
        assert(lexer_next(&lexer, &token) == ERR_OK);
        assert(token.type == expected[i]);
        token_dispose(&token);
    }
    fclose(input);
}

static void independent_lexers(void)
{
    FILE* a = source("f(\n  \"value\")\n");
    FILE* b = source("x\n  y");
    lexer_t first, second;
    token_t left = { 0 }, right = { 0 };
    assert(lexer_init(&first, a) == ERR_OK);
    assert(lexer_init(&second, b) == ERR_OK);
    assert(lexer_next(&first, &left) == ERR_OK);
    token_dispose(&left);
    assert(lexer_next(&first, &left) == ERR_OK);
    assert(left.type == TOKEN_LPAREN);
    token_dispose(&left);
    assert(lexer_next(&second, &right) == ERR_OK);
    assert(right.starts_line);
    token_dispose(&right);
    assert(lexer_next(&first, &left) == ERR_OK);
    assert(left.type == TOKEN_STRING && !left.starts_line);
    assert(lexer_next(&second, &right) == ERR_OK);
    assert(right.type == TOKEN_NEWLINE);
    token_dispose(&right);
    assert(lexer_next(&second, &right) == ERR_OK);
    assert(right.indentation == 2 && right.starts_line);
    token_dispose(&right);
    assert(strcmp(left.lexeme, "\"value\"") == 0);
    token_dispose(&left);
    fclose(a);
    fclose(b);
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt (summary): "Add regression tests for the refactored identifier(),
//          operator_token() and layout helpers: every operator at EOF,
//          identifier and keyword boundaries, lone underscore, // comments
//          at EOF and comments with indentation inside parentheses."
// Scope: operator_boundaries, identifier_boundaries, comment_layout.

static void operator_boundaries(void)
{
    const struct {
        const char* text;
        token_type_t type;
    } single[] = {
        { "(", TOKEN_LPAREN }, { ")", TOKEN_RPAREN }, { ":", TOKEN_COLON },
        { ",", TOKEN_COMMA }, { ";", TOKEN_SEMICOLON }, { ".", TOKEN_DOT },
        { "+", TOKEN_PLUS }, { "-", TOKEN_MINUS }, { "*", TOKEN_STAR },
        { "/", TOKEN_SLASH }, { "=", TOKEN_ASSIGN }, { "<", TOKEN_LESS },
        { ">", TOKEN_GREATER }, { "==", TOKEN_EQUAL }, { "!=", TOKEN_NOT_EQUAL },
        { "<=", TOKEN_LESS_EQUAL }, { ">=", TOKEN_GREATER_EQUAL }
    };
    for (size_t i = 0; i < sizeof single / sizeof single[0]; ++i) {
        const expected_t expected[] = {
            { single[i].type, single[i].text, 1, 1 },
            { TOKEN_EOF, NULL, 1, strlen(single[i].text) + 1 }
        };
        sequence(single[i].text, expected, 2);
    }

    /* Two-character operators take at most one '='; the rest starts a new token. */
    const struct {
        const char* text;
        expected_t first, second;
    } split[] = {
        { "===", { TOKEN_EQUAL, "==", 1, 1 }, { TOKEN_ASSIGN, "=", 1, 3 } },
        { "!==", { TOKEN_NOT_EQUAL, "!=", 1, 1 }, { TOKEN_ASSIGN, "=", 1, 3 } },
        { "<==", { TOKEN_LESS_EQUAL, "<=", 1, 1 }, { TOKEN_ASSIGN, "=", 1, 3 } },
        { ">==", { TOKEN_GREATER_EQUAL, ">=", 1, 1 }, { TOKEN_ASSIGN, "=", 1, 3 } },
        { "=!=", { TOKEN_ASSIGN, "=", 1, 1 }, { TOKEN_NOT_EQUAL, "!=", 1, 2 } },
        { "=<", { TOKEN_ASSIGN, "=", 1, 1 }, { TOKEN_LESS, "<", 1, 2 } },
        { "=>", { TOKEN_ASSIGN, "=", 1, 1 }, { TOKEN_GREATER, ">", 1, 2 } },
        { "<>", { TOKEN_LESS, "<", 1, 1 }, { TOKEN_GREATER, ">", 1, 2 } },
        { "--", { TOKEN_MINUS, "-", 1, 1 }, { TOKEN_MINUS, "-", 1, 2 } },
        { "*/", { TOKEN_STAR, "*", 1, 1 }, { TOKEN_SLASH, "/", 1, 2 } }
    };
    for (size_t i = 0; i < sizeof split / sizeof split[0]; ++i) {
        const expected_t expected[] = { split[i].first, split[i].second };
        sequence(split[i].text, expected, 2);
    }
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt (summary): "Write lexer tests for the BOOLTHEN and CONTROL tokens
//          from my updated diagram: ! && || <- alone and in context, splits
//          like !== <-- &&& ||| <=-, x<-1 as one arrow, lone & and | as
//          error 1, Boolean/true/false/for keywords, to/until/by as
//          identifiers, and VALMEM member chains after numbers."
// Scope: extension_tokens.

static void extension_tokens(void)
{
    const struct {
        const char* text;
        token_type_t type;
    } single[] = {
        { "!", TOKEN_NOT }, { "&&", TOKEN_AND }, { "||", TOKEN_OR },
        { "<-", TOKEN_LARROW }, { "true", TOKEN_TRUE }, { "false", TOKEN_FALSE },
        { "Boolean", TOKEN_TYPE_BOOLEAN }, { "for", TOKEN_FOR },
        { "to", TOKEN_IDENTIFIER }, { "until", TOKEN_IDENTIFIER },
        { "by", TOKEN_IDENTIFIER }, { "readBoolean", TOKEN_IDENTIFIER },
        { "trueish", TOKEN_IDENTIFIER }, { "False", TOKEN_IDENTIFIER },
        { "fort", TOKEN_IDENTIFIER }
    };
    for (size_t i = 0; i < sizeof single / sizeof single[0]; ++i) {
        const expected_t expected[] = {
            { single[i].type, single[i].text, 1, 1 },
            { TOKEN_EOF, NULL, 1, strlen(single[i].text) + 1 }
        };
        sequence(single[i].text, expected, 2);
    }

    /* Longest match: each operator takes at most its two characters. */
    const struct {
        const char* text;
        expected_t first, second;
    } split[] = {
        { "!!", { TOKEN_NOT, "!", 1, 1 }, { TOKEN_NOT, "!", 1, 2 } },
        { "!a", { TOKEN_NOT, "!", 1, 1 }, { TOKEN_IDENTIFIER, "a", 1, 2 } },
        { "a!", { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_NOT, "!", 1, 2 } },
        { "! =", { TOKEN_NOT, "!", 1, 1 }, { TOKEN_ASSIGN, "=", 1, 3 } },
        { "=!", { TOKEN_ASSIGN, "=", 1, 1 }, { TOKEN_NOT, "!", 1, 2 } },
        { "!(", { TOKEN_NOT, "!", 1, 1 }, { TOKEN_LPAREN, "(", 1, 2 } },
        { "<--", { TOKEN_LARROW, "<-", 1, 1 }, { TOKEN_MINUS, "-", 1, 3 } },
        { "<-=", { TOKEN_LARROW, "<-", 1, 1 }, { TOKEN_ASSIGN, "=", 1, 3 } },
        { "<=-", { TOKEN_LESS_EQUAL, "<=", 1, 1 }, { TOKEN_MINUS, "-", 1, 3 } },
        { "< -", { TOKEN_LESS, "<", 1, 1 }, { TOKEN_MINUS, "-", 1, 3 } },
        { "-<", { TOKEN_MINUS, "-", 1, 1 }, { TOKEN_LESS, "<", 1, 2 } },
        { "&&!", { TOKEN_AND, "&&", 1, 1 }, { TOKEN_NOT, "!", 1, 3 } },
        { "||!", { TOKEN_OR, "||", 1, 1 }, { TOKEN_NOT, "!", 1, 3 } },
        { "&&||", { TOKEN_AND, "&&", 1, 1 }, { TOKEN_OR, "||", 1, 3 } }
    };
    for (size_t i = 0; i < sizeof split / sizeof split[0]; ++i) {
        const expected_t expected[] = { split[i].first, split[i].second };
        sequence(split[i].text, expected, 2);
    }
    const expected_t not_newline[] = {
        { TOKEN_NOT, "!", 1, 1 }, { TOKEN_NEWLINE, "\n", 1, 2 },
        { TOKEN_ASSIGN, "=", 2, 1 }
    };
    sequence("!\n=", not_newline, 3);

    const expected_t condition[] = {
        { TOKEN_IF, "if", 1, 1 }, { TOKEN_NOT, "!", 1, 4 },
        { TOKEN_IDENTIFIER, "a", 1, 5 }, { TOKEN_AND, "&&", 1, 7 },
        { TOKEN_IDENTIFIER, "b", 1, 10 }, { TOKEN_OR, "||", 1, 12 },
        { TOKEN_IDENTIFIER, "x", 1, 15 }, { TOKEN_NOT_EQUAL, "!=", 1, 16 },
        { TOKEN_TRUE, "true", 1, 18 }, { TOKEN_THEN, "then", 1, 23 },
        { TOKEN_EOF, NULL, 1, 27 }
    };
    sequence("if !a && b || x!=true then", condition,
        sizeof condition / sizeof condition[0]);
    const expected_t declaration[] = {
        { TOKEN_VAL, "val", 1, 1 }, { TOKEN_IDENTIFIER, "b", 1, 5 },
        { TOKEN_COLON, ":", 1, 6 }, { TOKEN_TYPE_BOOLEAN, "Boolean", 1, 8 },
        { TOKEN_ASSIGN, "=", 1, 16 }, { TOKEN_FALSE, "false", 1, 18 },
        { TOKEN_EOF, NULL, 1, 23 }
    };
    sequence("val b: Boolean = false", declaration,
        sizeof declaration / sizeof declaration[0]);

    /* CONTROL: the generator arrow is one token even without spaces. */
    const expected_t generator[] = {
        { TOKEN_FOR, "for", 1, 1 }, { TOKEN_IDENTIFIER, "i", 1, 5 },
        { TOKEN_LARROW, "<-", 1, 7 }, { TOKEN_INTEGER, "1", 1, 10 },
        { TOKEN_IDENTIFIER, "to", 1, 12 }, { TOKEN_INTEGER, "10", 1, 15 },
        { TOKEN_IDENTIFIER, "by", 1, 18 }, { TOKEN_INTEGER, "2", 1, 21 },
        { TOKEN_DO, "do", 1, 23 }, { TOKEN_EOF, NULL, 1, 25 }
    };
    sequence("for i <- 1 to 10 by 2 do", generator,
        sizeof generator / sizeof generator[0]);
    const expected_t tight[] = {
        { TOKEN_IDENTIFIER, "x", 1, 1 }, { TOKEN_LARROW, "<-", 1, 2 },
        { TOKEN_INTEGER, "1", 1, 4 }, { TOKEN_EOF, NULL, 1, 5 }
    };
    sequence("x<-1", tight, sizeof tight / sizeof tight[0]);
    const expected_t spaced[] = {
        { TOKEN_IDENTIFIER, "x", 1, 1 }, { TOKEN_LESS, "<", 1, 3 },
        { TOKEN_MINUS, "-", 1, 5 }, { TOKEN_INTEGER, "1", 1, 6 },
        { TOKEN_EOF, NULL, 1, 7 }
    };
    sequence("x < -1", spaced, sizeof spaced / sizeof spaced[0]);
    const expected_t multiline_for[] = {
        { TOKEN_FOR, "for", 1, 1 }, { TOKEN_NEWLINE, "\n", 1, 4 },
        { TOKEN_IDENTIFIER, "i", 2, 5 }, { TOKEN_LARROW, "<-", 2, 7 },
        { TOKEN_INTEGER, "0", 2, 10 }, { TOKEN_IDENTIFIER, "until", 2, 12 },
        { TOKEN_IDENTIFIER, "n", 2, 18 }, { TOKEN_NEWLINE, "\n", 2, 19 },
        { TOKEN_IF, "if", 3, 5 }, { TOKEN_IDENTIFIER, "i", 3, 8 },
        { TOKEN_NOT_EQUAL, "!=", 3, 10 }, { TOKEN_INTEGER, "3", 3, 13 },
        { TOKEN_NEWLINE, "\n", 3, 14 }, { TOKEN_DO, "do", 4, 1 },
        { TOKEN_EOF, NULL, 4, 3 }
    };
    sequence("for\n    i <- 0 until n\n    if i != 3\ndo", multiline_for,
        sizeof multiline_for / sizeof multiline_for[0]);

    /* VALMEM needs no new token: members follow numbers and strings. */
    const expected_t members[] = {
        { TOKEN_INTEGER, "5", 1, 1 }, { TOKEN_DOT, ".", 1, 2 },
        { TOKEN_IDENTIFIER, "toString", 1, 3 }, { TOKEN_DOUBLE, "1.5", 1, 12 },
        { TOKEN_DOT, ".", 1, 15 }, { TOKEN_IDENTIFIER, "floor", 1, 16 },
        { TOKEN_DOT, ".", 1, 21 }, { TOKEN_IDENTIFIER, "toInt", 1, 22 },
        { TOKEN_STRING, "\"abc\"", 1, 28 }, { TOKEN_DOT, ".", 1, 33 },
        { TOKEN_IDENTIFIER, "length", 1, 34 }, { TOKEN_DOUBLE, "1.0d", 1, 41 },
        { TOKEN_DOT, ".", 1, 45 }, { TOKEN_IDENTIFIER, "toInt", 1, 46 },
        { TOKEN_EOF, NULL, 1, 51 }
    };
    sequence("5.toString 1.5.floor.toInt \"abc\".length 1.0d.toInt", members,
        sizeof members / sizeof members[0]);

    /* A lone & is not part of IFJ26 (a lone | is f32, see unused_extension_tokens). */
    const char* bad[] = { "&", "& &", "a & b", "&|", "|&", "&&&", "&\n&", "&=" };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i)
        expect_error(bad[i]);
}

static void identifier_boundaries(void)
{
    /* Keywords match only whole identifiers (section 1.1). */
    const char* longer[] = {
        "define", "done", "Doubles", "elsewhere", "iff", "imports", "Integer",
        "returned", "Stringy", "thenx", "Units", "value", "vars", "whileTrue",
        "Booleans", "cases", "classy", "extensions", "form", "Lists", "matches",
        "types", "de", "Do", "els", "whil", "if_", "_val", "Int2", "__", "_1", "a_"
    };
    for (size_t i = 0; i < sizeof longer / sizeof longer[0]; ++i) {
        const expected_t expected[] = { { TOKEN_IDENTIFIER, longer[i], 1, 1 } };
        sequence(longer[i], expected, 1);
    }

    const expected_t call[] = {
        { TOKEN_IF, "if", 1, 1 }, { TOKEN_LPAREN, "(", 1, 3 },
        { TOKEN_IDENTIFIER, "x", 1, 4 }, { TOKEN_RPAREN, ")", 1, 5 },
        { TOKEN_EOF, NULL, 1, 6 }
    };
    sequence("if(x)", call, sizeof call / sizeof call[0]);
    const expected_t assignment[] = {
        { TOKEN_VAL, "val", 1, 1 }, { TOKEN_IDENTIFIER, "x", 1, 5 },
        { TOKEN_ASSIGN, "=", 1, 6 }, { TOKEN_IDENTIFIER, "y", 1, 7 },
        { TOKEN_PLUS, "+", 1, 8 }, { TOKEN_IDENTIFIER, "z1", 1, 9 },
        { TOKEN_EOF, NULL, 1, 11 }
    };
    sequence("val x=y+z1", assignment, sizeof assignment / sizeof assignment[0]);
    const expected_t mixed[] = {
        { TOKEN_IDENTIFIER, "while_x", 1, 1 }, { TOKEN_LESS_EQUAL, "<=", 1, 8 },
        { TOKEN_TYPE_INT, "Int", 1, 10 }, { TOKEN_DOT, ".", 1, 13 },
        { TOKEN_IDENTIFIER, "s", 1, 14 }, { TOKEN_STRING, "\"t\"", 1, 15 },
        { TOKEN_ELSE, "else", 1, 18 }, { TOKEN_SEMICOLON, ";", 1, 22 },
        { TOKEN_THEN, "then", 1, 23 }, { TOKEN_EOF, NULL, 1, 27 }
    };
    sequence("while_x<=Int.s\"t\"else;then", mixed, sizeof mixed / sizeof mixed[0]);
    const expected_t underscores[] = {
        { TOKEN_IDENTIFIER, "__", 1, 1 }, { TOKEN_IDENTIFIER, "_1", 1, 4 },
        { TOKEN_IDENTIFIER, "a_", 1, 7 }
    };
    sequence("__ _1 a_", underscores, sizeof underscores / sizeof underscores[0]);

    /* A lone underscore is TOKEN_UNDERSCORE (f31), never an identifier;
     * cases are in unused_extension_tokens. */
}

static void comment_layout(void)
{
    /* A // comment at EOF ends a non-empty logical line outside parentheses. */
    const expected_t tight[] = {
        { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_NEWLINE, "\n", 1, 7 }
    };
    sequence("a//eof", tight, 2);
    const expected_t after_line[] = {
        { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_NEWLINE, "\n", 1, 2 },
        { TOKEN_EOF, NULL, 2, 7 }
    };
    sequence("a\n// eof", after_line, 3);
    const expected_t after_block[] = {
        { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_NEWLINE, "\n", 1, 17 }
    };
    sequence("a /* x */ // eof", after_block, 2);
    const expected_t not_nested[] = {
        { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_NEWLINE, "\n", 1, 21 }
    };
    sequence("a // x /* not nested", not_nested, 2);
    const expected_t inside_block[] = {
        { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_IDENTIFIER, "b", 1, 12 }
    };
    sequence("a /* // */ b", inside_block, 2);
    const expected_t open_paren[] = {
        { TOKEN_IDENTIFIER, "f", 1, 1 }, { TOKEN_LPAREN, "(", 1, 2 },
        { TOKEN_IDENTIFIER, "x", 1, 3 }, { TOKEN_EOF, NULL, 1, 11 }
    };
    sequence("f(x // eof", open_paren, 4);

    /* Comment-only lines are ignored; continuation lines inside parentheses
     * must be indented more than the start of the logical line (1.2). */
    const char* text = "f(a, // tail\n  /* gap */ b\n// ignored\n  )\n"
                       "  y = 1 /* multi\n*/\nz // end";
    const expected_t expected[] = {
        { TOKEN_IDENTIFIER, "f", 1, 1 }, { TOKEN_LPAREN, "(", 1, 2 },
        { TOKEN_IDENTIFIER, "a", 1, 3 }, { TOKEN_COMMA, ",", 1, 4 },
        { TOKEN_IDENTIFIER, "b", 2, 13 }, { TOKEN_RPAREN, ")", 4, 3 },
        { TOKEN_NEWLINE, "\n", 4, 4 }, { TOKEN_IDENTIFIER, "y", 5, 3 },
        { TOKEN_ASSIGN, "=", 5, 5 }, { TOKEN_INTEGER, "1", 5, 7 },
        { TOKEN_NEWLINE, "\n", 5, 17 }, { TOKEN_IDENTIFIER, "z", 7, 1 },
        { TOKEN_NEWLINE, "\n", 7, 9 }, { TOKEN_EOF, NULL, 7, 9 }
    };
    const size_t count = sizeof expected / sizeof expected[0];
    sequence(text, expected, count);
    const size_t indentation[] = { 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 0, 0, 0 };
    const int starts[] = { 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0 };
    FILE* input = source(text);
    lexer_t lexer;
    token_t token = { 0 };
    assert(lexer_init(&lexer, input) == ERR_OK);
    for (size_t i = 0; i < count; ++i) {
        assert(lexer_next(&lexer, &token) == ERR_OK);
        assert(token.indentation == indentation[i]);
        assert(token.starts_line == starts[i]);
        token_dispose(&token);
    }
    fclose(input);

    const expected_t continued[] = {
        { TOKEN_IDENTIFIER, "f", 1, 3 }, { TOKEN_LPAREN, "(", 1, 4 },
        { TOKEN_IDENTIFIER, "x", 2, 4 }, { TOKEN_RPAREN, ")", 2, 5 },
        { TOKEN_EOF, NULL, 2, 6 }
    };
    sequence("  f( // c\n   x)", continued, sizeof continued / sizeof continued[0]);
    const char* bad[] = { "f(\n// ok\nx)", "  f( // c\n  x)", "  f(\n  /* gap */ x)" };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i)
        expect_error(bad[i]);
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt (summary): "Add regression tests for block comments after
//          a multiline block comment: both reported inputs, repeated and
//          nested comments, EOF, unterminated comments and real tokens."
// Scope: multiline_comment_tail_layout.

/* Spec 1.1: a block comment without LF is whitespace, so it may follow
 * a multiline comment; a trailing comment with LF is another line end. */
static void multiline_comment_tail_layout(void)
{
    const expected_t plain[] = {
        { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_NEWLINE, "\n", 1, 5 },
        { TOKEN_IDENTIFIER, "b", 3, 1 }, { TOKEN_EOF, NULL, 3, 2 }
    };
    sequence("a /*\n*/ /* x */\nb", plain, sizeof plain / sizeof plain[0]);
    const expected_t paren[] = {
        { TOKEN_IDENTIFIER, "f", 1, 1 }, { TOKEN_LPAREN, "(", 1, 2 },
        { TOKEN_IDENTIFIER, "y", 3, 3 }, { TOKEN_RPAREN, ")", 3, 4 },
        { TOKEN_EOF, NULL, 3, 5 }
    };
    sequence("f( /*\n*/ /* x */\n  y)", paren, sizeof paren / sizeof paren[0]);
    sequence("a /*\n*/ /* x */  /* y */\t/* /* z */ */\nb", plain,
        sizeof plain / sizeof plain[0]);
    const expected_t at_eof[] = {
        { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_NEWLINE, "\n", 1, 5 },
        { TOKEN_EOF, NULL, 2, 11 }
    };
    sequence("a /*\n*/ /* x */", at_eof, sizeof at_eof / sizeof at_eof[0]);

    /* A trailing comment with LF adds no second NEWLINE. */
    const expected_t two_lines[] = {
        { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_NEWLINE, "\n", 1, 5 },
        { TOKEN_IDENTIFIER, "b", 4, 1 }, { TOKEN_EOF, NULL, 4, 2 }
    };
    sequence("a /*\n*/ /* y\n*/\nb", two_lines, sizeof two_lines / sizeof two_lines[0]);
    const expected_t paren_two_lines[] = {
        { TOKEN_IDENTIFIER, "f", 1, 1 }, { TOKEN_LPAREN, "(", 1, 2 },
        { TOKEN_IDENTIFIER, "z", 4, 3 }, { TOKEN_RPAREN, ")", 4, 4 },
        { TOKEN_EOF, NULL, 4, 5 }
    };
    sequence("f( /*\n*/ /* y\n*/\n  z)", paren_two_lines,
        sizeof paren_two_lines / sizeof paren_two_lines[0]);

    const char* text = "  a /*\n*/ /* x */\n  b";
    const expected_t indented[] = {
        { TOKEN_IDENTIFIER, "a", 1, 3 }, { TOKEN_NEWLINE, "\n", 1, 7 },
        { TOKEN_IDENTIFIER, "b", 3, 3 }, { TOKEN_EOF, NULL, 3, 4 }
    };
    const size_t count = sizeof indented / sizeof indented[0];
    sequence(text, indented, count);
    const size_t indentation[] = { 2, 2, 2, 0 };
    const int starts[] = { 1, 0, 1, 0 };
    FILE* input = source(text);
    lexer_t lexer;
    token_t token = { 0 };
    assert(lexer_init(&lexer, input) == ERR_OK);
    for (size_t i = 0; i < count; ++i) {
        assert(lexer_next(&lexer, &token) == ERR_OK);
        assert(token.indentation == indentation[i]);
        assert(token.starts_line == starts[i]);
        token_dispose(&token);
    }
    fclose(input);

    const char* bad[] = {
        "a /*\n*/ /* x", "a /*\n*/ /* /* x */", "a /*\n*/ /* y\n*/ /* z",
        "a /*\n*/ /* x */ b", "a /*\n*/ /* y\n*/ b", "f( /*\n*/ /* x */ y)"
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i)
        expect_error(bad[i]);
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt (summary): "Add focused regression tests for the enum/switch lexer
//          automaton: diagram token boundaries (1., 1._5, .5d, 1.2.3,
//          raw-string quotes, @main suffixes, f26 slash versus comments,
//          non-overlapping comment delimiters) and the settled layout
//          decisions: a tab before a comment-only line is ignored, // ends
//          the line after a multiline block comment, comments accept any
//          byte."
// Scope: automaton_boundaries, layout_decisions.

typedef struct {
    const char* text;
    expected_t tokens[3];
    size_t count;
} case_t;

static void automaton_boundaries(void)
{
    const case_t cases[] = {
        /* f18 enters q8 only with a digit lookahead (footnote 3). */
        { "1.", { { TOKEN_INTEGER, "1", 1, 1 }, { TOKEN_DOT, ".", 1, 2 } }, 2 },
        { "1._5", { { TOKEN_INTEGER, "1", 1, 1 }, { TOKEN_DOT, ".", 1, 2 }, { TOKEN_IDENTIFIER, "_5", 1, 3 } }, 3 },
        { "1.e5", { { TOKEN_INTEGER, "1", 1, 1 }, { TOKEN_DOT, ".", 1, 2 }, { TOKEN_IDENTIFIER, "e5", 1, 3 } }, 3 },
        { "._5", { { TOKEN_DOT, ".", 1, 1 }, { TOKEN_IDENTIFIER, "_5", 1, 2 } }, 2 },
        /* f19 has no '.' transition; f22 -0..9-> f19 starts the next Double. */
        { "1.2.3", { { TOKEN_DOUBLE, "1.2", 1, 1 }, { TOKEN_DOUBLE, ".3", 1, 4 } }, 2 },
        { ".5d", { { TOKEN_DOUBLE, ".5d", 1, 1 } }, 1 },
        { "01e5", { { TOKEN_DOUBLE, "01e5", 1, 1 } }, 1 },
        /* f23 needs a third quote for q16; f25 loops on '"' (code review). */
        { "\"\"x", { { TOKEN_STRING, "\"\"", 1, 1 }, { TOKEN_IDENTIFIER, "x", 1, 3 } }, 2 },
        { "\"\"\"\"\"x\"\"\"", { { TOKEN_STRING, "\"\"\"\"\"x\"\"\"", 1, 1 } }, 1 },
        { "\"\"\"\"\"\"\"\"", { { TOKEN_STRING, "\"\"\"\"\"\"\"\"", 1, 1 } }, 1 },
        { "\"a\rb\"", { { TOKEN_STRING, "\"a\rb\"", 1, 1 } }, 1 },
        { "@main(", { { TOKEN_MAIN, "@main", 1, 1 }, { TOKEN_LPAREN, "(", 1, 6 } }, 2 },
        /* f26 is TOKEN_SLASH unless / or * follows. */
        { "a/b", { { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_SLASH, "/", 1, 2 }, { TOKEN_IDENTIFIER, "b", 1, 3 } }, 3 },
        { "/ /", { { TOKEN_SLASH, "/", 1, 1 }, { TOKEN_SLASH, "/", 1, 3 } }, 2 },
        { "a/**/b", { { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_IDENTIFIER, "b", 1, 6 } }, 2 },
        /* Comment delimiters do not overlap: the star of the opening pair
         * cannot also start the closing pair. */
        { "x/*/ */y", { { TOKEN_IDENTIFIER, "x", 1, 1 }, { TOKEN_IDENTIFIER, "y", 1, 8 } }, 2 },
        { "/* */*/", { { TOKEN_STAR, "*", 1, 6 }, { TOKEN_SLASH, "/", 1, 7 } }, 2 }
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i)
        sequence(cases[i].text, cases[i].tokens, cases[i].count);

    const char* bad[] = {
        ".5_", "01.", "01.x", "1e2_d", "1_.5", "\"\"\x80", "@main1", "@main_",
        "@mai", "@ma in", "/*/", "/* /* */", "&x"
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i)
        expect_error(bad[i]);
}

static void layout_decisions(void)
{
    /* A leading tab is an error only on a line with a real token. */
    const case_t tabs[] = {
        { "\t// c\nx", { { TOKEN_IDENTIFIER, "x", 2, 1 } }, 1 },
        { "\t/* c */\nx", { { TOKEN_IDENTIFIER, "x", 2, 1 } }, 1 },
        { "a\n\t// c\nb", { { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_NEWLINE, "\n", 1, 2 }, { TOKEN_IDENTIFIER, "b", 3, 1 } }, 3 },
        { "x\n\t/* c\n*/\ny", { { TOKEN_IDENTIFIER, "x", 1, 1 }, { TOKEN_NEWLINE, "\n", 1, 2 }, { TOKEN_IDENTIFIER, "y", 4, 1 } }, 3 },
        { "/* c */\tx", { { TOKEN_IDENTIFIER, "x", 1, 9 } }, 1 }
    };
    for (size_t i = 0; i < sizeof tabs / sizeof tabs[0]; ++i)
        sequence(tabs[i].text, tabs[i].tokens, tabs[i].count);
    const expected_t paren[] = {
        { TOKEN_IDENTIFIER, "f", 1, 1 }, { TOKEN_LPAREN, "(", 1, 2 },
        { TOKEN_IDENTIFIER, "x", 3, 3 }, { TOKEN_RPAREN, ")", 3, 4 }
    };
    sequence("f(\n\t// c\n  x)", paren, sizeof paren / sizeof paren[0]);

    /* After a multiline block comment, // is the expected line end itself. */
    const case_t tails[] = {
        { "a /*\n*/ // c\nb", { { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_NEWLINE, "\n", 1, 5 }, { TOKEN_IDENTIFIER, "b", 3, 1 } }, 3 },
        { "a /*\n*/ // c", { { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_NEWLINE, "\n", 1, 5 }, { TOKEN_EOF, NULL, 2, 8 } }, 3 },
        { "a /*\n*/ /* x */ // c\nb", { { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_NEWLINE, "\n", 1, 5 }, { TOKEN_IDENTIFIER, "b", 3, 1 } }, 3 },
        { "a /*\n*///\nb", { { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_NEWLINE, "\n", 1, 5 }, { TOKEN_IDENTIFIER, "b", 3, 1 } }, 3 }
    };
    for (size_t i = 0; i < sizeof tails / sizeof tails[0]; ++i)
        sequence(tails[i].text, tails[i].tokens, tails[i].count);
    const expected_t paren_tail[] = {
        { TOKEN_IDENTIFIER, "f", 1, 1 }, { TOKEN_LPAREN, "(", 1, 2 },
        { TOKEN_IDENTIFIER, "y", 3, 3 }, { TOKEN_RPAREN, ")", 3, 4 }
    };
    sequence("f( /*\n*/ // c\n  y)", paren_tail, sizeof paren_tail / sizeof paren_tail[0]);

    /* Comments accept any byte; only string and token content is ASCII. */
    const case_t bytes[] = {
        { "// \xc4\x8d\nx", { { TOKEN_IDENTIFIER, "x", 2, 1 } }, 1 },
        { "a // \x80", { { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_NEWLINE, "\n", 1, 7 } }, 2 },
        { "/* \x80 */ x", { { TOKEN_IDENTIFIER, "x", 1, 9 } }, 1 }
    };
    for (size_t i = 0; i < sizeof bytes / sizeof bytes[0]; ++i)
        sequence(bytes[i].text, bytes[i].tokens, bytes[i].count);

    const char* bad[] = {
        "\t/* c */ x", "\t/* c\n*/ x", "a /*\n*/ / b", "a /*\n*/ /", "f(\n\t/* c */ x)",
        "f(\n// c\n\tx)"
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i)
        expect_error(bad[i]);
}

#ifdef LEXER_TEST_ALLOC_FAILURES
static void allocation_failures(void)
{
    /* Include string growth, numeric normalization, both comment kinds,
     * and allocation of logical newline tokens in a single source. */
    const char* text = "f(\n  \"abcdefghijklmnopqrstuvwxyz0123456789\", 1.25)\n"
                       "x // tail\ny /*\n*/";
    size_t failures = 0;
    for (size_t fail = 1; fail < 100; ++fail) {
        FILE* input = source(text);
        lexer_t lexer;
        token_t token = { 0 };
        assert(lexer_init(&lexer, input) == ERR_OK);
        allocation_calls = 0;
        fail_allocation = fail;
        lexer_status_t status;
        do {
            status = lexer_next(&lexer, &token);
            int end = status != ERR_OK || token.type == TOKEN_EOF;
            token_dispose(&token);
            if (end)
                break;
        } while (1);
        fail_allocation = 0;
        if (status == ERR_OK) {
            assert(allocation_calls < fail);
            fclose(input);
            break;
        }
        assert(status == ERR_INTERNAL);
        assert(lexer_next(&lexer, &token) == ERR_INTERNAL);
        assert(token.lexeme == NULL);
        ++failures;
        fclose(input);
    }
    assert(failures >= 8 && failures < 99);
    printf("Allocation failure cases passed: %zu\n", failures);
}
#endif

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt (summary): "Add regression tests for the raw string review fix:
//          extra closing quotes are content and the last three close the
//          literal; too few quotes stay an unterminated string."
// Scope: raw_string_quotes().
/* Lexemes keep the original spelling, so the value is the text without the
 * first three and the last three quotes. */
static void raw_string_quotes(void)
{
    const char* valid[] = {
        "\"\"\"say \"hi\"\"\"\"", /* value: say "hi" */
        "\"\"\"\"\"\"\"\"", /* value: "" */
        "\"\"\"x\"\"\"\"\"\"\"", /* value: x"""" */
        "\"\"\"\"\"\"" /* value: empty */
    };
    for (size_t i = 0; i < sizeof valid / sizeof valid[0]; ++i) {
        const expected_t expected[] = { { TOKEN_STRING, valid[i], 1, 1 } };
        sequence(valid[i], expected, 1);
    }
    const expected_t followed[] = {
        { TOKEN_STRING, "\"\"\"a\"\"\"\"", 1, 1 }, { TOKEN_IDENTIFIER, "b", 1, 10 },
        { TOKEN_STRING, "\"c\"", 1, 12 }
    };
    sequence("\"\"\"a\"\"\"\" b \"c\"", followed, 3);
    const char* bad[] = { "\"\"\"\"\"", "\"\"\"say \"hi\"\"" };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i)
        expect_error(bad[i]);
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt (summary): "Test my updated lexer diagram: lone _ is f31
//          TOKEN_UNDERSCORE, lone | is f32 TOKEN_PIPE (|| stays TOKEN_OR),
//          [ ] { } are f33..f36 alone and in context; _x and __ stay
//          identifiers; drop these inputs from the error-1 lists."
// Scope: unused_extension_tokens, the edited bad[] lists in errors,
//        automaton_boundaries, extension_tokens and identifier_boundaries.

typedef struct {
    const char* text;
    expected_t tokens[4];
    size_t count;
} extension_case_t;

static void unused_extension_tokens(void)
{
    const extension_case_t cases[] = {
        /* f31: a lone underscore, f31 -name char-> f1 keeps identifiers. */
        { "_", { { TOKEN_UNDERSCORE, "_", 1, 1 }, { TOKEN_EOF, NULL, 1, 2 } }, 2 },
        { "val _ = 1", { { TOKEN_VAL, "val", 1, 1 }, { TOKEN_UNDERSCORE, "_", 1, 5 }, { TOKEN_ASSIGN, "=", 1, 7 }, { TOKEN_INTEGER, "1", 1, 9 } }, 4 },
        { "(_)", { { TOKEN_LPAREN, "(", 1, 1 }, { TOKEN_UNDERSCORE, "_", 1, 2 }, { TOKEN_RPAREN, ")", 1, 3 } }, 3 },
        { "_+1", { { TOKEN_UNDERSCORE, "_", 1, 1 }, { TOKEN_PLUS, "+", 1, 2 }, { TOKEN_INTEGER, "1", 1, 3 } }, 3 },
        { "._", { { TOKEN_DOT, ".", 1, 1 }, { TOKEN_UNDERSCORE, "_", 1, 2 } }, 2 },
        { "_x _ __", { { TOKEN_IDENTIFIER, "_x", 1, 1 }, { TOKEN_UNDERSCORE, "_", 1, 4 }, { TOKEN_IDENTIFIER, "__", 1, 6 } }, 3 },
        { "a\n_", { { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_NEWLINE, "\n", 1, 2 }, { TOKEN_UNDERSCORE, "_", 2, 1 } }, 3 },
        /* f32: a lone pipe, f32 -|-> f29. */
        { "|", { { TOKEN_PIPE, "|", 1, 1 }, { TOKEN_EOF, NULL, 1, 2 } }, 2 },
        { "Int | Unit", { { TOKEN_TYPE_INT, "Int", 1, 1 }, { TOKEN_PIPE, "|", 1, 5 }, { TOKEN_TYPE_UNIT, "Unit", 1, 7 } }, 3 },
        { "a|b", { { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_PIPE, "|", 1, 2 }, { TOKEN_IDENTIFIER, "b", 1, 3 } }, 3 },
        { "|||", { { TOKEN_OR, "||", 1, 1 }, { TOKEN_PIPE, "|", 1, 3 } }, 2 },
        { "| |", { { TOKEN_PIPE, "|", 1, 1 }, { TOKEN_PIPE, "|", 1, 3 } }, 2 },
        { "|=", { { TOKEN_PIPE, "|", 1, 1 }, { TOKEN_ASSIGN, "=", 1, 2 } }, 2 },
        /* f33..f36 have no outgoing transition. */
        { "List[Int]", { { TOKEN_RESERVED_KEYWORD, "List", 1, 1 }, { TOKEN_LBRACKET, "[", 1, 5 }, { TOKEN_TYPE_INT, "Int", 1, 6 }, { TOKEN_RBRACKET, "]", 1, 9 } }, 4 },
        { "a[1]", { { TOKEN_IDENTIFIER, "a", 1, 1 }, { TOKEN_LBRACKET, "[", 1, 2 }, { TOKEN_INTEGER, "1", 1, 3 }, { TOKEN_RBRACKET, "]", 1, 4 } }, 4 },
        { "{ x }", { { TOKEN_LBRACE, "{", 1, 1 }, { TOKEN_IDENTIFIER, "x", 1, 3 }, { TOKEN_RBRACE, "}", 1, 5 } }, 3 },
        { "{}[]", { { TOKEN_LBRACE, "{", 1, 1 }, { TOKEN_RBRACE, "}", 1, 2 }, { TOKEN_LBRACKET, "[", 1, 3 }, { TOKEN_RBRACKET, "]", 1, 4 } }, 4 },
        { "[[", { { TOKEN_LBRACKET, "[", 1, 1 }, { TOKEN_LBRACKET, "[", 1, 2 }, { TOKEN_EOF, NULL, 1, 3 } }, 3 }
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i)
        sequence(cases[i].text, cases[i].tokens, cases[i].count);
}

int main(void)
{
    basic_program();
    operators();
    words();
    numbers();
    number_boundaries();
    errors();
    positions_and_ownership();
    string_literals();
    comments();
    layout_and_literal_errors();
    logical_lines();
    indentation_metadata();
    full_program();
    independent_lexers();
    operator_boundaries();
    identifier_boundaries();
    comment_layout();
    multiline_comment_tail_layout();
    automaton_boundaries();
    layout_decisions();
    raw_string_quotes();
    extension_tokens();
    unused_extension_tokens();
#ifdef LEXER_TEST_ALLOC_FAILURES
    allocation_failures();
#endif
    puts("Lexer: all 23 test groups passed.");
    return 0;
}
