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
    *lexer = (lexer_t) { .input = input, .line = 1, .column = 1, .at_line_start = 1 };
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

static void string_literal(lexer_t* lexer, buffer_t* buffer)
{
    if (!take(lexer, buffer))
        return;
    if (lexer->current == '"') {
        if (!take(lexer, buffer))
            return;
        if (lexer->current != '"')
            return; // Empty ordinary string.
        if (!take(lexer, buffer))
            return;
        size_t quotes = 0;
        while (lexer->current != EOF && lexer->status == LEXER_OK) {
            int c = lexer->current;
            if (c > 127) {
                invalid(lexer);
                return;
            }
            if (!take(lexer, buffer))
                return;
            quotes = c == '"' ? quotes + 1 : 0;
            if (quotes == 3)
                return;
        }
    } else {
        while (lexer->current != EOF && lexer->status == LEXER_OK) {
            int c = lexer->current;
            if (c == '\n' || c > 127) {
                invalid(lexer);
                return;
            }
            if (!take(lexer, buffer))
                return;
            if (c == '"')
                return;
            if (c == '\\') {
                c = lexer->current;
                if (c != '\\' && c != '"' && c != 'n' && c != 'r' && c != 't') {
                    invalid(lexer);
                    return;
                }
                if (!take(lexer, buffer))
                    return;
            }
        }
    }
    invalid(lexer);
}

static int horizontal_space(int c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v';
}

static void begin_physical_line(lexer_t* lexer)
{
    lexer->at_line_start = 1;
    lexer->physical_indentation = 0;
    lexer->leading_tab = 0;
    lexer->prefix_complete = 0;
}

static int newline_token(lexer_t* lexer, token_t* output, size_t line, size_t column)
{
    output->line = line;
    output->column = column;
    output->indentation = lexer->logical_indentation;
    output->lexeme = malloc(2);
    if (output->lexeme == NULL) {
        lexer->status = LEXER_INTERNAL_ERROR;
        return -1;
    }
    memcpy(output->lexeme, "\n", 2);
    output->length = 1;
    output->type = TOKEN_NEWLINE;
    lexer->logical_has_tokens = 0;
    return 1;
}

/* Returns whether the comment crossed a physical line; preserves I/O errors. */
static int block_comment(lexer_t* lexer, size_t* first_line, size_t* first_column)
{
    size_t depth = 1;
    int multiline = 0;
    advance(lexer);
    advance(lexer);
    while (depth != 0 && lexer->status == LEXER_OK) {
        if (lexer->current == EOF) {
            invalid(lexer);
            return multiline;
        }
        if (lexer->current == '\n' && !multiline) {
            *first_line = lexer->line;
            *first_column = lexer->column;
            multiline = 1;
        }
        if (lexer->current == '/' && lexer->lookahead == '*') {
            if (depth == SIZE_MAX) {
                lexer->status = LEXER_INTERNAL_ERROR;
                return multiline;
            }
            ++depth;
            advance(lexer);
            advance(lexer);
        } else if (lexer->current == '*' && lexer->lookahead == '/') {
            --depth;
            advance(lexer);
            advance(lexer);
        } else {
            advance(lexer);
        }
    }
    return multiline;
}

/* 1: emitted logical newline, 0: ready for token/EOF, -1: error. */
static int skip_layout(lexer_t* lexer, token_t* output)
{
    while (lexer->status == LEXER_OK) {
        if (horizontal_space(lexer->current)) {
            if (lexer->at_line_start && !lexer->prefix_complete) {
                if (lexer->current == ' ')
                    ++lexer->physical_indentation;
                if (lexer->current == '\t')
                    lexer->leading_tab = 1;
            }
            advance(lexer);
            continue;
        }
        if (lexer->current == '\n') {
            size_t line = lexer->line, column = lexer->column;
            advance(lexer);
            begin_physical_line(lexer);
            if (lexer->status != LEXER_OK)
                return -1;
            if (lexer->paren_depth == 0 && lexer->logical_has_tokens)
                return newline_token(lexer, output, line, column);
            continue;
        }
        if (lexer->current == EOF)
            return 0;
        output->line = lexer->line;
        output->column = lexer->column;
        if (lexer->at_line_start && lexer->leading_tab) {
            invalid(lexer);
            return -1;
        }
        lexer->prefix_complete = 1;
        if (lexer->current == '/' && lexer->lookahead == '/') {
            while (lexer->current != '\n' && lexer->current != EOF && lexer->status == LEXER_OK)
                advance(lexer);
            if (lexer->status != LEXER_OK)
                return -1;
            if (lexer->current == EOF && lexer->paren_depth == 0 && lexer->logical_has_tokens)
                return newline_token(lexer, output, lexer->line, lexer->column);
            continue;
        }
        if (lexer->current == '/' && lexer->lookahead == '*') {
            size_t line = 0, column = 0;
            int multiline = block_comment(lexer, &line, &column);
            if (lexer->status != LEXER_OK)
                return -1;
            if (multiline) {
                while (horizontal_space(lexer->current))
                    advance(lexer);
                if (lexer->status != LEXER_OK)
                    return -1;
                if (lexer->current != '\n' && lexer->current != EOF) {
                    output->line = lexer->line;
                    output->column = lexer->column;
                    invalid(lexer);
                    return -1;
                }
                if (lexer->current == '\n') {
                    advance(lexer);
                    begin_physical_line(lexer);
                }
                if (lexer->status != LEXER_OK)
                    return -1;
                if (lexer->paren_depth == 0 && lexer->logical_has_tokens)
                    return newline_token(lexer, output, line, column);
            }
            continue;
        }
        if (lexer->at_line_start && lexer->logical_has_tokens
            && lexer->physical_indentation <= lexer->logical_indentation) {
            invalid(lexer);
            return -1;
        }
        return 0;
    }
    return -1;
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
    int layout = skip_layout(lexer, output);
    if (layout != 0)
        return lexer->status;
    output->line = lexer->line;
    output->column = lexer->column;
    if (lexer->status != LEXER_OK)
        return lexer->status;
    if (lexer->current == EOF)
        return LEXER_OK;

    output->starts_line = !lexer->logical_has_tokens;
    if (output->starts_line)
        lexer->logical_indentation = lexer->physical_indentation;
    output->indentation = lexer->logical_indentation;
    lexer->logical_has_tokens = 1;
    lexer->at_line_start = 0;

    buffer_t buffer = { 0 };
    token_type_t type = TOKEN_EOF;
    if (lexer->current == '"') {
        type = TOKEN_STRING;
        string_literal(lexer, &buffer);
    } else if (id_start(lexer->current)) {
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
    if (type == TOKEN_LPAREN) {
        if (lexer->paren_depth == SIZE_MAX) {
            free(buffer.data);
            return lexer->status = LEXER_INTERNAL_ERROR;
        }
        ++lexer->paren_depth;
    } else if (type == TOKEN_RPAREN && lexer->paren_depth != 0) {
        --lexer->paren_depth;
    }
    output->type = type;
    output->lexeme = buffer.data;
    output->length = buffer.length;
    return LEXER_OK;
}
