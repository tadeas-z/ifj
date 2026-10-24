// Implementation of the IFJ26 imperative language compiler
// astree.c
// Author(s): Adam Zatloukal (xzatloa01)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

static ast_node_t* new_node(ast_node_kind_t kind, size_t line, size_t column);
static char* copy_string(const char* str);

static ast_node_t* new_node(ast_node_kind_t kind, size_t line, size_t column) {
    ast_node_t* node = calloc(1, sizeof(ast_node_t));
    if (node == NULL)
        return NULL;
    node->kind = kind;
    node->line = line;
    node->column = column;
    return node;
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Implement the copy_string helper"
// Scope: copy_string only

// Returns a heap copy of str (strdup is not part of C11), NULL if allocation fails.
static char* copy_string(const char* str) {
    size_t size = strlen(str) + 1;
    char* copy = malloc(size);
    if (copy == NULL)
        return NULL;
    memcpy(copy, str, size);
    return copy;
}

ast_node_t* ast_new_program(void) {
    return new_node(AST_PROGRAM, 0, 0);
}

ast_node_t* ast_new_func_def(const char* name, bool is_main, size_t line, size_t column) {
    ast_node_t* node = new_node(AST_FUNC_DEF, line, column);
    if (node == NULL)
        return NULL;
    node->func_def.name = copy_string(name);
    if (node->func_def.name == NULL) {
        free(node);
        return NULL;
    }
    node->func_def.is_main = is_main;
    node->func_def.params = NULL;
    node->func_def.param_count = 0;
    node->func_def.return_type = NULL;
    node->func_def.body = NULL;
    return node;
}
