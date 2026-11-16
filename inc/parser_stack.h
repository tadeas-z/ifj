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

/**
 * @brief Initializes the precedence stack by setting the top pointer to NULL.
 * 
 * @param s Pointer to the stack instance to be initialized.
 */
static void stack_init(tstack_t *s);

/**
 * @brief Pushes a new element onto the top of the stack.
 * 
 * @param s Pointer to the stack.
 * @param d Data element (token/symbol) to be pushed onto the stack.
 * @return true if allocation and push succeeded, false otherwise.
 */
static bool push(tstack_t *s, tdata_t d);

/**
 * @brief Removes the top element from the stack and frees its memory.
 *        Does nothing if the stack is empty.
 * 
 * @param s Pointer to the stack.
 */
static void pop(tstack_t *s);

/**
 * @brief Retrieves the data from the top element of the stack without removing it.
 * 
 * @param s Pointer to the stack.
 * @return TData Data structure of the top element, or a default empty structure on error.
 */
static tdata_t top(tstack_t *s);

/**
 * @brief Finds and returns the top-most terminal symbol on the stack,
 *        skipping any non-terminal symbols above it.
 * 
 * @param s Pointer to the stack.
 * @return TSElem* Pointer to the stack element containing the top terminal,
 *                 or NULL if no terminal is found.
 */
static tselem_t* get_top_terminal(tstack_t *s);

/**
 * @brief Pops all elements and frees all allocated memory associated with the stack.
 * 
 * @param s Pointer to the stack to be freed.
 */
static void stack_free(tstack_t *s);

#endif
