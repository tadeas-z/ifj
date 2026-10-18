// Implementation of the IFJ26 imperative language compiler
// lexer.c
// Author(s): Tadeáš Zonyga (xzonygt00)

#include "lexer.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char* data;
    size_t length, capacity;
} buffer_t;

static int digit(int c) { return c >= '0' && c <= '9'; }
static int letter(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static int id_start(int c) { return letter(c) || c == '_'; }
static int id_part(int c) { return id_start(c) || digit(c); }

void token_dispose(token_t* token)
{
    if (token) {
        free(token->lexeme);
        *token = (token_t) { 0 };
    }
}

lexer_status_t lexer_init(lexer_t* lexer, FILE* input)
{
    if (!lexer)
        return LEXER_INTERNAL_ERROR;
    *lexer = (lexer_t) { .input = input, .line = 1, .column = 1 };
    if (!input)
        return lexer->status = LEXER_INTERNAL_ERROR;
    lexer->current = fgetc(input);
    lexer->lookahead = fgetc(input);
    if (ferror(input))
        lexer->status = LEXER_INTERNAL_ERROR;
    return lexer->status;
}

static void advance(lexer_t* lexer)
{
    if (lexer->current == EOF)
        return;
    if (lexer->current == '\n') {
        ++lexer->line;
        lexer->column = 1;
    } else {
        ++lexer->column;
    }
    lexer->current = lexer->lookahead;
    lexer->lookahead = fgetc(lexer->input);
    if (ferror(lexer->input))
        lexer->status = LEXER_INTERNAL_ERROR;
}

static int take(lexer_t* lexer, buffer_t* buffer)
{
    if (buffer->length + 1 >= buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity * 2 : 32;
        if (buffer->capacity > SIZE_MAX / 2) {
            lexer->status = LEXER_INTERNAL_ERROR;
            return 0;
        }
        char* data = realloc(buffer->data, capacity);
        if (!data) {
            lexer->status = LEXER_INTERNAL_ERROR;
            return 0;
        }
        buffer->data = data;
        buffer->capacity = capacity;
    }
    buffer->data[buffer->length++] = (char)lexer->current;
    buffer->data[buffer->length] = '\0';
    advance(lexer);
    return lexer->status == LEXER_OK;
}

static lexer_status_t invalid(lexer_t* lexer)
{
    if (lexer->status == LEXER_OK)
        lexer->status = LEXER_ERROR;
    return lexer->status;
}

static token_type_t keyword(const char* text)
{
    static const struct {
        const char* text;
        token_type_t type;
    } words[] = {
        { "def", TOKEN_DEF }, { "do", TOKEN_DO }, { "Double", TOKEN_TYPE_DOUBLE },
        { "else", TOKEN_ELSE }, { "if", TOKEN_IF }, { "import", TOKEN_IMPORT },
        { "Int", TOKEN_TYPE_INT }, { "return", TOKEN_RETURN },
        { "String", TOKEN_TYPE_STRING }, { "then", TOKEN_THEN },
        { "Unit", TOKEN_TYPE_UNIT }, { "val", TOKEN_VAL }, { "var", TOKEN_VAR },
        { "while", TOKEN_WHILE }, { "Boolean", TOKEN_RESERVED_KEYWORD },
        { "case", TOKEN_RESERVED_KEYWORD }, { "class", TOKEN_RESERVED_KEYWORD },
        { "extension", TOKEN_RESERVED_KEYWORD }, { "for", TOKEN_RESERVED_KEYWORD },
        { "List", TOKEN_RESERVED_KEYWORD }, { "match", TOKEN_RESERVED_KEYWORD },
        { "type", TOKEN_RESERVED_KEYWORD }
    };
    for (size_t i = 0; i < sizeof words / sizeof words[0]; ++i)
        if (strcmp(text, words[i].text) == 0)
            return words[i].type;
    return TOKEN_IDENTIFIER;
}

/* A separator is valid only between two digits, including in exponents. */
static int digits(lexer_t* lexer, buffer_t* buffer)
{
    if (!digit(lexer->current)) {
        invalid(lexer);
        return 0;
    }
    do {
        if (!take(lexer, buffer))
            return 0;
        if (lexer->current == '_') {
            if (!digit(lexer->lookahead)) {
                invalid(lexer);
                return 0;
            }
            if (!take(lexer, buffer))
                return 0;
        }
    } while (digit(lexer->current));
    return 1;
}

static void number(lexer_t* lexer, buffer_t* buffer, token_type_t* type)
{
    int leading_zero = lexer->current == '0';
    int nonzero = 0;
    *type = TOKEN_INTEGER;
    if (lexer->current != '.') {
        if (!digits(lexer, buffer))
            return;
        for (size_t i = 0; i < buffer->length; ++i)
            if (buffer->data[i] >= '1' && buffer->data[i] <= '9')
                nonzero = 1;
    }
    if (lexer->current == '.' && digit(lexer->lookahead)) {
        *type = TOKEN_DOUBLE;
        if (!take(lexer, buffer) || !digits(lexer, buffer))
            return;
    }
    if (lexer->current == 'e' || lexer->current == 'E') {
        *type = TOKEN_DOUBLE;
        if (!take(lexer, buffer))
            return;
        if (lexer->current == '+' || lexer->current == '-')
            if (!take(lexer, buffer))
                return;
        if (!digits(lexer, buffer))
            return;
    }
    /* d/D is a suffix of a decimal literal, not an integer literal. */
    if (*type == TOKEN_DOUBLE && (lexer->current == 'd' || lexer->current == 'D'))
        if (!take(lexer, buffer))
            return;
    if (id_part(lexer->current)) {
        invalid(lexer);
        return;
    }
    if (*type == TOKEN_INTEGER && leading_zero && nonzero)
        invalid(lexer);
    if (*type == TOKEN_DOUBLE && lexer->status == LEXER_OK) {
        char* normalized = malloc(buffer->length + 1);
        if (!normalized) {
            lexer->status = LEXER_INTERNAL_ERROR;
            return;
        }
        size_t length = 0;
        for (size_t i = 0; i < buffer->length; ++i)
            if (buffer->data[i] != '_' && buffer->data[i] != 'd' && buffer->data[i] != 'D')
                normalized[length++] = buffer->data[i];
        normalized[length] = '\0';
        char* end;
        errno = 0;
        double value = strtod(normalized, &end);
        /* Nonzero subnormals are binary64 values, even when strtod reports
         * ERANGE. Overflow and underflow to zero are outside the range.
         */
        if (*end || !isfinite(value) || (errno == ERANGE && value == 0.0))
            invalid(lexer);
        free(normalized);
    }
}

lexer_status_t lexer_next(lexer_t* lexer, token_t* output)
{
    if (!output)
        return LEXER_INTERNAL_ERROR;
    *output = (token_t) { 0 };
    if (!lexer)
        return LEXER_INTERNAL_ERROR;
    output->line = lexer->line;
    output->column = lexer->column;
    if (lexer->status != LEXER_OK)
        return lexer->status;
    if (!lexer->input)
        return lexer->status = LEXER_INTERNAL_ERROR;
    while (lexer->current == ' ' || lexer->current == '\t' || lexer->current == '\r' || lexer->current == '\f' || lexer->current == '\v')
        advance(lexer);
    output->line = lexer->line;
    output->column = lexer->column;
    if (lexer->status != LEXER_OK)
        return lexer->status;
    if (lexer->current == EOF)
        return LEXER_OK;

    buffer_t buffer = { 0 };
    token_type_t type = TOKEN_EOF;
    if (id_start(lexer->current)) {
        do {
            if (!take(lexer, &buffer))
                break;
        } while (id_part(lexer->current));
        if (lexer->status == LEXER_OK) {
            if (strcmp(buffer.data, "_") == 0)
                invalid(lexer);
            else
                type = keyword(buffer.data);
        }
    } else if (digit(lexer->current) || (lexer->current == '.' && digit(lexer->lookahead))) {
        number(lexer, &buffer, &type);
    } else if (lexer->current == '@') {
        const char* annotation = "@main";
        for (size_t i = 0; annotation[i] && lexer->status == LEXER_OK; ++i) {
            if (lexer->current != annotation[i]) {
                invalid(lexer);
                break;
            }
            if (!take(lexer, &buffer))
                break;
        }
        if (id_part(lexer->current))
            invalid(lexer);
        type = TOKEN_MAIN;
    } else {
        int c = lexer->current;
        switch (c) {
        case '\n':
            type = TOKEN_NEWLINE;
            break;
        case '(':
            type = TOKEN_LPAREN;
            break;
        case ')':
            type = TOKEN_RPAREN;
            break;
        case ':':
            type = TOKEN_COLON;
            break;
        case ',':
            type = TOKEN_COMMA;
            break;
        case ';':
            type = TOKEN_SEMICOLON;
            break;
        case '.':
            type = TOKEN_DOT;
            break;
        case '+':
            type = TOKEN_PLUS;
            break;
        case '-':
            type = TOKEN_MINUS;
            break;
        case '*':
            type = TOKEN_STAR;
            break;
        case '/':
            type = TOKEN_SLASH;
            break;
        case '=':
            type = TOKEN_ASSIGN;
            break;
        case '<':
            type = TOKEN_LESS;
            break;
        case '>':
            type = TOKEN_GREATER;
            break;
        case '!':
            if (lexer->lookahead == '=')
                type = TOKEN_NOT_EQUAL;
            else
                invalid(lexer);
            break;
        default:
            invalid(lexer);
            break;
        }
        /* Comments belong to the next task: never tokenize their
         * contents as a valid sequence of operators and identifiers.
         */
        if (c == '/' && (lexer->lookahead == '/' || lexer->lookahead == '*'))
            invalid(lexer);
        if (lexer->status == LEXER_OK && take(lexer, &buffer)) {
            if ((c == '=' || c == '!' || c == '<' || c == '>') && lexer->current == '=') {
                if (c == '=')
                    type = TOKEN_EQUAL;
                if (c == '<')
                    type = TOKEN_LESS_EQUAL;
                if (c == '>')
                    type = TOKEN_GREATER_EQUAL;
                take(lexer, &buffer);
            }
        }
    }
    if (lexer->status != LEXER_OK) {
        free(buffer.data);
        return lexer->status;
    }
    output->type = type;
    output->lexeme = buffer.data;
    output->length = buffer.length;
    return LEXER_OK;
}
