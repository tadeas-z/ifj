// Implementation of the IFJ26 imperative language compiler
// token.h
// Author(s): Tadeáš Zonyga (xzonygt00)

/* Token types and the token structure shared by
 * the lexer and the parser. A token holds its type,
 * its text, its position in the source and
 * line/indentation information for the parser. */

#ifndef IFJ_TOKEN_H
#define IFJ_TOKEN_H

#include <stddef.h>

typedef enum {
    TOKEN_EOF,
    TOKEN_NEWLINE,
    TOKEN_IDENTIFIER,
    TOKEN_INTEGER,
    TOKEN_DOUBLE,
    TOKEN_DEF,
    TOKEN_DO,
    TOKEN_TYPE_DOUBLE,
    TOKEN_ELSE,
    TOKEN_IF,
    TOKEN_IMPORT,
    TOKEN_TYPE_INT,
    TOKEN_RETURN,
    TOKEN_TYPE_STRING,
    TOKEN_THEN,
    TOKEN_TYPE_UNIT,
    TOKEN_VAL,
    TOKEN_VAR,
    TOKEN_WHILE,
    TOKEN_RESERVED_KEYWORD,
    TOKEN_MAIN,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_COLON,
    TOKEN_COMMA,
    TOKEN_SEMICOLON,
    TOKEN_DOT,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_ASSIGN,
    TOKEN_EQUAL,
    TOKEN_NOT_EQUAL,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,
    TOKEN_STRING,
    /* Extensions BOOLTHEN and CONTROL, appended to keep earlier values. */
    TOKEN_TYPE_BOOLEAN,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_FOR,
    TOKEN_NOT,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_LARROW,
    TOKEN_UNDERSCORE,
    TOKEN_PIPE,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_LBRACE,
    TOKEN_RBRACE
} token_type_t;

typedef struct {
    token_type_t type;
    /* Original spelling, owned by the caller; NULL only for EOF/errors.
     * Numeric separators/suffixes are preserved. Minus is a separate token.
     */
    char* lexeme;
    size_t length;
    size_t line;
    size_t column;
    /* Leading spaces of the logical line; parser checks block structure. */
    size_t indentation;
    /* True only for the first non-comment token of a logical line. */
    int starts_line;
} token_t;

/* Frees the lexeme and resets the token, safe to call twice. */
void token_dispose(token_t* token);

#endif
