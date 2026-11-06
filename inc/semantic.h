#ifndef SEMANTIC_H_
#define SEMANTIC_H_

#include "ast.h"
#include "symtable.h"
#include "comp_error.h"


typedef struct
{
    symtable_t* symbol_table;
    ast_node_t* program;
    ast_node_t* main_func;
    ast_node_t* current_func;
} sema_ctx_t;

error_code_t semantic_analyze(ast_node_t* program);

#endif
