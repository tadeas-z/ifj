// Implementation of the IFJ26 imperative language compiler
// codegen_ast.c
// Author(s): Marek Zachensky (xzachem00)

#include "codegen_ast.h"

// minus, multiply by -1
static void generate_neg(codegen_t* codegen, ast_node_t* expr)
{
    symb_t minus = { 0 };
    if (expr->type->kind == T_DOUBLE) {
        minus.type = S_FLOAT;
        minus.float_val = -1.0;
    } else {
        minus.type = S_INT;
        minus.int_val = -1;
    }

    codegen_expr(codegen, expr->unary.operand);
    codegen_pushs(codegen, minus);
    codegen_stack_op(codegen, OP_MULS);
}

// opposite compare, NOT
static void generate_not(codegen_t* codegen, stack_op_t op)
{
    codegen_stack_op(codegen, op);
    codegen_stack_op(codegen, OP_NOTS);
}

// join two strings
static void generate_concat(codegen_t* codegen)
{
    var_t a = { FRAME_GF, "tmp", 1 };
    var_t b = { FRAME_GF, "tmp", 2 };
    symb_t sa = { 0 };
    sa.type = S_VAR;
    sa.var = a;
    symb_t sb = { 0 };
    sb.type = S_VAR;
    sb.var = b;

    codegen_pops(codegen, b);
    codegen_pops(codegen, a);
    codegen_binary_op(codegen, OP_CONCAT, a, sa, sb);
    codegen_pushs(codegen, sa);
}

// both sides, then operator
static void generate_binary(codegen_t* codegen, ast_node_t* expr)
{
    codegen_expr(codegen, expr->binary.left);
    codegen_expr(codegen, expr->binary.right);

    switch (expr->binary.op) {
    case AST_OP_ADD:
        if (expr->type->kind == T_STRING)
            generate_concat(codegen);
        else
            codegen_stack_op(codegen, OP_ADDS);
        break;
    case AST_OP_SUB:
        codegen_stack_op(codegen, OP_SUBS);
        break;
    case AST_OP_MUL:
        codegen_stack_op(codegen, OP_MULS);
        break;
    case AST_OP_DIV:
        if (expr->type->kind == T_INT)
            codegen_stack_op(codegen, OP_IDIVS);
        else
            codegen_stack_op(codegen, OP_DIVS);
        break;
    case AST_OP_LT:
        codegen_stack_op(codegen, OP_LTS);
        break;
    case AST_OP_GT:
        codegen_stack_op(codegen, OP_GTS);
        break;
    case AST_OP_EQ:
        codegen_stack_op(codegen, OP_EQS);
        break;
    case AST_OP_LE:
        generate_not(codegen, OP_GTS);
        break;
    case AST_OP_GE:
        generate_not(codegen, OP_LTS);
        break;
    case AST_OP_NE:
        generate_not(codegen, OP_EQS);
        break;
    }
}

// int to double
static void generate_convert(codegen_t* codegen, ast_node_t* expr)
{
    codegen_expr(codegen, expr->convert.operand);
    codegen_stack_op(codegen, OP_INT2FLOATS);
}

// push expression on the stack
void codegen_expr(codegen_t* codegen, ast_node_t* expr)
{
    symb_t symb = { 0 };

    switch (expr->kind) {
    case AST_INT_LIT:
        symb.type = S_INT;
        symb.int_val = expr->int_lit.value;
        codegen_pushs(codegen, symb);
        break;
    case AST_DOUBLE_LIT:
        symb.type = S_FLOAT;
        symb.float_val = expr->double_lit.value;
        codegen_pushs(codegen, symb);
        break;
    case AST_STRING_LIT:
        symb.type = S_STRING;
        symb.string_val = expr->string_lit.value;
        codegen_pushs(codegen, symb);
        break;
    case AST_UNIT_LIT:
        symb.type = S_NIL;
        codegen_pushs(codegen, symb);
        break;
    case AST_VAR_REF:
        symb.type = S_VAR;
        symb.var.frame = FRAME_LF;
        symb.var.name = expr->var_ref.name;
        symb.var.id = expr->var_ref.id;
        codegen_pushs(codegen, symb);
        break;
    case AST_UNARY:
        generate_neg(codegen, expr);
        break;
    case AST_BINARY:
        generate_binary(codegen, expr);
        break;
    case AST_CONVERT:
        generate_convert(codegen, expr);
        break;
    default:
        // calls are not done yet
        break;
    }
}
