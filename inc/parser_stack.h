// Implementation of the IFJ26 imperative language compiler
// parser_expression.h
// Author(s): Dusan Popovic (xpopovd00)

#ifndef SYNTATIC_STACK_H
#define SYNTATIC_STACK_H

#include <stdio.h>
#include <stdbool.h>
#include "token.h"
#include "ast.h"

typedef enum{
	SYMBOL_TERMINAL,
	SYMBOL_NON_TERMINAL
}symbol_type_t;

typedef struct {
	symbol_type_t symbol_type;
	token_type_t token_type;
	bool shift_mark;
    ast_node_t *ast_node;
}tdata_t;

typedef struct tselem{
	tdata_t data;
	struct tselem *next;
}tselem_t;

typedef struct{
	tselem_t *top;
}tstack_t;


#endif
