#include "lexer.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    TokenType type;
    const char *lexeme;
    size_t line, column;
} Expected;

static FILE *source(const char *text)
{
    FILE *input = tmpfile();
    assert(input);
    assert(fwrite(text, 1, strlen(text), input) == strlen(text));
    rewind(input);
    return input;
}

static void sequence(const char *text, const Expected *expected, size_t count)
{
    FILE *input = source(text);
    Lexer lexer;
    Token token = {0};
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
    const Expected expected[] = {
        {TOKEN_IMPORT, "import", 1, 1}, {TOKEN_IDENTIFIER, "ifj26", 1, 8},
        {TOKEN_DOT, ".", 1, 13}, {TOKEN_STAR, "*", 1, 14},
        {TOKEN_NEWLINE, "\n", 1, 15},
        {TOKEN_MAIN, "@main", 2, 1}, {TOKEN_DEF, "def", 2, 7},
        {TOKEN_IDENTIFIER, "run", 2, 11}, {TOKEN_LPAREN, "(", 2, 14},
        {TOKEN_RPAREN, ")", 2, 15}, {TOKEN_COLON, ":", 2, 16},
        {TOKEN_TYPE_UNIT, "Unit", 2, 18}, {TOKEN_ASSIGN, "=", 2, 23},
        {TOKEN_NEWLINE, "\n", 2, 24},
        {TOKEN_VAL, "val", 3, 3}, {TOKEN_IDENTIFIER, "x", 3, 7},
        {TOKEN_ASSIGN, "=", 3, 9}, {TOKEN_MINUS, "-", 3, 11},
        {TOKEN_INTEGER, "1_000", 3, 12}, {TOKEN_PLUS, "+", 3, 18},
        {TOKEN_DOUBLE, ".5e+2D", 3, 20}, {TOKEN_EOF, NULL, 3, 26}
    };
    sequence("import ifj26.*\n@main def run(): Unit =\n  val x = -1_000 + .5e+2D",
             expected, sizeof expected / sizeof expected[0]);
}

static void operators(void)
{
    const Expected expected[] = {
        {TOKEN_LPAREN, "(", 1, 1}, {TOKEN_RPAREN, ")", 1, 2},
        {TOKEN_COLON, ":", 1, 3}, {TOKEN_COMMA, ",", 1, 4},
        {TOKEN_SEMICOLON, ";", 1, 5}, {TOKEN_DOT, ".", 1, 6},
        {TOKEN_PLUS, "+", 1, 7}, {TOKEN_MINUS, "-", 1, 8},
        {TOKEN_STAR, "*", 1, 9}, {TOKEN_SLASH, "/", 1, 10},
        {TOKEN_ASSIGN, "=", 1, 12}, {TOKEN_EQUAL, "==", 1, 14},
        {TOKEN_NOT_EQUAL, "!=", 1, 17}, {TOKEN_LESS, "<", 1, 20},
        {TOKEN_LESS_EQUAL, "<=", 1, 22}, {TOKEN_GREATER, ">", 1, 25},
        {TOKEN_GREATER_EQUAL, ">=", 1, 27}, {TOKEN_EOF, NULL, 1, 29}
    };
    sequence("():,;.+-*/ = == != < <= > >=", expected,
             sizeof expected / sizeof expected[0]);
}

static void words(void)
{
    const char *names[] = {
        "def", "do", "Double", "else", "if", "import", "Int", "return",
        "String", "then", "Unit", "val", "var", "while", "Boolean", "case",
        "class", "extension", "for", "List", "match", "type", "main",
        "IF", "int", "val1", "_name", "a_b2", "readInt"
    };
    const TokenType types[] = {
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
        const Expected expected[] = {{types[i], names[i], 1, 1}};
        sequence(names[i], expected, 1);
    }
}

static void numbers(void)
{
    const char *integers[] = {"0", "00", "0_0", "42", "1_000_000", "2147483648"};
    const char *doubles[] = {
        ".5", "0.5", "01.5", "1.0d", "1.0D", "1e2", "1E-2", "1e+2D",
        "1_000.2_5e1_0d", ".5e-2", "0e0", "1.7976931348623157e308",
        "1e-310", "0e-9999"
    };
    for (size_t i = 0; i < sizeof integers / sizeof integers[0]; ++i) {
        const Expected expected[] = {{TOKEN_INTEGER, integers[i], 1, 1}};
        sequence(integers[i], expected, 1);
    }
    for (size_t i = 0; i < sizeof doubles / sizeof doubles[0]; ++i) {
        const Expected expected[] = {{TOKEN_DOUBLE, doubles[i], 1, 1}};
        sequence(doubles[i], expected, 1);
    }
    const Expected adjacent[] = {
        {TOKEN_INTEGER, "1", 1, 1}, {TOKEN_DOT, ".", 1, 2},
        {TOKEN_IDENTIFIER, "foo", 1, 3}, {TOKEN_MINUS, "-", 1, 6},
        {TOKEN_INTEGER, "2", 1, 7}
    };
    sequence("1.foo-2", adjacent, sizeof adjacent / sizeof adjacent[0]);
}

static void errors(void)
{
    const char *bad[] = {
        "_", "01", "0_1", "1_", "1__0", "1e", "1e+", "1e_2", "1e2_",
        "1.0_", "0x12", "12abc", "1L", "1.0f", "1.0Dfoo", "1d",
        "@", "@ main", "@other", "@mainly", "!", "&", "|", "%", "{", "}",
        "`name`", "\x80", "\"text\"", "// comment", "/* comment */",
        "1e309", "1e-9999"
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        FILE *input = source(bad[i]);
        Lexer lexer;
        Token token = {0};
        assert(lexer_init(&lexer, input) == LEXER_OK);
        LexerStatus status;
        do {
            status = lexer_next(&lexer, &token);
            if (status == LEXER_OK) assert(token.type != TOKEN_EOF);
            token_dispose(&token);
        } while (status == LEXER_OK);
        assert(status == LEXER_ERROR);
        assert(lexer_next(&lexer, &token) == LEXER_ERROR);
        assert(token.lexeme == NULL);
        fclose(input);
    }
    Lexer lexer;
    Token token = {0};
    assert(lexer_init(NULL, NULL) == LEXER_INTERNAL_ERROR);
    assert(lexer_init(&lexer, NULL) == LEXER_INTERNAL_ERROR);
    assert(lexer_next(&lexer, &token) == LEXER_INTERNAL_ERROR);
    assert(lexer_next(NULL, &token) == LEXER_INTERNAL_ERROR);
    assert(lexer_next(NULL, NULL) == LEXER_INTERNAL_ERROR);
}

static void positions_and_ownership(void)
{
    const Expected expected[] = {
        {TOKEN_IDENTIFIER, "a", 1, 3}, {TOKEN_NEWLINE, "\n", 1, 5},
        {TOKEN_NEWLINE, "\n", 2, 1}, {TOKEN_IDENTIFIER, "b", 3, 3},
        {TOKEN_EOF, NULL, 3, 4}
    };
    sequence("  a\r\n\n  b", expected, sizeof expected / sizeof expected[0]);
    sequence("", NULL, 0);
    sequence(" \t\r", NULL, 0);
    FILE *input = source("first second");
    Lexer lexer;
    Token first, second;
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
    const Expected long_expected[] = {{TOKEN_IDENTIFIER, long_name, 1, 1}};
    sequence(long_name, long_expected, 1);
}

int main(void)
{
    basic_program();
    operators();
    words();
    numbers();
    errors();
    positions_and_ownership();
    puts("Lexer: all tests passed.");
    return 0;
}
