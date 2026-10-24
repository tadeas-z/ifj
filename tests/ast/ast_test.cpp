// Implementation of the IFJ26 imperative language compiler
// ast_test.cpp
// Author(s): Adam Zatloukal (xzatloa01)
//
// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "make tests using google tests. First if the constructor fields are set
//          correctly, then for required vs optional args then test the function
//          definitions and params and then test building a tree the way a parser
//          will. Also add a case that will create a tree for a medium complexity
//          program. Later test ast free and allocation failures try to use
//          ast_print for visual representation as a feedback to the user"
//
// Build (allocation tests need the --wrap linker flags):
//   g++ -std=c++17 -Iinc -I<gtest>/include tests/ast/ast_test.cpp
//       src/common/ast.c src/common/dynamic_array.c src/common/types.c src/common/utils.c
//       <gtest-all.o> <gtest_main.o>
//       -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free -o ast_test
// The .c files must be compiled as C (gcc -std=c11 -c), not as C++.

extern "C" {
#include "ast.h"
#include "dynamic_array.h"
#include "types.h"
}

#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <functional>
#include <string>

// ========================================
//  Allocation tracking (ld --wrap)
// ========================================
// Counts allocations made while tracking is on, and can make the N-th one fail.
// Tracking is only switched on around AST calls, because gtest allocates as well.

extern "C" {
void* __real_malloc(size_t size);
void* __real_calloc(size_t count, size_t size);
void* __real_realloc(void* ptr, size_t size);
void __real_free(void* ptr);

static bool g_track = false;
static long g_live = 0;     // allocations minus frees while tracking
static long g_count = 0;    // allocation attempts while tracking
static long g_fail_at = 0;  // 0 = never fail, else fail the g_fail_at-th attempt

static bool should_fail(void)
{
    return g_track && g_fail_at != 0 && ++g_count == g_fail_at;
}

void* __wrap_malloc(size_t size)
{
    if (should_fail())
        return NULL;
    void* p = __real_malloc(size);
    if (g_track && p != NULL)
        g_live++;
    return p;
}

void* __wrap_calloc(size_t count, size_t size)
{
    if (should_fail())
        return NULL;
    void* p = __real_calloc(count, size);
    if (g_track && p != NULL)
        g_live++;
    return p;
}

void* __wrap_realloc(void* ptr, size_t size)
{
    if (should_fail())
        return NULL;
    void* p = __real_realloc(ptr, size);
    if (g_track && ptr == NULL && p != NULL)
        g_live++;
    return p;
}

void __wrap_free(void* ptr)
{
    if (g_track && ptr != NULL)
        g_live--;
    __real_free(ptr);
}
}

static void track_start(long fail_at = 0)
{
    g_live = 0;
    g_count = 0;
    g_fail_at = fail_at;
    g_track = true;
}

static long track_stop(void)
{
    g_track = false;
    g_fail_at = 0;
    return g_live;
}

// ========================================
//  Helpers
// ========================================

static ast_node_t* func_at(const ast_node_t* program, size_t i)
{
    return (ast_node_t*)dyn_arr_get(&program->program.func_list, i);
}

static ast_param_t* param_at(const ast_node_t* func_def, size_t i)
{
    return (ast_param_t*)dyn_arr_get(&func_def->func_def.params, i);
}

static ast_node_t* stmt_at(const ast_node_t* block, size_t i)
{
    return (ast_node_t*)dyn_arr_get(&block->block.statements, i);
}

static ast_node_t* arg_at(const ast_node_t* call, size_t i)
{
    return (ast_node_t*)dyn_arr_get(&call->call.args, i);
}

// Appends like the parser does: on failure the parser still owns the node
static void append_stmt(ast_node_t* block, ast_node_t* stmt)
{
    ASSERT_NE(stmt, nullptr);
    ASSERT_TRUE(dyn_arr_append(&block->block.statements, stmt));
}

static void append_arg(ast_node_t* call, ast_node_t* arg)
{
    ASSERT_NE(arg, nullptr);
    ASSERT_TRUE(dyn_arr_append(&call->call.args, arg));
}

// Prints the tree to stdout as visual feedback for whoever runs the tests
static void show_tree(const char* title, const ast_node_t* tree)
{
    printf("\n----- %s -----\n", title);
    ast_print(tree, stdout);
    printf("-----\n");
    fflush(stdout);
}

// Returns what ast_print writes for the tree.
// Uses a named file in the current directory - tmpfile() fails on Windows without admin rights.
static std::string print_to_string(const ast_node_t* tree)
{
    const char* path = "ast_print_test.tmp";
    FILE* f = fopen(path, "w+");
    if (f == nullptr) {
        ADD_FAILURE() << "cannot create " << path;
        return "";
    }
    ast_print(tree, f);
    std::string text;
    rewind(f);
    int c;
    while ((c = fgetc(f)) != EOF)
        text += (char)c;
    fclose(f);
    remove(path);
    return text;
}

// ========================================
//  1. Constructors set the fields correctly
// ========================================

class AstConstructorTest : public ::testing::Test { };

TEST_F(AstConstructorTest, ProgramStartsEmpty)
{
    ast_node_t* program = ast_new_program();
    ASSERT_NE(program, nullptr);
    EXPECT_EQ(program->kind, AST_PROGRAM);
    EXPECT_EQ(program->program.func_list.count, 0u);
    EXPECT_EQ(program->type, nullptr);
    ast_free(program);
}

TEST_F(AstConstructorTest, FuncDefDefaults)
{
    ast_node_t* fn = ast_new_func_def("factorial", false, 3, 1);
    ASSERT_NE(fn, nullptr);
    EXPECT_EQ(fn->kind, AST_FUNC_DEF);
    EXPECT_STREQ(fn->func_def.name, "factorial");
    EXPECT_FALSE(fn->func_def.is_main);
    EXPECT_EQ(fn->func_def.params.count, 0u);
    EXPECT_EQ(fn->func_def.locals.count, 0u);
    EXPECT_EQ(fn->func_def.return_type, nullptr);
    EXPECT_EQ(fn->func_def.body, nullptr);
    EXPECT_EQ(fn->line, 3u);
    EXPECT_EQ(fn->column, 1u);
    ast_free(fn);
}

TEST_F(AstConstructorTest, BlockStartsEmpty)
{
    ast_node_t* block = ast_new_block(4, 3);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(block->kind, AST_BLOCK);
    EXPECT_EQ(block->block.statements.count, 0u);
    EXPECT_EQ(block->line, 4u);
    EXPECT_EQ(block->column, 3u);
    ast_free(block);
}

TEST_F(AstConstructorTest, VarDefFields)
{
    ast_node_t* init = ast_new_int_lit(5, 2, 13);
    ast_node_t* node = ast_new_var_def("x", true, TYPE_INT, init, 2, 3);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->kind, AST_VAR_DEF);
    EXPECT_STREQ(node->var_def.name, "x");
    EXPECT_TRUE(node->var_def.is_const);
    EXPECT_EQ(node->var_def.declared_type, TYPE_INT);
    EXPECT_EQ(node->var_def.init, init);
    EXPECT_EQ(node->var_def.id, -1);
    EXPECT_EQ(node->line, 2u);
    EXPECT_EQ(node->column, 3u);
    ast_free(node);
}

TEST_F(AstConstructorTest, AssignFields)
{
    ast_node_t* value = ast_new_int_lit(1, 5, 9);
    ast_node_t* node = ast_new_assign("i", value, 5, 5);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->kind, AST_ASSIGN);
    EXPECT_STREQ(node->assign.name, "i");
    EXPECT_EQ(node->assign.value, value);
    EXPECT_EQ(node->assign.id, -1);
    ast_free(node);
}

TEST_F(AstConstructorTest, ReturnFields)
{
    ast_node_t* value = ast_new_var_ref("x", 7, 10);
    ast_node_t* node = ast_new_return(value, 7, 3);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->kind, AST_RETURN);
    EXPECT_EQ(node->return_stmt.value, value);
    ast_free(node);
}

TEST_F(AstConstructorTest, IfFields)
{
    ast_node_t* cond = ast_new_binary(AST_OP_LT, ast_new_var_ref("x", 3, 6), ast_new_int_lit(5, 3, 10), 3, 8);
    ast_node_t* then_block = ast_new_block(4, 5);
    ast_node_t* else_block = ast_new_block(6, 5);
    ast_node_t* node = ast_new_if(cond, then_block, else_block, 3, 3);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->kind, AST_IF);
    EXPECT_EQ(node->if_stmt.cond, cond);
    EXPECT_EQ(node->if_stmt.then_block, then_block);
    EXPECT_EQ(node->if_stmt.else_block, else_block);
    ast_free(node);
}

TEST_F(AstConstructorTest, WhileFields)
{
    ast_node_t* cond = ast_new_binary(AST_OP_GT, ast_new_var_ref("i", 2, 9), ast_new_int_lit(1, 2, 13), 2, 11);
    ast_node_t* body = ast_new_block(3, 5);
    ast_node_t* node = ast_new_while(cond, body, 2, 3);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->kind, AST_WHILE);
    EXPECT_EQ(node->while_stmt.cond, cond);
    EXPECT_EQ(node->while_stmt.body, body);
    ast_free(node);
}

TEST_F(AstConstructorTest, ExprStmtFields)
{
    ast_node_t* expr = ast_new_call("print", 4, 3);
    ast_node_t* node = ast_new_expr_stmt(expr, 4, 3);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->kind, AST_EXPR_STMT);
    EXPECT_EQ(node->expr_stmt.expr, expr);
    ast_free(node);
}

TEST_F(AstConstructorTest, IntLiteral)
{
    ast_node_t* node = ast_new_int_lit(1000000, 1, 9);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->kind, AST_INT_LIT);
    EXPECT_EQ(node->int_lit.value, 1000000);
    ast_free(node);
}

TEST_F(AstConstructorTest, IntLiteralKeepsFullRange)
{
    ast_node_t* node = ast_new_int_lit(INT64_MIN, 1, 1);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->int_lit.value, INT64_MIN);
    ast_free(node);
}

TEST_F(AstConstructorTest, DoubleLiteral)
{
    ast_node_t* node = ast_new_double_lit(1.01e-3, 1, 9);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->kind, AST_DOUBLE_LIT);
    EXPECT_DOUBLE_EQ(node->double_lit.value, 1.01e-3);
    ast_free(node);
}

TEST_F(AstConstructorTest, StringLiteralComputesLength)
{
    ast_node_t* node = ast_new_string_lit("a\nb", 1, 9);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->kind, AST_STRING_LIT);
    EXPECT_STREQ(node->string_lit.value, "a\nb");
    EXPECT_EQ(node->string_lit.length, 3u);
    ast_free(node);
}

TEST_F(AstConstructorTest, EmptyStringLiteral)
{
    ast_node_t* node = ast_new_string_lit("", 1, 9);
    ASSERT_NE(node, nullptr);
    EXPECT_STREQ(node->string_lit.value, "");
    EXPECT_EQ(node->string_lit.length, 0u);
    ast_free(node);
}

TEST_F(AstConstructorTest, UnitLiteral)
{
    ast_node_t* node = ast_new_unit_lit(6, 5);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->kind, AST_UNIT_LIT);
    EXPECT_EQ(node->line, 6u);
    EXPECT_EQ(node->column, 5u);
    ast_free(node);
}

TEST_F(AstConstructorTest, VarRefFields)
{
    ast_node_t* node = ast_new_var_ref("counter", 8, 12);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->kind, AST_VAR_REF);
    EXPECT_STREQ(node->var_ref.name, "counter");
    EXPECT_EQ(node->var_ref.id, -1);
    ast_free(node);
}

TEST_F(AstConstructorTest, UnaryNegation)
{
    // AST_OP_NEG is the first enum value (0), it must not be mistaken for "missing"
    ast_node_t* operand = ast_new_var_ref("x", 1, 2);
    ast_node_t* node = ast_new_unary(AST_OP_NEG, operand, 1, 1);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->kind, AST_UNARY);
    EXPECT_EQ(node->unary.op, AST_OP_NEG);
    EXPECT_EQ(node->unary.operand, operand);
    ast_free(node);
}

TEST_F(AstConstructorTest, BinaryEveryOperator)
{
    const ast_binary_op_t ops[] = { AST_OP_ADD, AST_OP_SUB, AST_OP_MUL, AST_OP_DIV, AST_OP_LT,
                                    AST_OP_LE, AST_OP_GT, AST_OP_GE, AST_OP_EQ, AST_OP_NE };
    for (ast_binary_op_t op : ops) {
        ast_node_t* left = ast_new_int_lit(1, 1, 1);
        ast_node_t* right = ast_new_int_lit(2, 1, 5);
        ast_node_t* node = ast_new_binary(op, left, right, 1, 3);
        ASSERT_NE(node, nullptr);
        EXPECT_EQ(node->kind, AST_BINARY);
        EXPECT_EQ(node->binary.op, op);
        EXPECT_EQ(node->binary.left, left);
        EXPECT_EQ(node->binary.right, right);
        ast_free(node);
    }
}

TEST_F(AstConstructorTest, CallStartsWithoutArgs)
{
    ast_node_t* node = ast_new_call("readInt", 2, 11);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->kind, AST_CALL);
    EXPECT_STREQ(node->call.name, "readInt");
    EXPECT_EQ(node->call.args.count, 0u);
    EXPECT_FALSE(node->call.is_builtin);
    ast_free(node);
}

TEST_F(AstConstructorTest, ConvertTakesPositionFromOperand)
{
    ast_node_t* operand = ast_new_var_ref("n", 9, 17);
    ast_node_t* node = ast_new_convert(operand);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->kind, AST_CONVERT);
    EXPECT_EQ(node->convert.operand, operand);
    EXPECT_EQ(node->type, TYPE_DOUBLE);
    EXPECT_EQ(node->line, 9u);
    EXPECT_EQ(node->column, 17u);
    ast_free(node);
}

TEST_F(AstConstructorTest, NamesAreCopiedNotBorrowed)
{
    // The parser reuses its token buffer, the AST must keep its own copies
    char buffer[16];

    strcpy(buffer, "fn");
    ast_node_t* fn = ast_new_func_def(buffer, false, 1, 1);
    strcpy(buffer, "var");
    ast_node_t* var_def = ast_new_var_def(buffer, false, NULL, ast_new_int_lit(1, 1, 1), 1, 1);
    strcpy(buffer, "assign");
    ast_node_t* assign = ast_new_assign(buffer, ast_new_int_lit(1, 1, 1), 1, 1);
    strcpy(buffer, "ref");
    ast_node_t* ref = ast_new_var_ref(buffer, 1, 1);
    strcpy(buffer, "call");
    ast_node_t* call = ast_new_call(buffer, 1, 1);
    strcpy(buffer, "text");
    ast_node_t* str = ast_new_string_lit(buffer, 1, 1);
    strcpy(buffer, "param");
    ASSERT_TRUE(ast_func_def_add_param(fn, buffer, TYPE_INT));

    strcpy(buffer, "OVERWRITTEN");

    EXPECT_STREQ(fn->func_def.name, "fn");
    EXPECT_STREQ(var_def->var_def.name, "var");
    EXPECT_STREQ(assign->assign.name, "assign");
    EXPECT_STREQ(ref->var_ref.name, "ref");
    EXPECT_STREQ(call->call.name, "call");
    EXPECT_STREQ(str->string_lit.value, "text");
    EXPECT_STREQ(param_at(fn, 0)->name, "param");

    ast_free(fn);
    ast_free(var_def);
    ast_free(assign);
    ast_free(ref);
    ast_free(call);
    ast_free(str);
}

// ========================================
//  2. Required vs optional arguments
// ========================================

class AstArgumentTest : public ::testing::Test {
protected:
    // A child the constructor should reject the call for. Freed after the test,
    // because a failed constructor leaves ownership with the caller.
    ast_node_t* child = nullptr;
    ast_node_t* child2 = nullptr;

    void SetUp() override
    {
        child = ast_new_int_lit(1, 1, 1);
        child2 = ast_new_int_lit(2, 1, 1);
        ASSERT_NE(child, nullptr);
        ASSERT_NE(child2, nullptr);
    }

    void TearDown() override
    {
        ast_free(child);
        ast_free(child2);
    }
};

TEST_F(AstArgumentTest, VarDefRequiresNameAndInit)
{
    EXPECT_EQ(ast_new_var_def(NULL, true, TYPE_INT, child, 1, 1), nullptr);
    EXPECT_EQ(ast_new_var_def("x", true, TYPE_INT, NULL, 1, 1), nullptr);
}

TEST_F(AstArgumentTest, VarDefTypeIsOptional)
{
    // val x = 1 - ": Type" omitted
    ast_node_t* node = ast_new_var_def("x", true, NULL, child, 1, 1);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->var_def.declared_type, nullptr);
    child = nullptr; // owned by node now
    ast_free(node);
}

TEST_F(AstArgumentTest, AssignRequiresNameAndValue)
{
    EXPECT_EQ(ast_new_assign(NULL, child, 1, 1), nullptr);
    EXPECT_EQ(ast_new_assign("x", NULL, 1, 1), nullptr);
}

TEST_F(AstArgumentTest, BareReturnIsAllowed)
{
    ast_node_t* node = ast_new_return(NULL, 1, 1);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->return_stmt.value, nullptr);
    ast_free(node);
}

TEST_F(AstArgumentTest, IfRequiresCondAndThen)
{
    EXPECT_EQ(ast_new_if(NULL, child, child2, 1, 1), nullptr);
    EXPECT_EQ(ast_new_if(child, NULL, child2, 1, 1), nullptr);
}

TEST_F(AstArgumentTest, IfElseIsOptional)
{
    // BOOLTHEN: if without else
    ast_node_t* node = ast_new_if(child, child2, NULL, 1, 1);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->if_stmt.else_block, nullptr);
    child = child2 = nullptr;
    ast_free(node);
}

TEST_F(AstArgumentTest, WhileRequiresCondAndBody)
{
    EXPECT_EQ(ast_new_while(NULL, child, 1, 1), nullptr);
    EXPECT_EQ(ast_new_while(child, NULL, 1, 1), nullptr);
}

TEST_F(AstArgumentTest, ExprStmtRequiresExpr)
{
    EXPECT_EQ(ast_new_expr_stmt(NULL, 1, 1), nullptr);
}

TEST_F(AstArgumentTest, StringRequiresValue)
{
    EXPECT_EQ(ast_new_string_lit(NULL, 1, 1), nullptr);
}

TEST_F(AstArgumentTest, VarRefRequiresName)
{
    EXPECT_EQ(ast_new_var_ref(NULL, 1, 1), nullptr);
}

TEST_F(AstArgumentTest, UnaryRequiresOperand)
{
    EXPECT_EQ(ast_new_unary(AST_OP_NEG, NULL, 1, 1), nullptr);
}

TEST_F(AstArgumentTest, BinaryRequiresBothOperands)
{
    EXPECT_EQ(ast_new_binary(AST_OP_ADD, NULL, child, 1, 1), nullptr);
    EXPECT_EQ(ast_new_binary(AST_OP_ADD, child, NULL, 1, 1), nullptr);
}

TEST_F(AstArgumentTest, CallRequiresName)
{
    EXPECT_EQ(ast_new_call(NULL, 1, 1), nullptr);
}

TEST_F(AstArgumentTest, ConvertRequiresOperand)
{
    EXPECT_EQ(ast_new_convert(NULL), nullptr);
}

TEST_F(AstArgumentTest, RejectedChildrenStayUsable)
{
    // After a rejected call the caller still owns the child and it is untouched
    ASSERT_EQ(ast_new_binary(AST_OP_ADD, child, NULL, 1, 1), nullptr);
    EXPECT_EQ(child->kind, AST_INT_LIT);
    EXPECT_EQ(child->int_lit.value, 1);
}

// ========================================
//  3. Function definitions and params
// ========================================

class AstFuncDefTest : public ::testing::Test {
protected:
    ast_node_t* fn = nullptr;

    void SetUp() override
    {
        fn = ast_new_func_def("add", false, 1, 1);
        ASSERT_NE(fn, nullptr);
    }

    void TearDown() override
    {
        ast_free(fn);
    }
};

TEST_F(AstFuncDefTest, MainFlagIsStored)
{
    ast_node_t* main_fn = ast_new_func_def("run", true, 10, 7);
    ASSERT_NE(main_fn, nullptr);
    EXPECT_TRUE(main_fn->func_def.is_main);
    ast_free(main_fn);
}

TEST_F(AstFuncDefTest, ParamsKeepDeclarationOrder)
{
    ASSERT_TRUE(ast_func_def_add_param(fn, "a", TYPE_INT));
    ASSERT_TRUE(ast_func_def_add_param(fn, "b", TYPE_DOUBLE));
    ASSERT_TRUE(ast_func_def_add_param(fn, "c", TYPE_STRING));

    ASSERT_EQ(fn->func_def.params.count, 3u);
    EXPECT_STREQ(param_at(fn, 0)->name, "a");
    EXPECT_STREQ(param_at(fn, 1)->name, "b");
    EXPECT_STREQ(param_at(fn, 2)->name, "c");
    EXPECT_EQ(param_at(fn, 0)->type, TYPE_INT);
    EXPECT_EQ(param_at(fn, 1)->type, TYPE_DOUBLE);
    EXPECT_EQ(param_at(fn, 2)->type, TYPE_STRING);
}

TEST_F(AstFuncDefTest, NewParamsHaveUnresolvedId)
{
    ASSERT_TRUE(ast_func_def_add_param(fn, "a", TYPE_INT));
    EXPECT_EQ(param_at(fn, 0)->id, -1);
}

TEST_F(AstFuncDefTest, ManyParamsGrowTheArray)
{
    // More params than DEFAULT_CAPACITY forces the dynamic array to reallocate
    const size_t count = DEFAULT_CAPACITY * 2 + 5;
    for (size_t i = 0; i < count; i++) {
        std::string name = "p" + std::to_string(i);
        ASSERT_TRUE(ast_func_def_add_param(fn, name.c_str(), i % 2 ? TYPE_INT : TYPE_STRING));
    }

    ASSERT_EQ(fn->func_def.params.count, count);
    for (size_t i = 0; i < count; i++) {
        std::string name = "p" + std::to_string(i);
        EXPECT_STREQ(param_at(fn, i)->name, name.c_str());
        EXPECT_EQ(param_at(fn, i)->type, i % 2 ? TYPE_INT : TYPE_STRING);
    }
}

TEST_F(AstFuncDefTest, AddParamRejectsInvalidInput)
{
    ast_node_t* block = ast_new_block(1, 1);
    EXPECT_FALSE(ast_func_def_add_param(NULL, "a", TYPE_INT));
    EXPECT_FALSE(ast_func_def_add_param(block, "a", TYPE_INT)); // not a function
    EXPECT_FALSE(ast_func_def_add_param(fn, NULL, TYPE_INT));
    EXPECT_FALSE(ast_func_def_add_param(fn, "a", NULL));
    EXPECT_EQ(fn->func_def.params.count, 0u);
    ast_free(block);
}

TEST_F(AstFuncDefTest, ParamTypesAreSharedSingletons)
{
    // Types are never copied - every Int param points to the same TYPE_INT
    ASSERT_TRUE(ast_func_def_add_param(fn, "a", TYPE_INT));
    ASSERT_TRUE(ast_func_def_add_param(fn, "b", TYPE_INT));
    EXPECT_EQ(param_at(fn, 0)->type, param_at(fn, 1)->type);
}

TEST_F(AstFuncDefTest, ReturnTypeAndBodyAreSetByParser)
{
    fn->func_def.return_type = TYPE_INT;
    fn->func_def.body = ast_new_block(2, 3);
    ASSERT_NE(fn->func_def.body, nullptr);
    EXPECT_EQ(fn->func_def.return_type, TYPE_INT);
    EXPECT_EQ(fn->func_def.body->kind, AST_BLOCK);
}

// ========================================
//  4. Building trees like the parser
// ========================================

class AstTreeTest : public ::testing::Test {
protected:
    ast_node_t* program = nullptr;

    void SetUp() override
    {
        program = ast_new_program();
        ASSERT_NE(program, nullptr);
    }

    void TearDown() override
    {
        ast_free(program);
    }

    ast_node_t* add_function(const char* name, bool is_main, size_t line)
    {
        ast_node_t* fn = ast_new_func_def(name, is_main, line, 1);
        if (fn == nullptr || !dyn_arr_append(&program->program.func_list, fn)) {
            ast_free(fn);
            return nullptr;
        }
        return fn;
    }
};

TEST_F(AstTreeTest, FunctionsKeepSourceOrder)
{
    ASSERT_NE(add_function("first", false, 1), nullptr);
    ASSERT_NE(add_function("second", false, 5), nullptr);
    ASSERT_NE(add_function("run", true, 9), nullptr);

    ASSERT_EQ(program->program.func_list.count, 3u);
    EXPECT_STREQ(func_at(program, 0)->func_def.name, "first");
    EXPECT_STREQ(func_at(program, 1)->func_def.name, "second");
    EXPECT_STREQ(func_at(program, 2)->func_def.name, "run");
    EXPECT_TRUE(func_at(program, 2)->func_def.is_main);
}

TEST_F(AstTreeTest, BlockKeepsStatementOrder)
{
    // val a = 1; var b = 2; b = a
    ast_node_t* block = ast_new_block(1, 1);
    append_stmt(block, ast_new_var_def("a", true, NULL, ast_new_int_lit(1, 1, 9), 1, 1));
    append_stmt(block, ast_new_var_def("b", false, NULL, ast_new_int_lit(2, 1, 20), 1, 12));
    append_stmt(block, ast_new_assign("b", ast_new_var_ref("a", 1, 27), 1, 23));

    ASSERT_EQ(block->block.statements.count, 3u);
    EXPECT_EQ(stmt_at(block, 0)->kind, AST_VAR_DEF);
    EXPECT_EQ(stmt_at(block, 1)->kind, AST_VAR_DEF);
    EXPECT_EQ(stmt_at(block, 2)->kind, AST_ASSIGN);
    EXPECT_STREQ(stmt_at(block, 0)->var_def.name, "a");
    EXPECT_STREQ(stmt_at(block, 1)->var_def.name, "b");
    ast_free(block);
}

TEST_F(AstTreeTest, CallArgumentsKeepOrder)
{
    // subStr(s, 1, 3)
    ast_node_t* call = ast_new_call("subStr", 1, 1);
    append_arg(call, ast_new_var_ref("s", 1, 8));
    append_arg(call, ast_new_int_lit(1, 1, 11));
    append_arg(call, ast_new_int_lit(3, 1, 14));

    ASSERT_EQ(call->call.args.count, 3u);
    EXPECT_EQ(arg_at(call, 0)->kind, AST_VAR_REF);
    EXPECT_EQ(arg_at(call, 1)->int_lit.value, 1);
    EXPECT_EQ(arg_at(call, 2)->int_lit.value, 3);
    ast_free(call);
}

TEST_F(AstTreeTest, ExpressionPrecedenceShape)
{
    // a + b * 2 -> the parser builds + at the root, * as its right child
    ast_node_t* mul = ast_new_binary(AST_OP_MUL, ast_new_var_ref("b", 1, 5), ast_new_int_lit(2, 1, 9), 1, 7);
    ast_node_t* add = ast_new_binary(AST_OP_ADD, ast_new_var_ref("a", 1, 1), mul, 1, 3);
    ASSERT_NE(add, nullptr);

    EXPECT_EQ(add->binary.op, AST_OP_ADD);
    EXPECT_EQ(add->binary.left->kind, AST_VAR_REF);
    EXPECT_EQ(add->binary.right, mul);
    EXPECT_EQ(mul->binary.op, AST_OP_MUL);
    show_tree("a + b * 2", add);
    ast_free(add);
}

TEST_F(AstTreeTest, NestedIfInsideWhile)
{
    // def loop(n: Int): Unit =
    //   var i = 0
    //   while i < n do
    //     if i == 3 then
    //       print("three")
    //     else
    //       print(i)
    //     i = i + 1
    ast_node_t* fn = add_function("loop", false, 1);
    ASSERT_NE(fn, nullptr);
    ASSERT_TRUE(ast_func_def_add_param(fn, "n", TYPE_INT));
    fn->func_def.return_type = TYPE_UNIT;
    fn->func_def.body = ast_new_block(2, 3);
    ast_node_t* body = fn->func_def.body;

    ast_node_t* var_i = ast_new_var_def("i", false, NULL, ast_new_int_lit(0, 2, 11), 2, 3);
    append_stmt(body, var_i);
    ASSERT_TRUE(dyn_arr_append(&fn->func_def.locals, var_i)); // borrowed, filled by semantics

    ast_node_t* print_three = ast_new_call("print", 5, 7);
    append_arg(print_three, ast_new_string_lit("three", 5, 13));
    ast_node_t* then_block = ast_new_block(5, 7);
    append_stmt(then_block, ast_new_expr_stmt(print_three, 5, 7));

    ast_node_t* print_i = ast_new_call("print", 7, 7);
    append_arg(print_i, ast_new_var_ref("i", 7, 13));
    ast_node_t* else_block = ast_new_block(7, 7);
    append_stmt(else_block, ast_new_expr_stmt(print_i, 7, 7));

    ast_node_t* if_cond = ast_new_binary(AST_OP_EQ, ast_new_var_ref("i", 4, 8), ast_new_int_lit(3, 4, 13), 4, 10);
    ast_node_t* loop_body = ast_new_block(4, 5);
    append_stmt(loop_body, ast_new_if(if_cond, then_block, else_block, 4, 5));
    ast_node_t* inc = ast_new_binary(AST_OP_ADD, ast_new_var_ref("i", 8, 9), ast_new_int_lit(1, 8, 13), 8, 11);
    append_stmt(loop_body, ast_new_assign("i", inc, 8, 5));

    ast_node_t* while_cond = ast_new_binary(AST_OP_LT, ast_new_var_ref("i", 3, 9), ast_new_var_ref("n", 3, 13), 3, 11);
    append_stmt(body, ast_new_while(while_cond, loop_body, 3, 3));

    // Walk the tree the way semantics would
    ASSERT_EQ(body->block.statements.count, 2u);
    ast_node_t* loop = stmt_at(body, 1);
    ASSERT_EQ(loop->kind, AST_WHILE);
    ASSERT_EQ(loop->while_stmt.body->block.statements.count, 2u);
    ast_node_t* if_node = stmt_at(loop->while_stmt.body, 0);
    ASSERT_EQ(if_node->kind, AST_IF);
    EXPECT_EQ(if_node->if_stmt.cond->binary.op, AST_OP_EQ);
    ast_node_t* then_call = stmt_at(if_node->if_stmt.then_block, 0)->expr_stmt.expr;
    EXPECT_STREQ(then_call->call.name, "print");
    EXPECT_STREQ(arg_at(then_call, 0)->string_lit.value, "three");
    EXPECT_EQ(stmt_at(loop->while_stmt.body, 1)->kind, AST_ASSIGN);
    EXPECT_EQ(fn->func_def.locals.count, 1u);
    EXPECT_EQ(fn->func_def.locals.items[0], stmt_at(body, 0)); // same node, borrowed

    show_tree("nested if inside while", program);
}

// Medium complexity program (based on the example in the IFJ26 specification):
//
//  1  import ifj26.*
//  2
//  3  def factorial(n: Int): Int =
//  4    var result = 1
//  5    var i: Int = n
//  6    while i > 1 do
//  7      result = result * i
//  8      i = i - 1
//  9    result
// 10
// 11  @main def run(): Unit =
// 12    val n = readInt()
// 13    val result = factorial(n)
// 14    if result > 100 then
// 15      print("big: ")
// 16    else
// 17      print("small: ")
// 18    print(result)
// 19    val half = result / 2.5     (semantics inserts CONVERT around result)
// 20    print(half)
// 21    return
TEST_F(AstTreeTest, MediumComplexityProgram)
{
    // --- def factorial(n: Int): Int
    ast_node_t* factorial = add_function("factorial", false, 3);
    ASSERT_NE(factorial, nullptr);
    ASSERT_TRUE(ast_func_def_add_param(factorial, "n", TYPE_INT));
    factorial->func_def.return_type = TYPE_INT;
    factorial->func_def.body = ast_new_block(4, 3);
    ast_node_t* f_body = factorial->func_def.body;

    ast_node_t* var_result = ast_new_var_def("result", false, NULL, ast_new_int_lit(1, 4, 16), 4, 3);
    ast_node_t* var_i = ast_new_var_def("i", false, TYPE_INT, ast_new_var_ref("n", 5, 16), 5, 3);
    append_stmt(f_body, var_result);
    append_stmt(f_body, var_i);

    ast_node_t* loop_body = ast_new_block(7, 5);
    ast_node_t* mul = ast_new_binary(AST_OP_MUL, ast_new_var_ref("result", 7, 14), ast_new_var_ref("i", 7, 23), 7, 21);
    append_stmt(loop_body, ast_new_assign("result", mul, 7, 5));
    ast_node_t* dec = ast_new_binary(AST_OP_SUB, ast_new_var_ref("i", 8, 9), ast_new_int_lit(1, 8, 13), 8, 11);
    append_stmt(loop_body, ast_new_assign("i", dec, 8, 5));
    ast_node_t* loop_cond = ast_new_binary(AST_OP_GT, ast_new_var_ref("i", 6, 9), ast_new_int_lit(1, 6, 13), 6, 11);
    append_stmt(f_body, ast_new_while(loop_cond, loop_body, 6, 3));
    append_stmt(f_body, ast_new_expr_stmt(ast_new_var_ref("result", 9, 3), 9, 3));

    // --- @main def run(): Unit
    ast_node_t* run = add_function("run", true, 11);
    ASSERT_NE(run, nullptr);
    run->func_def.return_type = TYPE_UNIT;
    run->func_def.body = ast_new_block(12, 3);
    ast_node_t* r_body = run->func_def.body;

    ast_node_t* read_call = ast_new_call("readInt", 12, 11);
    ast_node_t* val_n = ast_new_var_def("n", true, NULL, read_call, 12, 3);
    append_stmt(r_body, val_n);

    ast_node_t* fact_call = ast_new_call("factorial", 13, 16);
    append_arg(fact_call, ast_new_var_ref("n", 13, 26));
    ast_node_t* val_result = ast_new_var_def("result", true, NULL, fact_call, 13, 3);
    append_stmt(r_body, val_result);

    ast_node_t* big = ast_new_call("print", 15, 5);
    append_arg(big, ast_new_string_lit("big: ", 15, 11));
    ast_node_t* then_block = ast_new_block(15, 5);
    append_stmt(then_block, ast_new_expr_stmt(big, 15, 5));
    ast_node_t* small = ast_new_call("print", 17, 5);
    append_arg(small, ast_new_string_lit("small: ", 17, 11));
    ast_node_t* else_block = ast_new_block(17, 5);
    append_stmt(else_block, ast_new_expr_stmt(small, 17, 5));
    ast_node_t* if_cond = ast_new_binary(AST_OP_GT, ast_new_var_ref("result", 14, 6), ast_new_int_lit(100, 14, 15), 14, 13);
    append_stmt(r_body, ast_new_if(if_cond, then_block, else_block, 14, 3));

    ast_node_t* print_result = ast_new_call("print", 18, 3);
    append_arg(print_result, ast_new_var_ref("result", 18, 9));
    append_stmt(r_body, ast_new_expr_stmt(print_result, 18, 3));

    ast_node_t* converted = ast_new_convert(ast_new_var_ref("result", 19, 14));
    ast_node_t* div = ast_new_binary(AST_OP_DIV, converted, ast_new_double_lit(2.5, 19, 23), 19, 21);
    ast_node_t* val_half = ast_new_var_def("half", true, NULL, div, 19, 3);
    append_stmt(r_body, val_half);

    ast_node_t* print_half = ast_new_call("print", 20, 3);
    append_arg(print_half, ast_new_var_ref("half", 20, 9));
    append_stmt(r_body, ast_new_expr_stmt(print_half, 20, 3));
    append_stmt(r_body, ast_new_return(NULL, 21, 3));

    // Semantics: locals (borrowed) and builtin calls
    ASSERT_TRUE(dyn_arr_append(&factorial->func_def.locals, var_result));
    ASSERT_TRUE(dyn_arr_append(&factorial->func_def.locals, var_i));
    ASSERT_TRUE(dyn_arr_append(&run->func_def.locals, val_n));
    ASSERT_TRUE(dyn_arr_append(&run->func_def.locals, val_result));
    ASSERT_TRUE(dyn_arr_append(&run->func_def.locals, val_half));
    read_call->call.is_builtin = true;
    big->call.is_builtin = small->call.is_builtin = true;
    print_result->call.is_builtin = print_half->call.is_builtin = true;

    // Structure checks
    ASSERT_EQ(program->program.func_list.count, 2u);
    EXPECT_EQ(factorial->func_def.params.count, 1u);
    EXPECT_EQ(f_body->block.statements.count, 4u);
    EXPECT_EQ(stmt_at(f_body, 2)->kind, AST_WHILE);
    EXPECT_EQ(stmt_at(f_body, 3)->kind, AST_EXPR_STMT); // last statement = return value
    EXPECT_EQ(r_body->block.statements.count, 7u);
    EXPECT_EQ(stmt_at(r_body, 6)->kind, AST_RETURN);
    EXPECT_EQ(stmt_at(r_body, 6)->return_stmt.value, nullptr);
    EXPECT_EQ(div->binary.left->kind, AST_CONVERT);
    EXPECT_EQ(div->binary.left->type, TYPE_DOUBLE);
    EXPECT_FALSE(fact_call->call.is_builtin);
    EXPECT_EQ(run->func_def.locals.count, 3u);

    show_tree("medium complexity program", program);

    // ast_print output contains every function and statement kind used above
    std::string text = print_to_string(program);
    EXPECT_NE(text.find("FUNC_DEF factorial"), std::string::npos);
    EXPECT_NE(text.find("FUNC_DEF @main run"), std::string::npos);
    EXPECT_NE(text.find("WHILE"), std::string::npos);
    EXPECT_NE(text.find("CONVERT"), std::string::npos);
    EXPECT_NE(text.find("STRING_LIT \"small: \""), std::string::npos);
}

// ========================================
//  5. ast_free
// ========================================

class AstFreeTest : public ::testing::Test { };

// Builds a tree using all 18 node kinds, with locals sharing nodes with the body
static ast_node_t* build_every_kind_tree(void)
{
    ast_node_t* program = ast_new_program();
    ast_node_t* fn = ast_new_func_def("f", false, 1, 1);
    ast_func_def_add_param(fn, "a", TYPE_INT);
    ast_func_def_add_param(fn, "s", TYPE_STRING);
    fn->func_def.body = ast_new_block(2, 3);
    ast_node_t* body = fn->func_def.body;

    ast_node_t* var_def = ast_new_var_def("x", false, NULL,
        ast_new_binary(AST_OP_ADD, ast_new_var_ref("a", 2, 11), ast_new_int_lit(1, 2, 15), 2, 13), 2, 3);
    dyn_arr_append(&body->block.statements, var_def);
    dyn_arr_append(&fn->func_def.locals, var_def);

    ast_node_t* then_block = ast_new_block(4, 5);
    dyn_arr_append(&then_block->block.statements, ast_new_return(ast_new_var_ref("x", 4, 12), 4, 5));
    ast_node_t* else_block = ast_new_block(6, 5);
    ast_node_t* call = ast_new_call("print", 6, 5);
    dyn_arr_append(&call->call.args, ast_new_string_lit("hi", 6, 11));
    dyn_arr_append(&call->call.args, ast_new_unit_lit(6, 17));
    dyn_arr_append(&else_block->block.statements, ast_new_expr_stmt(call, 6, 5));
    dyn_arr_append(&else_block->block.statements, ast_new_return(NULL, 7, 5));
    dyn_arr_append(&body->block.statements, ast_new_if(
        ast_new_binary(AST_OP_LT, ast_new_var_ref("x", 3, 6), ast_new_int_lit(5, 3, 10), 3, 8),
        then_block, else_block, 3, 3));

    ast_node_t* loop_body = ast_new_block(9, 5);
    dyn_arr_append(&loop_body->block.statements,
        ast_new_assign("x", ast_new_unary(AST_OP_NEG, ast_new_var_ref("x", 9, 10), 9, 9), 9, 5));
    dyn_arr_append(&body->block.statements, ast_new_while(
        ast_new_binary(AST_OP_GT, ast_new_var_ref("x", 8, 9), ast_new_int_lit(0, 8, 13), 8, 11),
        loop_body, 8, 3));

    dyn_arr_append(&body->block.statements, ast_new_expr_stmt(
        ast_new_binary(AST_OP_MUL, ast_new_convert(ast_new_var_ref("x", 10, 3)), ast_new_double_lit(1.5, 10, 7), 10, 5),
        10, 3));

    dyn_arr_append(&program->program.func_list, fn);
    return program;
}

TEST_F(AstFreeTest, NullIsAllowed)
{
    ast_free(NULL);
    SUCCEED();
}

TEST_F(AstFreeTest, EveryKindTreeIsFreedWithoutLeaks)
{
    track_start();
    ast_node_t* program = build_every_kind_tree();
    long built = g_live;
    ast_free(program);
    long left = track_stop();

    EXPECT_GT(built, 0);
    EXPECT_EQ(left, 0) << "allocations not freed by ast_free";
}

TEST_F(AstFreeTest, LocalsAreNotFreedTwice)
{
    // locals point to the same VAR_DEF nodes as the body. If ast_free freed them
    // through locals as well, the counter would go below zero (or the heap would crash).
    track_start();
    ast_node_t* fn = ast_new_func_def("f", false, 1, 1);
    fn->func_def.body = ast_new_block(2, 3);
    for (int i = 0; i < 5; i++) {
        ast_node_t* var_def = ast_new_var_def("v", false, NULL, ast_new_int_lit(i, 2, 11), 2, 3);
        dyn_arr_append(&fn->func_def.body->block.statements, var_def);
        dyn_arr_append(&fn->func_def.locals, var_def);
    }
    ast_free(fn);
    EXPECT_EQ(track_stop(), 0);
}

TEST_F(AstFreeTest, HalfBuiltNodesAreFreed)
{
    // After a syntax error the parser may free nodes with missing children
    track_start();
    ast_node_t* right = ast_new_int_lit(2, 1, 5);
    ast_node_t* binary = ast_new_binary(AST_OP_ADD, ast_new_int_lit(1, 1, 1), right, 1, 3);
    binary->binary.right = nullptr; // pretend the right side was never attached
    ast_free(binary);               // must not crash on the missing child
    ast_free(right);                // the detached child is freed by its owner

    ast_node_t* fn = ast_new_func_def("f", false, 1, 1); // no body
    ast_free(fn);
    EXPECT_EQ(track_stop(), 0);
}

TEST_F(AstFreeTest, ManyStatementsAreFreed)
{
    track_start();
    ast_node_t* block = ast_new_block(1, 1);
    for (int i = 0; i < 500; i++)
        dyn_arr_append(&block->block.statements, ast_new_expr_stmt(ast_new_int_lit(i, 1, 1), 1, 1));
    ast_free(block);
    EXPECT_EQ(track_stop(), 0);
}

// ========================================
//  6. Allocation failures
// ========================================
// Fails the 1st, 2nd, ... allocation inside a constructor until it succeeds.
// Every failed call must return NULL and leave no allocation behind.

class AstAllocationTest : public ::testing::Test {
protected:
    // Returns the number of allocation points that were tested
    long check_every_failure(const char* what, const std::function<ast_node_t*()>& construct)
    {
        for (long k = 1; k < 100; k++) {
            track_start(k);
            ast_node_t* node = construct();
            long attempts = g_count;
            if (node != nullptr) {
                ast_free(node);
                track_stop();
                EXPECT_GT(k, 1) << what << ": first allocation failed but a node was returned";
                return k - 1;
            }
            long left = track_stop();
            EXPECT_EQ(left, 0) << what << ": leaked " << left << " allocation(s) when allocation #" << k << " failed";
            if (attempts < k) {
                ADD_FAILURE() << what << ": returned NULL without any failed allocation";
                return k;
            }
        }
        ADD_FAILURE() << what << ": never succeeded";
        return 0;
    }
};

TEST_F(AstAllocationTest, ConstructorsWithoutChildren)
{
    EXPECT_EQ(check_every_failure("program", [] { return ast_new_program(); }), 2);
    EXPECT_EQ(check_every_failure("func_def", [] { return ast_new_func_def("f", false, 1, 1); }), 4);
    EXPECT_EQ(check_every_failure("block", [] { return ast_new_block(1, 1); }), 2);
    EXPECT_EQ(check_every_failure("int_lit", [] { return ast_new_int_lit(1, 1, 1); }), 1);
    EXPECT_EQ(check_every_failure("double_lit", [] { return ast_new_double_lit(1.0, 1, 1); }), 1);
    EXPECT_EQ(check_every_failure("string_lit", [] { return ast_new_string_lit("hi", 1, 1); }), 2);
    EXPECT_EQ(check_every_failure("unit_lit", [] { return ast_new_unit_lit(1, 1); }), 1);
    EXPECT_EQ(check_every_failure("var_ref", [] { return ast_new_var_ref("x", 1, 1); }), 2);
    EXPECT_EQ(check_every_failure("call", [] { return ast_new_call("print", 1, 1); }), 3);
}

TEST_F(AstAllocationTest, FailedConstructorLeavesChildrenToCaller)
{
    // The child is created outside the tracked region; after a failure it must
    // still be alive (the caller frees it), after success the node owns it.
    struct Case {
        const char* name;
        std::function<ast_node_t*(ast_node_t*)> construct;
    };
    const Case cases[] = {
        { "var_def", [](ast_node_t* c) { return ast_new_var_def("x", true, NULL, c, 1, 1); } },
        { "assign", [](ast_node_t* c) { return ast_new_assign("x", c, 1, 1); } },
        { "return", [](ast_node_t* c) { return ast_new_return(c, 1, 1); } },
        { "expr_stmt", [](ast_node_t* c) { return ast_new_expr_stmt(c, 1, 1); } },
        { "unary", [](ast_node_t* c) { return ast_new_unary(AST_OP_NEG, c, 1, 1); } },
        { "convert", [](ast_node_t* c) { return ast_new_convert(c); } },
    };

    for (const Case& test : cases) {
        for (long k = 1; k < 100; k++) {
            ast_node_t* child = ast_new_int_lit(42, 1, 1);
            ASSERT_NE(child, nullptr);
            track_start(k);
            ast_node_t* node = test.construct(child);
            long left = track_stop();
            if (node != nullptr) {
                ast_free(node); // frees the child too
                break;
            }
            EXPECT_EQ(left, 0) << test.name << ": leaked when allocation #" << k << " failed";
            EXPECT_EQ(child->kind, AST_INT_LIT) << test.name << ": child was freed by a failed constructor";
            EXPECT_EQ(child->int_lit.value, 42);
            ast_free(child);
        }
    }
}

TEST_F(AstAllocationTest, FailedBinaryAndControlFlowKeepChildren)
{
    // These constructors allocate only the node, so failing allocation #1 is the only failure point
    ast_node_t* a = ast_new_int_lit(1, 1, 1);
    ast_node_t* b = ast_new_int_lit(2, 1, 1);
    ast_node_t* c = ast_new_block(1, 1);
    ASSERT_TRUE(a != nullptr && b != nullptr && c != nullptr);

    track_start(1);
    ast_node_t* binary = ast_new_binary(AST_OP_ADD, a, b, 1, 1);
    long left = track_stop();
    EXPECT_EQ(binary, nullptr);
    EXPECT_EQ(left, 0);

    track_start(1);
    ast_node_t* if_node = ast_new_if(a, c, NULL, 1, 1);
    left = track_stop();
    EXPECT_EQ(if_node, nullptr);
    EXPECT_EQ(left, 0);

    track_start(1);
    ast_node_t* while_node = ast_new_while(a, c, 1, 1);
    left = track_stop();
    EXPECT_EQ(while_node, nullptr);
    EXPECT_EQ(left, 0);

    // All three failed, so the children are untouched and still owned here
    EXPECT_EQ(a->int_lit.value, 1);
    EXPECT_EQ(b->int_lit.value, 2);
    EXPECT_EQ(c->kind, AST_BLOCK);
    ast_free(a);
    ast_free(b);
    ast_free(c);
}

TEST_F(AstAllocationTest, AddParamFailureLeavesFunctionIntact)
{
    ast_node_t* fn = ast_new_func_def("f", false, 1, 1);
    ASSERT_NE(fn, nullptr);
    ASSERT_TRUE(ast_func_def_add_param(fn, "first", TYPE_INT));

    for (long k = 1; k <= 2; k++) { // param struct, name copy
        track_start(k);
        bool ok = ast_func_def_add_param(fn, "second", TYPE_INT);
        long left = track_stop();
        EXPECT_FALSE(ok) << "allocation #" << k << " failed but add_param succeeded";
        EXPECT_EQ(left, 0) << "leaked when allocation #" << k << " failed";
        EXPECT_EQ(fn->func_def.params.count, 1u);
    }

    EXPECT_TRUE(ast_func_def_add_param(fn, "second", TYPE_INT));
    EXPECT_EQ(fn->func_def.params.count, 2u);
    EXPECT_STREQ(param_at(fn, 0)->name, "first");
    ast_free(fn);
}

// ========================================
//  7. ast_print
// ========================================

class AstPrintTest : public ::testing::Test { };

TEST_F(AstPrintTest, NullPrintsNull)
{
    EXPECT_EQ(print_to_string(NULL), "(null)\n");
}

TEST_F(AstPrintTest, ExactOutputOfSmallTree)
{
    // val x = a + 1
    ast_node_t* var_def = ast_new_var_def("x", true, NULL,
        ast_new_binary(AST_OP_ADD, ast_new_var_ref("a", 2, 11), ast_new_int_lit(1, 2, 15), 2, 13), 2, 3);
    ASSERT_NE(var_def, nullptr);

    EXPECT_EQ(print_to_string(var_def),
        "VAR_DEF val x [2:3]\n"
        "  BINARY + [2:13]\n"
        "    VAR_REF a [2:11]\n"
        "    INT_LIT 1 [2:15]\n");
    ast_free(var_def);
}

TEST_F(AstPrintTest, EveryKindTreeIsPrinted)
{
    ast_node_t* program = build_every_kind_tree();
    std::string text = print_to_string(program);
    const char* kinds[] = { "PROGRAM", "FUNC_DEF", "BLOCK", "VAR_DEF", "ASSIGN", "RETURN", "IF", "WHILE",
                            "EXPR_STMT", "INT_LIT", "DOUBLE_LIT", "STRING_LIT", "UNIT_LIT", "VAR_REF",
                            "UNARY", "BINARY", "CALL", "CONVERT" };
    for (const char* kind : kinds)
        EXPECT_NE(text.find(kind), std::string::npos) << kind << " missing from ast_print output";

    show_tree("every node kind", program);
    ast_free(program);
}
