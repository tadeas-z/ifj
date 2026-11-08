// Implementation of the IFJ26 imperative language compiler
// parser_expression.c
// Author(s): Dusan Popovic (xpopovd00)

#include <stdlib.h>
#include <string.h>
#include "astree.h"

void ast_free(ast_node_t* node) {
    if (node) {
        if (node->kind == AST_STRING_LIT && node->string_lit.value) {
            free(node->string_lit.value);
        }
        if (node->kind == AST_VAR_REF && node->var_ref.name) {
            free(node->var_ref.name);
        }
        if (node->kind == AST_BINARY) {
            ast_free(node->binary.left);
            ast_free(node->binary.right);
        }
        free(node);
    }
}
