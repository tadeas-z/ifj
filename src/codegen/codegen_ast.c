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
        // TODO add calls
        break;
    }
}

// value into the variable
static void generate_store(codegen_t* codegen, ast_node_t* value, const char* name, int id)
{
    var_t var = { FRAME_LF, name, id };

    codegen_expr(codegen, value);
    codegen_pops(codegen, var);
}

// jump if the condition is false
static void generate_cond(codegen_t* codegen, ast_node_t* cond, int num, const char* suffix)
{
    symb_t false_val = { 0 };
    false_val.type = S_BOOL;
    false_val.bool_val = false;

    codegen_expr(codegen, cond);
    codegen_pushs(codegen, false_val);
    codegen_jumpifeqs(codegen, num, suffix);
}

// then block, else block
static void generate_if(codegen_t* codegen, ast_node_t* stmt)
{
    int num = codegen_new_label(codegen);

    generate_cond(codegen, stmt->if_stmt.cond, num, "else");
    codegen_stmt(codegen, stmt->if_stmt.then_block);
    codegen_jump(codegen, num, "end");
    codegen_label(codegen, num, "else");
    codegen_stmt(codegen, stmt->if_stmt.else_block);
    codegen_label(codegen, num, "end");
}

// check, body, jump back
static void generate_while(codegen_t* codegen, ast_node_t* stmt)
{
    int num = codegen_new_label(codegen);

    codegen_label(codegen, num, "while");
    generate_cond(codegen, stmt->while_stmt.cond, num, "end");
    codegen_stmt(codegen, stmt->while_stmt.body);
    codegen_jump(codegen, num, "while");
    codegen_label(codegen, num, "end");
}

// statements one by one
static void generate_block(codegen_t* codegen, ast_node_t* stmt)
{
    for (size_t i = 0; i < stmt->block.statements.count; i++)
        codegen_stmt(codegen, dyn_arr_get(&stmt->block.statements, i));
}

// generate a statement
void codegen_stmt(codegen_t* codegen, ast_node_t* stmt)
{
    switch (stmt->kind) {
    case AST_BLOCK:
        generate_block(codegen, stmt);
        break;
    case AST_VAR_DEF:
        generate_store(codegen, stmt->var_def.init, stmt->var_def.name, stmt->var_def.id);
        break;
    case AST_ASSIGN:
        generate_store(codegen, stmt->assign.value, stmt->assign.name, stmt->assign.id);
        break;
    case AST_IF:
        generate_if(codegen, stmt);
        break;
    case AST_WHILE:
        generate_while(codegen, stmt);
        break;
    default:
        // TODO add return and calls
        break;
    }
}
