// Implementation of the IFJ26 imperative language compiler
// parser_stack.h
// Author(s): Dusan Popovic (xpopovd00)

// Data types of the precedence parser stack. Implemented as a linked list.
// Each item holds the symbol kind, token type, shift mark and for a non-terminal a pointer
// to its AST subtree. Stack operations are in parser_expression.c.

#ifndef SYNTATIC_STACK_H
#define SYNTATIC_STACK_H

#include "ast.h"
#include "token.h"
#include <stdbool.h>
#include <stdio.h>

typedef enum {
    SYMBOL_TERMINAL,
    SYMBOL_NON_TERMINAL
} symbol_type_t;

typedef struct {
    symbol_type_t symbol_type;
    token_type_t token_type;
    bool shift_mark;
    ast_node_t* ast_node;
} tdata_t;

typedef struct tselem {
    tdata_t data;
    struct tselem* next;
} tselem_t;

typedef struct {
    tselem_t* top;
} tstack_t;

#endif
