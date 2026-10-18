// Implementation of the IFJ26 imperative language compiler
// lexer.h
// Author(s): Tadeáš Zonyga (xzonygt00)

#ifndef IFJ_LEXER_H
#define IFJ_LEXER_H

#include "token.h"
#include <stdio.h>

typedef enum {
    LEXER_OK = 0,
    LEXER_ERROR = 1,
    LEXER_INTERNAL_ERROR = 99
} lexer_status_t;

typedef struct {
    FILE* input; /* Borrowed, lexer never closes it. */
    int current, lookahead;
    size_t line, column; /* Physical position, starting at 1. */
    lexer_status_t status; /* Sticky error, reinitialize to recover. */
} lexer_t;

/* Numeric validation uses strtod, keep LC_NUMERIC in the C locale. */
lexer_status_t lexer_init(lexer_t* lexer, FILE* input);
/* Dispose the previous output before reuse. Output is initialized even on
 * failure. Repeated calls after EOF return EOF at the same position.
 */
lexer_status_t lexer_next(lexer_t* lexer, token_t* output);

#endif
