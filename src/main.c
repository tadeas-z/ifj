// Implementation of the IFJ26 imperative language compiler
// main.c
// Author(s): Marek Zachensky (xzachem00)

#include "codegen_ast.h"
#include "comp_error.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include <stdio.h>

// source from stdin, IFJcode26 to stdout
int main(void)
{
    lexer_t lexer;
    error_code_t err = lexer_init(&lexer, stdin);
    if (err != ERR_OK)
        return err;

    ast_node_t* program = NULL;
    err = parser_main(&lexer, &program);
    if (err != ERR_OK)
        return err;

    err = semantic_analyze(program);
    if (err != ERR_OK) {
        ast_free(program);
        return err;
    }

    codegen_t* codegen = codegen_init(stdout);
    if (codegen == NULL) {
        ast_free(program);
        return ERR_INTERNAL;
    }

    codegen_program(codegen, program);
    codegen_free(codegen);
    ast_free(program);
    return ERR_OK;
}
