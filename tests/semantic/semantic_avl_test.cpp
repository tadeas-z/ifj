// Implementation of the IFJ26 imperative language compiler
// semantic_avl_test.cpp
// Author(s): Adam Zatloukal (xzatloa01)
//
// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Create an ast for an avl tree based on the code in symtable.c and perform
//          semantic analysis on. make tests where it passes and tests where it fails
//          for all different kinds of reasons"
// Scope: the whole file
//
// This is TEST INPUT for the semantic analyzer: an IFJ26 program (written below as a
// hand-made AST) whose functions mirror the shape of the AVL tree in symtable.c. It is
// not used by the compiler and does not replace or touch symtable.c.
//
// IFJ26 has no structs or pointers, so the tree lives in a String used as memory:
// node n takes the 4 characters at 4*n (key, left, right, height), read with ord and
// written with subStr + chr. Node 0 is NULL. Its key slot is used like out_node in
// bst_insert - functions that change the tree return the new memory and store the
// resulting subtree root there, result(mem) reads it.
//
// One valid program, every failing test changes exactly one line of it (a variant).
// Each variant is analyzed with three different function orders, the result must not
// depend on the order in which functions are checked.

extern "C" {
#include "ast.h"
#include "dynamic_array.h"
#include "semantic.h"
}

#include <gtest/gtest.h>

#include "semantic_builders.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <tuple>
#include <vector>

// ----------------------------------------
//  Allocation failure injection
// ----------------------------------------
// Linked with -Wl,--wrap=calloc. While calloc_countdown is >= 0 it is decremented
// on every call and the call that brings it from 0 fails. -1 means "never fail".
static int calloc_countdown = -1;

extern "C" void* __real_calloc(size_t count, size_t size);

extern "C" void* __wrap_calloc(size_t count, size_t size)
{
    if (calloc_countdown >= 0) {
        if (calloc_countdown == 0) {
            calloc_countdown = -1;
            return nullptr;
        }
        calloc_countdown--;
    }
    return __real_calloc(count, size);
}

// ========================================
//  The program
// ========================================
//
// def get(mem: String, node: Int, field: Int): Int = { ord(mem, 4 * node + field) }
//
// def set(mem: String, node: Int, field: Int, value: Int): String = {
//     val pos = 4 * node + field
//     subStr(mem, 0, pos) + chr(value) + subStr(mem, pos + 1, strLength(mem))
// }
//
// def result(mem: String) = { get(mem, 0, 0) }
//
// def max(n1: Int, n2: Int): Int = { if (n1 > n2) { return n1 } else { return n2 } }
//
// def get_height(mem: String, node: Int) = {
//     var h = 0
//     if (node != 0) { h = get(mem, node, 3) }
//     h
// }
//
// def update_height(mem: String, node: Int): String = {
//     if (node == 0) { return mem }
//     val h = max(get_height(mem, get(mem, node, 1)), get_height(mem, get(mem, node, 2))) + 1
//     set(mem, node, 3, h)
// }
//
// def balance_factor(mem: String, node: Int) = {
//     get_height(mem, get(mem, node, 2)) - get_height(mem, get(mem, node, 1))
// }
//
// def rotate_left(mem: String, old_root: Int): String = {      rotate_right: 1 <-> 2, old_right
//     if (old_root == 0) { return set(mem, 0, 0, old_root) }
//     val new_root = get(mem, old_root, 2)
//     if (new_root == 0) { return set(mem, 0, 0, old_root) }
//     val old_left = get(mem, new_root, 1)
//     var m = set(mem, new_root, 1, old_root)
//     m = set(m, old_root, 2, old_left)
//     m = update_height(m, old_root)
//     m = update_height(m, new_root)
//     set(m, 0, 0, new_root)
// }
//
// def left_left_case(mem: String, node: Int) = { rotate_right(mem, node) }
// def left_right_case(mem: String, node: Int) = {               right_left_case: mirrored
//     val m = rotate_left(mem, get(mem, node, 1))
//     val m2 = set(m, node, 1, result(m))
//     rotate_right(m2, node)
// }
// def right_right_case(mem: String, node: Int) = { rotate_left(mem, node) }
//
// def rebalance(mem: String, node: Int): String = {
//     val m = update_height(mem, node)
//     val bf = balance_factor(m, node)
//     if (bf == -2) {
//         if (balance_factor(m, get(m, node, 1)) <= 0) { return left_left_case(m, node) }
//         else { return left_right_case(m, node) }
//     }
//     if (bf == 2) {
//         if (balance_factor(m, get(m, node, 2)) >= 0) { return right_right_case(m, node) }
//         else { return right_left_case(m, node) }
//     }
//     set(m, 0, 0, node)
// }
//
// def allocate_node(mem: String, key: Int) = {
//     val index = strLength(mem) / 4
//     val m = mem + chr(key) + chr(0) + chr(0) + chr(1)
//     set(m, 0, 0, index)
// }
//
// def bst_insert(mem: String, node: Int, key: Int): String = {
//     if (node == 0) { return allocate_node(mem, key) }
//     val node_key = get(mem, node, 0)
//     var m = mem
//     if (key < node_key) {
//         m = bst_insert(m, get(m, node, 1), key)
//         m = set(m, node, 1, result(m))
//     } else {
//         if (key > node_key) {
//             m = bst_insert(m, get(m, node, 2), key)
//             m = set(m, node, 2, result(m))
//         } else {
//             return set(m, 0, 0, node)                              // already present
//         }
//     }
//     rebalance(m, node)
// }
//
// def bst_find(mem: String, root: Int, key: Int): Int = {
//     var node = root
//     while (node != 0) {
//         val node_key = get(mem, node, 0)
//         if (key == node_key) { return node }
//         if (key < node_key) { node = get(mem, node, 1) } else { node = get(mem, node, 2) }
//     }
//     0
// }
//
// def in_order(mem: String, node: Int): Unit = {
//     if (node != 0) {
//         in_order(mem, get(mem, node, 1))
//         print(get(mem, node, 0))
//         print(" ")
//         in_order(mem, get(mem, node, 2))
//     }
// }
//
// def is_balanced(mem: String, node: Int): Boolean = {
//     if (node == 0) { return true }
//     val bf = balance_factor(mem, node)
//     if (bf < -1) { return false }
//     if (bf > 1) { return false }
//     if (is_balanced(mem, get(mem, node, 1))) { return is_balanced(mem, get(mem, node, 2)) }
//     else { return false }
// }
//
// @main def main(): Unit = {
//     var mem = chr(0) + chr(0) + chr(0) + chr(0)
//     var root = 0
//     var i = 1
//     val count = readInt()
//     while (i <= count) {
//         mem = bst_insert(mem, root, i)
//         root = result(mem)
//         i = i + 1
//     }
//     in_order(mem, root)
//     print("\n")
//     val height = get_height(mem, root)
//     val ratio: Double = height
//     print(toStr(ratio / 2))
//     val found = bst_find(mem, root, readInt())
//     if (found != 0) { print("found") } else { print("not found") }
//     val ok = is_balanced(mem, root)
//     if (ok) { print("balanced") }
// }

// Which single line differs from the valid program (see the table of failing tests)
enum avl_variant_t {
    VALID,

    // Valid alternatives
    P_MAX_INFERRED,
    P_ALL_EXPLICIT,
    P_BOOLEAN_FLAG,
    P_BOOL_OPERATORS,
    P_VALMEM,

    // 3 - undefined
    U_TYPO_VARIABLE,
    U_TYPO_FUNCTION,
    U_CALLER_VARIABLE,
    U_OUT_OF_SCOPE,
    U_USE_BEFORE_DEF,
    U_NO_MAIN,

    // 4 - redefinition
    R_DUP_FUNCTION,
    R_DUP_PARAM,
    R_DUP_LOCAL,
    R_BUILTIN_NAME,
    R_TWO_MAINS,

    // 5 - call
    C_ARG_COUNT,
    C_ARG_TYPE,
    C_BUILTIN_ARG_TYPE,
    C_BUILTIN_ARG_COUNT,
    C_MAIN_PARAMS,

    // 6 - type
    T_STRING_PLUS_INT,
    T_ASSIGN_TYPE,
    T_DOUBLE_TO_INT,
    T_DECLARED_TYPE,
    T_CONDITION_NOT_BOOL,
    T_RETURN_TYPE,
    T_BODY_TYPE,
    T_RECURSION_INFERRED,
    T_RETURN_IN_INFERRED,
    T_RULE3_MISSING_RETURN,
    T_RULE3_INFERRED_VALUES,
    T_MAIN_NON_UNIT,
    T_NEG_STRING,
    T_COMPARE_INT_STRING,
    T_BOOL_ARITHMETIC,
    T_NOT_INT,
    T_AND_INT,
    T_MEMBER_RECEIVER,
    U_UNKNOWN_MEMBER,

    // 7 - assignment
    A_ASSIGN_VAL,
    A_ASSIGN_PARAM,

    // 10 - other
    O_BARE_RETURN,
};

// In which order the functions are put into program->func_list
enum avl_order_t {
    ORDER_SOURCE, // as listed above, main last
    ORDER_MAIN_FIRST, // main first - everything is checked lazily through calls
    ORDER_REVERSED, // reversed - nearly every call goes to a function defined later
};

// The built program plus handles to the nodes some tests inspect
struct avl_program_t {
    ast_node_t* program = nullptr;
    std::map<std::string, ast_node_t*> funcs; // by name
    ast_node_t* ratio_def = nullptr; // val ratio: Double = height
    ast_node_t* ratio_div = nullptr; // ratio / 2
    ast_node_t* insert_call = nullptr; // bst_insert(mem, root, i) in main
    ast_node_t* print_call = nullptr; // print("\n") in main
};

enum { KEY = 0, LEFT = 1, RIGHT = 2, HEIGHT = 3 };

// Shorthands for the program
static ast_node_t* eq_(ast_node_t* l, ast_node_t* r) { return bin(AST_OP_EQ, l, r); }
static ast_node_t* ne_(ast_node_t* l, ast_node_t* r) { return bin(AST_OP_NE, l, r); }
static ast_node_t* lt_(ast_node_t* l, ast_node_t* r) { return bin(AST_OP_LT, l, r); }
static ast_node_t* le_(ast_node_t* l, ast_node_t* r) { return bin(AST_OP_LE, l, r); }
static ast_node_t* gt_(ast_node_t* l, ast_node_t* r) { return bin(AST_OP_GT, l, r); }
static ast_node_t* ge_(ast_node_t* l, ast_node_t* r) { return bin(AST_OP_GE, l, r); }
static ast_node_t* add_(ast_node_t* l, ast_node_t* r) { return bin(AST_OP_ADD, l, r); }
static ast_node_t* sub_(ast_node_t* l, ast_node_t* r) { return bin(AST_OP_SUB, l, r); }
static ast_node_t* mul_(ast_node_t* l, ast_node_t* r) { return bin(AST_OP_MUL, l, r); }
static ast_node_t* div_(ast_node_t* l, ast_node_t* r) { return bin(AST_OP_DIV, l, r); }
static ast_node_t* true_() { return B(true); }
static ast_node_t* false_() { return B(false); }
static ast_node_t* chr_(int64_t code) { return call("chr", { I(code) }); }
static ast_node_t* print_(const char* text) { return expr(call("print", { S(text) })); }

// get(<mem>, <node>, field)
static ast_node_t* get_(ast_node_t* mem, ast_node_t* node, int field) { return call("get", { mem, node, I(field) }); }

// set(<mem>, <node>, field, <value>)
static ast_node_t* set_(ast_node_t* mem, ast_node_t* node, int field, ast_node_t* value)
{
    return call("set", { mem, node, I(field), value });
}

// Builds the program in variant v. Only the selected side of every "v == X ? .. : .."
// is evaluated, so no node is built and then left unused.
static avl_program_t build_avl(avl_variant_t v, avl_order_t order = ORDER_SOURCE)
{
    avl_program_t avl;
    avl.program = ast_new_program();
    EXPECT_NE(avl.program, nullptr);

    std::vector<ast_node_t*> funcs; // source order
    auto def = [&](const char* name, type_t* ret_type, const param_list_t& params, const node_list_t& body,
                   bool is_main = false) {
        ast_node_t* fn = make_func(name, ret_type, params, body, is_main);
        funcs.push_back(fn);
        avl.funcs[name] = fn;
    };
    const param_list_t mem_node = { { "mem", TYPE_STRING }, { "node", TYPE_INT } };
    const bool all_explicit = (v == P_ALL_EXPLICIT);

    // def get(mem: String, node: Int, field: Int): Int = { ord(mem, 4 * node + field) }
    //   R_BUILTIN_NAME:     def ord(...)
    //   C_BUILTIN_ARG_TYPE: ord(mem, 4.0 * node + field)
    def(v == R_BUILTIN_NAME ? "ord" : "get", TYPE_INT, { { "mem", TYPE_STRING }, { "node", TYPE_INT }, { "field", TYPE_INT } },
        { expr(call("ord", { V("mem"), add_(mul_(v == C_BUILTIN_ARG_TYPE ? D(4.0) : I(4), V("node")), V("field")) })) });

    // def set(mem: String, node: Int, field: Int, value: Int): String
    //   P_VALMEM: subStr(mem, pos + 1, mem.length)
    //   R_DUP_PARAM: set(mem: String, node: Int, field: Int, field: Int)
    def("set", TYPE_STRING,
        { { "mem", TYPE_STRING }, { "node", TYPE_INT }, { "field", TYPE_INT }, { v == R_DUP_PARAM ? "field" : "value", TYPE_INT } },
        { val("pos", add_(mul_(I(4), V("node")), V("field"))),
            expr(add_(add_(call("subStr", { V("mem"), I(0), V("pos") }), call("chr", { V("value") })),
                call("subStr", { V("mem"), add_(V("pos"), I(1)),
                    v == P_VALMEM ? M(V("mem"), "length") : call("strLength", { V("mem") }) }))) });

    // def result(mem: String) = { get(mem, 0, 0) }
    def("result", all_explicit ? TYPE_INT : nullptr, { { "mem", TYPE_STRING } }, { expr(get_(V("mem"), I(0), KEY)) });

    // def max(n1: Int, n2: Int): Int = { if (n1 > n2) { return n1 } else { return n2 } }
    //   T_RULE3_MISSING_RETURN: ... else { n2 }
    //   P_MAX_INFERRED:         def max(n1: Int, n2: Int) = { var m = n2; if (n1 > n2) { m = n1 }; m }
    const param_list_t n1_n2 = { { "n1", TYPE_INT }, { "n2", TYPE_INT } };
    if (v == P_MAX_INFERRED)
        def("max", nullptr, n1_n2,
            { var("m", V("n2")), if_(gt_(V("n1"), V("n2")), { assign("m", V("n1")) }), expr(V("m")) });
    else
        def("max", TYPE_INT, n1_n2,
            { if_else(gt_(V("n1"), V("n2")), { ret(V("n1")) },
                { v == T_RULE3_MISSING_RETURN ? expr(V("n2")) : ret(V("n2")) }) });

    // def get_height(mem: String, node: Int) = { var h = 0; if (node != 0) { h = get(mem, node, 3) }; h }
    //   T_BODY_TYPE:             def get_height(mem: String, node: Int): String = ...
    //   T_RULE3_INFERRED_VALUES: def get_height(mem: String, node: Int) = { if (node == 0) { 0 } else { get(mem, node, 3) } }
    if (v == T_RULE3_INFERRED_VALUES)
        def("get_height", nullptr, mem_node,
            { if_else(eq_(V("node"), I(0)), { expr(I(0)) }, { expr(get_(V("mem"), V("node"), HEIGHT)) }) });
    else
        def("get_height", v == T_BODY_TYPE ? TYPE_STRING : (all_explicit ? TYPE_INT : nullptr), mem_node,
            { var("h", I(0)), if_(ne_(V("node"), I(0)), { assign("h", get_(V("mem"), V("node"), HEIGHT)) }), expr(V("h")) });

    // def update_height(mem: String, node: Int): String
    //   C_ARG_TYPE: set(mem, node, 3, toStr(h))
    def("update_height", TYPE_STRING, mem_node,
        { if_(eq_(V("node"), I(0)), { ret(V("mem")) }),
            val("h",
                add_(call("max",
                         { call("get_height", { V("mem"), get_(V("mem"), V("node"), LEFT) }),
                             call("get_height", { V("mem"), get_(V("mem"), V("node"), RIGHT) }) }),
                    I(1))),
            expr(set_(V("mem"), V("node"), HEIGHT, v == C_ARG_TYPE ? call("toStr", { V("h") }) : V("h"))) });

    // def balance_factor(mem: String, node: Int) = { get_height(mem, right) - get_height(mem, left) }
    def("balance_factor", all_explicit ? TYPE_INT : nullptr, mem_node,
        { expr(sub_(call("get_height", { V("mem"), get_(V("mem"), V("node"), RIGHT) }),
            call("get_height", { V("mem"), get_(V("mem"), V("node"), LEFT) }))) });

    // def rotate_left / rotate_right(mem: String, old_root: Int): String
    //   child = side the new root comes from, inner = the subtree that changes parent
    //   O_BARE_RETURN:   rotate_left: if (old_root == 0) { return }
    //   T_RETURN_TYPE:   rotate_left: if (old_root == 0) { return old_root }
    //   U_TYPO_VARIABLE: rotate_left: val old_left = get(mem, new_rot, 1)
    auto rotation = [&](const char* name, int child, int inner, const char* inner_name, bool is_left) {
        ast_node_t* guard_ret;
        if (is_left && v == O_BARE_RETURN)
            guard_ret = ret();
        else if (is_left && v == T_RETURN_TYPE)
            guard_ret = ret(V("old_root"));
        else
            guard_ret = ret(set_(V("mem"), I(0), KEY, V("old_root")));

        def(name, TYPE_STRING, { { "mem", TYPE_STRING }, { "old_root", TYPE_INT } },
            { if_(eq_(V("old_root"), I(0)), { guard_ret }),
                val("new_root", get_(V("mem"), V("old_root"), child)),
                if_(eq_(V("new_root"), I(0)), { ret(set_(V("mem"), I(0), KEY, V("old_root"))) }),
                val(inner_name, get_(V("mem"), V(is_left && v == U_TYPO_VARIABLE ? "new_rot" : "new_root"), inner)),
                var("m", set_(V("mem"), V("new_root"), inner, V("old_root"))),
                assign("m", set_(V("m"), V("old_root"), child, V(inner_name))),
                assign("m", call("update_height", { V("m"), V("old_root") })),
                assign("m", call("update_height", { V("m"), V("new_root") })),
                expr(set_(V("m"), I(0), KEY, V("new_root"))) });
    };
    rotation("rotate_left", RIGHT, LEFT, "old_left", true);
    rotation("rotate_right", LEFT, RIGHT, "old_right", false);

    // The four rebalance cases
    //   R_DUP_LOCAL:          left_right_case: val m = ...; val m = set(...)
    //   T_RETURN_IN_INFERRED: left_right_case: return rotate_right(m2, node)
    type_t* case_type = all_explicit ? TYPE_STRING : nullptr;
    def("left_left_case", case_type, mem_node, { expr(call("rotate_right", { V("mem"), V("node") })) });

    ast_node_t* lr_last = call("rotate_right", { V("m2"), V("node") });
    def("left_right_case", case_type, mem_node,
        { val("m", call("rotate_left", { V("mem"), get_(V("mem"), V("node"), LEFT) })),
            val(v == R_DUP_LOCAL ? "m" : "m2", set_(V("m"), V("node"), LEFT, call("result", { V("m") }))),
            v == T_RETURN_IN_INFERRED ? ret(lr_last) : expr(lr_last) });

    def("right_left_case", case_type, mem_node,
        { val("m", call("rotate_right", { V("mem"), get_(V("mem"), V("node"), RIGHT) })),
            val("m2", set_(V("m"), V("node"), RIGHT, call("result", { V("m") }))),
            expr(call("rotate_left", { V("m2"), V("node") })) });

    def("right_right_case", case_type, mem_node, { expr(call("rotate_left", { V("mem"), V("node") })) });

    // def rebalance(mem: String, node: Int): String
    //   T_NEG_STRING:    if (bf == -"2")
    //   U_TYPO_FUNCTION: return left_rigth_case(m, node)
    def("rebalance", TYPE_STRING, mem_node,
        { val("m", call("update_height", { V("mem"), V("node") })),
            val("bf", call("balance_factor", { V("m"), V("node") })),
            if_(eq_(V("bf"), v == T_NEG_STRING ? neg(S("2")) : neg(I(2))),
                { if_else(le_(call("balance_factor", { V("m"), get_(V("m"), V("node"), LEFT) }), I(0)),
                    { ret(call("left_left_case", { V("m"), V("node") })) },
                    { ret(call(v == U_TYPO_FUNCTION ? "left_rigth_case" : "left_right_case", { V("m"), V("node") })) }) }),
            if_(eq_(V("bf"), I(2)),
                { if_else(ge_(call("balance_factor", { V("m"), get_(V("m"), V("node"), RIGHT) }), I(0)),
                    { ret(call("right_right_case", { V("m"), V("node") })) },
                    { ret(call("right_left_case", { V("m"), V("node") })) }) }),
            expr(set_(V("m"), I(0), KEY, V("node"))) });

    // def allocate_node(mem: String, key: Int) = { ... }
    //   P_VALMEM:            val index = mem.length / 4
    //   T_MEMBER_RECEIVER:   val index = mem.toInt / 4      (toInt needs a Double)
    //   U_UNKNOWN_MEMBER:    val index = mem.size / 4       (not a member)
    //   U_USE_BEFORE_DEF:    val index = strLength(m) / 4       (m is defined on the next line)
    //   C_BUILTIN_ARG_COUNT: val index = strLength() / 4
    //   T_STRING_PLUS_INT:   val m = mem + key + chr(0) + ...
    def("allocate_node", all_explicit ? TYPE_STRING : nullptr, { { "mem", TYPE_STRING }, { "key", TYPE_INT } },
        { val("index",
              div_(v == P_VALMEM                ? M(V("mem"), "length")
                      : v == T_MEMBER_RECEIVER     ? M(V("mem"), "toInt")
                      : v == U_UNKNOWN_MEMBER      ? M(V("mem"), "size")
                      : v == C_BUILTIN_ARG_COUNT   ? call("strLength")
                                                   : call("strLength", { V(v == U_USE_BEFORE_DEF ? "m" : "mem") }),
                  I(4))),
            val("m",
                add_(add_(add_(v == T_STRING_PLUS_INT ? add_(V("mem"), V("key")) : add_(V("mem"), call("chr", { V("key") })),
                              chr_(0)),
                         chr_(0)),
                    chr_(1))),
            expr(set_(V("m"), I(0), KEY, V("index"))) });

    // def bst_insert(mem: String, node: Int, key: Int): String
    //   C_ARG_COUNT: m = bst_insert(m, key)
    def("bst_insert", TYPE_STRING, { { "mem", TYPE_STRING }, { "node", TYPE_INT }, { "key", TYPE_INT } },
        { if_(eq_(V("node"), I(0)), { ret(call("allocate_node", { V("mem"), V("key") })) }),
            val("node_key", get_(V("mem"), V("node"), KEY)),
            var("m", V("mem")),
            if_else(lt_(V("key"), V("node_key")),
                { assign("m",
                      v == C_ARG_COUNT ? call("bst_insert", { V("m"), V("key") })
                                       : call("bst_insert", { V("m"), get_(V("m"), V("node"), LEFT), V("key") })),
                    assign("m", set_(V("m"), V("node"), LEFT, call("result", { V("m") }))) },
                { if_else(gt_(V("key"), V("node_key")),
                    { assign("m", call("bst_insert", { V("m"), get_(V("m"), V("node"), RIGHT), V("key") })),
                        assign("m", set_(V("m"), V("node"), RIGHT, call("result", { V("m") }))) },
                    { ret(set_(V("m"), I(0), KEY, V("node"))) }) }),
            expr(call("rebalance", { V("m"), V("node") })) });

    // def bst_find(mem: String, root: Int, key: Int): Int
    //   A_ASSIGN_PARAM:       def bst_find(mem: String, node: Int, key: Int) - and the loop moves the PARAM
    //                         (the way bst_find in symtable.c moves its node argument)
    //   A_ASSIGN_VAL:         if (key < node_key) { node_key = get(mem, node, 1) }
    //   T_COMPARE_INT_STRING: if (key == mem) { return node }
    //   U_OUT_OF_SCOPE:       the last line is node_key instead of 0
    //   P_BOOLEAN_FLAG:       the loop runs on a Boolean variable instead of returning from inside
    if (v == P_BOOLEAN_FLAG) {
        // var node = root; var found = 0; var searching = node != 0
        // while (searching) {
        //     val node_key = get(mem, node, 0)
        //     if (key == node_key) { found = node; searching = false }
        //     else { if (key < node_key) { node = get(mem, node, 1) } else { node = get(mem, node, 2) }; searching = node != 0 }
        // }
        // found
        def("bst_find", TYPE_INT, { { "mem", TYPE_STRING }, { "root", TYPE_INT }, { "key", TYPE_INT } },
            { var("node", V("root")), var("found", I(0)), var("searching", ne_(V("node"), I(0))),
                while_(V("searching"),
                    { val("node_key", get_(V("mem"), V("node"), KEY)),
                        if_else(eq_(V("key"), V("node_key")), { assign("found", V("node")), assign("searching", false_()) },
                            { if_else(lt_(V("key"), V("node_key")), { assign("node", get_(V("mem"), V("node"), LEFT)) },
                                  { assign("node", get_(V("mem"), V("node"), RIGHT)) }),
                                assign("searching", ne_(V("node"), I(0))) }) }),
                expr(V("found")) });
    } else {
        node_list_t body;
        if (v != A_ASSIGN_PARAM)
            body.push_back(var("node", V("root")));
        body.push_back(while_(ne_(V("node"), I(0)),
            { val("node_key", get_(V("mem"), V("node"), KEY)),
                if_(eq_(V("key"), V(v == T_COMPARE_INT_STRING ? "mem" : "node_key")), { ret(V("node")) }),
                if_else(lt_(V("key"), V("node_key")),
                    { assign(v == A_ASSIGN_VAL ? "node_key" : "node", get_(V("mem"), V("node"), LEFT)) },
                    { assign("node", get_(V("mem"), V("node"), RIGHT)) }) }));
        body.push_back(expr(v == U_OUT_OF_SCOPE ? V("node_key") : I(0)));
        def("bst_find", TYPE_INT,
            { { "mem", TYPE_STRING }, { v == A_ASSIGN_PARAM ? "node" : "root", TYPE_INT }, { "key", TYPE_INT } }, body);
    }

    // def in_order(mem: String, node: Int): Unit
    //   T_RECURSION_INFERRED: def in_order(mem: String, node: Int) = ...   (recursive, so the type must be written)
    //   U_CALLER_VARIABLE:    print(root)                                  (root is main's variable)
    def("in_order", v == T_RECURSION_INFERRED ? nullptr : TYPE_UNIT, mem_node,
        { if_(ne_(V("node"), I(0)),
            { expr(call("in_order", { V("mem"), get_(V("mem"), V("node"), LEFT) })),
                expr(call("print", { get_(V("mem"), V("node"), KEY) })),
                expr(call("print", { v == U_CALLER_VARIABLE ? V("root") : S(" ") })),
                expr(call("in_order", { V("mem"), get_(V("mem"), V("node"), RIGHT) })) }) });

    // def is_balanced(mem: String, node: Int): Boolean   (BOOLTHEN: a Boolean result)
    //   P_BOOL_OPERATORS: the same check written with ! && || (AI-generated using: Claude Code,
    //                     model: Claude Fable 5.2, prompt: "can you also add the proposed tests for boolthen")
    //       if (node == 0) { return true }
    //       val bf = balance_factor(mem, node)
    //       val ok = !(bf < -1 || bf > 1)
    //       ok && is_balanced(mem, get(mem, node, 1)) && is_balanced(mem, get(mem, node, 2))
    //   T_NOT_INT: val ok = !bf
    //   T_AND_INT: bf && is_balanced(...) && is_balanced(...)
    if (v == P_BOOL_OPERATORS || v == T_NOT_INT || v == T_AND_INT) {
        def("is_balanced", TYPE_BOOL, mem_node,
            { if_(eq_(V("node"), I(0)), { ret(true_()) }),
                val("bf", call("balance_factor", { V("mem"), V("node") })),
                val("ok",
                    not_(v == T_NOT_INT ? V("bf") : bin(AST_OP_OR, lt_(V("bf"), neg(I(1))), gt_(V("bf"), I(1))))),
                expr(bin(AST_OP_AND,
                    bin(AST_OP_AND, V(v == T_AND_INT ? "bf" : "ok"),
                        call("is_balanced", { V("mem"), get_(V("mem"), V("node"), LEFT) })),
                    call("is_balanced", { V("mem"), get_(V("mem"), V("node"), RIGHT) }))) });
    } else {
        def("is_balanced", TYPE_BOOL, mem_node,
            { if_(eq_(V("node"), I(0)), { ret(true_()) }),
                val("bf", call("balance_factor", { V("mem"), V("node") })),
                if_(lt_(V("bf"), neg(I(1))), { ret(false_()) }),
                if_(gt_(V("bf"), I(1)), { ret(false_()) }),
                if_else(call("is_balanced", { V("mem"), get_(V("mem"), V("node"), LEFT) }),
                    { ret(call("is_balanced", { V("mem"), get_(V("mem"), V("node"), RIGHT) })) }, { ret(false_()) }) });
    }

    // @main def main(): Unit
    //   U_NO_MAIN:            def main(): Unit = ...        (no @main annotation)
    //   C_MAIN_PARAMS:        @main def main(args: String): Unit
    //   T_MAIN_NON_UNIT:      @main def main() = { ...; root }
    //   T_CONDITION_NOT_BOOL: while (i)
    //   T_ASSIGN_TYPE:        root = mem
    //   T_DOUBLE_TO_INT:      root = result(mem) / 2.0
    //   T_DECLARED_TYPE:      val height: String = get_height(mem, root)
    //   T_BOOL_ARITHMETIC:    val ok = is_balanced(mem, root) + 1
    ast_node_t* new_root;
    if (v == T_ASSIGN_TYPE)
        new_root = V("mem");
    else if (v == T_DOUBLE_TO_INT)
        new_root = div_(call("result", { V("mem") }), D(2.0));
    else
        new_root = call("result", { V("mem") });

    avl.insert_call = call("bst_insert", { V("mem"), V("root"), V("i") });
    avl.print_call = call("print", { S("\n") });
    avl.ratio_div = div_(V("ratio"), I(2));
    avl.ratio_def = val("ratio", V("height"), TYPE_DOUBLE);

    node_list_t main_body = {
        var("mem", add_(add_(add_(chr_(0), chr_(0)), chr_(0)), chr_(0))),
        var("root", I(0)),
        var("i", I(1)),
        val("count", call("readInt")),
        while_(v == T_CONDITION_NOT_BOOL ? V("i") : le_(V("i"), V("count")),
            { assign("mem", avl.insert_call), assign("root", new_root), assign("i", add_(V("i"), I(1))) }),
        expr(call("in_order", { V("mem"), V("root") })),
        expr(avl.print_call),
        val("height", call("get_height", { V("mem"), V("root") }), v == T_DECLARED_TYPE ? TYPE_STRING : nullptr),
        avl.ratio_def,
        // P_VALMEM: print((ratio / 2).toString)
        expr(call("print", { v == P_VALMEM ? M(avl.ratio_div, "toString") : call("toStr", { avl.ratio_div }) })),
        val("found", call("bst_find", { V("mem"), V("root"), call("readInt") })),
        if_else(ne_(V("found"), I(0)), { print_("found") }, { print_("not found") }),
        val("ok",
            v == T_BOOL_ARITHMETIC ? add_(call("is_balanced", { V("mem"), V("root") }), I(1))
                                   : call("is_balanced", { V("mem"), V("root") })),
        if_(V("ok"), { print_("balanced") }),
    };
    if (v == T_MAIN_NON_UNIT)
        main_body.push_back(expr(V("root")));

    def("main", v == T_MAIN_NON_UNIT ? nullptr : TYPE_UNIT,
        v == C_MAIN_PARAMS ? param_list_t { { "args", TYPE_STRING } } : param_list_t {}, main_body, v != U_NO_MAIN);

    // R_TWO_MAINS:    a second @main def start(): Unit = {}
    // R_DUP_FUNCTION: a second def max(a: Int, b: Int): Int = { a }
    if (v == R_TWO_MAINS)
        def("start", TYPE_UNIT, {}, {}, true);
    if (v == R_DUP_FUNCTION)
        def("max", TYPE_INT, { { "a", TYPE_INT }, { "b", TYPE_INT } }, { expr(V("a")) });

    // Function order
    if (order == ORDER_REVERSED) {
        std::reverse(funcs.begin(), funcs.end());
    } else if (order == ORDER_MAIN_FIRST) {
        auto main_it = std::find_if(funcs.begin(), funcs.end(),
            [](ast_node_t* fn) { return std::string(fn->func_def.name) == "main"; });
        std::rotate(funcs.begin(), main_it, main_it + 1);
    }
    for (ast_node_t* fn : funcs)
        EXPECT_TRUE(dyn_arr_append(&avl.program->program.func_list, fn));
    return avl;
}

static std::string order_name(avl_order_t order)
{
    switch (order) {
    case ORDER_SOURCE:
        return "SourceOrder";
    case ORDER_MAIN_FIRST:
        return "MainFirst";
    default:
        return "Reversed";
    }
}

// ----------------------------------------
//  Fixture
// ----------------------------------------

class SemanticAvlTest : public ::testing::Test {
protected:
    avl_program_t avl;

    void TearDown() override
    {
        calloc_countdown = -1;
        ast_free(avl.program);
    }

    error_code_t analyze(avl_variant_t v, avl_order_t order = ORDER_SOURCE)
    {
        avl = build_avl(v, order);
        return semantic_analyze(avl.program);
    }

    ast_node_t* fn(const char* name) { return avl.funcs.at(name); }
};

// ========================================
//  Passing
// ========================================

class SemanticAvlOrderTest : public SemanticAvlTest, public ::testing::WithParamInterface<avl_order_t> { };

TEST_P(SemanticAvlOrderTest, ValidProgramIsOk)
{
    EXPECT_EQ(analyze(VALID, GetParam()), ERR_OK);
}

// The inferred types must not depend on which function was checked first
TEST_P(SemanticAvlOrderTest, InferredReturnTypes)
{
    ASSERT_EQ(analyze(VALID, GetParam()), ERR_OK);

    EXPECT_EQ(type_name(fn("result")->func_def.return_type), "Int");
    EXPECT_EQ(type_name(fn("get_height")->func_def.return_type), "Int");
    EXPECT_EQ(type_name(fn("balance_factor")->func_def.return_type), "Int");
    EXPECT_EQ(type_name(fn("allocate_node")->func_def.return_type), "String");
    EXPECT_EQ(type_name(fn("left_left_case")->func_def.return_type), "String");
    EXPECT_EQ(type_name(fn("left_right_case")->func_def.return_type), "String");
    EXPECT_EQ(type_name(fn("right_left_case")->func_def.return_type), "String");
    EXPECT_EQ(type_name(fn("right_right_case")->func_def.return_type), "String");
}

INSTANTIATE_TEST_SUITE_P(Orders, SemanticAvlOrderTest, ::testing::Values(ORDER_SOURCE, ORDER_MAIN_FIRST, ORDER_REVERSED),
    [](const ::testing::TestParamInfo<avl_order_t>& info) { return order_name(info.param); });

// Written types stay as they were
TEST_F(SemanticAvlTest, ExplicitReturnTypesAreKept)
{
    ASSERT_EQ(analyze(VALID), ERR_OK);

    EXPECT_EQ(type_name(fn("get")->func_def.return_type), "Int");
    EXPECT_EQ(type_name(fn("set")->func_def.return_type), "String");
    EXPECT_EQ(type_name(fn("bst_insert")->func_def.return_type), "String");
    EXPECT_EQ(type_name(fn("bst_find")->func_def.return_type), "Int");
    EXPECT_EQ(type_name(fn("in_order")->func_def.return_type), "Unit");
    EXPECT_EQ(type_name(fn("is_balanced")->func_def.return_type), "Boolean");
}

// val ratio: Double = height; print(toStr(ratio / 2))   -> both Ints get converted
TEST_F(SemanticAvlTest, ConversionsInMain)
{
    ASSERT_EQ(analyze(VALID), ERR_OK);

    EXPECT_TRUE(is_convert_of(avl.ratio_def->var_def.init, AST_VAR_REF));
    EXPECT_EQ(type_name(avl.ratio_div->type), "Double");
    EXPECT_EQ(avl.ratio_div->binary.left->kind, AST_VAR_REF) << "ratio is already Double";
    EXPECT_TRUE(is_convert_of(avl.ratio_div->binary.right, AST_INT_LIT));
}

TEST_F(SemanticAvlTest, CallsAreResolved)
{
    ASSERT_EQ(analyze(VALID), ERR_OK);

    EXPECT_FALSE(avl.insert_call->call.is_builtin);
    EXPECT_EQ(type_name(avl.insert_call->type), "String");
    EXPECT_TRUE(avl.print_call->call.is_builtin);
    EXPECT_EQ(type_name(avl.print_call->type), "Unit");
}

// mem, root, i, count, height, ratio, found, ok
TEST_F(SemanticAvlTest, MainLocals)
{
    ASSERT_EQ(analyze(VALID), ERR_OK);
    EXPECT_EQ(fn("main")->func_def.locals.count, 8u);
}

// node (var) and node_key (val inside the loop)
TEST_F(SemanticAvlTest, BstFindLocals)
{
    ASSERT_EQ(analyze(VALID), ERR_OK);
    EXPECT_EQ(fn("bst_find")->func_def.locals.count, 2u);
}

// Valid alternatives of single functions
struct avl_valid_case_t {
    avl_variant_t variant;
    const char* name;
};

static void PrintTo(const avl_valid_case_t& c, std::ostream* os) { *os << c.name; }

class SemanticAvlValidVariantTest
    : public SemanticAvlTest,
      public ::testing::WithParamInterface<std::tuple<avl_valid_case_t, avl_order_t>> { };

TEST_P(SemanticAvlValidVariantTest, IsOk)
{
    EXPECT_EQ(analyze(std::get<0>(GetParam()).variant, std::get<1>(GetParam())), ERR_OK);
}

INSTANTIATE_TEST_SUITE_P(Variants, SemanticAvlValidVariantTest,
    ::testing::Combine(::testing::Values(
                           // def max(n1: Int, n2: Int) = { var m = n2; if (n1 > n2) { m = n1 }; m }
                           avl_valid_case_t { P_MAX_INFERRED, "MaxWithInferredType" },
                           // every inferred function gets its type written
                           avl_valid_case_t { P_ALL_EXPLICIT, "AllTypesWritten" },
                           // bst_find loops on a Boolean variable (BOOLTHEN) instead of returning from the loop
                           avl_valid_case_t { P_BOOLEAN_FLAG, "FindWithBooleanFlag" },
                           // is_balanced written with !, && and || instead of ifs (BOOLTHEN)
                           avl_valid_case_t { P_BOOL_OPERATORS, "IsBalancedWithBooleanOperators" },
                           // mem.length instead of strLength(mem), (ratio / 2).toString instead of toStr(...) (VALMEM)
                           avl_valid_case_t { P_VALMEM, "MembersInsteadOfBuiltins" }),
        ::testing::Values(ORDER_SOURCE, ORDER_MAIN_FIRST, ORDER_REVERSED)),
    [](const ::testing::TestParamInfo<std::tuple<avl_valid_case_t, avl_order_t>>& info) {
        return std::string(std::get<0>(info.param).name) + "_" + order_name(std::get<1>(info.param));
    });

// Prints the AST of the valid program before and after semantic analysis.
// DISABLED_ -> not run by make test, only on request:
//   ./build/tests/semantic_avl_test --gtest_also_run_disabled_tests --gtest_filter='*PrintAst*'
TEST_F(SemanticAvlTest, DISABLED_PrintAstBeforeAndAfter)
{
    avl = build_avl(VALID);

    std::printf("===== AVL program: AST before semantic analysis =====\n");
    ast_print(avl.program, stdout);

    error_code_t err = semantic_analyze(avl.program);

    std::printf("\n===== AVL program: AST after semantic analysis (result %d) =====\n", static_cast<int>(err));
    ast_print(avl.program, stdout);
    std::fflush(stdout);

    EXPECT_EQ(err, ERR_OK);
}

// P_MAX_INFERRED really infers Int
TEST_F(SemanticAvlTest, InferredMaxIsInt)
{
    ASSERT_EQ(analyze(P_MAX_INFERRED), ERR_OK);
    EXPECT_EQ(type_name(fn("max")->func_def.return_type), "Int");
}

// ========================================
//  Failing - one changed line each
// ========================================

struct avl_fail_case_t {
    avl_variant_t variant;
    const char* name;
    error_code_t err;
};

static void PrintTo(const avl_fail_case_t& c, std::ostream* os) { *os << c.name; }

static const avl_fail_case_t FAILING[] = {
    // ---- 3: undefined ----
    // rotate_left: val old_left = get(mem, new_rot, 1)
    { U_TYPO_VARIABLE, "TypoInVariableName", ERR_SEM_UNDEF },
    // rebalance: return left_rigth_case(m, node)
    { U_TYPO_FUNCTION, "TypoInFunctionName", ERR_SEM_UNDEF },
    // in_order: print(root) - root is a variable of main, not of in_order
    { U_CALLER_VARIABLE, "VariableOfAnotherFunction", ERR_SEM_UNDEF },
    // bst_find: node_key is used after the loop that defines it
    { U_OUT_OF_SCOPE, "VariableUsedOutsideItsBlock", ERR_SEM_UNDEF },
    // allocate_node: val index = strLength(m) / 4 - m is defined on the next line
    { U_USE_BEFORE_DEF, "VariableUsedBeforeDefinition", ERR_SEM_UNDEF },
    // def main(): Unit = ... - no @main annotation
    { U_NO_MAIN, "MissingMainAnnotation", ERR_SEM_UNDEF },

    // ---- 4: redefinition ----
    // a second def max(a: Int, b: Int): Int
    { R_DUP_FUNCTION, "FunctionDefinedTwice", ERR_SEM_REDEF },
    // def set(mem: String, node: Int, field: Int, field: Int)
    { R_DUP_PARAM, "DuplicateParameter", ERR_SEM_REDEF },
    // left_right_case: val m = ...; val m = set(...)
    { R_DUP_LOCAL, "LocalDefinedTwiceInSameBlock", ERR_SEM_REDEF },
    // def ord(mem: String, node: Int, field: Int): Int - ord is a builtin
    { R_BUILTIN_NAME, "FunctionNamedLikeBuiltin", ERR_SEM_REDEF },
    // a second @main def start(): Unit
    { R_TWO_MAINS, "TwoMainFunctions", ERR_SEM_REDEF },

    // ---- 5: call ----
    // bst_insert: m = bst_insert(m, key) - node argument missing
    { C_ARG_COUNT, "MissingArgument", ERR_SEM_CALL },
    // update_height: set(mem, node, 3, toStr(h)) - String for an Int parameter
    { C_ARG_TYPE, "WrongArgumentType", ERR_SEM_CALL },
    // get: ord(mem, 4.0 * node + field) - Double for the Int index of a builtin
    { C_BUILTIN_ARG_TYPE, "WrongBuiltinArgumentType", ERR_SEM_CALL },
    // allocate_node: strLength() / 4
    { C_BUILTIN_ARG_COUNT, "MissingBuiltinArgument", ERR_SEM_CALL },
    // @main def main(args: String): Unit
    { C_MAIN_PARAMS, "MainWithParameter", ERR_SEM_CALL },

    // ---- 6: type ----
    // allocate_node: val m = mem + key + chr(0) + ... - no automatic Int to String
    { T_STRING_PLUS_INT, "StringPlusInt", ERR_SEM_TYPE },
    // main: root = mem - String into an Int variable
    { T_ASSIGN_TYPE, "AssignStringToIntVariable", ERR_SEM_TYPE },
    // main: root = result(mem) / 2.0 - Double into an Int variable
    { T_DOUBLE_TO_INT, "AssignDoubleToIntVariable", ERR_SEM_TYPE },
    // main: val height: String = get_height(mem, root)
    { T_DECLARED_TYPE, "InitDoesNotMatchDeclaredType", ERR_SEM_TYPE },
    // main: while (i)
    { T_CONDITION_NOT_BOOL, "IntCondition", ERR_SEM_TYPE },
    // rotate_left: if (old_root == 0) { return old_root } - Int from a String function
    { T_RETURN_TYPE, "ReturnValueOfWrongType", ERR_SEM_TYPE },
    // def get_height(mem: String, node: Int): String - the body ends with the Int h
    { T_BODY_TYPE, "BodyDoesNotMatchReturnType", ERR_SEM_TYPE },
    // def in_order(mem: String, node: Int) = ... - recursive without a written type
    { T_RECURSION_INFERRED, "RecursiveFunctionWithoutType", ERR_SEM_TYPE },
    // left_right_case: return rotate_right(m2, node) - return in a function without a written type
    { T_RETURN_IN_INFERRED, "ReturnWithoutWrittenType", ERR_SEM_TYPE },
    // max: if (n1 > n2) { return n1 } else { n2 } - one path without return
    { T_RULE3_MISSING_RETURN, "IfPathWithoutReturn", ERR_SEM_TYPE },
    // def get_height(...) = { if (node == 0) { 0 } else { get(mem, node, 3) } } - an if has no value
    { T_RULE3_INFERRED_VALUES, "IfUsedAsValue", ERR_SEM_TYPE },
    // @main def main() = { ...; root } - main inferred as Int
    { T_MAIN_NON_UNIT, "MainReturnsInt", ERR_SEM_TYPE },
    // rebalance: if (bf == -"2")
    { T_NEG_STRING, "NegatedString", ERR_SEM_TYPE },
    // bst_find: if (key == mem) - Int compared with String
    { T_COMPARE_INT_STRING, "IntComparedWithString", ERR_SEM_TYPE },
    // main: val ok = is_balanced(mem, root) + 1
    { T_BOOL_ARITHMETIC, "BooleanInArithmetic", ERR_SEM_TYPE },
    // is_balanced (operator version): val ok = !bf - ! on an Int
    { T_NOT_INT, "NotOfInt", ERR_SEM_TYPE },
    // is_balanced (operator version): bf && is_balanced(...) - Int operand of &&
    { T_AND_INT, "AndWithIntOperand", ERR_SEM_TYPE },
    // allocate_node: val index = mem.toInt / 4 - toInt needs a Double receiver (VALMEM)
    { T_MEMBER_RECEIVER, "MemberWithWrongReceiver", ERR_SEM_TYPE },
    // allocate_node: val index = mem.size / 4 - not a member (VALMEM, code to be confirmed)
    { U_UNKNOWN_MEMBER, "UnknownMember", ERR_SEM_UNDEF },

    // ---- 7: assignment ----
    // bst_find: node_key = get(mem, node, 1) - node_key is a val
    { A_ASSIGN_VAL, "AssignToVal", ERR_SEM_ASSIGN },
    // def bst_find(mem: String, node: Int, key: Int) moving its node parameter like the C version
    { A_ASSIGN_PARAM, "AssignToParameter", ERR_SEM_ASSIGN },

    // ---- 10: other ----
    // rotate_left: if (old_root == 0) { return } - bare return in a String function (open forum question)
    { O_BARE_RETURN, "BareReturnInStringFunction", ERR_SEM_OTHER },
};

class SemanticAvlFailTest : public SemanticAvlTest,
                            public ::testing::WithParamInterface<std::tuple<avl_fail_case_t, avl_order_t>> { };

TEST_P(SemanticAvlFailTest, IsReported)
{
    const avl_fail_case_t& c = std::get<0>(GetParam());
    EXPECT_EQ(analyze(c.variant, std::get<1>(GetParam())), c.err);
}

INSTANTIATE_TEST_SUITE_P(Variants, SemanticAvlFailTest,
    ::testing::Combine(::testing::ValuesIn(FAILING), ::testing::Values(ORDER_SOURCE, ORDER_MAIN_FIRST, ORDER_REVERSED)),
    [](const ::testing::TestParamInfo<std::tuple<avl_fail_case_t, avl_order_t>>& info) {
        return std::string(std::get<0>(info.param).name) + "_" + order_name(std::get<1>(info.param));
    });

// ========================================
//  Allocation failures (99)
// ========================================

// Fail the 1st, 2nd, 3rd, ... calloc on a fresh AVL program each time
TEST(SemanticAvlAllocTest, EveryAllocationFailureIsInternalError)
{
    int failures = 0;
    for (int n = 0; n < 5000; n++) {
        avl_program_t avl = build_avl(VALID, ORDER_MAIN_FIRST);

        calloc_countdown = n;
        error_code_t err = semantic_analyze(avl.program);
        bool failure_was_triggered = (calloc_countdown == -1);
        calloc_countdown = -1;

        ast_free(avl.program);

        if (!failure_was_triggered) {
            EXPECT_EQ(err, ERR_OK) << "no allocation failed, the run must succeed";
            break;
        }
        EXPECT_EQ(err, ERR_INTERNAL) << "calloc number " << n << " failed";
        failures++;
    }
    EXPECT_GT(failures, 0) << "semantic_analyze never called calloc";
}
