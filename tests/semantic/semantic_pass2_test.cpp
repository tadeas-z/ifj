// Implementation of the IFJ26 imperative language compiler
// semantic_pass2_test.cpp
// Author(s): Adam Zatloukal (xzatloa01)
//
// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Ok now lets create tests for everything we have done. First create all
//          the tests you have recommended throughout this chat ... Use google tests
//          similar to ast or symtable tests" (black-box tests of pass 2 through
//          semantic_analyze: literal / unary / binary expression types and CONVERT
//          insertion, val/var definitions, scopes and shadowing, assignment, if and
//          while conditions, return rules, explicit and inferred return types,
//          rule 3 (if as the last statement), @main return type, user and builtin
//          calls, lazy checking and caller context, direct and indirect recursion,
//          allocation failures)
// Scope: the whole file
//
// Every test builds a program by hand with the builders from semantic_builders.hpp,
// so a test reads like the IFJ26 source it represents (written in the comment above it).

extern "C" {
#include "ast.h"
#include "dynamic_array.h"
#include "semantic.h"
}

#include <gtest/gtest.h>

#include "semantic_builders.hpp"

#include <initializer_list>
#include <string>
#include <utility>
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

// ----------------------------------------
//  Helpers (the AST builders are in semantic_builders.hpp)
// ----------------------------------------

// A literal (or Boolean expression) of the given type
static ast_node_t* operand_of(type_kind_t kind)
{
    switch (kind) {
    case T_INT:
        return I(2);
    case T_DOUBLE:
        return D(2.5);
    case T_STRING:
        return S("a");
    case T_UNIT:
        return U();
    case T_BOOL:
        return cond();
    default:
        return nullptr;
    }
}

// ----------------------------------------
//  Fixture
// ----------------------------------------

class SemanticPass2Test : public ::testing::Test {
protected:
    ast_node_t* program = nullptr;

    void SetUp() override
    {
        program = ast_new_program();
        ASSERT_NE(program, nullptr);
    }

    void TearDown() override
    {
        calloc_countdown = -1;
        ast_free(program);
    }

    // def <name>(<params>): <ret_type> = { <body> }      ret_type == nullptr -> not written
    ast_node_t* func(const char* name, type_t* ret_type, const param_list_t& params, node_list_t body,
        bool is_main = false)
    {
        ast_node_t* fn = ast_new_func_def(name, is_main, 1, 1);
        EXPECT_NE(fn, nullptr);
        for (const auto& param : params)
            EXPECT_TRUE(ast_func_def_add_param(fn, param.first, param.second));
        fn->func_def.return_type = ret_type;
        fn->func_def.body = block(body);
        EXPECT_TRUE(dyn_arr_append(&program->program.func_list, fn));
        return fn;
    }

    // @main def main(): Unit = { <body> }
    ast_node_t* main_(node_list_t body) { return func("main", TYPE_UNIT, {}, body, true); }

    error_code_t analyze() { return semantic_analyze(program); }

    // @main def main(): Unit = { val r = <e> }
    error_code_t analyze_expr(ast_node_t* e)
    {
        main_({ val("r", e) });
        return analyze();
    }
};

// ========================================
//  Expressions
// ========================================

// ----------------------------------------
//  Literals
// ----------------------------------------

TEST_F(SemanticPass2Test, IntLiteralIsInt)
{
    ast_node_t* e = I(1);
    ASSERT_EQ(analyze_expr(e), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Int");
}

TEST_F(SemanticPass2Test, DoubleLiteralIsDouble)
{
    ast_node_t* e = D(1.5);
    ASSERT_EQ(analyze_expr(e), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Double");
}

TEST_F(SemanticPass2Test, StringLiteralIsString)
{
    ast_node_t* e = S("abc");
    ASSERT_EQ(analyze_expr(e), ERR_OK);
    EXPECT_EQ(type_name(e->type), "String");
}

TEST_F(SemanticPass2Test, UnitLiteralIsUnit)
{
    ast_node_t* e = U();
    ASSERT_EQ(analyze_expr(e), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Unit");
}

// ----------------------------------------
//  Unary minus: numeric only, keeps the type
// ----------------------------------------

struct unary_case_t {
    type_kind_t operand;
    error_code_t err;
};

// gtest prints a parameter that has no printer byte by byte (padding included),
// these printers keep the output readable and valgrind quiet
static void PrintTo(const unary_case_t& c, std::ostream* os) { *os << "-" << kind_name(c.operand); }

class SemanticUnaryTest : public SemanticPass2Test, public ::testing::WithParamInterface<unary_case_t> { };

// val r = -<operand>
TEST_P(SemanticUnaryTest, NegationOfOperand)
{
    const unary_case_t& c = GetParam();
    ast_node_t* e = neg(operand_of(c.operand));

    ASSERT_EQ(analyze_expr(e), c.err);
    if (c.err == ERR_OK) {
        EXPECT_EQ(type_name(e->type), kind_name(c.operand)) << "negation must not change the type";
    }
}

INSTANTIATE_TEST_SUITE_P(Operands, SemanticUnaryTest,
    ::testing::Values(unary_case_t { T_INT, ERR_OK }, unary_case_t { T_DOUBLE, ERR_OK },
        unary_case_t { T_STRING, ERR_SEM_TYPE }, unary_case_t { T_UNIT, ERR_SEM_TYPE },
        unary_case_t { T_BOOL, ERR_SEM_TYPE }),
    [](const ::testing::TestParamInfo<unary_case_t>& info) { return kind_name(info.param.operand); });

// --x  -> nested unary keeps Int
TEST_F(SemanticPass2Test, DoubleNegationIsOk)
{
    ast_node_t* e = neg(neg(I(1)));
    ASSERT_EQ(analyze_expr(e), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Int");
}

// -(1 + 2.5) -> Double
TEST_F(SemanticPass2Test, NegationOfBinaryExpressionIsOk)
{
    ast_node_t* e = neg(bin(AST_OP_ADD, I(1), D(2.5)));
    ASSERT_EQ(analyze_expr(e), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Double");
}

// ----------------------------------------
//  Binary operators: the operator table (spec section 3)
// ----------------------------------------

struct binary_case_t {
    ast_binary_op_t op;
    type_kind_t left;
    type_kind_t right;
    error_code_t err;
    type_kind_t result; // only when err == ERR_OK
    bool convert_left; // left operand must be wrapped in CONVERT
    bool convert_right;
};

static std::vector<binary_case_t> binary_cases()
{
    std::vector<binary_case_t> cases;
    auto ok = [&](ast_binary_op_t op, type_kind_t l, type_kind_t r, type_kind_t result, bool conv_l, bool conv_r) {
        cases.push_back({ op, l, r, ERR_OK, result, conv_l, conv_r });
    };
    auto bad = [&](ast_binary_op_t op, type_kind_t l, type_kind_t r) {
        cases.push_back({ op, l, r, ERR_SEM_TYPE, T_ANY, false, false });
    };

    // + - * /: Int op Int -> Int, at least one Double -> Double (Int side converted)
    for (ast_binary_op_t op : { AST_OP_ADD, AST_OP_SUB, AST_OP_MUL, AST_OP_DIV }) {
        ok(op, T_INT, T_INT, T_INT, false, false);
        ok(op, T_INT, T_DOUBLE, T_DOUBLE, true, false);
        ok(op, T_DOUBLE, T_INT, T_DOUBLE, false, true);
        ok(op, T_DOUBLE, T_DOUBLE, T_DOUBLE, false, false);
        bad(op, T_INT, T_STRING);
        bad(op, T_STRING, T_INT);
        bad(op, T_DOUBLE, T_STRING);
        bad(op, T_INT, T_UNIT);
        bad(op, T_UNIT, T_UNIT);
        bad(op, T_BOOL, T_INT);
        bad(op, T_BOOL, T_BOOL);
        // String + String is concatenation, no other arithmetic operator works on strings
        if (op == AST_OP_ADD)
            ok(op, T_STRING, T_STRING, T_STRING, false, false);
        else
            bad(op, T_STRING, T_STRING);
    }

    // < <= > >=: two numbers (converted when mixed) or two strings -> Boolean
    for (ast_binary_op_t op : { AST_OP_LT, AST_OP_LE, AST_OP_GT, AST_OP_GE }) {
        ok(op, T_INT, T_INT, T_BOOL, false, false);
        ok(op, T_INT, T_DOUBLE, T_BOOL, true, false);
        ok(op, T_DOUBLE, T_INT, T_BOOL, false, true);
        ok(op, T_DOUBLE, T_DOUBLE, T_BOOL, false, false);
        ok(op, T_STRING, T_STRING, T_BOOL, false, false);
        bad(op, T_STRING, T_INT);
        bad(op, T_INT, T_STRING);
        bad(op, T_BOOL, T_BOOL);
        bad(op, T_BOOL, T_INT);
        bad(op, T_UNIT, T_UNIT);
    }

    // == !=: same types or two numbers (converted when mixed) -> Boolean
    for (ast_binary_op_t op : { AST_OP_EQ, AST_OP_NE }) {
        ok(op, T_INT, T_INT, T_BOOL, false, false);
        ok(op, T_INT, T_DOUBLE, T_BOOL, true, false);
        ok(op, T_DOUBLE, T_INT, T_BOOL, false, true);
        ok(op, T_DOUBLE, T_DOUBLE, T_BOOL, false, false);
        ok(op, T_STRING, T_STRING, T_BOOL, false, false);
        ok(op, T_UNIT, T_UNIT, T_BOOL, false, false);
        ok(op, T_BOOL, T_BOOL, T_BOOL, false, false); // BOOLTHEN: Booleans can be compared with == !=
        bad(op, T_STRING, T_INT);
        bad(op, T_INT, T_STRING);
        bad(op, T_DOUBLE, T_STRING);
        bad(op, T_BOOL, T_INT);
        bad(op, T_UNIT, T_INT);
    }

    // && ||: BOOLTHEN, both operands must be Boolean -> Boolean
    for (ast_binary_op_t op : { AST_OP_AND, AST_OP_OR }) {
        ok(op, T_BOOL, T_BOOL, T_BOOL, false, false);
        bad(op, T_BOOL, T_INT);
        bad(op, T_INT, T_BOOL);
        bad(op, T_INT, T_INT);
        bad(op, T_DOUBLE, T_BOOL);
        bad(op, T_STRING, T_STRING);
        bad(op, T_BOOL, T_STRING);
        bad(op, T_UNIT, T_UNIT);
        bad(op, T_BOOL, T_UNIT);
    }
    return cases;
}

static std::string op_name(ast_binary_op_t op)
{
    switch (op) {
    case AST_OP_ADD:
        return "Add";
    case AST_OP_SUB:
        return "Sub";
    case AST_OP_MUL:
        return "Mul";
    case AST_OP_DIV:
        return "Div";
    case AST_OP_LT:
        return "Lt";
    case AST_OP_LE:
        return "Le";
    case AST_OP_GT:
        return "Gt";
    case AST_OP_GE:
        return "Ge";
    case AST_OP_EQ:
        return "Eq";
    case AST_OP_NE:
        return "Ne";
    case AST_OP_AND:
        return "And";
    case AST_OP_OR:
        return "Or";
    default:
        return "Op";
    }
}

static void PrintTo(const binary_case_t& c, std::ostream* os)
{
    *os << kind_name(c.left) << " " << op_name(c.op) << " " << kind_name(c.right);
}

class SemanticBinaryTest : public SemanticPass2Test, public ::testing::WithParamInterface<binary_case_t> { };

// val r = <left> <op> <right>
TEST_P(SemanticBinaryTest, OperandTypes)
{
    const binary_case_t& c = GetParam();
    ast_node_t* e = bin(c.op, operand_of(c.left), operand_of(c.right));

    ASSERT_EQ(analyze_expr(e), c.err);
    if (c.err != ERR_OK)
        return;

    EXPECT_EQ(type_name(e->type), kind_name(c.result));
    EXPECT_EQ(e->binary.left->kind == AST_CONVERT, c.convert_left) << "CONVERT on the left operand";
    EXPECT_EQ(e->binary.right->kind == AST_CONVERT, c.convert_right) << "CONVERT on the right operand";
    if (c.convert_left) {
        EXPECT_TRUE(is_convert_of(e->binary.left, AST_INT_LIT));
    }
    if (c.convert_right) {
        EXPECT_TRUE(is_convert_of(e->binary.right, AST_INT_LIT));
    }
}

INSTANTIATE_TEST_SUITE_P(OperatorTable, SemanticBinaryTest, ::testing::ValuesIn(binary_cases()),
    [](const ::testing::TestParamInfo<binary_case_t>& info) {
        return op_name(info.param.op) + "_" + kind_name(info.param.left) + "_" + kind_name(info.param.right);
    });

// ----------------------------------------
//  Nested expressions
// ----------------------------------------

// (1 + 2.5) * 3 -> the inner result is Double, so the outer 3 gets converted
TEST_F(SemanticPass2Test, ConvertedResultPropagatesUpwards)
{
    ast_node_t* inner = bin(AST_OP_ADD, I(1), D(2.5));
    ast_node_t* e = bin(AST_OP_MUL, inner, I(3));

    ASSERT_EQ(analyze_expr(e), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Double");
    EXPECT_EQ(e->binary.left, inner) << "a Double operand is never wrapped";
    EXPECT_TRUE(is_convert_of(e->binary.right, AST_INT_LIT));
    EXPECT_TRUE(is_convert_of(inner->binary.left, AST_INT_LIT));
}

// (1 + 2) * 3 -> stays Int, no CONVERT anywhere
TEST_F(SemanticPass2Test, IntOnlyExpressionHasNoConvert)
{
    ast_node_t* inner = bin(AST_OP_ADD, I(1), I(2));
    ast_node_t* e = bin(AST_OP_MUL, inner, I(3));

    ASSERT_EQ(analyze_expr(e), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Int");
    EXPECT_EQ(e->binary.left, inner);
    EXPECT_EQ(e->binary.right->kind, AST_INT_LIT);
}

// 7 / 2 is integer division -> Int (codegen picks IDIV from the type)
TEST_F(SemanticPass2Test, IntDivisionIsInt)
{
    ast_node_t* e = bin(AST_OP_DIV, I(7), I(2));
    ASSERT_EQ(analyze_expr(e), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Int");
}

// "a" + "b" + "c"
TEST_F(SemanticPass2Test, ChainedConcatenationIsOk)
{
    ast_node_t* e = bin(AST_OP_ADD, bin(AST_OP_ADD, S("a"), S("b")), S("c"));
    ASSERT_EQ(analyze_expr(e), ERR_OK);
    EXPECT_EQ(type_name(e->type), "String");
}

// "a" + 1 + 2 -> the String + Int part is an error even when nested
TEST_F(SemanticPass2Test, NestedStringPlusIntIsTypeError)
{
    EXPECT_EQ(analyze_expr(bin(AST_OP_ADD, bin(AST_OP_ADD, S("a"), I(1)), I(2))), ERR_SEM_TYPE);
}

// (1 < 2) == (2.5 > 1) -> Boolean == Boolean, the inner 1 gets converted
TEST_F(SemanticPass2Test, ComparisonOfTwoConditionsIsOk)
{
    ast_node_t* right = bin(AST_OP_GT, D(2.5), I(1));
    ast_node_t* e = bin(AST_OP_EQ, cond(), right);

    ASSERT_EQ(analyze_expr(e), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Boolean");
    EXPECT_TRUE(is_convert_of(right->binary.right, AST_INT_LIT));
}

// (1 < 2) + 1
TEST_F(SemanticPass2Test, ArithmeticOnConditionIsTypeError)
{
    EXPECT_EQ(analyze_expr(bin(AST_OP_ADD, cond(), I(1))), ERR_SEM_TYPE);
}

// x + 1 where x is not defined -> 3 from the operand, not 6
TEST_F(SemanticPass2Test, UndefinedOperandIsUndefined)
{
    EXPECT_EQ(analyze_expr(bin(AST_OP_ADD, V("x"), I(1))), ERR_SEM_UNDEF);
}

// 1 + x -> the right operand is checked too
TEST_F(SemanticPass2Test, UndefinedRightOperandIsUndefined)
{
    EXPECT_EQ(analyze_expr(bin(AST_OP_ADD, I(1), V("x"))), ERR_SEM_UNDEF);
}

// ========================================
//  Variables
// ========================================

// ----------------------------------------
//  Definitions (val / var)
// ----------------------------------------

// val x = 1; x   -> x is Int
TEST_F(SemanticPass2Test, InferredVariableTypeComesFromInit)
{
    ast_node_t* def = val("x", I(1));
    ast_node_t* ref = V("x");
    main_({ def, expr(ref) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(ref->type), "Int");
    EXPECT_NE(def->var_def.id, -1) << "the definition gets an id";
    EXPECT_EQ(ref->var_ref.id, def->var_def.id) << "the reference points to the definition";
}

// val x: Double = 1; x   -> init wrapped in CONVERT, x is Double
TEST_F(SemanticPass2Test, DeclaredDoubleWithIntInitInsertsConvert)
{
    ast_node_t* def = val("x", I(1), TYPE_DOUBLE);
    ast_node_t* ref = V("x");
    main_({ def, expr(ref) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_TRUE(is_convert_of(def->var_def.init, AST_INT_LIT));
    EXPECT_EQ(type_name(ref->type), "Double") << "the declared type wins over the init type";
}

// val x: Int = 1 -> no CONVERT
TEST_F(SemanticPass2Test, DeclaredTypeMatchingInitHasNoConvert)
{
    ast_node_t* def = val("x", I(1), TYPE_INT);
    main_({ def });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(def->var_def.init->kind, AST_INT_LIT);
}

// val x: Int = 2.5 -> there is no implicit Double -> Int
TEST_F(SemanticPass2Test, DeclaredIntWithDoubleInitIsTypeError)
{
    main_({ val("x", D(2.5), TYPE_INT) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// var x: Int = "a"
TEST_F(SemanticPass2Test, DeclaredIntWithStringInitIsTypeError)
{
    main_({ var("x", S("a"), TYPE_INT) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// val s: String = 1
TEST_F(SemanticPass2Test, DeclaredStringWithIntInitIsTypeError)
{
    main_({ val("s", I(1), TYPE_STRING) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// val x = y   (y undefined)
TEST_F(SemanticPass2Test, UndefinedVariableInInitIsUndefined)
{
    main_({ val("x", V("y")) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// val y = x; val x = 1   -> x is used before its definition
TEST_F(SemanticPass2Test, UseBeforeDefinitionInSameBlockIsUndefined)
{
    main_({ val("y", V("x")), val("x", I(1)) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// val x = x + 1   (no outer x) -> the new x is not visible in its own init
TEST_F(SemanticPass2Test, SelfReferenceInInitIsUndefined)
{
    main_({ val("x", bin(AST_OP_ADD, V("x"), I(1))) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// val x = 1; var x = 2
TEST_F(SemanticPass2Test, RedefinitionInSameBlockIsRedefinition)
{
    main_({ val("x", I(1)), var("x", I(2)) });
    EXPECT_EQ(analyze(), ERR_SEM_REDEF);
}

// val x = 1; val x = "a"   -> a different type does not matter
TEST_F(SemanticPass2Test, RedefinitionWithDifferentTypeIsRedefinition)
{
    main_({ val("x", I(1)), val("x", S("a")) });
    EXPECT_EQ(analyze(), ERR_SEM_REDEF);
}

// val x = 1; if (1 < 2) { val x = "a"; x }   -> inner x shadows, different id and type
TEST_F(SemanticPass2Test, ShadowingInNestedBlockIsOk)
{
    ast_node_t* outer = val("x", I(1));
    ast_node_t* inner = val("x", S("a"));
    ast_node_t* ref = V("x");
    main_({ outer, if_(cond(), { inner, expr(ref) }) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_NE(inner->var_def.id, outer->var_def.id);
    EXPECT_EQ(ref->var_ref.id, inner->var_def.id) << "the innermost definition wins";
    EXPECT_EQ(type_name(ref->type), "String");
}

// val x = 1; if (1 < 2) { val x = x + 1 }   -> the init still sees the OUTER x
TEST_F(SemanticPass2Test, ShadowingInitRefersToOuterVariable)
{
    ast_node_t* outer = val("x", I(1));
    ast_node_t* ref = V("x");
    ast_node_t* inner = val("x", bin(AST_OP_ADD, ref, I(1)));
    main_({ outer, if_(cond(), { inner }) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(ref->var_ref.id, outer->var_def.id);
    EXPECT_NE(inner->var_def.id, outer->var_def.id);
}

// val x = 1; if (..) { .. }; x   -> after the block the outer x is visible again
TEST_F(SemanticPass2Test, OuterVariableVisibleAgainAfterShadowingBlock)
{
    ast_node_t* outer = val("x", I(1));
    ast_node_t* ref = V("x");
    main_({ outer, if_(cond(), { val("x", S("a")) }), expr(ref) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(ref->var_ref.id, outer->var_def.id);
    EXPECT_EQ(type_name(ref->type), "Int");
}

// if (..) { val x = 1 } else { val x = "a" }   -> branches are separate scopes
TEST_F(SemanticPass2Test, SameNameInBothBranchesIsOk)
{
    main_({ if_else(cond(), { val("x", I(1)) }, { val("x", S("a")) }) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// if (..) { val x = 1 }; x
TEST_F(SemanticPass2Test, VariableNotVisibleAfterIfBlockIsUndefined)
{
    main_({ if_(cond(), { val("x", I(1)) }), expr(V("x")) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// if (..) { val x = 1 } else { x }   -> then-branch variables are not visible in else
TEST_F(SemanticPass2Test, ThenVariableNotVisibleInElseIsUndefined)
{
    main_({ if_else(cond(), { val("x", I(1)) }, { expr(V("x")) }) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// while (..) { val x = 1 }; x
TEST_F(SemanticPass2Test, VariableNotVisibleAfterWhileBodyIsUndefined)
{
    main_({ while_(cond(), { val("x", I(1)) }), expr(V("x")) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// val x = 1; while (..) { if (..) { x } }   -> outer variables are visible in nested blocks
TEST_F(SemanticPass2Test, OuterVariableVisibleInNestedBlocks)
{
    main_({ val("x", I(1)), while_(cond(), { if_(cond(), { expr(V("x")) }) }) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// Every val/var of a function (also nested ones) ends up in func_def.locals
TEST_F(SemanticPass2Test, LocalsContainEveryDefinitionOfTheFunction)
{
    ast_node_t* a = val("a", I(1));
    ast_node_t* b = var("b", I(2));
    ast_node_t* c = val("c", I(3));
    ast_node_t* main_fn = main_({ a, if_(cond(), { b }), while_(cond(), { c }) });

    ASSERT_EQ(analyze(), ERR_OK);
    dyn_arr_t* locals = &main_fn->func_def.locals;
    ASSERT_EQ(locals->count, 3u);
    EXPECT_EQ(dyn_arr_get(locals, 0), a);
    EXPECT_EQ(dyn_arr_get(locals, 1), b);
    EXPECT_EQ(dyn_arr_get(locals, 2), c);
}

// Locals of one function do not end up in another one
TEST_F(SemanticPass2Test, LocalsBelongToTheirOwnFunction)
{
    ast_node_t* f = func("f", TYPE_UNIT, {}, { val("y", I(1)) });
    ast_node_t* main_fn = main_({ val("x", I(1)), expr(call("f")) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(main_fn->func_def.locals.count, 1u);
    EXPECT_EQ(f->func_def.locals.count, 1u);
}

// Shadowed variables with the same name still get unique ids
TEST_F(SemanticPass2Test, EveryVariableGetsUniqueId)
{
    ast_node_t* a = val("x", I(1));
    ast_node_t* b = val("x", I(2));
    ast_node_t* c = val("x", I(3));
    main_({ a, if_(cond(), { b, while_(cond(), { c }) }) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_NE(a->var_def.id, b->var_def.id);
    EXPECT_NE(a->var_def.id, c->var_def.id);
    EXPECT_NE(b->var_def.id, c->var_def.id);
}

// val b = 1 < 2; if (b) {}   -> BOOLTHEN: Booleans can be stored and used as conditions
TEST_F(SemanticPass2Test, BooleanVariableIsOk)
{
    ast_node_t* ref = V("b");
    main_({ val("b", cond()), if_(ref, {}) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(ref->type), "Boolean");
}

// ----------------------------------------
//  Parameters
// ----------------------------------------

// def f(a: Int, b: String): Unit = { a; b }
TEST_F(SemanticPass2Test, ParamsAreVisibleInBody)
{
    ast_node_t* ref_a = V("a");
    ast_node_t* ref_b = V("b");
    ast_node_t* f = func("f", TYPE_UNIT, { { "a", TYPE_INT }, { "b", TYPE_STRING } }, { expr(ref_a), expr(ref_b) });
    main_({});

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(ref_a->type), "Int");
    EXPECT_EQ(type_name(ref_b->type), "String");

    ast_param_t* param_a = static_cast<ast_param_t*>(dyn_arr_get(&f->func_def.params, 0));
    EXPECT_NE(param_a->id, -1) << "params get an id";
    EXPECT_EQ(ref_a->var_ref.id, param_a->id);
}

// def f(a: Int): Unit = { if (..) { a } }
TEST_F(SemanticPass2Test, ParamsAreVisibleInNestedBlocks)
{
    func("f", TYPE_UNIT, { { "a", TYPE_INT } }, { if_(cond(), { expr(V("a")) }) });
    main_({});
    EXPECT_EQ(analyze(), ERR_OK);
}

// def f(a: Int): Unit = {}; main: a   -> a param is not visible outside its function
TEST_F(SemanticPass2Test, ParamNotVisibleInOtherFunctionIsUndefined)
{
    func("f", TYPE_UNIT, { { "a", TYPE_INT } }, {});
    main_({ expr(V("a")) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// ----------------------------------------
//  Assignment
// ----------------------------------------

// var x = 1; x = 2
TEST_F(SemanticPass2Test, AssignToVarIsOk)
{
    ast_node_t* def = var("x", I(1));
    ast_node_t* asg = assign("x", I(2));
    main_({ def, asg });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(asg->assign.id, def->var_def.id);
}

// val x = 1; x = 2
TEST_F(SemanticPass2Test, AssignToValIsAssignError)
{
    main_({ val("x", I(1)), assign("x", I(2)) });
    EXPECT_EQ(analyze(), ERR_SEM_ASSIGN);
}

// def f(a: Int): Unit = { a = 2 }
TEST_F(SemanticPass2Test, AssignToParamIsAssignError)
{
    func("f", TYPE_UNIT, { { "a", TYPE_INT } }, { assign("a", I(2)) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_ASSIGN);
}

// var x: Double = 1.0; x = 2   -> the value gets converted
TEST_F(SemanticPass2Test, AssignIntToDoubleVarInsertsConvert)
{
    ast_node_t* asg = assign("x", I(2));
    main_({ var("x", D(1.0)), asg });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_TRUE(is_convert_of(asg->assign.value, AST_INT_LIT));
}

// var x = 1; x = 2.5   -> the type of a variable never changes
TEST_F(SemanticPass2Test, AssignDoubleToIntVarIsTypeError)
{
    main_({ var("x", I(1)), assign("x", D(2.5)) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// var x = 1; x = "a"
TEST_F(SemanticPass2Test, AssignStringToIntVarIsTypeError)
{
    main_({ var("x", I(1)), assign("x", S("a")) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// var x = 1; x = y   (y undefined)
TEST_F(SemanticPass2Test, UndefinedVariableInAssignedValueIsUndefined)
{
    main_({ var("x", I(1)), assign("x", V("y")) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// var x = 1; while (..) { x = x + 1 }   -> assigning to an outer variable from a nested block
TEST_F(SemanticPass2Test, AssignToOuterVarFromNestedBlockIsOk)
{
    main_({ var("x", I(1)), while_(cond(), { assign("x", bin(AST_OP_ADD, V("x"), I(1))) }) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// var x = 1; if (..) { val x = 2; x = 3 }   -> the assignment targets the inner val
TEST_F(SemanticPass2Test, AssignToShadowingValIsAssignError)
{
    main_({ var("x", I(1)), if_(cond(), { val("x", I(2)), assign("x", I(3)) }) });
    EXPECT_EQ(analyze(), ERR_SEM_ASSIGN);
}

// ========================================
//  if / while
// ========================================

class SemanticConditionTest : public SemanticPass2Test, public ::testing::WithParamInterface<type_kind_t> { };

// if (<non-Boolean>) {}
TEST_P(SemanticConditionTest, NonBooleanIfConditionIsTypeError)
{
    main_({ if_(operand_of(GetParam()), {}) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// while (<non-Boolean>) {}
TEST_P(SemanticConditionTest, NonBooleanWhileConditionIsTypeError)
{
    main_({ while_(operand_of(GetParam()), {}) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

INSTANTIATE_TEST_SUITE_P(NonBooleanTypes, SemanticConditionTest, ::testing::Values(T_INT, T_DOUBLE, T_STRING, T_UNIT),
    [](const ::testing::TestParamInfo<type_kind_t>& info) { return kind_name(info.param); });

// if (..) { } else { }
TEST_F(SemanticPass2Test, IfElseWithBooleanConditionIsOk)
{
    main_({ if_else(cond(), {}, {}) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// if (..) { }   -> BOOLTHEN allows if without else
TEST_F(SemanticPass2Test, IfWithoutElseIsOk)
{
    main_({ if_(cond(), { expr(call("print", { S("a") })) }) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// while (1 < 2.5) {}   -> condition operands get converted
TEST_F(SemanticPass2Test, MixedNumericConditionInsertsConvert)
{
    ast_node_t* c = bin(AST_OP_LT, I(1), D(2.5));
    main_({ while_(c, {}) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_TRUE(is_convert_of(c->binary.left, AST_INT_LIT));
}

// if (readBoolean()) {}
TEST_F(SemanticPass2Test, BooleanCallAsConditionIsOk)
{
    main_({ if_(call("readBoolean"), {}) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// if (x < 1) {}   (x undefined)
TEST_F(SemanticPass2Test, UndefinedVariableInConditionIsUndefined)
{
    main_({ if_(bin(AST_OP_LT, V("x"), I(1)), {}) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// if (..) { x } else { }
TEST_F(SemanticPass2Test, ErrorInThenBranchIsReported)
{
    main_({ if_else(cond(), { expr(V("x")) }, {}) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// if (..) { } else { x }
TEST_F(SemanticPass2Test, ErrorInElseBranchIsReported)
{
    main_({ if_else(cond(), {}, { expr(V("x")) }) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// while (..) { x }
TEST_F(SemanticPass2Test, ErrorInWhileBodyIsReported)
{
    main_({ while_(cond(), { expr(V("x")) }) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// Unreachable code is checked too: if (..) { return; x }
TEST_F(SemanticPass2Test, ErrorAfterReturnIsStillReported)
{
    main_({ if_(cond(), { ret(), expr(V("x")) }) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// ========================================
//  return
// ========================================

// def f() = { return 1 }   -> a function containing return needs an explicit type
TEST_F(SemanticPass2Test, ReturnInInferredFunctionIsTypeError)
{
    func("f", nullptr, {}, { ret(I(1)) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f() = { if (..) { return 1 }; 2 }   -> also when the return is nested
TEST_F(SemanticPass2Test, NestedReturnInInferredFunctionIsTypeError)
{
    func("f", nullptr, {}, { if_(cond(), { ret(I(1)) }), expr(I(2)) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Int = { return 1 }
TEST_F(SemanticPass2Test, ReturnMatchingValueIsOk)
{
    func("f", TYPE_INT, {}, { ret(I(1)) });
    main_({});
    EXPECT_EQ(analyze(), ERR_OK);
}

// def f(): Double = { return 1 }   -> the value gets converted
TEST_F(SemanticPass2Test, ReturnIntInDoubleFunctionInsertsConvert)
{
    ast_node_t* r = ret(I(1));
    func("f", TYPE_DOUBLE, {}, { r });
    main_({});

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_TRUE(is_convert_of(r->return_stmt.value, AST_INT_LIT));
}

// def f(): Int = { return "a" }
TEST_F(SemanticPass2Test, ReturnStringInIntFunctionIsTypeError)
{
    func("f", TYPE_INT, {}, { ret(S("a")) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Int = { return 2.5 }
TEST_F(SemanticPass2Test, ReturnDoubleInIntFunctionIsTypeError)
{
    func("f", TYPE_INT, {}, { ret(D(2.5)) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Unit = { return 5 }   -> rule 4: the value of an explicit-Unit function is discarded
TEST_F(SemanticPass2Test, ReturnValueInUnitFunctionIsDiscarded)
{
    ast_node_t* r = ret(I(5));
    func("f", TYPE_UNIT, {}, { r });
    main_({});

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(r->return_stmt.value->kind, AST_INT_LIT) << "nothing to convert, the value is discarded";
}

// def f(): Unit = { return }
TEST_F(SemanticPass2Test, BareReturnInUnitFunctionIsOk)
{
    func("f", TYPE_UNIT, {}, { ret() });
    main_({});
    EXPECT_EQ(analyze(), ERR_OK);
}

// @main def main(): Unit = { return }
TEST_F(SemanticPass2Test, BareReturnInMainIsOk)
{
    main_({ ret() });
    EXPECT_EQ(analyze(), ERR_OK);
}

// def f(): Int = { return y }   (y undefined)
TEST_F(SemanticPass2Test, UndefinedVariableInReturnValueIsUndefined)
{
    func("f", TYPE_INT, {}, { ret(V("y")) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// def f(): Int = { while (..) { if (..) { return 1 } }; 2 }
TEST_F(SemanticPass2Test, ReturnInsideNestedBlocksIsOk)
{
    func("f", TYPE_INT, {}, { while_(cond(), { if_(cond(), { ret(I(1)) }) }), expr(I(2)) });
    main_({});
    EXPECT_EQ(analyze(), ERR_OK);
}

// ========================================
//  Function result type
// ========================================

// ----------------------------------------
//  Explicit return type: body result must be compatible
// ----------------------------------------

// def f(): Double = { 1 }   -> the last expression gets converted, the AST types stay consistent
TEST_F(SemanticPass2Test, DoubleFunctionWithIntBodyInsertsConvert)
{
    ast_node_t* f = func("f", TYPE_DOUBLE, {}, { expr(I(1)) });
    main_({});

    ASSERT_EQ(analyze(), ERR_OK);
    ast_node_t* last = stmt_at(f, 0);
    EXPECT_TRUE(is_convert_of(last->expr_stmt.expr, AST_INT_LIT));
    EXPECT_EQ(type_name(last->type), "Double") << "statement type updated after the conversion";
    EXPECT_EQ(type_name(f->func_def.body->type), "Double") << "body type updated after the conversion";
}

// def f(): Int = { 1 + 2 }
TEST_F(SemanticPass2Test, IntFunctionWithIntBodyIsOk)
{
    func("f", TYPE_INT, {}, { expr(bin(AST_OP_ADD, I(1), I(2))) });
    main_({});
    EXPECT_EQ(analyze(), ERR_OK);
}

// def f(): String = { readLine() }
TEST_F(SemanticPass2Test, StringFunctionEndingWithCallIsOk)
{
    func("f", TYPE_STRING, {}, { expr(call("readLine")) });
    main_({});
    EXPECT_EQ(analyze(), ERR_OK);
}

// def f(): Int = { "a" }
TEST_F(SemanticPass2Test, IntFunctionWithStringBodyIsTypeError)
{
    func("f", TYPE_INT, {}, { expr(S("a")) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Int = { 2.5 }
TEST_F(SemanticPass2Test, IntFunctionWithDoubleBodyIsTypeError)
{
    func("f", TYPE_INT, {}, { expr(D(2.5)) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Int = { val x = 1 }   -> the body result is Unit
TEST_F(SemanticPass2Test, IntFunctionEndingWithVarDefIsTypeError)
{
    func("f", TYPE_INT, {}, { val("x", I(1)) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Int = { var x = 1; x = 2 }
TEST_F(SemanticPass2Test, IntFunctionEndingWithAssignIsTypeError)
{
    func("f", TYPE_INT, {}, { var("x", I(1)), assign("x", I(2)) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Int = { while (..) { } }
TEST_F(SemanticPass2Test, IntFunctionEndingWithWhileIsTypeError)
{
    func("f", TYPE_INT, {}, { while_(cond(), {}) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Int = { }
TEST_F(SemanticPass2Test, IntFunctionWithEmptyBodyIsTypeError)
{
    func("f", TYPE_INT, {}, {});
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Int = { () }
TEST_F(SemanticPass2Test, IntFunctionEndingWithUnitIsTypeError)
{
    func("f", TYPE_INT, {}, { expr(U()) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Int = { val x = 1; return x }
TEST_F(SemanticPass2Test, IntFunctionEndingWithReturnIsOk)
{
    func("f", TYPE_INT, {}, { val("x", I(1)), ret(V("x")) });
    main_({});
    EXPECT_EQ(analyze(), ERR_OK);
}

// def f(): Unit = { 1 }   -> rule 4: discarded
TEST_F(SemanticPass2Test, UnitFunctionWithIntBodyIsOk)
{
    func("f", TYPE_UNIT, {}, { expr(I(1)) });
    main_({});
    EXPECT_EQ(analyze(), ERR_OK);
}

// def f(): Unit = { "a" }
TEST_F(SemanticPass2Test, UnitFunctionWithStringBodyIsOk)
{
    func("f", TYPE_UNIT, {}, { expr(S("a")) });
    main_({});
    EXPECT_EQ(analyze(), ERR_OK);
}

// def f(): Unit = { }
TEST_F(SemanticPass2Test, UnitFunctionWithEmptyBodyIsOk)
{
    func("f", TYPE_UNIT, {}, {});
    main_({});
    EXPECT_EQ(analyze(), ERR_OK);
}

// ----------------------------------------
//  Inferred return type
// ----------------------------------------

// def f() = { 1 }
TEST_F(SemanticPass2Test, InferredFromLastExpression)
{
    ast_node_t* f = func("f", nullptr, {}, { expr(I(1)) });
    main_({});

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(f->func_def.return_type), "Int");
}

// def f() = { "a"; 2.5 }   -> only the LAST statement counts
TEST_F(SemanticPass2Test, InferredFromLastStatementOnly)
{
    ast_node_t* f = func("f", nullptr, {}, { expr(S("a")), expr(D(2.5)) });
    main_({});

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(f->func_def.return_type), "Double");
}

// def f(a: Double) = { a }
TEST_F(SemanticPass2Test, InferredFromParam)
{
    ast_node_t* f = func("f", nullptr, { { "a", TYPE_DOUBLE } }, { expr(V("a")) });
    main_({});

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(f->func_def.return_type), "Double");
}

// def f() = { 1 < 2 }   -> BOOLTHEN: a function can return Boolean
TEST_F(SemanticPass2Test, InferredBooleanIsOk)
{
    ast_node_t* f = func("f", nullptr, {}, { expr(cond()) });
    main_({});

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(f->func_def.return_type), "Boolean");
}

struct unit_body_case_t {
    const char* name;
    ast_node_t* (*make_last)(); // builds the last statement
};

static void PrintTo(const unit_body_case_t& c, std::ostream* os) { *os << c.name; }

class SemanticInferredUnitTest : public SemanticPass2Test, public ::testing::WithParamInterface<unit_body_case_t> { };

// def f() = { var x = 1; <last> }   -> var def / assign / while / if / nothing make the result Unit
TEST_P(SemanticInferredUnitTest, BodyResultIsUnit)
{
    ast_node_t* last = GetParam().make_last();
    ast_node_t* f = last == nullptr ? func("f", nullptr, {}, {}) : func("f", nullptr, {}, { var("x", I(1)), last });
    main_({});

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(f->func_def.return_type), "Unit");
}

INSTANTIATE_TEST_SUITE_P(LastStatements, SemanticInferredUnitTest,
    ::testing::Values(unit_body_case_t { "EmptyBody", []() -> ast_node_t* { return nullptr; } },
        unit_body_case_t { "VarDef", []() { return val("y", I(2)); } },
        unit_body_case_t { "Assign", []() { return assign("x", I(2)); } },
        unit_body_case_t { "While", []() { return while_(cond(), {}); } },
        unit_body_case_t { "UnitLiteral", []() { return expr(U()); } },
        unit_body_case_t { "PrintCall", []() { return expr(call("print", { V("x") })); } }),
    [](const ::testing::TestParamInfo<unit_body_case_t>& info) { return std::string(info.param.name); });

// main: val x: Int = g(); def g() = { "a" }   -> the caller sees the inferred type
TEST_F(SemanticPass2Test, InferredTypeIsUsedByCaller)
{
    main_({ val("x", call("g"), TYPE_INT) });
    func("g", nullptr, {}, { expr(S("a")) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// main: val x: String = g(); def g() = { "a" }
TEST_F(SemanticPass2Test, InferredTypeMatchingCallerIsOk)
{
    main_({ val("x", call("g"), TYPE_STRING) });
    func("g", nullptr, {}, { expr(S("a")) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// def f() = { g() }; def g() = { 1 }   -> inference through another inferred function
TEST_F(SemanticPass2Test, InferredThroughAnotherInferredFunction)
{
    ast_node_t* f = func("f", nullptr, {}, { expr(call("g")) });
    ast_node_t* g = func("g", nullptr, {}, { expr(I(1)) });
    main_({});

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(f->func_def.return_type), "Int");
    EXPECT_EQ(type_name(g->func_def.return_type), "Int");
}

// ----------------------------------------
//  Rule 3: if as the last statement of a function body
// ----------------------------------------

// def f(): Unit = { if (..) { 1 } else { "a" } }   -> (a) explicit Unit, branches do not matter
TEST_F(SemanticPass2Test, ExplicitUnitWithValueBranchesIsOk)
{
    func("f", TYPE_UNIT, {}, { if_else(cond(), { expr(I(1)) }, { expr(S("a")) }) });
    main_({});
    EXPECT_EQ(analyze(), ERR_OK);
}

// def f(): Int = { if (..) { return 1 } else { return 2 } }   -> (b) every path returns
TEST_F(SemanticPass2Test, ExplicitIntWithReturnInEveryBranchIsOk)
{
    ast_node_t* f = func("f", TYPE_INT, {}, { if_else(cond(), { ret(I(1)) }, { ret(I(2)) }) });
    main_({});

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(f->func_def.body->type), "Int");
}

// def f(): Int = { if (..) { val x = 1; return x } else { return 2 } }
TEST_F(SemanticPass2Test, ReturnAfterOtherStatementsInBranchIsOk)
{
    func("f", TYPE_INT, {}, { if_else(cond(), { val("x", I(1)), ret(V("x")) }, { ret(I(2)) }) });
    main_({});
    EXPECT_EQ(analyze(), ERR_OK);
}

// def f(): Int = { if (..) { if (..) { return 1 } else { return 2 } } else { return 3 } }
TEST_F(SemanticPass2Test, NestedIfWithReturnOnEveryPathIsOk)
{
    func("f", TYPE_INT, {},
        { if_else(cond(), { if_else(cond(), { ret(I(1)) }, { ret(I(2)) }) }, { ret(I(3)) }) });
    main_({});
    EXPECT_EQ(analyze(), ERR_OK);
}

// def f(): Int = { if (..) { return 1 } else { 2 } }
TEST_F(SemanticPass2Test, ExplicitIntWithBranchWithoutReturnIsTypeError)
{
    func("f", TYPE_INT, {}, { if_else(cond(), { ret(I(1)) }, { expr(I(2)) }) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Int = { if (..) { 1 } else { 2 } }   -> values without return are not enough
TEST_F(SemanticPass2Test, ExplicitIntWithValueBranchesIsTypeError)
{
    func("f", TYPE_INT, {}, { if_else(cond(), { expr(I(1)) }, { expr(I(2)) }) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Int = { if (..) { return 1 } }   -> the missing else is a path without return
TEST_F(SemanticPass2Test, ExplicitIntWithIfWithoutElseIsTypeError)
{
    func("f", TYPE_INT, {}, { if_(cond(), { ret(I(1)) }) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Int = { if (..) { if (..) { return 1 } } else { return 3 } }   -> a path deep inside is missing
TEST_F(SemanticPass2Test, NestedIfWithMissingReturnIsTypeError)
{
    func("f", TYPE_INT, {}, { if_else(cond(), { if_(cond(), { ret(I(1)) }) }, { ret(I(3)) }) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f() = { if (..) { print(1) } else { val x = 1 } }   -> (c) inferred, every path Unit
TEST_F(SemanticPass2Test, InferredWithUnitBranchesIsUnit)
{
    ast_node_t* f = func("f", nullptr, {}, { if_else(cond(), { expr(call("print", { I(1) })) }, { val("x", I(1)) }) });
    main_({});

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(f->func_def.return_type), "Unit");
}

// def f() = { if (..) { } else { } }
TEST_F(SemanticPass2Test, InferredWithEmptyBranchesIsUnit)
{
    ast_node_t* f = func("f", nullptr, {}, { if_else(cond(), {}, {}) });
    main_({});

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(f->func_def.return_type), "Unit");
}

// def f() = { if (..) { print(1) } }   -> without else the missing branch is Unit
TEST_F(SemanticPass2Test, InferredWithIfWithoutElseIsUnit)
{
    ast_node_t* f = func("f", nullptr, {}, { if_(cond(), { expr(call("print", { I(1) })) }) });
    main_({});

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(f->func_def.return_type), "Unit");
}

// def f() = { if (..) { 1 } else { 2 } }   -> an if is not an expression, the type cannot be inferred
TEST_F(SemanticPass2Test, InferredWithValueBranchesIsTypeError)
{
    func("f", nullptr, {}, { if_else(cond(), { expr(I(1)) }, { expr(I(2)) }) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f() = { if (..) { print(1) } else { 2 } }
TEST_F(SemanticPass2Test, InferredWithOneValueBranchIsTypeError)
{
    func("f", nullptr, {}, { if_else(cond(), { expr(call("print", { I(1) })) }, { expr(I(2)) }) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f() = { if (..) { if (..) { 1 } else { } } else { } }   -> checked recursively
TEST_F(SemanticPass2Test, InferredWithNestedValueBranchIsTypeError)
{
    func("f", nullptr, {}, { if_else(cond(), { if_else(cond(), { expr(I(1)) }, {}) }, {}) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Int = { if (..) { 1 } else { "a" }; 2 }   -> an if that is NOT last is just Unit
TEST_F(SemanticPass2Test, IfThatIsNotLastDoesNotMatter)
{
    func("f", TYPE_INT, {}, { if_else(cond(), { expr(I(1)) }, { expr(S("a")) }), expr(I(2)) });
    main_({});
    EXPECT_EQ(analyze(), ERR_OK);
}

// ----------------------------------------
//  @main result type
// ----------------------------------------

// @main def main() = { print("a") }
TEST_F(SemanticPass2Test, InferredMainWithUnitBodyIsOk)
{
    func("main", nullptr, {}, { expr(call("print", { S("a") })) }, true);
    EXPECT_EQ(analyze(), ERR_OK);
}

// @main def main() = { }
TEST_F(SemanticPass2Test, InferredMainWithEmptyBodyIsOk)
{
    func("main", nullptr, {}, {}, true);
    EXPECT_EQ(analyze(), ERR_OK);
}

// @main def main() = { 1 }   -> inferred Int, @main must be Unit
TEST_F(SemanticPass2Test, InferredMainWithIntBodyIsTypeError)
{
    func("main", nullptr, {}, { expr(I(1)) }, true);
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// @main def main(): Unit = { 1 }   -> explicit Unit discards the value
TEST_F(SemanticPass2Test, ExplicitUnitMainEndingWithExpressionIsOk)
{
    main_({ expr(I(1)) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// @main def main() = { return }   -> return needs an explicit type, also in main
TEST_F(SemanticPass2Test, ReturnInInferredMainIsTypeError)
{
    func("main", nullptr, {}, { ret() }, true);
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// ========================================
//  Calls
// ========================================

// ----------------------------------------
//  User functions
// ----------------------------------------

// main: f()   (f not defined)
TEST_F(SemanticPass2Test, CallOfUndefinedFunctionIsUndefined)
{
    main_({ expr(call("f")) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// val f = 1; f()   -> a variable is not a function
TEST_F(SemanticPass2Test, CallOfVariableIsUndefined)
{
    main_({ val("f", I(1)), expr(call("f")) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// main: f(1); def f(a: Int): Unit = {}   -> a function can be called before its definition
TEST_F(SemanticPass2Test, CallBeforeDefinitionIsOk)
{
    ast_node_t* c = call("f", { I(1) });
    main_({ expr(c) });
    func("f", TYPE_UNIT, { { "a", TYPE_INT } }, {});

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_FALSE(c->call.is_builtin);
}

// def f(a: Int, b: Int): Unit = {}; f(1)
TEST_F(SemanticPass2Test, TooFewArgumentsIsCallError)
{
    func("f", TYPE_UNIT, { { "a", TYPE_INT }, { "b", TYPE_INT } }, {});
    main_({ expr(call("f", { I(1) })) });
    EXPECT_EQ(analyze(), ERR_SEM_CALL);
}

// def f(a: Int): Unit = {}; f(1, 2)
TEST_F(SemanticPass2Test, TooManyArgumentsIsCallError)
{
    func("f", TYPE_UNIT, { { "a", TYPE_INT } }, {});
    main_({ expr(call("f", { I(1), I(2) })) });
    EXPECT_EQ(analyze(), ERR_SEM_CALL);
}

// def f(): Unit = {}; f(1)
TEST_F(SemanticPass2Test, ArgumentToParameterlessFunctionIsCallError)
{
    func("f", TYPE_UNIT, {}, {});
    main_({ expr(call("f", { I(1) })) });
    EXPECT_EQ(analyze(), ERR_SEM_CALL);
}

// def f(a: Int): Unit = {}; f("a")   -> a wrong argument type is 5, not 6
TEST_F(SemanticPass2Test, WrongArgumentTypeIsCallError)
{
    func("f", TYPE_UNIT, { { "a", TYPE_INT } }, {});
    main_({ expr(call("f", { S("a") })) });
    EXPECT_EQ(analyze(), ERR_SEM_CALL);
}

// def f(a: Int): Unit = {}; f(2.5)
TEST_F(SemanticPass2Test, DoubleArgumentForIntParamIsCallError)
{
    func("f", TYPE_UNIT, { { "a", TYPE_INT } }, {});
    main_({ expr(call("f", { D(2.5) })) });
    EXPECT_EQ(analyze(), ERR_SEM_CALL);
}

// def f(a: Int, b: String): Unit = {}; f(1, 2)   -> the second argument is checked too
TEST_F(SemanticPass2Test, WrongSecondArgumentTypeIsCallError)
{
    func("f", TYPE_UNIT, { { "a", TYPE_INT }, { "b", TYPE_STRING } }, {});
    main_({ expr(call("f", { I(1), I(2) })) });
    EXPECT_EQ(analyze(), ERR_SEM_CALL);
}

// def f(a: Double): Unit = {}; f(1)   -> the argument gets converted (and stays in the call)
TEST_F(SemanticPass2Test, IntArgumentForDoubleParamInsertsConvert)
{
    func("f", TYPE_UNIT, { { "a", TYPE_DOUBLE } }, {});
    ast_node_t* c = call("f", { I(1) });
    main_({ expr(c) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_TRUE(is_convert_of(arg_at(c, 0), AST_INT_LIT));
}

// def f(a: Int, b: Double, c: Double): Unit = {}; f(1, 2, 3.5)   -> only the Int->Double one
TEST_F(SemanticPass2Test, OnlyArgumentsThatNeedItAreConverted)
{
    func("f", TYPE_UNIT, { { "a", TYPE_INT }, { "b", TYPE_DOUBLE }, { "c", TYPE_DOUBLE } }, {});
    ast_node_t* c = call("f", { I(1), I(2), D(3.5) });
    main_({ expr(c) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(arg_at(c, 0)->kind, AST_INT_LIT);
    EXPECT_TRUE(is_convert_of(arg_at(c, 1), AST_INT_LIT));
    EXPECT_EQ(arg_at(c, 2)->kind, AST_DOUBLE_LIT);
}

// f(y)   (y undefined) -> the argument error wins
TEST_F(SemanticPass2Test, UndefinedVariableInArgumentIsUndefined)
{
    func("f", TYPE_UNIT, { { "a", TYPE_INT } }, {});
    main_({ expr(call("f", { V("y") })) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// f(1 + 2, g())   -> FUNEXP: arguments are any expressions, calls included
TEST_F(SemanticPass2Test, ExpressionAndCallArgumentsAreOk)
{
    func("f", TYPE_UNIT, { { "a", TYPE_INT }, { "b", TYPE_STRING } }, {});
    func("g", TYPE_STRING, {}, { expr(S("a")) });
    main_({ expr(call("f", { bin(AST_OP_ADD, I(1), I(2)), call("g") })) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// val x = f() + 1   -> FUNEXP: a call inside an expression
TEST_F(SemanticPass2Test, CallInsideExpressionIsOk)
{
    func("f", TYPE_INT, {}, { expr(I(1)) });
    ast_node_t* e = bin(AST_OP_ADD, call("f"), D(1.5));

    ASSERT_EQ(analyze_expr(e), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Double");
    EXPECT_TRUE(is_convert_of(e->binary.left, AST_CALL)) << "an Int call result gets converted like any operand";
}

// def f(): String = { "a" }; val x = f()   -> the call has the return type
TEST_F(SemanticPass2Test, CallHasReturnTypeOfFunction)
{
    func("f", TYPE_STRING, {}, { expr(S("a")) });
    ast_node_t* c = call("f");

    ASSERT_EQ(analyze_expr(c), ERR_OK);
    EXPECT_EQ(type_name(c->type), "String");
}

// ----------------------------------------
//  Builtin functions
// ----------------------------------------

struct builtin_case_t {
    const char* name;
    std::vector<type_kind_t> args;
    error_code_t err;
    type_kind_t result; // only when err == ERR_OK
};

static void PrintTo(const builtin_case_t& c, std::ostream* os)
{
    *os << c.name << "(";
    for (size_t i = 0; i < c.args.size(); i++)
        *os << (i > 0 ? ", " : "") << kind_name(c.args[i]);
    *os << ")";
}

class SemanticBuiltinCallTest : public SemanticPass2Test, public ::testing::WithParamInterface<builtin_case_t> { };

// val r = <builtin>(<args>)
TEST_P(SemanticBuiltinCallTest, BuiltinCall)
{
    const builtin_case_t& c = GetParam();
    ast_node_t* node = call(c.name);
    for (type_kind_t kind : c.args)
        EXPECT_TRUE(dyn_arr_append(&node->call.args, operand_of(kind)));

    ASSERT_EQ(analyze_expr(node), c.err);
    if (c.err != ERR_OK)
        return;
    EXPECT_TRUE(node->call.is_builtin);
    EXPECT_EQ(type_name(node->type), kind_name(c.result));
}

INSTANTIATE_TEST_SUITE_P(Builtins, SemanticBuiltinCallTest,
    ::testing::Values(
        // print(Any): Unit
        builtin_case_t { "print", { T_INT }, ERR_OK, T_UNIT },
        builtin_case_t { "print", { T_DOUBLE }, ERR_OK, T_UNIT },
        builtin_case_t { "print", { T_STRING }, ERR_OK, T_UNIT },
        builtin_case_t { "print", { T_UNIT }, ERR_OK, T_UNIT },
        builtin_case_t { "print", { T_BOOL }, ERR_OK, T_UNIT },
        builtin_case_t { "print", {}, ERR_SEM_CALL, T_ANY },
        builtin_case_t { "print", { T_INT, T_INT }, ERR_SEM_CALL, T_ANY },
        // read*(): T
        builtin_case_t { "readLine", {}, ERR_OK, T_STRING },
        builtin_case_t { "readLine", { T_INT }, ERR_SEM_CALL, T_ANY },
        builtin_case_t { "readInt", {}, ERR_OK, T_INT },
        builtin_case_t { "readInt", { T_INT }, ERR_SEM_CALL, T_ANY },
        builtin_case_t { "readDouble", {}, ERR_OK, T_DOUBLE },
        builtin_case_t { "readBoolean", {}, ERR_OK, T_BOOL },
        // toStr(Any): String
        builtin_case_t { "toStr", { T_INT }, ERR_OK, T_STRING },
        builtin_case_t { "toStr", { T_DOUBLE }, ERR_OK, T_STRING },
        builtin_case_t { "toStr", { T_BOOL }, ERR_OK, T_STRING },
        builtin_case_t { "toStr", {}, ERR_SEM_CALL, T_ANY },
        // toInt(Double): Int
        builtin_case_t { "toInt", { T_DOUBLE }, ERR_OK, T_INT },
        builtin_case_t { "toInt", { T_INT }, ERR_OK, T_INT },
        builtin_case_t { "toInt", { T_STRING }, ERR_SEM_CALL, T_ANY },
        // strLength(String): Int
        builtin_case_t { "strLength", { T_STRING }, ERR_OK, T_INT },
        builtin_case_t { "strLength", { T_INT }, ERR_SEM_CALL, T_ANY },
        // subStr(String, Int, Int): String
        builtin_case_t { "subStr", { T_STRING, T_INT, T_INT }, ERR_OK, T_STRING },
        builtin_case_t { "subStr", { T_STRING, T_INT }, ERR_SEM_CALL, T_ANY },
        builtin_case_t { "subStr", { T_STRING, T_DOUBLE, T_INT }, ERR_SEM_CALL, T_ANY },
        builtin_case_t { "subStr", { T_INT, T_INT, T_INT }, ERR_SEM_CALL, T_ANY },
        // ord(String, Int): Int
        builtin_case_t { "ord", { T_STRING, T_INT }, ERR_OK, T_INT },
        builtin_case_t { "ord", { T_STRING }, ERR_SEM_CALL, T_ANY },
        builtin_case_t { "ord", { T_INT, T_STRING }, ERR_SEM_CALL, T_ANY },
        // chr(Int): String
        builtin_case_t { "chr", { T_INT }, ERR_OK, T_STRING },
        builtin_case_t { "chr", { T_DOUBLE }, ERR_SEM_CALL, T_ANY },
        builtin_case_t { "chr", { T_STRING }, ERR_SEM_CALL, T_ANY }),
    [](const ::testing::TestParamInfo<builtin_case_t>& info) {
        std::string name = info.param.name;
        if (info.param.args.empty())
            name += "_none";
        for (type_kind_t kind : info.param.args)
            name += "_" + kind_name(kind);
        return name;
    });

// print(1)   -> an Any param never converts
TEST_F(SemanticPass2Test, PrintDoesNotConvertItsArgument)
{
    ast_node_t* c = call("print", { I(1) });
    main_({ expr(c) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(arg_at(c, 0)->kind, AST_INT_LIT);
}

// toInt(1)   -> toInt takes a Double, the Int argument is converted
TEST_F(SemanticPass2Test, ToIntWithIntArgumentInsertsConvert)
{
    ast_node_t* c = call("toInt", { I(1) });
    main_({ expr(c) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_TRUE(is_convert_of(arg_at(c, 0), AST_INT_LIT));
}

// print(toStr(1) + "a")   -> builtins nest like any other call
TEST_F(SemanticPass2Test, NestedBuiltinCallsAreOk)
{
    main_({ expr(call("print", { bin(AST_OP_ADD, call("toStr", { I(1) }), S("a")) })) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// ----------------------------------------
//  Lazy checking and the caller's context
// ----------------------------------------

// def f(): Unit = { y }   -> a function that is never called is still checked
TEST_F(SemanticPass2Test, UncalledFunctionIsStillChecked)
{
    func("f", TYPE_UNIT, {}, { expr(V("y")) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// main: f(); def f(): Unit = { y }   -> the error found through a lazy check is reported
TEST_F(SemanticPass2Test, ErrorInLazilyCheckedCalleeIsReported)
{
    main_({ expr(call("f")) });
    func("f", TYPE_UNIT, {}, { expr(V("y")) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// main: f(); f(); def f(): Unit = { val x = 1 }   -> f is checked once, its locals are not duplicated
TEST_F(SemanticPass2Test, FunctionIsCheckedOnlyOnce)
{
    main_({ expr(call("f")), expr(call("f")) });
    ast_node_t* f = func("f", TYPE_UNIT, {}, { val("x", I(1)) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(f->func_def.locals.count, 1u);
}

// main: val x = 1; f(); def f() = { x }   -> main's x is on the scope stack, but not f's
TEST_F(SemanticPass2Test, CalleeCannotSeeCallerVariables)
{
    main_({ val("x", I(1)), expr(call("f")) });
    func("f", nullptr, {}, { expr(V("x")) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// main: val x = 1; if (..) { val y = 2; f() }; def f() = { y }   -> also from a nested block
TEST_F(SemanticPass2Test, CalleeCannotSeeCallerNestedVariables)
{
    main_({ val("x", I(1)), if_(cond(), { val("y", I(2)), expr(call("f")) }) });
    func("f", nullptr, {}, { expr(V("y")) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// main: val x = 1; f("a"); x   with def f(x: String): Unit = { val y = x }
// -> after the lazy check main sees its own x again (context restored)
TEST_F(SemanticPass2Test, CallerContextIsRestoredAfterLazyCheck)
{
    ast_node_t* outer = val("x", I(1));
    ast_node_t* ref = V("x");
    main_({ outer, expr(call("f", { S("a") })), expr(ref) });
    func("f", TYPE_UNIT, { { "x", TYPE_STRING } }, { val("y", V("x")) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(ref->var_ref.id, outer->var_def.id);
    EXPECT_EQ(type_name(ref->type), "Int");
}

// main: var x = 1; f(); x = 2   -> after the lazy check the assignment still finds main's var
TEST_F(SemanticPass2Test, AssignAfterLazyCheckIsOk)
{
    main_({ var("x", I(1)), expr(call("f")), assign("x", I(2)) });
    func("f", TYPE_UNIT, {}, { val("x", S("a")) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// main: f(); def f() = { g() }; def g() = { 1 }   -> a chain of lazy checks
TEST_F(SemanticPass2Test, ChainOfLazyChecksIsOk)
{
    main_({ val("x", call("f"), TYPE_INT) });
    func("f", nullptr, {}, { expr(call("g")) });
    func("g", nullptr, {}, { expr(I(1)) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// ----------------------------------------
//  Recursion
// ----------------------------------------

// def f(n: Int): Int = { f(n) }
TEST_F(SemanticPass2Test, DirectRecursionWithExplicitTypeIsOk)
{
    func("f", TYPE_INT, { { "n", TYPE_INT } }, { expr(call("f", { V("n") })) });
    main_({});
    EXPECT_EQ(analyze(), ERR_OK);
}

// def f(n: Int) = { f(n) }
TEST_F(SemanticPass2Test, DirectRecursionWithInferredTypeIsTypeError)
{
    func("f", nullptr, { { "n", TYPE_INT } }, { expr(call("f", { V("n") })) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Unit = { f() }
TEST_F(SemanticPass2Test, DirectRecursionOfUnitFunctionIsOk)
{
    func("f", TYPE_UNIT, {}, { expr(call("f")) });
    main_({});
    EXPECT_EQ(analyze(), ERR_OK);
}

// def fact(n: Int): Int = { if (n < 1) { return 1 } else { return n * fact(n - 1) } }
TEST_F(SemanticPass2Test, FactorialIsOk)
{
    func("fact", TYPE_INT, { { "n", TYPE_INT } },
        { if_else(bin(AST_OP_LT, V("n"), I(1)), { ret(I(1)) },
            { ret(bin(AST_OP_MUL, V("n"), call("fact", { bin(AST_OP_SUB, V("n"), I(1)) }))) }) });
    main_({ expr(call("print", { call("fact", { I(5) }) })) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// def f(n: Int) = { if (n < 1) { print(n) } else { f(n - 1) } }   -> recursion hidden in a branch
TEST_F(SemanticPass2Test, InferredRecursionInBranchIsTypeError)
{
    func("f", nullptr, { { "n", TYPE_INT } },
        { if_else(bin(AST_OP_LT, V("n"), I(1)), { expr(call("print", { V("n") })) },
            { expr(call("f", { bin(AST_OP_SUB, V("n"), I(1)) })) }) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Int = { g() }; def g(): Int = { f() }   -> mutual recursion, both explicit
TEST_F(SemanticPass2Test, IndirectRecursionWithExplicitTypesIsOk)
{
    func("f", TYPE_INT, {}, { expr(call("g")) });
    func("g", TYPE_INT, {}, { expr(call("f")) });
    main_({});
    EXPECT_EQ(analyze(), ERR_OK);
}

// def f(): Int = { g() }; def g() = { f() }   -> f is checked first, g is the inferred one
TEST_F(SemanticPass2Test, IndirectRecursionExplicitFirstIsTypeError)
{
    func("f", TYPE_INT, {}, { expr(call("g")) });
    func("g", nullptr, {}, { expr(call("f")) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def g() = { f() }; def f(): Int = { g() }   -> the same program, g is checked first
TEST_F(SemanticPass2Test, IndirectRecursionInferredFirstIsTypeError)
{
    func("g", nullptr, {}, { expr(call("f")) });
    func("f", TYPE_INT, {}, { expr(call("g")) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// main: f(); def f(): Int = { g() }; def g() = { f() }   -> the cycle is entered from main
TEST_F(SemanticPass2Test, IndirectRecursionReachedFromMainIsTypeError)
{
    main_({ expr(call("f")) });
    func("f", TYPE_INT, {}, { expr(call("g")) });
    func("g", nullptr, {}, { expr(call("f")) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// f -> h -> g -> f with only h inferred, in every definition order
class SemanticThreeCycleTest : public SemanticPass2Test, public ::testing::WithParamInterface<int> { };

TEST_P(SemanticThreeCycleTest, InferredFunctionInCycleIsTypeError)
{
    auto add_f = [&]() { func("f", TYPE_INT, {}, { expr(call("h")) }); };
    auto add_h = [&]() { func("h", nullptr, {}, { expr(call("g")) }); };
    auto add_g = [&]() { func("g", TYPE_INT, {}, { expr(call("f")) }); };

    switch (GetParam()) {
    case 0:
        add_f(), add_h(), add_g();
        break;
    case 1:
        add_h(), add_g(), add_f();
        break;
    default:
        add_g(), add_f(), add_h();
        break;
    }
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

static std::string cycle_order_name(const ::testing::TestParamInfo<int>& info)
{
    const char* names[] = { "FirstF", "FirstH", "FirstG" };
    return names[info.param];
}

INSTANTIATE_TEST_SUITE_P(DefinitionOrders, SemanticThreeCycleTest, ::testing::Values(0, 1, 2), cycle_order_name);

// def h() = { f() }; def f(): Int = { g() }; def g(): Int = { f() }
// -> h calls INTO the cycle but is not part of it, so its type can be inferred
TEST_F(SemanticPass2Test, FunctionCallingIntoCycleMayBeInferred)
{
    ast_node_t* h = func("h", nullptr, {}, { expr(call("f")) });
    func("f", TYPE_INT, {}, { expr(call("g")) });
    func("g", TYPE_INT, {}, { expr(call("f")) });
    main_({ expr(call("h")) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(h->func_def.return_type), "Int");
}

// ========================================
//  BOOLTHEN: true / false, !, && and ||, else if
// ========================================
// AI-generated using: Claude Code
//              model: Claude Fable 5.2
// Prompt: "can you also add the proposed tests for boolthen" (! and && / || against
//          every operand type, Boolean literals also as conditions and in returns,
//          else if chains; the && / || rows are in binary_cases above)
// Scope: this section up to "Open questions"

// ----------------------------------------
//  Literals
// ----------------------------------------

TEST_F(SemanticPass2Test, TrueLiteralIsBoolean)
{
    ast_node_t* e = B(true);
    ASSERT_EQ(analyze_expr(e), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Boolean");
}

TEST_F(SemanticPass2Test, FalseLiteralIsBoolean)
{
    ast_node_t* e = B(false);
    ASSERT_EQ(analyze_expr(e), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Boolean");
}

// val b = true; val x = 1; x   -> a literal must not disturb what follows it
TEST_F(SemanticPass2Test, StatementsAfterBooleanLiteralAreChecked)
{
    ast_node_t* ref = V("x");
    main_({ val("b", B(true)), val("x", I(1)), expr(ref) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(ref->type), "Int");
}

// ----------------------------------------
//  Unary !: Boolean only, result Boolean
// ----------------------------------------

struct not_case_t {
    type_kind_t operand;
    error_code_t err;
};

static void PrintTo(const not_case_t& c, std::ostream* os) { *os << "!" << kind_name(c.operand); }

class SemanticNotTest : public SemanticPass2Test, public ::testing::WithParamInterface<not_case_t> { };

// val r = !<operand>
TEST_P(SemanticNotTest, NotOfOperand)
{
    const not_case_t& c = GetParam();
    ast_node_t* e = not_(operand_of(c.operand));

    ASSERT_EQ(analyze_expr(e), c.err);
    if (c.err == ERR_OK) {
        EXPECT_EQ(type_name(e->type), "Boolean");
    }
}

INSTANTIATE_TEST_SUITE_P(Operands, SemanticNotTest,
    ::testing::Values(not_case_t { T_BOOL, ERR_OK }, not_case_t { T_INT, ERR_SEM_TYPE },
        not_case_t { T_DOUBLE, ERR_SEM_TYPE }, not_case_t { T_STRING, ERR_SEM_TYPE },
        not_case_t { T_UNIT, ERR_SEM_TYPE }),
    [](const ::testing::TestParamInfo<not_case_t>& info) { return kind_name(info.param.operand); });

// !true
TEST_F(SemanticPass2Test, NotOfLiteralIsBoolean)
{
    ast_node_t* e = not_(B(true));
    ASSERT_EQ(analyze_expr(e), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Boolean");
}

// !!false
TEST_F(SemanticPass2Test, DoubleNotIsOk)
{
    ast_node_t* e = not_(not_(B(false)));
    ASSERT_EQ(analyze_expr(e), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Boolean");
}

// -true   -> minus works on numbers only
TEST_F(SemanticPass2Test, NegationOfBooleanLiteralIsTypeError)
{
    EXPECT_EQ(analyze_expr(neg(B(true))), ERR_SEM_TYPE);
}

// !x   (x undefined) -> the operand's error wins
TEST_F(SemanticPass2Test, NotOfUndefinedVariableIsUndefined)
{
    EXPECT_EQ(analyze_expr(not_(V("x"))), ERR_SEM_UNDEF);
}

// ----------------------------------------
//  && and || in larger expressions
// ----------------------------------------

// !(1 < 2) && (2.5 > 1 || true)   -> Boolean, the inner 1 is still converted
TEST_F(SemanticPass2Test, MixedBooleanExpressionIsOk)
{
    ast_node_t* greater = bin(AST_OP_GT, D(2.5), I(1));
    ast_node_t* e = bin(AST_OP_AND, not_(cond()), bin(AST_OP_OR, greater, B(true)));

    ASSERT_EQ(analyze_expr(e), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Boolean");
    EXPECT_TRUE(is_convert_of(greater->binary.right, AST_INT_LIT));
    EXPECT_EQ(e->binary.left->kind, AST_UNARY) << "Boolean operands are never converted";
}

// true && false || true
TEST_F(SemanticPass2Test, ChainedBooleanOperatorsAreOk)
{
    ast_node_t* e = bin(AST_OP_OR, bin(AST_OP_AND, B(true), B(false)), B(true));
    ASSERT_EQ(analyze_expr(e), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Boolean");
}

// true == false
TEST_F(SemanticPass2Test, BooleanLiteralsCanBeCompared)
{
    ast_node_t* e = bin(AST_OP_EQ, B(true), B(false));
    ASSERT_EQ(analyze_expr(e), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Boolean");
}

// true < false   -> only == and != work on Booleans
TEST_F(SemanticPass2Test, OrderingOfBooleanLiteralsIsTypeError)
{
    EXPECT_EQ(analyze_expr(bin(AST_OP_LT, B(true), B(false))), ERR_SEM_TYPE);
}

// true + 1
TEST_F(SemanticPass2Test, BooleanLiteralPlusIntIsTypeError)
{
    EXPECT_EQ(analyze_expr(bin(AST_OP_ADD, B(true), I(1))), ERR_SEM_TYPE);
}

// true && (1 + 2)   -> the non-Boolean side is found even when it is nested
TEST_F(SemanticPass2Test, AndWithArithmeticOperandIsTypeError)
{
    EXPECT_EQ(analyze_expr(bin(AST_OP_AND, B(true), bin(AST_OP_ADD, I(1), I(2)))), ERR_SEM_TYPE);
}

// true || x   (x undefined) -> the right side is checked although || would skip it at runtime
TEST_F(SemanticPass2Test, UndefinedRightOperandOfOrIsUndefined)
{
    EXPECT_EQ(analyze_expr(bin(AST_OP_OR, B(true), V("x"))), ERR_SEM_UNDEF);
}

// ----------------------------------------
//  Booleans stored, passed and returned
// ----------------------------------------

// val b: Boolean = true
TEST_F(SemanticPass2Test, DeclaredBooleanVariableIsOk)
{
    main_({ val("b", B(true), TYPE_BOOL) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// val b: Boolean = 1
TEST_F(SemanticPass2Test, DeclaredBooleanWithIntInitIsTypeError)
{
    main_({ val("b", I(1), TYPE_BOOL) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// val x: Int = true
TEST_F(SemanticPass2Test, DeclaredIntWithBooleanInitIsTypeError)
{
    main_({ val("x", B(true), TYPE_INT) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// var b = true; b = !b && 1 < 2
TEST_F(SemanticPass2Test, AssignBooleanExpressionToBooleanVarIsOk)
{
    main_({ var("b", B(true)), assign("b", bin(AST_OP_AND, not_(V("b")), cond())) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// var b = true; b = 1
TEST_F(SemanticPass2Test, AssignIntToBooleanVarIsTypeError)
{
    main_({ var("b", B(true)), assign("b", I(1)) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// var x = 1; x = false
TEST_F(SemanticPass2Test, AssignBooleanToIntVarIsTypeError)
{
    main_({ var("x", I(1)), assign("x", B(false)) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def both(a: Boolean, b: Boolean) = { a && b }; val r = both(true, 1 < 2)
TEST_F(SemanticPass2Test, BooleanParamsAndInferredBooleanResult)
{
    ast_node_t* f = func("both", nullptr, { { "a", TYPE_BOOL }, { "b", TYPE_BOOL } },
        { expr(bin(AST_OP_AND, V("a"), V("b"))) });
    ast_node_t* c = call("both", { B(true), cond() });

    ASSERT_EQ(analyze_expr(c), ERR_OK);
    EXPECT_EQ(type_name(f->func_def.return_type), "Boolean");
    EXPECT_EQ(type_name(c->type), "Boolean");
}

// def f(a: Int): Unit = {}; f(true)   -> a wrong argument type is 5
TEST_F(SemanticPass2Test, BooleanArgumentForIntParamIsCallError)
{
    func("f", TYPE_UNIT, { { "a", TYPE_INT } }, {});
    main_({ expr(call("f", { B(true) })) });
    EXPECT_EQ(analyze(), ERR_SEM_CALL);
}

// def f(a: Boolean): Unit = {}; f(1)
TEST_F(SemanticPass2Test, IntArgumentForBooleanParamIsCallError)
{
    func("f", TYPE_UNIT, { { "a", TYPE_BOOL } }, {});
    main_({ expr(call("f", { I(1) })) });
    EXPECT_EQ(analyze(), ERR_SEM_CALL);
}

// def f(): Boolean = { return true }
TEST_F(SemanticPass2Test, ReturnBooleanLiteralIsOk)
{
    func("f", TYPE_BOOL, {}, { ret(B(true)) });
    main_({});
    EXPECT_EQ(analyze(), ERR_OK);
}

// def f(): Boolean = { return 1 }
TEST_F(SemanticPass2Test, ReturnIntInBooleanFunctionIsTypeError)
{
    func("f", TYPE_BOOL, {}, { ret(I(1)) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Int = { return false }
TEST_F(SemanticPass2Test, ReturnBooleanInIntFunctionIsTypeError)
{
    func("f", TYPE_INT, {}, { ret(B(false)) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Boolean = { !true }
TEST_F(SemanticPass2Test, BooleanFunctionWithBooleanBodyIsOk)
{
    func("f", TYPE_BOOL, {}, { expr(not_(B(true))) });
    main_({});
    EXPECT_EQ(analyze(), ERR_OK);
}

// def f(): Boolean = { 1 }
TEST_F(SemanticPass2Test, BooleanFunctionWithIntBodyIsTypeError)
{
    func("f", TYPE_BOOL, {}, { expr(I(1)) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// print(true); val s: String = toStr(false)
TEST_F(SemanticPass2Test, PrintAndToStrAcceptBooleanLiterals)
{
    main_({ expr(call("print", { B(true) })), val("s", call("toStr", { B(false) }), TYPE_STRING) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// ----------------------------------------
//  Conditions
// ----------------------------------------

// if (true) { }; while (false) { }
TEST_F(SemanticPass2Test, BooleanLiteralsAsConditionsAreOk)
{
    main_({ if_(B(true), {}), while_(B(false), {}) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// val x = 1; if (!(x < 2) || x == 3 && true) { }
TEST_F(SemanticPass2Test, CompoundConditionIsOk)
{
    main_({ val("x", I(1)),
        if_(bin(AST_OP_OR, not_(bin(AST_OP_LT, V("x"), I(2))), bin(AST_OP_AND, bin(AST_OP_EQ, V("x"), I(3)), B(true))),
            {}) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// if (1 && true) { }   -> the error is in the condition's operand
TEST_F(SemanticPass2Test, IntOperandInConditionIsTypeError)
{
    main_({ if_(bin(AST_OP_AND, I(1), B(true)), {}) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// val x = 1; while (!x) { }
TEST_F(SemanticPass2Test, NotOfIntVariableInConditionIsTypeError)
{
    main_({ val("x", I(1)), while_(not_(V("x")), {}) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// ----------------------------------------
//  else if (an if nested in the else block) as the last statement - rule 3
// ----------------------------------------

// def f(n: Int): Int = { if (n < 0) { return 1 } else if (n == 0) { return 2 } else { return 3 } }
TEST_F(SemanticPass2Test, ElseIfChainWithReturnEverywhereIsOk)
{
    func("f", TYPE_INT, { { "n", TYPE_INT } },
        { if_else(bin(AST_OP_LT, V("n"), I(0)), { ret(I(1)) },
            { if_else(bin(AST_OP_EQ, V("n"), I(0)), { ret(I(2)) }, { ret(I(3)) }) }) });
    main_({});
    EXPECT_EQ(analyze(), ERR_OK);
}

// def f(n: Int): Int = { if (n < 0) { return 1 } else if (n == 0) { return 2 } }   -> no final else
TEST_F(SemanticPass2Test, ElseIfChainWithoutFinalElseIsTypeError)
{
    func("f", TYPE_INT, { { "n", TYPE_INT } },
        { if_else(bin(AST_OP_LT, V("n"), I(0)), { ret(I(1)) }, { if_(bin(AST_OP_EQ, V("n"), I(0)), { ret(I(2)) }) }) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(n: Int) = { if (n < 0) { print(1) } else if (n == 0) { print(2) } }   -> every path Unit
TEST_F(SemanticPass2Test, InferredElseIfChainOfUnitBranchesIsUnit)
{
    ast_node_t* f = func("f", nullptr, { { "n", TYPE_INT } },
        { if_else(bin(AST_OP_LT, V("n"), I(0)), { expr(call("print", { I(1) })) },
            { if_(bin(AST_OP_EQ, V("n"), I(0)), { expr(call("print", { I(2) })) }) }) });
    main_({});

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(f->func_def.return_type), "Unit");
}

// def f(n: Int) = { if (n < 0) { print(1) } else if (n == 0) { 2 } }   -> one path has a value
TEST_F(SemanticPass2Test, InferredElseIfChainWithValueBranchIsTypeError)
{
    func("f", nullptr, { { "n", TYPE_INT } },
        { if_else(bin(AST_OP_LT, V("n"), I(0)), { expr(call("print", { I(1) })) },
            { if_(bin(AST_OP_EQ, V("n"), I(0)), { expr(I(2)) }) }) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// ========================================
//  Open questions (forum) - current behaviour, change if the answer differs
// ========================================

// x = 1 with no x at all: 3 (undefined) or 7 ("must be a defined modifiable variable, otherwise 7")?
TEST_F(SemanticPass2Test, OpenQuestion_AssignToUndefinedVariableIsUndefined)
{
    main_({ assign("x", I(1)) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// def f(): Int = { return }: 10 (disallowed return) or 6 (section 2.3)?
TEST_F(SemanticPass2Test, OpenQuestion_BareReturnInIntFunctionIsOtherError)
{
    func("f", TYPE_INT, {}, { ret() });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_OTHER);
}

// def f(a: Int): Unit = { val a = 1 }: params live in their own scope (Scala) -> OK,
// the spec's "params behave as local variables of the body" would make it 4
TEST_F(SemanticPass2Test, OpenQuestion_ParamShadowedInBodyIsOk)
{
    func("f", TYPE_UNIT, { { "a", TYPE_INT } }, { val("a", I(1)) });
    main_({});
    EXPECT_EQ(analyze(), ERR_OK);
}

// ========================================
//  Allocation failures (99)
// ========================================

// A program that exercises most allocations of pass 2: symbols in several scopes,
// CONVERT nodes, lazy checks of called functions
static ast_node_t* build_alloc_program()
{
    ast_node_t* program = ast_new_program();
    EXPECT_NE(program, nullptr);

    auto add = [&](const char* name, bool is_main, type_t* ret_type, const param_list_t& params, node_list_t body) {
        ast_node_t* fn = ast_new_func_def(name, is_main, 1, 1);
        EXPECT_NE(fn, nullptr);
        for (const auto& param : params)
            EXPECT_TRUE(ast_func_def_add_param(fn, param.first, param.second));
        fn->func_def.return_type = ret_type;
        fn->func_def.body = block(body);
        EXPECT_TRUE(dyn_arr_append(&program->program.func_list, fn));
    };

    // @main def main(): Unit = {
    //     val x: Double = 1
    //     var y = twice(2)
    //     if (y < x) { print(toStr(y)) } else { val z = half(y) }
    //     while (y > 0) { y = y - 1 }
    // }
    add("main", true, TYPE_UNIT, {},
        { val("x", I(1), TYPE_DOUBLE), var("y", call("twice", { I(2) })),
            if_else(bin(AST_OP_LT, V("y"), V("x")), { expr(call("print", { call("toStr", { V("y") }) })) },
                { val("z", call("half", { V("y") })) }),
            while_(bin(AST_OP_GT, V("y"), I(0)), { assign("y", bin(AST_OP_SUB, V("y"), I(1))) }) });
    // def twice(a: Int) = { a * 2 }
    add("twice", false, nullptr, { { "a", TYPE_INT } }, { expr(bin(AST_OP_MUL, V("a"), I(2))) });
    // def half(a: Double): Double = { return a / 2 }
    add("half", false, TYPE_DOUBLE, { { "a", TYPE_DOUBLE } }, { ret(bin(AST_OP_DIV, V("a"), I(2))) });
    return program;
}

// Fail the 1st, 2nd, 3rd, ... calloc inside semantic_analyze, each on a fresh program
// (analysis changes the AST, so a program is never analyzed twice). Every run must
// end with 99 - and not crash or leak (run under valgrind) - until n is larger than
// the number of callocs and the run succeeds.
TEST(SemanticPass2AllocTest, EveryAllocationFailureIsInternalError)
{
    int failures = 0;
    for (int n = 0; n < 1000; n++) {
        ast_node_t* program = build_alloc_program();

        calloc_countdown = n;
        error_code_t err = semantic_analyze(program);
        bool failure_was_triggered = (calloc_countdown == -1);
        calloc_countdown = -1;

        ast_free(program);

        if (!failure_was_triggered) {
            EXPECT_EQ(err, ERR_OK) << "no allocation failed, the run must succeed";
            break;
        }
        EXPECT_EQ(err, ERR_INTERNAL) << "calloc number " << n << " failed";
        failures++;
    }
    EXPECT_GT(failures, 0) << "semantic_analyze never called calloc";
}
