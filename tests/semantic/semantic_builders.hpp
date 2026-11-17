// Implementation of the IFJ26 imperative language compiler
// semantic_builders.hpp
// Author(s): Adam Zatloukal (xzatloa01)
//
// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Create an ast for an avl tree based on the code in symtable.c and perform
//          semantic analysis on it" (the AST builders shared by the semantic test
//          files, moved here from semantic_pass2_test.cpp)
// Scope: the whole file
//
// Small builders so a hand-made AST reads like the IFJ26 source it represents.
// Positions do not matter for semantics, every node is at 1:1.

#ifndef SEMANTIC_BUILDERS_HPP_
#define SEMANTIC_BUILDERS_HPP_

extern "C" {
#include "ast.h"
#include "dynamic_array.h"
}

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

using node_list_t = std::vector<ast_node_t*>;
using param_list_t = std::vector<std::pair<const char*, type_t*>>;

// ----------------------------------------
//  Expressions
// ----------------------------------------

inline ast_node_t* I(int64_t value) { return ast_new_int_lit(value, 1, 1); }
inline ast_node_t* D(double value) { return ast_new_double_lit(value, 1, 1); }
inline ast_node_t* S(const char* value) { return ast_new_string_lit(value, 1, 1); }
inline ast_node_t* U() { return ast_new_unit_lit(1, 1); }
inline ast_node_t* V(const char* name) { return ast_new_var_ref(name, 1, 1); }
inline ast_node_t* neg(ast_node_t* operand) { return ast_new_unary(AST_OP_NEG, operand, 1, 1); }

// BOOLTHEN: true / false, !<operand>
inline ast_node_t* B(bool value) { return ast_new_bool_lit(value, 1, 1); }
inline ast_node_t* not_(ast_node_t* operand) { return ast_new_unary(AST_OP_NOT, operand, 1, 1); }

// VALMEM: <receiver>.<name>   - receiver first, so M(V("s"), "length") reads like s.length
inline ast_node_t* M(ast_node_t* receiver, const char* name) { return ast_new_member(name, receiver, 1, 1); }

inline ast_node_t* bin(ast_binary_op_t op, ast_node_t* left, ast_node_t* right)
{
    return ast_new_binary(op, left, right, 1, 1);
}

// 1 < 2 - a Boolean expression usable as a condition
inline ast_node_t* cond() { return bin(AST_OP_LT, I(1), I(2)); }

inline ast_node_t* call(const char* name, const node_list_t& args = {})
{
    ast_node_t* node = ast_new_call(name, 1, 1);
    EXPECT_NE(node, nullptr);
    for (ast_node_t* arg : args)
        EXPECT_TRUE(dyn_arr_append(&node->call.args, arg));
    return node;
}

// ----------------------------------------
//  Statements
// ----------------------------------------

inline ast_node_t* expr(ast_node_t* e) { return ast_new_expr_stmt(e, 1, 1); }
inline ast_node_t* assign(const char* name, ast_node_t* value) { return ast_new_assign(name, value, 1, 1); }
inline ast_node_t* ret(ast_node_t* value = nullptr) { return ast_new_return(value, 1, 1); }

// val <name> [: type] = <init>
inline ast_node_t* val(const char* name, ast_node_t* init, type_t* type = nullptr)
{
    return ast_new_var_def(name, true, type, init, 1, 1);
}

// var <name> [: type] = <init>
inline ast_node_t* var(const char* name, ast_node_t* init, type_t* type = nullptr)
{
    return ast_new_var_def(name, false, type, init, 1, 1);
}

inline ast_node_t* block(const node_list_t& stmts)
{
    ast_node_t* node = ast_new_block(1, 1);
    EXPECT_NE(node, nullptr);
    for (ast_node_t* stmt : stmts)
        EXPECT_TRUE(dyn_arr_append(&node->block.statements, stmt));
    return node;
}

// if (<c>) { <then> }      - without else (BOOLTHEN)
inline ast_node_t* if_(ast_node_t* c, const node_list_t& then_stmts)
{
    return ast_new_if(c, block(then_stmts), nullptr, 1, 1);
}

// if (<c>) { <then> } else { <else> }
inline ast_node_t* if_else(ast_node_t* c, const node_list_t& then_stmts, const node_list_t& else_stmts)
{
    return ast_new_if(c, block(then_stmts), block(else_stmts), 1, 1);
}

// while (<c>) { <body> }
inline ast_node_t* while_(ast_node_t* c, const node_list_t& body)
{
    return ast_new_while(c, block(body), 1, 1);
}

// AI-generated using: Claude Code, model Claude Opus 5.5
// Prompt: "Update the tests and write tests for the control" (shared for builder)
// Scope: for_ only
//
// CONTROL: for <name> <- <from> to <to> [by <step>] [if <guard>]... do { <body> }
//          inclusive == false -> until, step == nullptr -> no by
inline ast_node_t* for_(const char* name, ast_node_t* from, ast_node_t* to, ast_node_t* step, bool inclusive,
    const node_list_t& guards, const node_list_t& body)
{
    ast_node_t* node = ast_new_for(name, from, to, step, inclusive, 1, 1);
    EXPECT_NE(node, nullptr);
    for (ast_node_t* guard : guards)
        EXPECT_TRUE(dyn_arr_append(&node->for_stmt.guards, guard));
    node->for_stmt.body = block(body);
    return node;
}

// [@main] def <name>(<params>): <ret_type> = { <body> }      ret_type == nullptr -> not written
inline ast_node_t* make_func(const char* name, type_t* ret_type, const param_list_t& params, const node_list_t& body,
    bool is_main = false)
{
    ast_node_t* fn = ast_new_func_def(name, is_main, 1, 1);
    EXPECT_NE(fn, nullptr);
    for (const auto& param : params)
        EXPECT_TRUE(ast_func_def_add_param(fn, param.first, param.second));
    fn->func_def.return_type = ret_type;
    fn->func_def.body = block(body);
    return fn;
}

// ----------------------------------------
//  Inspection
// ----------------------------------------

inline std::string kind_name(type_kind_t kind)
{
    switch (kind) {
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
    default:
        return "Other";
    }
}

// Readable type for EXPECT_EQ messages, "NULL" when semantics did not set it
inline std::string type_name(const type_t* type) { return type == nullptr ? "NULL" : kind_name(type->kind); }

inline ast_node_t* stmt_at(ast_node_t* func_def, size_t i)
{
    return static_cast<ast_node_t*>(dyn_arr_get(&func_def->func_def.body->block.statements, i));
}

inline ast_node_t* arg_at(ast_node_t* call_node, size_t i)
{
    return static_cast<ast_node_t*>(dyn_arr_get(&call_node->call.args, i));
}

// node is a Double CONVERT node wrapping a node of kind inner
inline bool is_convert_of(const ast_node_t* node, ast_node_kind_t inner)
{
    return node != nullptr && node->kind == AST_CONVERT && node->type == TYPE_DOUBLE
        && node->convert.operand != nullptr && node->convert.operand->kind == inner;
}

#endif // SEMANTIC_BUILDERS_HPP_
