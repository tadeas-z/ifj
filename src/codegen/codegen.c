// Implementation of the IFJ26 imperative language compiler
// codegen.c
// Author(s): Marek Zachensky (xzachem00)

#include "codegen.h"
#include <stdlib.h>

struct codegen {
    FILE* out;
};

codegen_t* codegen_init(FILE* output)
{
    codegen_t* codegen = malloc(sizeof(codegen_t));
    if (codegen == NULL)
        return NULL;

    codegen->out = output;
    return codegen;
}

void codegen_free(codegen_t* codegen)
{
    free(codegen);
}

void codegen_program_begin(codegen_t* codegen)
{
    fprintf(codegen->out, ".IFJcode26\n");
    // main needs TF for PUSHFRAME
    fprintf(codegen->out, "CREATEFRAME\n");
    fprintf(codegen->out, "CALL $main\n");
    // jump over the functions to the end
    fprintf(codegen->out, "JUMP $end\n");
}

void codegen_program_end(codegen_t* codegen)
{
    fprintf(codegen->out, "LABEL $end\n");
    fprintf(codegen->out, "EXIT int@0\n");
}
