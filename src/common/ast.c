// Implementation of the IFJ26 imperative language compiler
// ast.c
// Author(s): Adam Zatloukal (xzatloa01)

#include "ast.h"
#include "dynamic_array.h"
#include "utils.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "can you create a macro function that checks the return value of
//          dyn arr init and frees it in case it failed and returns"
// Scope: INIT_ARR_OR_FAIL and its uses in the constructors

// Initializes arr, on failure frees the half-built node and returns NULL
// from the calling constructor. Only for functions returning ast_node_t*.
#define INIT_ARR_OR_FAIL(node, arr) \
    do {                            \
        if (!dyn_arr_init(arr)) {   \
            ast_free(node);         \
            return NULL;            \
        }                           \
    } while (0)

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "can you also create a macro for copy string and checking its return value"
// Scope: COPY_STR_OR_FAIL and its use in ast_new_func_def

// Stores a heap copy of src in dest, on failure frees the half-built node and
// returns NULL from the calling constructor. Only for functions returning ast_node_t*.
#define COPY_STR_OR_FAIL(node, dest, src) \
    do {                                  \
        (dest) = copy_string(src);        \
        if ((dest) == NULL) {             \
            ast_free(node);               \
            return NULL;                  \
        }                                 \
    } while (0)

static ast_node_t* new_node(ast_node_kind_t kind, size_t line, size_t column);
static void free_param(void* ptr);

// Allocates and sets a new ast node. Returns null on mem alloc fail, else new node
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

// ----------------------------------------
//  Program
// ----------------------------------------

ast_node_t* ast_new_program(void)
{
    ast_node_t* node = new_node(AST_PROGRAM, 0, 0);
    if (node == NULL)
        return NULL;

    INIT_ARR_OR_FAIL(node, &node->program.func_list);

    return node;
}

// ----------------------------------------
//  Functions
// ----------------------------------------

ast_node_t* ast_new_func_def(const char* name, bool is_main, size_t line, size_t column)
{
    ast_node_t* node = new_node(AST_FUNC_DEF, line, column);
    if (node == NULL)
        return NULL;
    COPY_STR_OR_FAIL(node, node->func_def.name, name);
    node->func_def.is_main = is_main;
    node->func_def.return_type = NULL;
    node->func_def.body = NULL;

    INIT_ARR_OR_FAIL(node, &node->func_def.params);
    INIT_ARR_OR_FAIL(node, &node->func_def.locals);

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

ast_node_t* ast_new_block(size_t line, size_t column)
{
    ast_node_t* node = new_node(AST_BLOCK, line, column);
    if (node == NULL)
        return NULL;

    INIT_ARR_OR_FAIL(node, &node->block.statements);
    return node;
}

// ----------------------------------------
//  Statements constructors
// ----------------------------------------

ast_node_t* ast_new_var_def(const char* name, bool is_const, type_t* declared_type, ast_node_t* init, size_t line, size_t column)
{
    // Declared type can be NULL whne ": Type" is omitted
    if (name == NULL || init == NULL)
        return NULL;

    ast_node_t* node = new_node(AST_VAR_DEF, line, column);
    if (node == NULL)
        return NULL;

    COPY_STR_OR_FAIL(node, node->var_def.name, name);
    node->var_def.is_const = is_const;
    node->var_def.declared_type = declared_type;
    node->var_def.init = init;
    node->var_def.id = -1;

    return node;
}

ast_node_t* ast_new_assign(const char* name, ast_node_t* value, size_t line, size_t column)
{
    if (name == NULL || value == NULL)
        return NULL;

    ast_node_t* node = new_node(AST_ASSIGN, line, column);
    if (node == NULL)
        return NULL;

    COPY_STR_OR_FAIL(node, node->assign.name, name);
    node->assign.value = value;
    node->assign.id = -1;

    return node;
}

ast_node_t* ast_new_return(ast_node_t* value, size_t line, size_t column)
{
    // NULL is a valid return value
    ast_node_t* node = new_node(AST_RETURN, line, column);
    if (node == NULL)
        return NULL;

    node->return_stmt.value = value;
    return node;
}

ast_node_t* ast_new_if(ast_node_t* cond, ast_node_t* then_block, ast_node_t* else_block, size_t line, size_t column)
{
    // else_block can be NULL IF BOOLTHEN extension is being implemented
    if (cond == NULL || then_block == NULL)
        return NULL;

    ast_node_t* node = new_node(AST_IF, line, column);
    if (node == NULL)
        return NULL;

    node->if_stmt.cond = cond;
    node->if_stmt.then_block = then_block;
    node->if_stmt.else_block = else_block;
    return node;
}

ast_node_t* ast_new_while(ast_node_t* cond, ast_node_t* body, size_t line, size_t column)
{
    if (cond == NULL || body == NULL)
        return NULL;

    ast_node_t* node = new_node(AST_WHILE, line, column);
    if (node == NULL)
        return NULL;

    node->while_stmt.cond = cond;
    node->while_stmt.body = body;
    return node;
}

ast_node_t* ast_new_expr_stmt(ast_node_t* expr, size_t line, size_t column)
{
    if (expr == NULL)
        return NULL;

    ast_node_t* node = new_node(AST_EXPR_STMT, line, column);
    if (node == NULL)
        return NULL;

    node->expr_stmt.expr = expr;
    return node;
}

// ----------------------------------------
//  Expressions
// ----------------------------------------

ast_node_t* ast_new_int_lit(int64_t value, size_t line, size_t column)
{
    ast_node_t* node = new_node(AST_INT_LIT, line, column);
    if (node == NULL)
        return NULL;

    node->int_lit.value = value;
    return node;
}

ast_node_t* ast_new_double_lit(double value, size_t line, size_t column)
{
    ast_node_t* node = new_node(AST_DOUBLE_LIT, line, column);
    if (node == NULL)
        return NULL;

    node->double_lit.value = value;
    return node;
}

ast_node_t* ast_new_string_lit(const char* value, size_t line, size_t column)
{
    if (value == NULL)
        return NULL;

    ast_node_t* node = new_node(AST_STRING_LIT, line, column);
    if (node == NULL)
        return NULL;

    COPY_STR_OR_FAIL(node, node->string_lit.value, value);
    node->string_lit.length = strlen(value);

    return node;
}

ast_node_t* ast_new_unit_lit(size_t line, size_t column)
{
    ast_node_t* node = new_node(AST_UNIT_LIT, line, column);
    if (node == NULL)
        return NULL;
    return node;
}

ast_node_t* ast_new_bool_lit(bool value, size_t line, size_t column)
{
    ast_node_t* node = new_node(AST_BOOL_LIT, line, column);
    if (node == NULL)
        return NULL;

    node->bool_lit.value = value;
    return node;
}

ast_node_t* ast_new_var_ref(const char* name, size_t line, size_t column)
{
    if (name == NULL)
        return NULL;

    ast_node_t* node = new_node(AST_VAR_REF, line, column);
    if (node == NULL)
        return NULL;

    node->var_ref.id = -1;
    COPY_STR_OR_FAIL(node, node->var_ref.name, name);
    return node;
}

ast_node_t* ast_new_unary(ast_unary_op_t op, ast_node_t* operand, size_t line, size_t column)
{
    if (operand == NULL)
        return NULL;

    ast_node_t* node = new_node(AST_UNARY, line, column);
    if (node == NULL)
        return NULL;

    node->unary.op = op;
    node->unary.operand = operand;
    return node;
}

ast_node_t* ast_new_binary(ast_binary_op_t op, ast_node_t* left, ast_node_t* right, size_t line, size_t column)
{
    if (left == NULL || right == NULL)
        return NULL;

    ast_node_t* node = new_node(AST_BINARY, line, column);
    if (node == NULL)
        return NULL;

    node->binary.left = left;
    node->binary.op = op;
    node->binary.right = right;
    return node;
}

ast_node_t* ast_new_call(const char* name, size_t line, size_t column)
{
    if (name == NULL)
        return NULL;

    ast_node_t* node = new_node(AST_CALL, line, column);
    if (node == NULL)
        return NULL;

    COPY_STR_OR_FAIL(node, node->call.name, name);
    INIT_ARR_OR_FAIL(node, &node->call.args);
    return node;
}

ast_node_t* ast_new_convert(ast_node_t* operand)
{
    if (operand == NULL)
        return NULL;

    ast_node_t* node = new_node(AST_CONVERT, operand->line, operand->column);
    if (node == NULL)
        return NULL;

    node->convert.operand = operand;
    node->type = TYPE_DOUBLE;
    return node;
}

ast_node_t* ast_new_member(const char* name, ast_node_t* receiver, size_t line, size_t column)
{
    if (receiver == NULL || name == NULL)
        return NULL;

    ast_node_t *node = new_node(AST_MEMBER, line, column);
    if (node == NULL)
        return NULL;

    COPY_STR_OR_FAIL(node, node->member.name, name);
    node->member.receiver = receiver;

    return node;
}

// ----------------------------------------
//  Free AST (utility)
// ----------------------------------------

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

    case AST_BOOL_LIT:
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

    case AST_MEMBER:
        free(node->member.name);
        ast_free(node->member.receiver);
    }

    free(node);
}

// ----------------------------------------
//  Debug
// ----------------------------------------

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Implement the debug print function. Use a lookup table that you can
//          search in based on the node type and print the result - make it a little verbose
//          but not too much"
// Scope: KIND_NAMES, BINARY_OP_NAMES, print_list, print_node and ast_print

// AI-generated using: Claude Code
//              model: Claude Fable 5.2
// Prompt: "update ast_print so that it works with boolthen" + "Dont make the debug
//          print function too complicated make it print the same info as before"
// Scope: the BOOL_LIT, && and || entries of the tables below, UNARY_OP_NAMES and the
//        AST_BOOL_LIT and AST_UNARY cases in print_header
//
// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "update ast_print for MEMBER"
// Scope: the MEMBER entry of KIND_NAMES and the AST_MEMBER cases in print_header
//        and print_children

// Same order as ast_node_kind_t, ast_unary_op_t and ast_binary_op_t
static const char* KIND_NAMES[] = {
    "PROGRAM", "FUNC_DEF", "BLOCK",
    "VAR_DEF", "ASSIGN", "RETURN", "IF", "WHILE", "EXPR_STMT",
    "INT_LIT", "DOUBLE_LIT", "STRING_LIT", "UNIT_LIT", "BOOL_LIT", "VAR_REF", "UNARY", "BINARY", "CALL", "CONVERT",
    "MEMBER"
};
static const char* UNARY_OP_NAMES[] = { "-", "!" };
static const char* BINARY_OP_NAMES[] = { "+", "-", "*", "/", "<", "<=", ">", ">=", "==", "!=", "&&", "||" };

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "update the ast print function" (so that it also shows the fields
//          filled by the semantic analyzer: types, ids, is_builtin, inferred
//          return type, function params and locals count)
// Scope: type_name, print_id, print_header, print_children and the new print_node

static void print_node(const ast_node_t* node, FILE* out, int depth);

static void print_list(const dyn_arr_t* list, FILE* out, int depth)
{
    for (size_t i = 0; i < list->count; i++)
        print_node(dyn_arr_get(list, i), out, depth);
}

// Readable name of a type, "?" when it is not known (yet)
static const char* type_name(const type_t* type)
{
    if (type == NULL)
        return "?";

    switch (type->kind) {
    case T_INT:
        return "Int";
    case T_DOUBLE:
        return "Double";
    case T_STRING:
        return "String";
    case T_UNIT:
        return "Unit";
    case T_BOOL:
        return "Boolean";
    case T_ANY:
        return "Any";
    case T_UNION:
        return "Union";
    case T_LIST:
        return "List";
    case T_FUNC:
        return "Func";
    case T_CLASS:
        return "Class";
    }
    return "?";
}

// Variable id assigned by semantics (-1 = not resolved yet, printed as nothing)
static void print_id(int id, FILE* out)
{
    if (id >= 0)
        fprintf(out, " #%d", id);
}

// Kind specific data of the node (without position and children)
static void print_header(const ast_node_t* node, FILE* out)
{
    switch (node->kind) {
    case AST_FUNC_DEF: {
        const ast_func_def_t* fd = &node->func_def;
        fprintf(out, " %s%s(", fd->is_main ? "@main " : "", fd->name);
        for (size_t i = 0; i < fd->params.count; i++) {
            const ast_param_t* param = dyn_arr_get(&fd->params, i);
            fprintf(out, "%s%s", i > 0 ? ", " : "", param->name);
            print_id(param->id, out);
            fprintf(out, ": %s", type_name(param->type));
        }
        fprintf(out, ") -> %s", type_name(fd->return_type));
        if (fd->locals.count > 0)
            fprintf(out, " locals=%zu", fd->locals.count);
        break;
    }
    case AST_VAR_DEF:
        fprintf(out, " %s %s", node->var_def.is_const ? "val" : "var", node->var_def.name);
        print_id(node->var_def.id, out);
        if (node->var_def.declared_type != NULL)
            fprintf(out, " (declared %s)", type_name(node->var_def.declared_type));
        break;
    case AST_ASSIGN:
        fprintf(out, " %s", node->assign.name);
        print_id(node->assign.id, out);
        break;
    case AST_INT_LIT:
        fprintf(out, " %lld", (long long)node->int_lit.value);
        break;
    case AST_DOUBLE_LIT:
        fprintf(out, " %g", node->double_lit.value);
        break;
    case AST_STRING_LIT:
        fprintf(out, " \"%s\"", node->string_lit.value);
        break;
    case AST_BOOL_LIT:
        fprintf(out, " %s", node->bool_lit.value ? "true" : "false");
        break;
    case AST_VAR_REF:
        fprintf(out, " %s", node->var_ref.name);
        print_id(node->var_ref.id, out);
        break;
    case AST_UNARY:
        fprintf(out, " %s", UNARY_OP_NAMES[node->unary.op]);
        break;
    case AST_BINARY:
        fprintf(out, " %s", BINARY_OP_NAMES[node->binary.op]);
        break;
    case AST_CALL:
        fprintf(out, " %s%s", node->call.name, node->call.is_builtin ? " builtin" : "");
        break;
    case AST_MEMBER:
        fprintf(out, " .%s", node->member.name);
        break;
    default:
        break;
    }
}

static void print_children(const ast_node_t* node, FILE* out, int depth)
{
    switch (node->kind) {
    case AST_PROGRAM:
        print_list(&node->program.func_list, out, depth);
        break;
    case AST_FUNC_DEF:
        print_node(node->func_def.body, out, depth);
        break;
    case AST_BLOCK:
        print_list(&node->block.statements, out, depth);
        break;
    case AST_VAR_DEF:
        print_node(node->var_def.init, out, depth);
        break;
    case AST_ASSIGN:
        print_node(node->assign.value, out, depth);
        break;
    case AST_RETURN:
        if (node->return_stmt.value != NULL)
            print_node(node->return_stmt.value, out, depth);
        break;
    case AST_IF:
        print_node(node->if_stmt.cond, out, depth);
        print_node(node->if_stmt.then_block, out, depth);
        print_node(node->if_stmt.else_block, out, depth);
        break;
    case AST_WHILE:
        print_node(node->while_stmt.cond, out, depth);
        print_node(node->while_stmt.body, out, depth);
        break;
    case AST_EXPR_STMT:
        print_node(node->expr_stmt.expr, out, depth);
        break;
    case AST_UNARY:
        print_node(node->unary.operand, out, depth);
        break;
    case AST_BINARY:
        print_node(node->binary.left, out, depth);
        print_node(node->binary.right, out, depth);
        break;
    case AST_CALL:
        print_list(&node->call.args, out, depth);
        break;
    case AST_CONVERT:
        print_node(node->convert.operand, out, depth);
        break;
    case AST_MEMBER:
        print_node(node->member.receiver, out, depth);
        break;
    default: // literals and variable references have no children
        break;
    }
}

// Prints one node per line: indentation, kind, its own data, ": Type" once semantics
// filled it, [line:column], then its children one level deeper
static void print_node(const ast_node_t* node, FILE* out, int depth)
{
    fprintf(out, "%*s", depth * 2, "");
    if (node == NULL) {
        fprintf(out, "(null)\n");
        return;
    }

    fprintf(out, "%s", KIND_NAMES[node->kind]);
    print_header(node, out);
    if (node->type != NULL)
        fprintf(out, " : %s", type_name(node->type));
    fprintf(out, " [%zu:%zu]\n", node->line, node->column);

    print_children(node, out, depth + 1);
}

void ast_print(const ast_node_t* node, FILE* out)
{
    print_node(node, out, 0);
}
