#ifndef IFJ_LEXER_H
#define IFJ_LEXER_H

#include <stdio.h>
#include "token.h"

typedef enum {
    LEXER_OK = 0,
    LEXER_ERROR = 1,
    LEXER_INTERNAL_ERROR = 99
} LexerStatus;

typedef struct {
    FILE *input;                 /* Borrowed, lexer never closes it. */
    int current, lookahead;
    size_t line, column;         /* Physical position, starting at 1. */
    LexerStatus status;          /* Sticky error, reinitialize to recover. */
} Lexer;

/* Numeric validation uses strtod, keep LC_NUMERIC in the C locale. */
LexerStatus lexer_init(Lexer *lexer, FILE *input);
/* Dispose the previous output before reuse. Output is initialized even on
 * failure. Repeated calls after EOF return EOF at the same position.
 */
LexerStatus lexer_next(Lexer *lexer, Token *output);

#endif
