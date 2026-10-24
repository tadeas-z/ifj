// Implementation of the IFJ26 imperative language compiler
// astree.c
// Author(s): Adam Zatloukal (xzatloa01)

#include "ast.h"
#include "dynamic_array.h"
#include "utils.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static ast_node_t* new_node(ast_node_kind_t kind, size_t line, size_t column);
static void free_param(void* ptr);

static ast_node_t* new_node(ast_node_kind_t kind, size_t line, size_t column)
{
    ast_node_t* node = calloc(1, sizeof(ast_node_t));
    if (node == NULL)
        return NULL;
    node->kind = kind;
    node->line = line;
    node->column = column;
    return node;
}

ast_node_t* ast_new_program(void)
{
    ast_node_t* node = new_node(AST_PROGRAM, 0, 0);
    if (node == NULL)
        return NULL;

    bool ret_val = dyn_arr_init(&node->program.func_list);
    if (ret_val == false) {
        free(node);
        return NULL;
    }

    return node;
}

ast_node_t* ast_new_func_def(const char* name, bool is_main, size_t line, size_t column)
{
    ast_node_t* node = new_node(AST_FUNC_DEF, line, column);
    if (node == NULL)
        return NULL;
    node->func_def.name = copy_string(name);
    if (node->func_def.name == NULL) {
        free(node);
        return NULL;
    }
    node->func_def.is_main = is_main;
    node->func_def.return_type = NULL;
    node->func_def.body = NULL;

    bool ret_val = dyn_arr_init(&(node->func_def.params));
    if (ret_val == false) {
        free(node->func_def.name);
        free(node);
        return NULL;
    }

    ret_val = dyn_arr_init(&node->func_def.locals);
    if (ret_val == false) {
        free(node->func_def.name);
        dyn_arr_free(&node->func_def.params, NULL);
        free(node);
        return NULL;
    }

    return node;
}

bool ast_func_def_add_param(ast_node_t* func_def_node, const char* name, type_t* type)
{
    // Check for invalid params
    if (func_def_node == NULL || func_def_node->kind != AST_FUNC_DEF || name == NULL || type == NULL)
        return false;

    // Get reference to node data
    ast_func_def_t* func_def = &(func_def_node->func_def);

    // Add the param
    ast_param_t* param = malloc(sizeof(ast_param_t));
    if (param == NULL)
        return false;

    param->name = copy_string(name);
    if (param->name == NULL) {
        free(param);
        return false;
    }

    param->type = type;
    param->id = -1;

    // Append the param to the params array
    bool ret_val = dyn_arr_append(&func_def->params, param);
    if (ret_val == false) {
        free(param->name);
        free(param);
        return false;
    }

    return true;
}

// Internal helper used to free a struct of type ast_param_t.
// It is passed here as a null pointer because the pointers to the param
// are inside dyn_array_t which only works with void pointers
static void free_param(void* ptr)
{
    ast_param_t* param = ptr;
    free(param->name);
    free(param);
}

// Internal helper needed only becacuse dyn_arr_free
// accepts function points only with this signature
static void free_node(void* ptr)
{
    ast_free(ptr);
}


void ast_free(ast_node_t* node)
{
    if (node == NULL)
        return;

    // Every case frees only what it owns.
    // For example AST_FUNC_DEF only frees the locals array not the items inside of it
    // since they will be freed by the body of the function
    switch (node->kind) {

    // Program structure
    case AST_PROGRAM:
        dyn_arr_free(&node->program.func_list, free_node);
        break;

    case AST_FUNC_DEF: {
        ast_func_def_t* func_def = &node->func_def;

        free(func_def->name);
        ast_free(func_def->body);
        dyn_arr_free(&func_def->locals, NULL);
        dyn_arr_free(&func_def->params, free_param);

        break;
    }

    case AST_BLOCK:
        dyn_arr_free(&node->block.statements, free_node);
        break;

    // Statements
    case AST_VAR_DEF:
        free(node->var_def.name);
        ast_free(node->var_def.init);

        break;

    case AST_ASSIGN:
        free(node->assign.name);
        ast_free(node->assign.value);
        break;

    case AST_RETURN:
        ast_free(node->return_stmt.value);
        break;

    case AST_IF:
        ast_free(node->if_stmt.cond);
        ast_free(node->if_stmt.then_block);
        ast_free(node->if_stmt.else_block);
        break;

    case AST_WHILE:
        ast_free(node->while_stmt.cond);
        ast_free(node->while_stmt.body);
        break;

    case AST_EXPR_STMT:
        ast_free(node->expr_stmt.expr);
        break;

    // Expressions
    case AST_INT_LIT:
        break;

    case AST_DOUBLE_LIT:
        break;

    case AST_STRING_LIT:
        free(node->string_lit.value);
        break;

    case AST_UNIT_LIT:
        break;

    case AST_VAR_REF:
        free(node->var_ref.name);
        break;

    case AST_UNARY:
        ast_free(node->unary.operand);
        break;

    case AST_BINARY:
        ast_free(node->binary.left);
        ast_free(node->binary.right);
        break;

    case AST_CALL:
        free(node->call.name);
        dyn_arr_free(&node->call.args, free_node);
        break;

    case AST_CONVERT:
        ast_free(node->convert.operand);
        break;

    }

    free(node);
}
