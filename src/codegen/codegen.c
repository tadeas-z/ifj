// Implementation of the IFJ26 imperative language compiler
// codegen.c
// Author(s): Marek Zachensky (xzachem00)

#include "codegen.h"
#include <stdlib.h>

struct codegen {
    FILE* out;
};

// same order as stack_op_t in header
static const char* op_names[] = {
    "ADDS",
    "SUBS",
    "MULS",
    "DIVS",
    "IDIVS",
    "LTS",
    "GTS",
    "EQS",
    "NOTS",
    "INT2FLOATS"
};

// string@
static void print_string(codegen_t* codegen, const char* s)
{
    fprintf(codegen->out, "string@");

    int i = 0;
    while (s[i] != '\0') {
        unsigned char ch = s[i];

        // escape spaces, control chars, non ASCII, # and backslash
        if (ch <= 32 || ch == '#' || ch == '\\' || ch >= 127)
            fprintf(codegen->out, "\\%03d", ch);
        else
            fprintf(codegen->out, "%c", ch);

        i++;
    }
}

// int@
static void print_int(codegen_t* codegen, long long n)
{
    fprintf(codegen->out, "int@%lld", n);
}

// float@
static void print_float(codegen_t* codegen, double d)
{
    fprintf(codegen->out, "float@%a", d);
}

// bool@
static void print_bool(codegen_t* codegen, bool b)
{
    if (b)
        fprintf(codegen->out, "bool@true");
    else
        fprintf(codegen->out, "bool@false");
}

// nil@
static void print_nil(codegen_t* codegen)
{
    fprintf(codegen->out, "nil@nil");
}

// LF@name$id
static void print_var(codegen_t* codegen, var_t var)
{
    const char* frame_name;
    if (var.frame == FRAME_GF)
        frame_name = "GF";
    else if (var.frame == FRAME_LF)
        frame_name = "LF";
    else
        frame_name = "TF";

    fprintf(codegen->out, "%s@%s$%d", frame_name, var.name, var.id);
}

// variable or constant
static void print_symb(codegen_t* codegen, symb_t symb)
{
    if (symb.type == S_VAR)
        print_var(codegen, symb.var);
    else if (symb.type == S_INT)
        print_int(codegen, symb.int_val);
    else if (symb.type == S_FLOAT)
        print_float(codegen, symb.float_val);
    else if (symb.type == S_BOOL)
        print_bool(codegen, symb.bool_val);
    else if (symb.type == S_NIL)
        print_nil(codegen);
    else
        print_string(codegen, symb.string_val);
}

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

void codegen_defvar(codegen_t* codegen, var_t var)
{
    fprintf(codegen->out, "DEFVAR ");
    print_var(codegen, var);
    fprintf(codegen->out, "\n");
}

void codegen_move(codegen_t* codegen, var_t var, symb_t symb)
{
    fprintf(codegen->out, "MOVE ");
    print_var(codegen, var);
    fprintf(codegen->out, " ");
    print_symb(codegen, symb);
    fprintf(codegen->out, "\n");
}

void codegen_write(codegen_t* codegen, symb_t symb)
{
    fprintf(codegen->out, "WRITE ");
    print_symb(codegen, symb);
    fprintf(codegen->out, "\n");
}

void codegen_func_begin(codegen_t* codegen, const char* name, bool is_main)
{
    // entry function also gets $main
    if (is_main)
        fprintf(codegen->out, "LABEL $main\n");
    fprintf(codegen->out, "LABEL %s\n", name);
    fprintf(codegen->out, "PUSHFRAME\n");
}

void codegen_func_end(codegen_t* codegen)
{
    fprintf(codegen->out, "POPFRAME\n");
    fprintf(codegen->out, "RETURN\n");
}

void codegen_createframe(codegen_t* codegen)
{
    fprintf(codegen->out, "CREATEFRAME\n");
}

void codegen_call(codegen_t* codegen, const char* name)
{
    fprintf(codegen->out, "CALL %s\n", name);
}

void codegen_pushs(codegen_t* codegen, symb_t symb)
{
    fprintf(codegen->out, "PUSHS ");
    print_symb(codegen, symb);
    fprintf(codegen->out, "\n");
}

void codegen_pops(codegen_t* codegen, var_t var)
{
    fprintf(codegen->out, "POPS ");
    print_var(codegen, var);
    fprintf(codegen->out, "\n");
}

void codegen_stack_op(codegen_t* codegen, stack_op_t op)
{
    fprintf(codegen->out, "%s\n", op_names[op]);
}
