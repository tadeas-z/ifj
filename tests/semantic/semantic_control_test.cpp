// Implementation of the IFJ26 imperative language compiler
// semantic_control_test.cpp
// Author(s): Adam Zatloukal (xzatloa01)
//
// AI-generated using: Claude Code
//              model: Claude Opus 5.5, Claude Fable 5.2
// Prompt: "Update the tests and write tests for the control. From the ones you suggested
//          up to more complex ones"
// Scope: the whole file
//
// Tests of the CONTROL extension (spec 5.3) in the semantic analyzer:
//   - for loops: bounds, step, guards, the immutable loop variable, scopes, ids and locals
//   - if used as an expression: else required, branch types (Int/Double conversion),
//     branches ending with return (skipped like in Scala), nested ifs and else-if chains
//   - rule 3: a function body ending with an if that has a value
//   - combinations with each other and with BOOLTHEN, FUNEXP and VALMEM
//
// Every test builds a program by hand with the builders from semantic_builders.hpp,
// the IFJ26 source it represents is written in the comment above it.

extern "C" {
#include "ast.h"
#include "dynamic_array.h"
#include "semantic.h"
}

#include <gtest/gtest.h>

#include "semantic_builders.hpp"

// ----------------------------------------
//  Fixture and helpers
// ----------------------------------------

class SemanticControlTest : public ::testing::Test {
protected:
    ast_node_t* program = nullptr;

    void SetUp() override
    {
        program = ast_new_program();
        ASSERT_NE(program, nullptr);
    }

    void TearDown() override { ast_free(program); }

    // def <name>(<params>): <ret_type> = { <body> }      ret_type == nullptr -> not written
    ast_node_t* func(const char* name, type_t* ret_type, const param_list_t& params, const node_list_t& body,
        bool is_main = false)
    {
        ast_node_t* fn = make_func(name, ret_type, params, body, is_main);
        EXPECT_TRUE(dyn_arr_append(&program->program.func_list, fn));
        return fn;
    }

    // @main def main(): Unit = { <body> }
    ast_node_t* main_(const node_list_t& body) { return func("main", TYPE_UNIT, {}, body, true); }

    error_code_t analyze() { return semantic_analyze(program); }
};

// Last statement of a block
static ast_node_t* last_of(ast_node_t* blk)
{
    dyn_arr_t* stmts = &blk->block.statements;
    return static_cast<ast_node_t*>(dyn_arr_get(stmts, stmts->count - 1));
}

// Expression of the last statement of a block (the block must end with an expression)
static ast_node_t* last_expr(ast_node_t* blk) { return last_of(blk)->expr_stmt.expr; }

static ast_node_t* local_at(ast_node_t* fn, size_t i)
{
    return static_cast<ast_node_t*>(dyn_arr_get(&fn->func_def.locals, i));
}

// print(<e>)
static ast_node_t* print_(ast_node_t* e) { return expr(call("print", { e })); }

static ast_node_t* lt_(ast_node_t* l, ast_node_t* r) { return bin(AST_OP_LT, l, r); }
static ast_node_t* le_(ast_node_t* l, ast_node_t* r) { return bin(AST_OP_LE, l, r); }
static ast_node_t* gt_(ast_node_t* l, ast_node_t* r) { return bin(AST_OP_GT, l, r); }
static ast_node_t* eq_(ast_node_t* l, ast_node_t* r) { return bin(AST_OP_EQ, l, r); }
static ast_node_t* ne_(ast_node_t* l, ast_node_t* r) { return bin(AST_OP_NE, l, r); }
static ast_node_t* add_(ast_node_t* l, ast_node_t* r) { return bin(AST_OP_ADD, l, r); }
static ast_node_t* sub_(ast_node_t* l, ast_node_t* r) { return bin(AST_OP_SUB, l, r); }
static ast_node_t* mul_(ast_node_t* l, ast_node_t* r) { return bin(AST_OP_MUL, l, r); }

// ========================================
//  for - basics
// ========================================

// for i <- 1 to 10 do print(i)
TEST_F(SemanticControlTest, ForBasicLoop)
{
    ast_node_t* arg = V("i");
    ast_node_t* loop = for_("i", I(1), I(10), nullptr, true, {}, { print_(arg) });
    ast_node_t* m = main_({ loop });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_GE(loop->for_stmt.var_id, 0);
    EXPECT_GE(loop->for_stmt.end_id, 0);
    EXPECT_EQ(loop->for_stmt.step_id, -1) << "no by -> no $step";
    EXPECT_NE(loop->for_stmt.var_id, loop->for_stmt.end_id);
    EXPECT_EQ(arg->var_ref.id, loop->for_stmt.var_id);
    EXPECT_EQ(type_name(loop->type), "Unit");
    ASSERT_EQ(m->func_def.locals.count, 1u);
    EXPECT_EQ(local_at(m, 0), loop) << "the for node is a local (codegen declares i, $end, $step)";
}

// for i <- 0 until 10 do print(i)
TEST_F(SemanticControlTest, ForUntilLoop)
{
    main_({ for_("i", I(0), I(10), nullptr, false, {}, { print_(V("i")) }) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// for i <- 10 to 1 by -2 do print(i)
TEST_F(SemanticControlTest, ForStepGetsOwnId)
{
    ast_node_t* loop = for_("i", I(10), I(1), neg(I(2)), true, {}, { print_(V("i")) });
    main_({ loop });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_GE(loop->for_stmt.step_id, 0);
    EXPECT_NE(loop->for_stmt.step_id, loop->for_stmt.var_id);
    EXPECT_NE(loop->for_stmt.step_id, loop->for_stmt.end_id);
}

// for i <- 1 to 3 do ()
TEST_F(SemanticControlTest, ForEmptyBody)
{
    main_({ for_("i", I(1), I(3), nullptr, true, {}, {}) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// ========================================
//  for - bounds and step
// ========================================

TEST_F(SemanticControlTest, ForFromDoubleIsTypeError)
{
    main_({ for_("i", D(1.5), I(10), nullptr, true, {}, {}) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

TEST_F(SemanticControlTest, ForToStringIsTypeError)
{
    main_({ for_("i", I(1), S("10"), nullptr, false, {}, {}) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

TEST_F(SemanticControlTest, ForToBooleanIsTypeError)
{
    main_({ for_("i", I(1), B(true), nullptr, true, {}, {}) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

TEST_F(SemanticControlTest, ForStepDoubleIsTypeError)
{
    main_({ for_("i", I(1), I(10), D(2.5), true, {}, {}) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// by 0.0 is a type error, not a zero step
TEST_F(SemanticControlTest, ForStepDoubleZeroIsTypeError)
{
    main_({ for_("i", I(1), I(10), D(0.0), true, {}, {}) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

TEST_F(SemanticControlTest, ForStepBooleanIsTypeError)
{
    main_({ for_("i", I(1), I(10), B(true), true, {}, {}) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// for i <- 1 to 10 by 0   (error code to be confirmed on the forum)
TEST_F(SemanticControlTest, ForLiteralZeroStepIsError)
{
    main_({ for_("i", I(1), I(10), I(0), true, {}, {}) });
    EXPECT_EQ(analyze(), ERR_SEM_OTHER);
}

// for i <- 1 to 10 by -0
TEST_F(SemanticControlTest, ForNegativeZeroStepIsError)
{
    main_({ for_("i", I(1), I(10), neg(I(0)), true, {}, {}) });
    EXPECT_EQ(analyze(), ERR_SEM_OTHER);
}

// var s = 0; for i <- 1 to 10 by s do ()   -> checked at runtime (EXIT 40), not by the compiler
TEST_F(SemanticControlTest, ForZeroStepInVariableIsOk)
{
    main_({ var("s", I(0)), for_("i", I(1), I(10), V("s"), true, {}, {}) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// for i <- 1 to 10 by 1 - 1   -> not a literal, checked at runtime
TEST_F(SemanticControlTest, ForZeroStepExpressionIsOk)
{
    main_({ for_("i", I(1), I(10), sub_(I(1), I(1)), true, {}, {}) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// for i <- "a" to 10 by 0   -> the type error of the bound is found first
TEST_F(SemanticControlTest, ForBoundTypeErrorBeforeZeroStep)
{
    main_({ for_("i", S("a"), I(10), I(0), true, {}, {}) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// for i <- 1 to n do ()   -> n is not defined
TEST_F(SemanticControlTest, ForUndefinedBoundIsUndefined)
{
    main_({ for_("i", I(1), V("n"), nullptr, true, {}, {}) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// ========================================
//  for - guards
// ========================================

// for i <- 0 until 10 if i != 3 if i < 8 do print(i)
TEST_F(SemanticControlTest, ForGuardsSeeLoopVariable)
{
    ast_node_t* guard_ref = V("i");
    ast_node_t* loop = for_("i", I(0), I(10), nullptr, false, { ne_(guard_ref, I(3)), lt_(V("i"), I(8)) },
        { print_(V("i")) });
    main_({ loop });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(guard_ref->var_ref.id, loop->for_stmt.var_id);
}

// for i <- 0 until 10 if i do ()
TEST_F(SemanticControlTest, ForIntGuardIsTypeError)
{
    main_({ for_("i", I(0), I(10), nullptr, false, { V("i") }, {}) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// val flag = true; for i <- 0 until 10 if flag if !(i == 2) do ()   -> Boolean variable and ! (BOOLTHEN)
TEST_F(SemanticControlTest, ForBooleanVariableGuard)
{
    main_({ val("flag", B(true)), for_("i", I(0), I(10), nullptr, false, { V("flag"), not_(eq_(V("i"), I(2))) }, {}) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// for i <- 1 to 3 if t > 0 do { val t = 1 }   -> the guard cannot see variables of the body
TEST_F(SemanticControlTest, ForGuardCannotSeeBodyVariable)
{
    main_({ for_("i", I(1), I(3), nullptr, true, { gt_(V("t"), I(0)) }, { val("t", I(1)) }) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// ========================================
//  for - loop variable and scopes
// ========================================

// for i <- 1 to 3 do i = 5
TEST_F(SemanticControlTest, ForAssignToLoopVariableIsAssignError)
{
    main_({ for_("i", I(1), I(3), nullptr, true, {}, { assign("i", I(5)) }) });
    EXPECT_EQ(analyze(), ERR_SEM_ASSIGN);
}

// for i <- 1 to 3 do { var i = 0; i = 5 }   -> the body is its own scope
TEST_F(SemanticControlTest, ForBodyCanShadowLoopVariable)
{
    main_({ for_("i", I(1), I(3), nullptr, true, {}, { var("i", I(0)), assign("i", I(5)) }) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// val i = 5; for i <- 1 to i do print(i)   -> the bound uses the outer i, the body the loop i
TEST_F(SemanticControlTest, ForBoundUsesOuterVariable)
{
    ast_node_t* outer = val("i", I(5));
    ast_node_t* bound = V("i");
    ast_node_t* body_ref = V("i");
    ast_node_t* loop = for_("i", I(1), bound, nullptr, true, {}, { print_(body_ref) });
    main_({ outer, loop });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(bound->var_ref.id, outer->var_def.id);
    EXPECT_EQ(body_ref->var_ref.id, loop->for_stmt.var_id);
    EXPECT_NE(outer->var_def.id, loop->for_stmt.var_id);
}

// var i = 0; for i <- 1 to 3 do (); i = 7   -> after the loop, i is the outer var again
TEST_F(SemanticControlTest, ForOuterVariableBackAfterLoop)
{
    ast_node_t* outer = var("i", I(0));
    ast_node_t* after = assign("i", I(7));
    main_({ outer, for_("i", I(1), I(3), nullptr, true, {}, {}), after });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(after->assign.id, outer->var_def.id);
}

// for i <- 1 to 3 do (); print(i)
TEST_F(SemanticControlTest, ForLoopVariableGoneAfterLoop)
{
    main_({ for_("i", I(1), I(3), nullptr, true, {}, {}), print_(V("i")) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// for i <- 1 to 3 do { for j <- 1 to i by 1 do print(j) }; for i <- 1 to 3 by 1 do ()
TEST_F(SemanticControlTest, ForSequentialAndNestedLoopsGetOwnIds)
{
    ast_node_t* inner = for_("j", I(1), V("i"), I(1), true, {}, { print_(V("j")) });
    ast_node_t* a = for_("i", I(1), I(3), nullptr, true, {}, { inner });
    ast_node_t* b = for_("i", I(1), I(3), I(1), true, {}, {});
    ast_node_t* m = main_({ a, b });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_NE(a->for_stmt.var_id, b->for_stmt.var_id);
    EXPECT_NE(a->for_stmt.end_id, inner->for_stmt.end_id);
    EXPECT_NE(inner->for_stmt.step_id, b->for_stmt.step_id);
    EXPECT_EQ(m->func_def.locals.count, 3u);
}

// val step = 2; val end = 9; for i <- 0 to end by step do print(step)   -> no clash with $end and $step
TEST_F(SemanticControlTest, ForUserNamesDontClashWithHiddenVariables)
{
    ast_node_t* user_step = val("step", I(2));
    ast_node_t* user_end = val("end", I(9));
    ast_node_t* step_ref = V("step");
    ast_node_t* end_ref = V("end");
    main_({ user_step, user_end,
        for_("i", I(0), V("end"), V("step"), true, {}, { print_(step_ref), print_(end_ref) }) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(step_ref->var_ref.id, user_step->var_def.id);
    EXPECT_EQ(end_ref->var_ref.id, user_end->var_def.id);
}

// ========================================
//  for - in functions
// ========================================

// def f() = { for i <- 1 to 3 do () }   -> inferred Unit
TEST_F(SemanticControlTest, ForAsLastStatementIsUnit)
{
    ast_node_t* f = func("f", nullptr, {}, { for_("i", I(1), I(3), nullptr, true, {}, {}) });
    main_({ expr(call("f")) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(f->func_def.return_type), "Unit");
}

// def f(): Int = { for i <- 1 to 3 do return i; 0 }
TEST_F(SemanticControlTest, ForReturnInsideLoop)
{
    func("f", TYPE_INT, {}, { for_("i", I(1), I(3), nullptr, true, {}, { ret(V("i")) }), expr(I(0)) });
    main_({ expr(call("f")) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// def f() = { for i <- 1 to 3 do return i; 0 }   -> return needs a written return type
TEST_F(SemanticControlTest, ForReturnInsideLoopOfInferredFunctionIsTypeError)
{
    func("f", nullptr, {}, { for_("i", I(1), I(3), nullptr, true, {}, { ret(V("i")) }), expr(I(0)) });
    main_({ expr(call("f")) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(n: Int) = { for i <- 0 until n do print(i) }   -> a parameter as the bound
TEST_F(SemanticControlTest, ForParameterAsBound)
{
    ast_node_t* bound = V("n");
    ast_node_t* f = func("f", nullptr, { { "n", TYPE_INT } }, { for_("i", I(0), bound, nullptr, false, {}, { print_(V("i")) }) });
    main_({ expr(call("f", { I(4) })) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(bound->var_ref.id, static_cast<ast_param_t*>(dyn_arr_get(&f->func_def.params, 0))->id);
}

// ========================================
//  if as an expression
// ========================================

// val a = if c then 1 else 2
TEST_F(SemanticControlTest, IfExprIntInt)
{
    ast_node_t* e = if_else(cond(), { expr(I(1)) }, { expr(I(2)) });
    ast_node_t* a = val("a", e);
    main_({ a });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Int");
    EXPECT_EQ(type_name(a->var_def.init->type), "Int");
}

// val a = if c then 1 else 2.5   -> Double, the Int branch gets a CONVERT
TEST_F(SemanticControlTest, IfExprIntDoubleConvertsIntBranch)
{
    ast_node_t* e = if_else(cond(), { expr(I(1)) }, { expr(D(2.5)) });
    main_({ val("a", e) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Double");
    EXPECT_TRUE(is_convert_of(last_expr(e->if_stmt.then_block), AST_INT_LIT));
    EXPECT_EQ(last_expr(e->if_stmt.else_block)->kind, AST_DOUBLE_LIT);
}

// val a = if c then 1.5 else 2   -> the conversion also works in the else branch
TEST_F(SemanticControlTest, IfExprDoubleIntConvertsElseBranch)
{
    ast_node_t* e = if_else(cond(), { expr(D(1.5)) }, { expr(I(2)) });
    main_({ val("a", e) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Double");
    EXPECT_TRUE(is_convert_of(last_expr(e->if_stmt.else_block), AST_INT_LIT));
}

// val s = if c then "a" else "b"
TEST_F(SemanticControlTest, IfExprStrings)
{
    ast_node_t* e = if_else(cond(), { expr(S("a")) }, { expr(S("b")) });
    main_({ val("s", e) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(e->type), "String");
}

// val b = if c then true else false; if b then print(1)   (BOOLTHEN)
TEST_F(SemanticControlTest, IfExprBooleans)
{
    ast_node_t* e = if_else(cond(), { expr(B(true)) }, { expr(B(false)) });
    main_({ val("b", e), if_(V("b"), { print_(I(1)) }) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Boolean");
}

// val u = if c then print(1) else print(2)   -> Unit
TEST_F(SemanticControlTest, IfExprUnitUnit)
{
    ast_node_t* e = if_else(cond(), { print_(I(1)) }, { print_(I(2)) });
    main_({ val("u", e) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Unit");
}

TEST_F(SemanticControlTest, IfExprStringIntIsTypeError)
{
    main_({ val("a", if_else(cond(), { expr(S("x")) }, { expr(I(2)) })) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

TEST_F(SemanticControlTest, IfExprBooleanIntIsTypeError)
{
    main_({ val("a", if_else(cond(), { expr(B(true)) }, { expr(I(2)) })) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// val a = if c then 1   -> an if-expression needs else
TEST_F(SemanticControlTest, IfExprWithoutElseIsTypeError)
{
    main_({ val("a", if_(cond(), { expr(I(1)) })) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// val a = if 1 then 1 else 2
TEST_F(SemanticControlTest, IfExprIntConditionIsTypeError)
{
    main_({ val("a", if_else(I(1), { expr(I(1)) }, { expr(I(2)) })) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// val a = if c then { val t = 1 } else 2   -> Unit and Int
TEST_F(SemanticControlTest, IfExprUnitBranchAndIntIsTypeError)
{
    main_({ val("a", if_else(cond(), { val("t", I(1)) }, { expr(I(2)) })) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// val a = if c then { val t = 1; t } else 2   -> a branch can have its own variables
TEST_F(SemanticControlTest, IfExprBranchWithLocal)
{
    ast_node_t* e = if_else(cond(), { val("t", I(1)), expr(V("t")) }, { expr(I(2)) });
    main_({ val("a", e) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Int");
}

// val a = if c then { val t = 1; t } else 2; print(t)   -> t lives only in its branch
TEST_F(SemanticControlTest, IfExprBranchVariableNotVisibleAfter)
{
    main_({ val("a", if_else(cond(), { val("t", I(1)), expr(V("t")) }, { expr(I(2)) })), print_(V("t")) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// val a = if c then y else 2   -> y is not defined
TEST_F(SemanticControlTest, IfExprUndefinedInBranch)
{
    main_({ val("a", if_else(cond(), { expr(V("y")) }, { expr(I(2)) })) });
    EXPECT_EQ(analyze(), ERR_SEM_UNDEF);
}

// ========================================
//  if as an expression - positions
// ========================================

// val a: Double = if c then 1 else 2   -> the whole if is converted by the var definition
TEST_F(SemanticControlTest, IfExprIntoDeclaredDouble)
{
    ast_node_t* a = val("a", if_else(cond(), { expr(I(1)) }, { expr(I(2)) }), TYPE_DOUBLE);
    main_({ a });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_TRUE(is_convert_of(a->var_def.init, AST_IF));
}

// val a: Int = if c then 1 else 2.5   -> Double into Int
TEST_F(SemanticControlTest, IfExprDoubleIntoDeclaredIntIsTypeError)
{
    main_({ val("a", if_else(cond(), { expr(I(1)) }, { expr(D(2.5)) }), TYPE_INT) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// var x = 0.0; x = if c then 1 else 2
TEST_F(SemanticControlTest, IfExprAsAssignedValue)
{
    ast_node_t* asg = assign("x", if_else(cond(), { expr(I(1)) }, { expr(I(2)) }));
    main_({ var("x", D(0.0)), asg });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_TRUE(is_convert_of(asg->assign.value, AST_IF));
}

// var x = 0; x = if c then "a" else "b"
TEST_F(SemanticControlTest, IfExprAssignedToWrongTypeIsTypeError)
{
    main_({ var("x", I(0)), assign("x", if_else(cond(), { expr(S("a")) }, { expr(S("b")) })) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Double = { return if c then 1 else 2 }
TEST_F(SemanticControlTest, IfExprAsReturnValue)
{
    ast_node_t* r = ret(if_else(cond(), { expr(I(1)) }, { expr(I(2)) }));
    func("f", TYPE_DOUBLE, {}, { r });
    main_({ expr(call("f")) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_TRUE(is_convert_of(r->return_stmt.value, AST_IF));
}

// print(if c then 1 else "a")   -> 6 even though print takes any value
TEST_F(SemanticControlTest, IfExprMismatchAsPrintArgumentIsTypeError)
{
    main_({ print_(if_else(cond(), { expr(I(1)) }, { expr(S("a")) })) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def h(d: Double): Double = d;  h(if c then 1 else 2)   -> the argument is converted (FUNEXP)
TEST_F(SemanticControlTest, IfExprAsUserCallArgument)
{
    func("h", TYPE_DOUBLE, { { "d", TYPE_DOUBLE } }, { expr(V("d")) });
    ast_node_t* c = call("h", { if_else(cond(), { expr(I(1)) }, { expr(I(2)) }) });
    main_({ expr(c) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_TRUE(is_convert_of(arg_at(c, 0), AST_IF));
}

// def h(n: Int): Int = n;  h(if c then 1 else 2.5)   -> Double argument for an Int parameter
TEST_F(SemanticControlTest, IfExprDoubleAsIntArgumentIsCallError)
{
    func("h", TYPE_INT, { { "n", TYPE_INT } }, { expr(V("n")) });
    main_({ expr(call("h", { if_else(cond(), { expr(I(1)) }, { expr(D(2.5)) }) })) });
    EXPECT_EQ(analyze(), ERR_SEM_CALL);
}

// val a = 1 + (if c then 2 else 3)
TEST_F(SemanticControlTest, IfExprAsBinaryOperand)
{
    ast_node_t* b = add_(I(1), if_else(cond(), { expr(I(2)) }, { expr(I(3)) }));
    main_({ val("a", b) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(b->type), "Int");
}

// val s = "x" + (if c then "a" else "b")   -> string concatenation
TEST_F(SemanticControlTest, IfExprInStringConcatenation)
{
    ast_node_t* b = add_(S("x"), if_else(cond(), { expr(S("a")) }, { expr(S("b")) }));
    main_({ val("s", b) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(b->type), "String");
}

// val ok = (if c then 1 else 2.5) < 3
TEST_F(SemanticControlTest, IfExprInComparison)
{
    ast_node_t* b = lt_(if_else(cond(), { expr(I(1)) }, { expr(D(2.5)) }), I(3));
    main_({ val("ok", b) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(b->type), "Boolean");
}

// val n = (if c then 1.5 else 2).toInt   (VALMEM)
TEST_F(SemanticControlTest, IfExprAsMemberReceiver)
{
    ast_node_t* m = M(if_else(cond(), { expr(D(1.5)) }, { expr(I(2)) }), "toInt");
    main_({ val("n", m) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(m->type), "Int");
}

// val n = (if c then "ab" else "c").length   (VALMEM)
TEST_F(SemanticControlTest, IfExprStringMember)
{
    ast_node_t* m = M(if_else(cond(), { expr(S("ab")) }, { expr(S("c")) }), "length");
    main_({ val("n", m) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(m->type), "Int");
}

// if c then 1 else "x"; print(1)   -> a statement if (not last) may have any branches
TEST_F(SemanticControlTest, StatementIfWithMismatchedBranchesIsOk)
{
    main_({ if_else(cond(), { expr(I(1)) }, { expr(S("x")) }), print_(I(1)) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// if c then print(1); print(2)   -> a statement if still does not need else (BOOLTHEN)
TEST_F(SemanticControlTest, StatementIfWithoutElseIsOk)
{
    main_({ if_(cond(), { print_(I(1)) }), print_(I(2)) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// ========================================
//  if as an expression - nesting and else if
// ========================================

// val a = if c then (if d then 1 else 2) else 2.5   -> the inner if gets the CONVERTs
TEST_F(SemanticControlTest, IfExprNestedIfInBranch)
{
    ast_node_t* inner = if_else(cond(), { expr(I(1)) }, { expr(I(2)) });
    ast_node_t* e = if_else(cond(), { inner }, { expr(D(2.5)) });
    main_({ val("a", e) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Double");
    EXPECT_EQ(type_name(inner->type), "Double");
    EXPECT_TRUE(is_convert_of(last_expr(inner->if_stmt.then_block), AST_INT_LIT));
    EXPECT_TRUE(is_convert_of(last_expr(inner->if_stmt.else_block), AST_INT_LIT));
}

// val a = if c then (if d then 1 else "x") else 2   -> the inner mismatch is reported
TEST_F(SemanticControlTest, IfExprNestedMismatchIsTypeError)
{
    ast_node_t* inner = if_else(cond(), { expr(I(1)) }, { expr(S("x")) });
    main_({ val("a", if_else(cond(), { inner }, { expr(I(2)) })) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// val a = if c then (if d then 1) else 2   -> the inner if has no value
TEST_F(SemanticControlTest, IfExprNestedWithoutElseIsTypeError)
{
    ast_node_t* inner = if_(cond(), { expr(I(1)) });
    main_({ val("a", if_else(cond(), { inner }, { expr(I(2)) })) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// var x = 3; val s = if x < 0 then "neg" else if x == 0 then "zero" else "pos"
TEST_F(SemanticControlTest, IfExprElseIfChain)
{
    ast_node_t* e = if_else(lt_(V("x"), I(0)), { expr(S("neg")) },
        { if_else(eq_(V("x"), I(0)), { expr(S("zero")) }, { expr(S("pos")) }) });
    main_({ var("x", I(3)), val("s", e) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(e->type), "String");
}

// val a = if c then 1 else if d then 2.5 else 3   -> Double, both Ints converted
TEST_F(SemanticControlTest, IfExprElseIfChainNumeric)
{
    ast_node_t* inner = if_else(cond(), { expr(D(2.5)) }, { expr(I(3)) });
    ast_node_t* e = if_else(cond(), { expr(I(1)) }, { inner });
    main_({ val("a", e) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Double");
    EXPECT_TRUE(is_convert_of(last_expr(e->if_stmt.then_block), AST_INT_LIT));
    EXPECT_TRUE(is_convert_of(last_expr(inner->if_stmt.else_block), AST_INT_LIT));
}

// val a = if c then 1 else if d then 2   -> the chain has no final else
TEST_F(SemanticControlTest, IfExprElseIfChainWithoutFinalElseIsTypeError)
{
    main_({ val("a", if_else(cond(), { expr(I(1)) }, { if_(cond(), { expr(I(2)) }) })) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// ========================================
//  if as an expression - branches ending with return (like Scala)
// ========================================

// def f(): Int = { val x = if c then return 0 else 5; x }   -> x is Int
TEST_F(SemanticControlTest, IfExprReturnBranchIsSkipped)
{
    ast_node_t* e = if_else(cond(), { ret(I(0)) }, { expr(I(5)) });
    func("f", TYPE_INT, {}, { val("x", e), expr(V("x")) });
    main_({ expr(call("f")) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Int");
}

// def f(): Double = { val x = if c then 1.5 else return 0; x }   -> x is Double
TEST_F(SemanticControlTest, IfExprReturnInElseBranchIsSkipped)
{
    ast_node_t* e = if_else(cond(), { expr(D(1.5)) }, { ret(I(0)) });
    func("f", TYPE_DOUBLE, {}, { val("x", e), expr(V("x")) });
    main_({ expr(call("f")) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Double");
}

// def g(): String = { val n = if c then return "no" else 5; toStr(n) }
//   -> "no" is checked against g, the if is Int
TEST_F(SemanticControlTest, IfExprReturnValueBelongsToFunction)
{
    ast_node_t* e = if_else(cond(), { ret(S("no")) }, { expr(I(5)) });
    func("g", TYPE_STRING, {}, { val("n", e), expr(call("toStr", { V("n") })) });
    main_({ expr(call("g")) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Int");
}

// def g(): Int = { val n = if c then return "no" else 5; n }   -> the returned String does not fit g
TEST_F(SemanticControlTest, IfExprReturnOfWrongTypeIsTypeError)
{
    func("g", TYPE_INT, {}, { val("n", if_else(cond(), { ret(S("no")) }, { expr(I(5)) })), expr(V("n")) });
    main_({ expr(call("g")) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Int = { val x = if c then return 1 else return 2; x }   -> the if never has a value
TEST_F(SemanticControlTest, IfExprBothBranchesReturnIsTypeError)
{
    func("f", TYPE_INT, {}, { val("x", if_else(cond(), { ret(I(1)) }, { ret(I(2)) })), expr(V("x")) });
    main_({ expr(call("f")) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Int = { val x = if c then (if d then return 0 else 1) else 2.5; 0 }   -> Double, CONVERT on 1
TEST_F(SemanticControlTest, IfExprNestedPartialReturn)
{
    ast_node_t* inner = if_else(cond(), { ret(I(0)) }, { expr(I(1)) });
    ast_node_t* e = if_else(cond(), { inner }, { expr(D(2.5)) });
    func("f", TYPE_INT, {}, { val("x", e), expr(I(0)) });
    main_({ expr(call("f")) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Double");
    EXPECT_TRUE(is_convert_of(last_expr(inner->if_stmt.else_block), AST_INT_LIT));
}

// def f() = { val x = if c then return 0 else 5; x }   -> return needs a written return type
TEST_F(SemanticControlTest, IfExprReturnInInferredFunctionIsTypeError)
{
    func("f", nullptr, {}, { val("x", if_else(cond(), { ret(I(0)) }, { expr(I(5)) })), expr(V("x")) });
    main_({ expr(call("f")) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// ========================================
//  Rule 3 - a body ending with an if that has a value
// ========================================

// def abs(x: Int): Int = if x < 0 then 0 - x else x
TEST_F(SemanticControlTest, Rule3AbsWithExplicitInt)
{
    ast_node_t* fn = func("abs", TYPE_INT, { { "x", TYPE_INT } },
        { if_else(lt_(V("x"), I(0)), { expr(sub_(I(0), V("x"))) }, { expr(V("x")) }) });
    main_({ expr(call("abs", { neg(I(5)) })) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(fn->func_def.return_type), "Int");
}

// def f() = if c then 1 else 2   -> inferred Int
TEST_F(SemanticControlTest, Rule3InferredInt)
{
    ast_node_t* fn = func("f", nullptr, {}, { if_else(cond(), { expr(I(1)) }, { expr(I(2)) }) });
    main_({ expr(call("f")) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(fn->func_def.return_type), "Int");
}

// def f() = if c then 1 else 2.5   -> inferred Double, CONVERT on 1
TEST_F(SemanticControlTest, Rule3InferredDouble)
{
    ast_node_t* the_if = if_else(cond(), { expr(I(1)) }, { expr(D(2.5)) });
    ast_node_t* fn = func("f", nullptr, {}, { the_if });
    main_({ expr(call("f")) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(fn->func_def.return_type), "Double");
    EXPECT_TRUE(is_convert_of(last_expr(the_if->if_stmt.then_block), AST_INT_LIT));
}

// def f(): Double = if c then 1 else 2   -> both branches converted to the written type
TEST_F(SemanticControlTest, Rule3ExplicitDoubleConvertsBothBranches)
{
    ast_node_t* the_if = if_else(cond(), { expr(I(1)) }, { expr(I(2)) });
    func("f", TYPE_DOUBLE, {}, { the_if });
    main_({ expr(call("f")) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_TRUE(is_convert_of(last_expr(the_if->if_stmt.then_block), AST_INT_LIT));
    EXPECT_TRUE(is_convert_of(last_expr(the_if->if_stmt.else_block), AST_INT_LIT));
}

// def f(): Int = if c then 1 else 2.5
TEST_F(SemanticControlTest, Rule3ExplicitIntWithDoubleBranchIsTypeError)
{
    func("f", TYPE_INT, {}, { if_else(cond(), { expr(I(1)) }, { expr(D(2.5)) }) });
    main_({ expr(call("f")) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Int = if c then 1 else "x"
TEST_F(SemanticControlTest, Rule3ExplicitIntWithMismatchIsTypeError)
{
    func("f", TYPE_INT, {}, { if_else(cond(), { expr(I(1)) }, { expr(S("x")) }) });
    main_({ expr(call("f")) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f() = if c then 1 else "x"   -> no common type and not Unit
TEST_F(SemanticControlTest, Rule3InferredMismatchIsTypeError)
{
    func("f", nullptr, {}, { if_else(cond(), { expr(I(1)) }, { expr(S("x")) }) });
    main_({ expr(call("f")) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f(): Int = if c then return 1 else 5
TEST_F(SemanticControlTest, Rule3ReturnAndValueBranch)
{
    func("f", TYPE_INT, {}, { if_else(cond(), { ret(I(1)) }, { expr(I(5)) }) });
    main_({ expr(call("f")) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// def f(): Int = if c then return 1 else return 2   -> still the "every path returns" case
TEST_F(SemanticControlTest, Rule3EveryBranchReturns)
{
    func("f", TYPE_INT, {}, { if_else(cond(), { ret(I(1)) }, { ret(I(2)) }) });
    main_({ expr(call("f")) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// def f(): Unit = if c then 1 else "x"   -> the value is discarded
TEST_F(SemanticControlTest, Rule3ExplicitUnitAcceptsAnything)
{
    func("f", TYPE_UNIT, {}, { if_else(cond(), { expr(I(1)) }, { expr(S("x")) }) });
    main_({ expr(call("f")) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// def f() = if c then print(1)   -> no else, the missing branch is Unit
TEST_F(SemanticControlTest, Rule3InferredWithoutElseIsUnit)
{
    ast_node_t* fn = func("f", nullptr, {}, { if_(cond(), { print_(I(1)) }) });
    main_({ expr(call("f")) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(fn->func_def.return_type), "Unit");
}

// def f(): Int = if c then 1   -> no else in an Int function
TEST_F(SemanticControlTest, Rule3ExplicitIntWithoutElseIsTypeError)
{
    func("f", TYPE_INT, {}, { if_(cond(), { expr(I(1)) }) });
    main_({ expr(call("f")) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def f() = if c then (if d then 1 else 2.5) else 3   -> nested, inferred Double
TEST_F(SemanticControlTest, Rule3NestedInferred)
{
    ast_node_t* inner = if_else(cond(), { expr(I(1)) }, { expr(D(2.5)) });
    ast_node_t* the_if = if_else(cond(), { inner }, { expr(I(3)) });
    ast_node_t* fn = func("f", nullptr, {}, { the_if });
    main_({ expr(call("f")) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(fn->func_def.return_type), "Double");
    EXPECT_TRUE(is_convert_of(last_expr(inner->if_stmt.then_block), AST_INT_LIT));
    EXPECT_TRUE(is_convert_of(last_expr(the_if->if_stmt.else_block), AST_INT_LIT));
}

// def sign(x: Int) = if x < 0 then "neg" else if x == 0 then "zero" else "pos"   -> inferred String
TEST_F(SemanticControlTest, Rule3ElseIfChainInferred)
{
    ast_node_t* fn = func("sign", nullptr, { { "x", TYPE_INT } },
        { if_else(lt_(V("x"), I(0)), { expr(S("neg")) },
            { if_else(eq_(V("x"), I(0)), { expr(S("zero")) }, { expr(S("pos")) }) }) });
    main_({ print_(call("sign", { I(4) })) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(fn->func_def.return_type), "String");
}

// def f(a: Boolean, b: Boolean): Double = if a then 1 else if b then return 2.5 else 3
TEST_F(SemanticControlTest, Rule3ChainWithReturnInExplicitDouble)
{
    ast_node_t* inner = if_else(V("b"), { ret(D(2.5)) }, { expr(I(3)) });
    ast_node_t* the_if = if_else(V("a"), { expr(I(1)) }, { inner });
    func("f", TYPE_DOUBLE, { { "a", TYPE_BOOL }, { "b", TYPE_BOOL } }, { the_if });
    main_({ print_(call("f", { B(true), B(false) })) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_TRUE(is_convert_of(last_expr(the_if->if_stmt.then_block), AST_INT_LIT));
    EXPECT_TRUE(is_convert_of(last_expr(inner->if_stmt.else_block), AST_INT_LIT));
}

// def f(a: Boolean, b: Boolean): Int = if a then 1 else if b then return 2.5 else 3   -> 2.5 does not fit Int
TEST_F(SemanticControlTest, Rule3ChainWithWrongReturnIsTypeError)
{
    func("f", TYPE_INT, { { "a", TYPE_BOOL }, { "b", TYPE_BOOL } },
        { if_else(V("a"), { expr(I(1)) }, { if_else(V("b"), { ret(D(2.5)) }, { expr(I(3)) }) }) });
    main_({});
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// @main def main() = if c then 1 else 2   -> main is inferred as Int
TEST_F(SemanticControlTest, Rule3MainEndingWithIntIfIsTypeError)
{
    func("main", nullptr, {}, { if_else(cond(), { expr(I(1)) }, { expr(I(2)) }) }, true);
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// @main def main(): Unit = if c then 1 else "x"
TEST_F(SemanticControlTest, Rule3MainExplicitUnitEndingWithIfIsOk)
{
    main_({ if_else(cond(), { expr(I(1)) }, { expr(S("x")) }) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// @main def main(): Unit = { val r: Int = g() };  def g() = if c then 1 else 2.5
//   -> g is checked lazily from main, its inferred Double does not fit r
TEST_F(SemanticControlTest, Rule3InferredTypeSeenByEarlierCaller)
{
    main_({ val("r", call("g"), TYPE_INT) });
    func("g", nullptr, {}, { if_else(cond(), { expr(I(1)) }, { expr(D(2.5)) }) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// @main def main(): Unit = { val r: Double = g() };  def g() = if c then 1 else 2
TEST_F(SemanticControlTest, Rule3InferredIntConvertedAtCaller)
{
    ast_node_t* r = val("r", call("g"), TYPE_DOUBLE);
    main_({ r });
    func("g", nullptr, {}, { if_else(cond(), { expr(I(1)) }, { expr(I(2)) }) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_TRUE(is_convert_of(r->var_def.init, AST_CALL));
}

// def fib(n: Int): Int = if n < 2 then n else fib(n - 1) + fib(n - 2)
TEST_F(SemanticControlTest, Rule3RecursiveFibonacci)
{
    func("fib", TYPE_INT, { { "n", TYPE_INT } },
        { if_else(lt_(V("n"), I(2)), { expr(V("n")) },
            { expr(add_(call("fib", { sub_(V("n"), I(1)) }), call("fib", { sub_(V("n"), I(2)) }))) }) });
    main_({ print_(call("fib", { I(10) })) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// def fib(n: Int) = if n < 2 then n else fib(n - 1) + fib(n - 2)   -> recursive needs a written type
TEST_F(SemanticControlTest, Rule3RecursiveWithoutTypeIsTypeError)
{
    func("fib", nullptr, { { "n", TYPE_INT } },
        { if_else(lt_(V("n"), I(2)), { expr(V("n")) },
            { expr(add_(call("fib", { sub_(V("n"), I(1)) }), call("fib", { sub_(V("n"), I(2)) }))) }) });
    main_({ print_(call("fib", { I(10) })) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// ========================================
//  Combinations
// ========================================

// val s = if c then { for i <- 1 to 3 do print(i); 1 } else 2   -> a loop inside an if-expression
TEST_F(SemanticControlTest, ForInsideIfExprBranch)
{
    ast_node_t* loop = for_("i", I(1), I(3), nullptr, true, {}, { print_(V("i")) });
    ast_node_t* e = if_else(cond(), { loop, expr(I(1)) }, { expr(I(2)) });
    ast_node_t* s = val("s", e);
    ast_node_t* m = main_({ s });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Int");
    ASSERT_EQ(m->func_def.locals.count, 2u);
    EXPECT_EQ(local_at(m, 0), loop) << "the init is checked before s is defined";
    EXPECT_EQ(local_at(m, 1), s);
}

// val u = if c then { for i <- 1 to 3 do () } else print(1)   -> both branches Unit
TEST_F(SemanticControlTest, IfExprBranchEndingWithForIsUnit)
{
    ast_node_t* e = if_else(cond(), { for_("i", I(1), I(3), nullptr, true, {}, {}) }, { print_(I(1)) });
    main_({ val("u", e) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Unit");
}

// val a = if c then { for i <- 1 to 3 do () } else 1   -> Unit and Int
TEST_F(SemanticControlTest, IfExprBranchEndingWithForAndIntIsTypeError)
{
    main_({ val("a", if_else(cond(), { for_("i", I(1), I(3), nullptr, true, {}, {}) }, { expr(I(1)) })) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// for i <- 0 until 10 if (if i < 5 then true else i == 7) do print(i)   -> an if-expression as a guard
TEST_F(SemanticControlTest, IfExprAsGuard)
{
    ast_node_t* guard = if_else(lt_(V("i"), I(5)), { expr(B(true)) }, { expr(eq_(V("i"), I(7))) });
    main_({ for_("i", I(0), I(10), nullptr, false, { guard }, { print_(V("i")) }) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(guard->type), "Boolean");
}

// for i <- 0 until (if c then 5 else 10) do print(i)
TEST_F(SemanticControlTest, IfExprAsForBound)
{
    main_({ for_("i", I(0), if_else(cond(), { expr(I(5)) }, { expr(I(10)) }), nullptr, false, {}, { print_(V("i")) }) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// for i <- 0 until (if c then 5 else 10.5) do ()   -> a Double bound
TEST_F(SemanticControlTest, IfExprDoubleAsForBoundIsTypeError)
{
    main_({ for_("i", I(0), if_else(cond(), { expr(I(5)) }, { expr(D(10.5)) }), nullptr, false, {}, {}) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// for i <- 1 to 10 do { val x = if i < 5 then i else 0.5; print(x) }
TEST_F(SemanticControlTest, LoopVariableInIfExprInBody)
{
    ast_node_t* e = if_else(lt_(V("i"), I(5)), { expr(V("i")) }, { expr(D(0.5)) });
    main_({ for_("i", I(1), I(10), nullptr, true, {}, { val("x", e), print_(V("x")) }) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Double");
    EXPECT_TRUE(is_convert_of(last_expr(e->if_stmt.then_block), AST_VAR_REF));
}

// for i <- 1 to 3 do { val x = if c then { val i = 2.5; i } else i; print(x) }
//   -> the branch shadows the loop variable, the else branch still sees the loop variable
TEST_F(SemanticControlTest, IfExprBranchShadowsLoopVariable)
{
    ast_node_t* inner_i = val("i", D(2.5));
    ast_node_t* then_ref = V("i");
    ast_node_t* else_ref = V("i");
    ast_node_t* e = if_else(cond(), { inner_i, expr(then_ref) }, { expr(else_ref) });
    ast_node_t* loop = for_("i", I(1), I(3), nullptr, true, {}, { val("x", e), print_(V("x")) });
    main_({ loop });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Double");
    EXPECT_EQ(then_ref->var_ref.id, inner_i->var_def.id);
    EXPECT_EQ(else_ref->var_ref.id, loop->for_stmt.var_id);
    EXPECT_TRUE(is_convert_of(last_expr(e->if_stmt.else_block), AST_VAR_REF));
}

// for i <- 1 to 3 do { for j <- i until 2 * i by 1 if j != i do print(i * j) }
//   -> the inner bounds and guard use the outer loop variable
TEST_F(SemanticControlTest, NestedLoopsUseOuterLoopVariable)
{
    ast_node_t* inner_from = V("i");
    ast_node_t* inner = for_("j", inner_from, mul_(I(2), V("i")), I(1), false, { ne_(V("j"), V("i")) },
        { print_(mul_(V("i"), V("j"))) });
    ast_node_t* outer = for_("i", I(1), I(3), nullptr, true, {}, { inner });
    main_({ outer });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(inner_from->var_ref.id, outer->for_stmt.var_id);
    EXPECT_NE(inner->for_stmt.var_id, outer->for_stmt.var_id);
}

// var k = 0
// while k < 3 do { for i <- 0 until k do print(i); k = k + 1 }
// for i <- 0 until 3 do { var j = i; while j > 0 do j = j - 1 }
TEST_F(SemanticControlTest, LoopsInsideLoops)
{
    ast_node_t* m = main_({ var("k", I(0)),
        while_(lt_(V("k"), I(3)),
            { for_("i", I(0), V("k"), nullptr, false, {}, { print_(V("i")) }), assign("k", add_(V("k"), I(1))) }),
        for_("i", I(0), I(3), nullptr, false, {},
            { var("j", V("i")), while_(gt_(V("j"), I(0)), { assign("j", sub_(V("j"), I(1))) }) }) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(m->func_def.locals.count, 4u) << "k, two fors, j";
}

// val s = "hello"; for i <- 0 until s.length do print(chr(ord(s, i)))   (VALMEM, FUNEXP)
TEST_F(SemanticControlTest, ForBoundWithMemberAndCalls)
{
    main_({ val("s", S("hello")),
        for_("i", I(0), M(V("s"), "length"), nullptr, false, {},
            { print_(call("chr", { call("ord", { V("s"), V("i") }) })) }) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// for i <- 0 to strLength(s) - 1 by readInt() do ()   -> calls as bound and step (FUNEXP)
TEST_F(SemanticControlTest, ForBoundAndStepFromCalls)
{
    ast_node_t* loop = for_("i", I(0), sub_(call("strLength", { S("abc") }), I(1)), call("readInt"), true, {}, {});
    main_({ loop });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_GE(loop->for_stmt.step_id, 0);
}

// for i <- 0 to 3 by readDouble() do ()   -> a Double step from a call
TEST_F(SemanticControlTest, ForDoubleStepFromCallIsTypeError)
{
    main_({ for_("i", I(0), I(3), call("readDouble"), true, {}, {}) });
    EXPECT_EQ(analyze(), ERR_SEM_TYPE);
}

// def isqrt(n: Int): Int = {
//     for i <- 0 to n if i * i <= n if (i + 1) * (i + 1) > n do return i
//     -1
// }
TEST_F(SemanticControlTest, ForWithGuardsAndReturnInFunction)
{
    func("isqrt", TYPE_INT, { { "n", TYPE_INT } },
        { for_("i", I(0), V("n"), nullptr, true,
              { le_(mul_(V("i"), V("i")), V("n")), gt_(mul_(add_(V("i"), I(1)), add_(V("i"), I(1))), V("n")) },
              { ret(V("i")) }),
            expr(neg(I(1))) });
    main_({ print_(call("isqrt", { I(17) })) });
    EXPECT_EQ(analyze(), ERR_OK);
}

// def sum_to(n: Int) = { var s = 0; for i <- 1 to n do s = s + i; s }   -> inferred Int
TEST_F(SemanticControlTest, ForAccumulatorInInferredFunction)
{
    ast_node_t* fn = func("sum_to", nullptr, { { "n", TYPE_INT } },
        { var("s", I(0)), for_("i", I(1), V("n"), nullptr, true, {}, { assign("s", add_(V("s"), V("i"))) }),
            expr(V("s")) });
    main_({ print_(call("sum_to", { I(10) })) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(fn->func_def.return_type), "Int");
    EXPECT_EQ(fn->func_def.locals.count, 2u) << "s and the for";
}

// print(toStr(if c then { var s = 0; for i <- 1 to 10 if i != 5 do s = s + i; s } else 0))
//   -> a loop inside an if-expression inside call arguments
TEST_F(SemanticControlTest, DeepNesting)
{
    ast_node_t* e = if_else(cond(),
        { var("s", I(0)),
            for_("i", I(1), I(10), nullptr, true, { ne_(V("i"), I(5)) }, { assign("s", add_(V("s"), V("i"))) }),
            expr(V("s")) },
        { expr(I(0)) });
    ast_node_t* m = main_({ print_(call("toStr", { e })) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(e->type), "Int");
    EXPECT_EQ(m->func_def.locals.count, 2u) << "s and the for";
}

// def pick(c: Boolean, a: Int, b: Double) = if c then a else b
// @main: val x = pick(true, 1, 2) + (if false then 1 else pick(false, 3, 4.5))
//   -> inferred Double used in another if-expression and in arithmetic
TEST_F(SemanticControlTest, InferredIfFunctionUsedInIfExpr)
{
    ast_node_t* fn = func("pick", nullptr, { { "c", TYPE_BOOL }, { "a", TYPE_INT }, { "b", TYPE_DOUBLE } },
        { if_else(V("c"), { expr(V("a")) }, { expr(V("b")) }) });
    ast_node_t* e = if_else(B(false), { expr(I(1)) }, { expr(call("pick", { B(false), I(3), D(4.5) })) });
    ast_node_t* sum = add_(call("pick", { B(true), I(1), I(2) }), e);
    main_({ val("x", sum) });

    ASSERT_EQ(analyze(), ERR_OK);
    EXPECT_EQ(type_name(fn->func_def.return_type), "Double");
    EXPECT_EQ(type_name(e->type), "Double");
    EXPECT_EQ(type_name(sum->type), "Double");
}
