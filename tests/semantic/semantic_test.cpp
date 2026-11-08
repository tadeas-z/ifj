// Implementation of the IFJ26 imperative language compiler
// semantic_test.cpp
// Author(s): Adam Zatloukal (xzatloa01)
//
// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Can you create the proposed tests for pass 0 and 1" (black-box tests
//          through semantic_analyze: every builtin name clashes with a user
//          function (4), Print with a different case is fine, only main / main
//          without a return type / main at any position is fine (0), no main or
//          an empty program (3), two mains, duplicate function, duplicate param (4),
//          main with params (5), main returning Int/String/Double (6), non-main
//          functions with params and return types are fine, NULL / non-program
//          input and allocation failures (99))
// Scope: the whole file
//
// Note: these tests are about the declarations only. Every function gets a
// trivial body that pass 2 accepts: a literal of its written return type
// (0, 0.0, "") or "()" when the type is Unit or not written.
// Pass 2 itself is tested in semantic_pass2_test.cpp.

extern "C" {
#include "ast.h"
#include "dynamic_array.h"
#include "semantic.h"
}

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

// ----------------------------------------
//  Allocation failure injection
// ----------------------------------------
// The test binary is linked with -Wl,--wrap=calloc, so every calloc call goes
// through __wrap_calloc. While calloc_countdown is >= 0 it is decremented on
// every call and the call that brings it from 0 fails (returns NULL).
// -1 means "never fail".
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
//  Fixture: builds a program by hand, frees it afterwards
// ----------------------------------------

using param_list_t = std::vector<std::pair<const char*, type_t*>>;

class SemanticPass01Test : public ::testing::Test {
protected:
    ast_node_t* program = nullptr;

    void SetUp() override
    {
        program = ast_new_program();
        ASSERT_NE(program, nullptr);
    }

    void TearDown() override
    {
        calloc_countdown = -1; // never leak a failure into other tests
        ast_free(program);
    }

    // A literal of the given type, so the body matches the written return type
    static ast_node_t* literal_of(type_t* type)
    {
        if (type == nullptr)
            return ast_new_unit_lit(2, 3);
        switch (type->kind) {
        case T_INT:
            return ast_new_int_lit(0, 2, 3);
        case T_DOUBLE:
            return ast_new_double_lit(0.0, 2, 3);
        case T_STRING:
            return ast_new_string_lit("", 2, 3);
        default:
            return ast_new_unit_lit(2, 3);
        }
    }

    // def <name>(<params>): <ret> = <literal of ret>     ret == nullptr -> return type not written
    ast_node_t* add_func(const char* name, bool is_main, type_t* ret, const param_list_t& params = {})
    {
        ast_node_t* fn = ast_new_func_def(name, is_main, 1, 1);
        EXPECT_NE(fn, nullptr);
        for (const auto& param : params)
            EXPECT_TRUE(ast_func_def_add_param(fn, param.first, param.second));
        fn->func_def.return_type = ret;

        ast_node_t* body = ast_new_block(2, 3);
        EXPECT_NE(body, nullptr);
        EXPECT_TRUE(dyn_arr_append(&body->block.statements, ast_new_expr_stmt(literal_of(ret), 2, 3)));
        fn->func_def.body = body;

        EXPECT_TRUE(dyn_arr_append(&program->program.func_list, fn));
        return fn;
    }

    // @main def main(): Unit = ()
    void add_main() { add_func("main", true, TYPE_UNIT); }

    error_code_t analyze() { return semantic_analyze(program); }
};

// ----------------------------------------
//  Invalid input (99)
// ----------------------------------------

TEST(SemanticInputTest, NullProgramIsInternalError)
{
    EXPECT_EQ(semantic_analyze(nullptr), ERR_INTERNAL);
}

TEST(SemanticInputTest, NonProgramNodeIsInternalError)
{
    ast_node_t* block = ast_new_block(1, 1);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(semantic_analyze(block), ERR_INTERNAL);
    ast_free(block);
}

// ----------------------------------------
//  Valid declarations (0)
// ----------------------------------------

TEST_F(SemanticPass01Test, OnlyMainIsOk)
{
    add_main();
    EXPECT_EQ(analyze(), ERR_OK);
}

// @main def main() = ()  -> the return type is inferred in pass 2, pass 1 must accept it
TEST_F(SemanticPass01Test, MainWithoutWrittenReturnTypeIsOk)
{
    add_func("main", true, nullptr);
    EXPECT_EQ(analyze(), ERR_OK);
}

// @main does not have to be called "main"
TEST_F(SemanticPass01Test, MainWithDifferentNameIsOk)
{
    add_func("start", true, TYPE_UNIT);
    EXPECT_EQ(analyze(), ERR_OK);
}

TEST_F(SemanticPass01Test, MainAsFirstFunctionIsOk)
{
    add_main();
    add_func("f", false, TYPE_INT);
    add_func("g", false, nullptr);
    EXPECT_EQ(analyze(), ERR_OK);
}

TEST_F(SemanticPass01Test, MainInTheMiddleIsOk)
{
    add_func("f", false, TYPE_INT);
    add_main();
    add_func("g", false, nullptr);
    EXPECT_EQ(analyze(), ERR_OK);
}

TEST_F(SemanticPass01Test, MainAsLastFunctionIsOk)
{
    add_func("f", false, TYPE_INT);
    add_func("g", false, nullptr);
    add_main();
    EXPECT_EQ(analyze(), ERR_OK);
}

TEST_F(SemanticPass01Test, NonMainFunctionsWithParamsAndReturnTypesAreOk)
{
    add_func("f", false, TYPE_INT, { { "a", TYPE_INT }, { "b", TYPE_DOUBLE } });
    add_func("g", false, TYPE_STRING, { { "s", TYPE_STRING } });
    add_func("h", false, TYPE_UNIT, { { "x", TYPE_INT }, { "y", TYPE_INT }, { "z", TYPE_STRING } });
    add_func("k", false, nullptr, { { "d", TYPE_DOUBLE } });
    add_main();
    EXPECT_EQ(analyze(), ERR_OK);
}

// Param names are local to their function, two functions may both have "a"
TEST_F(SemanticPass01Test, SameParamNameInDifferentFunctionsIsOk)
{
    add_func("f", false, TYPE_INT, { { "a", TYPE_INT } });
    add_func("g", false, TYPE_INT, { { "a", TYPE_INT } });
    add_main();
    EXPECT_EQ(analyze(), ERR_OK);
}

// Names are case sensitive, "Print" is not the builtin "print"
TEST_F(SemanticPass01Test, BuiltinNameWithDifferentCaseIsOk)
{
    add_func("Print", false, TYPE_UNIT, { { "x", TYPE_INT } });
    add_main();
    EXPECT_EQ(analyze(), ERR_OK);
}

// semantic_analyze keeps no global state, a second program is analyzed independently
TEST_F(SemanticPass01Test, TwoIndependentRunsAreOk)
{
    add_main();
    EXPECT_EQ(analyze(), ERR_OK);

    ast_node_t* second = ast_new_program();
    ASSERT_NE(second, nullptr);
    ast_node_t* fn = ast_new_func_def("main", true, 1, 1);
    ASSERT_NE(fn, nullptr);
    fn->func_def.return_type = TYPE_UNIT;
    fn->func_def.body = ast_new_block(2, 3);
    ASSERT_TRUE(dyn_arr_append(&second->program.func_list, fn));

    EXPECT_EQ(semantic_analyze(second), ERR_OK);
    ast_free(second);
}

// ----------------------------------------
//  Missing @main (3)
// ----------------------------------------

TEST_F(SemanticPass01Test, NoMainIsUndefined)
{
    add_func("f", false, TYPE_UNIT);
    add_func("g", false, TYPE_INT, { { "a", TYPE_INT } });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// A function called "main" without the @main annotation is not the entry point
TEST_F(SemanticPass01Test, FunctionNamedMainWithoutAnnotationIsUndefined)
{
    add_func("main", false, TYPE_UNIT);
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

TEST_F(SemanticPass01Test, EmptyProgramIsUndefined)
{
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// ----------------------------------------
//  Redefinitions (4)
// ----------------------------------------

TEST_F(SemanticPass01Test, TwoMainFunctionsIsRedefinition)
{
    add_func("main", true, TYPE_UNIT);
    add_func("start", true, TYPE_UNIT);
    EXPECT_EQ(analyze(), ERR_SEM_REDEF);
}

TEST_F(SemanticPass01Test, DuplicateFunctionIsRedefinition)
{
    add_func("f", false, TYPE_INT);
    add_func("f", false, TYPE_INT);
    add_main();
    EXPECT_EQ(analyze(), ERR_SEM_REDEF);
}

// Same name, different signature - there is no overloading
TEST_F(SemanticPass01Test, DuplicateFunctionWithDifferentSignatureIsRedefinition)
{
    add_func("f", false, TYPE_INT, { { "a", TYPE_INT } });
    add_func("f", false, TYPE_STRING, { { "s", TYPE_STRING }, { "t", TYPE_STRING } });
    add_main();
    EXPECT_EQ(analyze(), ERR_SEM_REDEF);
}

// "main" (plain) and "@main main" share the name
TEST_F(SemanticPass01Test, PlainMainAndAnnotatedMainIsRedefinition)
{
    add_func("main", false, TYPE_UNIT);
    add_main();
    EXPECT_EQ(analyze(), ERR_SEM_REDEF);
}

TEST_F(SemanticPass01Test, DuplicateParamIsRedefinition)
{
    add_func("f", false, TYPE_INT, { { "a", TYPE_INT }, { "a", TYPE_INT } });
    add_main();
    EXPECT_EQ(analyze(), ERR_SEM_REDEF);
}

// Duplicate is detected even when the two params have different types / are not adjacent
TEST_F(SemanticPass01Test, NonAdjacentDuplicateParamIsRedefinition)
{
    add_func("f", false, TYPE_INT, { { "a", TYPE_INT }, { "b", TYPE_INT }, { "a", TYPE_STRING } });
    add_main();
    EXPECT_EQ(analyze(), ERR_SEM_REDEF);
}

// Every builtin is in the function table: a user function with its name is a redefinition
class SemanticBuiltinClashTest : public SemanticPass01Test,
                                 public ::testing::WithParamInterface<const char*> { };

TEST_P(SemanticBuiltinClashTest, UserFunctionWithBuiltinNameIsRedefinition)
{
    add_func(GetParam(), false, TYPE_UNIT, { { "x", TYPE_INT } });
    add_main();
    EXPECT_EQ(analyze(), ERR_SEM_REDEF);
}

INSTANTIATE_TEST_SUITE_P(AllBuiltins, SemanticBuiltinClashTest,
    ::testing::Values("print", "readLine", "readInt", "readDouble", "toStr",
        "toInt", "strLength", "subStr", "ord", "chr"),
    [](const ::testing::TestParamInfo<const char*>& info) { return std::string(info.param); });

// ----------------------------------------
//  @main with params (5)
// ----------------------------------------

TEST_F(SemanticPass01Test, MainWithOneParamIsCallError)
{
    add_func("main", true, TYPE_UNIT, { { "x", TYPE_INT } });
    EXPECT_EQ(analyze(), ERR_SEM_CALL);
}

TEST_F(SemanticPass01Test, MainWithTwoParamsIsCallError)
{
    add_func("main", true, TYPE_UNIT, { { "x", TYPE_INT }, { "s", TYPE_STRING } });
    EXPECT_EQ(analyze(), ERR_SEM_CALL);
}

// ----------------------------------------
//  @main with a written non-Unit return type (6)
// ----------------------------------------

class SemanticMainReturnTest : public SemanticPass01Test,
                               public ::testing::WithParamInterface<type_t*> { };

TEST_P(SemanticMainReturnTest, MainReturningNonUnitIsTypeError)
{
    add_func("main", true, GetParam());
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

INSTANTIATE_TEST_SUITE_P(NonUnitTypes, SemanticMainReturnTest,
    ::testing::Values(TYPE_INT, TYPE_DOUBLE, TYPE_STRING),
    [](const ::testing::TestParamInfo<type_t*>& info) {
        switch (info.param->kind) {
        case T_INT:
            return std::string("Int");
        case T_DOUBLE:
            return std::string("Double");
        case T_STRING:
            return std::string("String");
        default:
            return std::string("Other");
        }
    });

// ----------------------------------------
//  Allocation failures (99)
// ----------------------------------------

// Fail the 1st, 2nd, 3rd, ... calloc inside semantic_analyze, one run each.
// Every run must end with 99 (and not crash or leak - run under valgrind),
// until n is larger than the number of callocs and the run succeeds.
TEST_F(SemanticPass01Test, EveryAllocationFailureIsInternalError)
{
    add_func("f", false, TYPE_INT, { { "a", TYPE_INT }, { "b", TYPE_INT } });
    add_main();

    int failures = 0;
    for (int n = 0; n < 1000; n++) {
        calloc_countdown = n;
        error_code_t err = analyze();
        bool failure_was_triggered = (calloc_countdown == -1);
        calloc_countdown = -1;

        if (!failure_was_triggered) {
            EXPECT_EQ(err, ERR_OK) << "no allocation failed, the run must succeed";
            break;
        }
        EXPECT_EQ(err, ERR_INTERNAL) << "calloc number " << n << " failed";
        failures++;
    }
    EXPECT_GT(failures, 0) << "semantic_analyze never called calloc";
}
