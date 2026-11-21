// Implementation of the IFJ26 imperative language compiler
// semantic.h
// Author(s): Adam Zatloukal (xzatloa01)

// Header for semantic.h
// The only thing that SHOULD be used here is semantic_analyze,
// which performs semantic analysis of the provided programs ast

#ifndef SEMANTIC_H_
#define SEMANTIC_H_

#include "ast.h"
#include "comp_error.h"
#include "symtable.h"

typedef struct sema_frame {
    symbol_t* func_sym;
    struct sema_frame* caller;
} sema_frame_t;

typedef struct
{
    symtable_t* symbol_table;
    ast_node_t* program;
    ast_node_t* main_func;
    ast_node_t* current_func;
    int base_depth;
    sema_frame_t* call_chain;
} sema_ctx_t;

error_code_t semantic_analyze(ast_node_t* program);

#endif
