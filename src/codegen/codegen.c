// Implementation of the IFJ26 imperative language compiler
// codegen.c
// Author(s): Marek Zachensky (xzachem00)

#include "codegen.h"
#include <stdlib.h>

struct codegen {
    FILE* out;
    int label_count;
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

// same order as unary_op_t in header
static const char* unary_names[] = {
    "INT2CHAR",
    "FLOAT2INT",
    "INT2STR",
    "FLOAT2STR",
    "STRLEN"
};

// same order as binary_op_t in header
static const char* binary_names[] = {
    "CONCAT",
    "GETCHAR",
    "STRI2INT"
};

// built in functions, result goes on the stack
static const char* builtins =
    // readLine()
    "LABEL readLine\n"
    "PUSHFRAME\n"
    "DEFVAR LF@r$0\n"
    "READ LF@r$0 string\n"
    "JUMPIFNEQ $readLine$ok LF@r$0 nil@nil\n"
    "MOVE LF@r$0 string@\n"
    "LABEL $readLine$ok\n"
    "PUSHS LF@r$0\n"
    "POPFRAME\n"
    "RETURN\n"

    // readInt()
    "LABEL readInt\n"
    "PUSHFRAME\n"
    "DEFVAR LF@r$0\n"
    "READ LF@r$0 int\n"
    "JUMPIFNEQ $readInt$ok LF@r$0 nil@nil\n"
    "MOVE LF@r$0 int@0\n"
    "LABEL $readInt$ok\n"
    "PUSHS LF@r$0\n"
    "POPFRAME\n"
    "RETURN\n"

    // readDouble()
    "LABEL readDouble\n"
    "PUSHFRAME\n"
    "DEFVAR LF@r$0\n"
    "READ LF@r$0 float\n"
    "JUMPIFNEQ $readDouble$ok LF@r$0 nil@nil\n"
    "MOVE LF@r$0 float@0x0p+0\n"
    "LABEL $readDouble$ok\n"
    "PUSHS LF@r$0\n"
    "POPFRAME\n"
    "RETURN\n"

    // toStr(value)
    "LABEL toStr\n"
    "PUSHFRAME\n"
    "DEFVAR LF@r$0\n"
    "TYPE LF@r$0 LF@value$0\n"
    "JUMPIFEQ $toStr$int LF@r$0 string@int\n"
    "JUMPIFEQ $toStr$float LF@r$0 string@float\n"
    "JUMPIFEQ $toStr$nil LF@r$0 string@nil\n"
    "MOVE LF@r$0 LF@value$0\n"
    "JUMP $toStr$end\n"
    "LABEL $toStr$int\n"
    "INT2STR LF@r$0 LF@value$0\n"
    "JUMP $toStr$end\n"
    "LABEL $toStr$float\n"
    "FLOAT2STR LF@r$0 LF@value$0\n"
    "JUMP $toStr$end\n"
    "LABEL $toStr$nil\n"
    "MOVE LF@r$0 string@()\n"
    "LABEL $toStr$end\n"
    "PUSHS LF@r$0\n"
    "POPFRAME\n"
    "RETURN\n"

    // toInt(value)
    "LABEL toInt\n"
    "PUSHFRAME\n"
    "DEFVAR LF@r$0\n"
    "FLOAT2INT LF@r$0 LF@value$0\n"
    "PUSHS LF@r$0\n"
    "POPFRAME\n"
    "RETURN\n"

    // strLength(value)
    "LABEL strLength\n"
    "PUSHFRAME\n"
    "DEFVAR LF@r$0\n"
    "STRLEN LF@r$0 LF@value$0\n"
    "PUSHS LF@r$0\n"
    "POPFRAME\n"
    "RETURN\n"

    // subStr(value, start, end), incorect index exits with 40
    "LABEL subStr\n"
    "PUSHFRAME\n"
    "DEFVAR LF@len$0\n"
    "STRLEN LF@len$0 LF@value$0\n"
    "DEFVAR LF@b$0\n"
    "LT LF@b$0 LF@start$0 int@0\n"
    "JUMPIFEQ $subStr$err LF@b$0 bool@true\n"
    "LT LF@b$0 LF@end$0 int@0\n"
    "JUMPIFEQ $subStr$err LF@b$0 bool@true\n"
    "GT LF@b$0 LF@start$0 LF@end$0\n"
    "JUMPIFEQ $subStr$err LF@b$0 bool@true\n"
    "GT LF@b$0 LF@start$0 LF@len$0\n"
    "JUMPIFEQ $subStr$err LF@b$0 bool@true\n"
    "JUMPIFEQ $subStr$err LF@start$0 LF@len$0\n"
    "GT LF@b$0 LF@end$0 LF@len$0\n"
    "JUMPIFEQ $subStr$err LF@b$0 bool@true\n"
    "DEFVAR LF@r$0\n"
    "MOVE LF@r$0 string@\n"
    "DEFVAR LF@c$0\n"
    "LABEL $subStr$loop\n"
    "JUMPIFEQ $subStr$end LF@start$0 LF@end$0\n"
    "GETCHAR LF@c$0 LF@value$0 LF@start$0\n"
    "CONCAT LF@r$0 LF@r$0 LF@c$0\n"
    "ADD LF@start$0 LF@start$0 int@1\n"
    "JUMP $subStr$loop\n"
    "LABEL $subStr$end\n"
    "PUSHS LF@r$0\n"
    "POPFRAME\n"
    "RETURN\n"
    "LABEL $subStr$err\n"
    "EXIT int@40\n"

    // ord(value, index), -1 if index is incorrect
    "LABEL ord\n"
    "PUSHFRAME\n"
    "DEFVAR LF@len$0\n"
    "STRLEN LF@len$0 LF@value$0\n"
    "DEFVAR LF@b$0\n"
    "LT LF@b$0 LF@index$0 int@0\n"
    "JUMPIFEQ $ord$bad LF@b$0 bool@true\n"
    "GT LF@b$0 LF@index$0 LF@len$0\n"
    "JUMPIFEQ $ord$bad LF@b$0 bool@true\n"
    "JUMPIFEQ $ord$bad LF@index$0 LF@len$0\n"
    "DEFVAR LF@r$0\n"
    "STRI2INT LF@r$0 LF@value$0 LF@index$0\n"
    "PUSHS LF@r$0\n"
    "POPFRAME\n"
    "RETURN\n"
    "LABEL $ord$bad\n"
    "PUSHS int@-1\n"
    "POPFRAME\n"
    "RETURN\n"

    // chr(code)
    "LABEL chr\n"
    "PUSHFRAME\n"
    "DEFVAR LF@r$0\n"
    "INT2CHAR LF@r$0 LF@code$0\n"
    "PUSHS LF@r$0\n"
    "POPFRAME\n"
    "RETURN\n";

// ===== print =====

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

// $num$suffix
static void print_label(codegen_t* codegen, int num, const char* suffix)
{
    fprintf(codegen->out, "$%d$%s", num, suffix);
}

// ===== codegen =====

codegen_t* codegen_init(FILE* output)
{
    codegen_t* codegen = malloc(sizeof(codegen_t));
    if (codegen == NULL)
        return NULL;

    codegen->out = output;
    codegen->label_count = 0;
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

int codegen_new_label(codegen_t* codegen)
{
    codegen->label_count++;
    return codegen->label_count;
}

void codegen_label(codegen_t* codegen, int num, const char* suffix)
{
    fprintf(codegen->out, "LABEL ");
    print_label(codegen, num, suffix);
    fprintf(codegen->out, "\n");
}

void codegen_jump(codegen_t* codegen, int num, const char* suffix)
{
    fprintf(codegen->out, "JUMP ");
    print_label(codegen, num, suffix);
    fprintf(codegen->out, "\n");
}

void codegen_jumpifeqs(codegen_t* codegen, int num, const char* suffix)
{
    fprintf(codegen->out, "JUMPIFEQS ");
    print_label(codegen, num, suffix);
    fprintf(codegen->out, "\n");
}

void codegen_unary_op(codegen_t* codegen, unary_op_t op, var_t var, symb_t symb)
{
    fprintf(codegen->out, "%s ", unary_names[op]);
    print_var(codegen, var);
    fprintf(codegen->out, " ");
    print_symb(codegen, symb);
    fprintf(codegen->out, "\n");
}

void codegen_binary_op(codegen_t* codegen, binary_op_t op, var_t var, symb_t symb1, symb_t symb2)
{
    fprintf(codegen->out, "%s ", binary_names[op]);
    print_var(codegen, var);
    fprintf(codegen->out, " ");
    print_symb(codegen, symb1);
    fprintf(codegen->out, " ");
    print_symb(codegen, symb2);
    fprintf(codegen->out, "\n");
}

void codegen_read(codegen_t* codegen, var_t var, const char* type)
{
    fprintf(codegen->out, "READ ");
    print_var(codegen, var);
    fprintf(codegen->out, " %s\n", type);
}

void codegen_builtins(codegen_t* codegen)
{
    fprintf(codegen->out, "%s", builtins);
}
