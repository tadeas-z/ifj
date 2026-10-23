// Implementation of the IFJ26 imperative language compiler
// test_lexer.c
// Author(s): Tadeáš Zonyga (xzonygt00)

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
    assert(lexer_init(&lexer, input) == LEXER_OK);
    for (size_t i = 0; i < count; ++i) {
        assert(lexer_next(&lexer, &token) == LEXER_OK);
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
    assert(lexer_next(&lexer, &token) == LEXER_OK);
    assert(token.type == TOKEN_EOF);
    size_t line = token.line, column = token.column;
    assert(lexer_next(&lexer, &token) == LEXER_OK);
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
        TOKEN_RESERVED_KEYWORD, TOKEN_RESERVED_KEYWORD, TOKEN_RESERVED_KEYWORD,
        TOKEN_RESERVED_KEYWORD, TOKEN_RESERVED_KEYWORD, TOKEN_RESERVED_KEYWORD,
        TOKEN_RESERVED_KEYWORD, TOKEN_RESERVED_KEYWORD, TOKEN_IDENTIFIER,
        TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_IDENTIFIER,
        TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_IDENTIFIER
    };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i) {
        const expected_t expected[] = { { types[i], names[i], 1, 1 } };
        sequence(names[i], expected, 1);
    }
}

static void numbers(void)
{
    const char* integers[] = { "0", "00", "0_0", "42", "1_000_000", "2147483648" };
    const char* doubles[] = {
        ".5", "0.5", "01.5", "1.0d", "1.0D", "1e2", "1E-2", "1e+2D",
        "1_000.2_5e1_0d", ".5e-2", "0e0", "1.7976931348623157e308",
        "1e-310", "0e-9999"
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

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt (summary): "Add regression tests for numeric token boundaries,
// including 10abc\"y\", lexeme lengths and positions. Assess unsupported
// numeric suffixes individually and preserve malformed-number checks."
// Scope: number_boundaries(), numeric cases in errors() and test registration.
/* A number ends after its longest valid literal; the next character is not
 * consumed and starts the next token. Unsupported forms (hex, L, f, d on an
 * integer) therefore split into tokens the parser rejects. */
static void number_boundaries(void)
{
    const expected_t regression[] = {
        { TOKEN_INTEGER, "10", 1, 1 }, { TOKEN_IDENTIFIER, "abc", 1, 3 },
        { TOKEN_STRING, "\"y\"", 1, 6 }, { TOKEN_EOF, NULL, 1, 9 }
    };
    sequence("10abc\"y\"", regression, sizeof regression / sizeof regression[0]);

    const struct {
        const char* text;
        expected_t first, second;
    } split[] = {
        { "12abc", { TOKEN_INTEGER, "12", 1, 1 }, { TOKEN_IDENTIFIER, "abc", 1, 3 } },
        { "0x12", { TOKEN_INTEGER, "0", 1, 1 }, { TOKEN_IDENTIFIER, "x12", 1, 2 } },
        { "1L", { TOKEN_INTEGER, "1", 1, 1 }, { TOKEN_IDENTIFIER, "L", 1, 2 } },
        { "1.0f", { TOKEN_DOUBLE, "1.0", 1, 1 }, { TOKEN_IDENTIFIER, "f", 1, 4 } },
        { "1.0Dfoo", { TOKEN_DOUBLE, "1.0D", 1, 1 }, { TOKEN_IDENTIFIER, "foo", 1, 5 } },
        { "1d", { TOKEN_INTEGER, "1", 1, 1 }, { TOKEN_IDENTIFIER, "d", 1, 2 } },
        { "1e2x", { TOKEN_DOUBLE, "1e2", 1, 1 }, { TOKEN_IDENTIFIER, "x", 1, 4 } },
        { "1_0.5_0dx", { TOKEN_DOUBLE, "1_0.5_0d", 1, 1 }, { TOKEN_IDENTIFIER, "x", 1, 9 } }
    };
    for (size_t i = 0; i < sizeof split / sizeof split[0]; ++i) {
        const expected_t expected[] = { split[i].first, split[i].second };
        sequence(split[i].text, expected, 2);
    }
}

static void errors(void)
{
    const char* bad[] = {
        "_", "01", "0_1", "1_", "1__0", "1e", "1e+", "1e_2", "1e2_",
        "1.0_", "1ex", "1e+x", "1_x", "1.5_d", "01abc", "00_1", "1e2_x",
        "@", "@ main", "@other", "@mainly", "!", "&", "|", "%", "{", "}",
        "`name`", "\x80",
        "1e309", "1e-9999"
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        FILE* input = source(bad[i]);
        lexer_t lexer;
        token_t token = { 0 };
        assert(lexer_init(&lexer, input) == LEXER_OK);
        lexer_status_t status;
        do {
            status = lexer_next(&lexer, &token);
            if (status == LEXER_OK)
                assert(token.type != TOKEN_EOF);
            token_dispose(&token);
        } while (status == LEXER_OK);
        assert(status == LEXER_ERROR);
        assert(lexer_next(&lexer, &token) == LEXER_ERROR);
        assert(token.lexeme == NULL);
        fclose(input);
    }
    lexer_t lexer;
    token_t token = { 0 };
    assert(lexer_init(NULL, NULL) == LEXER_INTERNAL_ERROR);
    assert(lexer_init(&lexer, NULL) == LEXER_INTERNAL_ERROR);
    assert(lexer_next(&lexer, &token) == LEXER_INTERNAL_ERROR);
    assert(lexer_next(NULL, &token) == LEXER_INTERNAL_ERROR);
    assert(lexer_next(NULL, NULL) == LEXER_INTERNAL_ERROR);
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
    assert(lexer_init(&lexer, input) == LEXER_OK);
    assert(lexer_next(&lexer, &first) == LEXER_OK);
    assert(lexer_next(&lexer, &second) == LEXER_OK);
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
    assert(lexer_init(&lexer, input) == LEXER_OK);
    lexer_status_t status;
    size_t count = 0;
    do {
        status = lexer_next(&lexer, &token);
        if (status == LEXER_OK && token.type == TOKEN_EOF) {
            fprintf(stderr, "Expected lexical error for: %s\n", text);
            assert(0);
        }
        token_dispose(&token);
        assert(++count <= strlen(text) + 2);
    } while (status == LEXER_OK);
    assert(status == LEXER_ERROR);
    assert(lexer_next(&lexer, &token) == LEXER_ERROR);
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
    assert(lexer_init(&lexer, input) == LEXER_OK);
    for (size_t i = 0; i < sizeof types / sizeof types[0]; ++i) {
        assert(lexer_next(&lexer, &token) == LEXER_OK);
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
    assert(lexer_init(&lexer, input) == LEXER_OK);
    for (size_t i = 0; i < sizeof expected / sizeof expected[0]; ++i) {
        assert(lexer_next(&lexer, &token) == LEXER_OK);
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
    assert(lexer_init(&first, a) == LEXER_OK);
    assert(lexer_init(&second, b) == LEXER_OK);
    assert(lexer_next(&first, &left) == LEXER_OK);
    token_dispose(&left);
    assert(lexer_next(&first, &left) == LEXER_OK);
    assert(left.type == TOKEN_LPAREN);
    token_dispose(&left);
    assert(lexer_next(&second, &right) == LEXER_OK);
    assert(right.starts_line);
    token_dispose(&right);
    assert(lexer_next(&first, &left) == LEXER_OK);
    assert(left.type == TOKEN_STRING && !left.starts_line);
    assert(lexer_next(&second, &right) == LEXER_OK);
    assert(right.type == TOKEN_NEWLINE);
    token_dispose(&right);
    assert(lexer_next(&second, &right) == LEXER_OK);
    assert(right.indentation == 2 && right.starts_line);
    token_dispose(&right);
    assert(strcmp(left.lexeme, "\"value\"") == 0);
    token_dispose(&left);
    fclose(a);
    fclose(b);
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
        assert(lexer_init(&lexer, input) == LEXER_OK);
        allocation_calls = 0;
        fail_allocation = fail;
        lexer_status_t status;
        do {
            status = lexer_next(&lexer, &token);
            int end = status != LEXER_OK || token.type == TOKEN_EOF;
            token_dispose(&token);
            if (end)
                break;
        } while (1);
        fail_allocation = 0;
        if (status == LEXER_OK) {
            assert(allocation_calls < fail);
            fclose(input);
            break;
        }
        assert(status == LEXER_INTERNAL_ERROR);
        assert(lexer_next(&lexer, &token) == LEXER_INTERNAL_ERROR);
        assert(token.lexeme == NULL);
        ++failures;
        fclose(input);
    }
    assert(failures >= 8 && failures < 99);
    printf("Allocation failure cases passed: %zu\n", failures);
}
#endif

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
#ifdef LEXER_TEST_ALLOC_FAILURES
    allocation_failures();
#endif
    puts("Lexer: all 14 test groups passed.");
    return 0;
}
