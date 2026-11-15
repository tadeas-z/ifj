// Implementation of the IFJ26 imperative language compiler
// codegen_ast_test.cpp
// Author(s): Marek Zachensky (xzachem00)

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Create tests using google-tests for codegen_expr. Build small
//          expression trees with the ast_new_* constructors, set the types
//          like semantics would and check the exact IFJcode26 text for
//          literals, variables, operator order, Int and Double division,
//          <= >= !=, string concatenation, unary minus and conversion"
// Prompt: "Create tests using google-tests for codegen_stmt"
// Prompt: "Create tests using google-tests for function calls"
// Scope: whole file

extern "C" {
#include "codegen_ast.h"
}

#include <gtest/gtest.h>

#include <cstdio>
#include <string>

// Fresh generator writing to a tmpfile for every test
class CodegenExprTest : public ::testing::Test {
protected:
    FILE* out = nullptr;
    codegen_t* codegen = nullptr;
    ast_node_t* tree = nullptr;

    void SetUp() override
    {
        out = tmpfile();
        ASSERT_NE(out, nullptr);
        codegen = codegen_init(out);
        ASSERT_NE(codegen, nullptr);
    }

    void TearDown() override
    {
        ast_free(tree);
        codegen_free(codegen);
        fclose(out);
    }

    // Everything written to the tmpfile so far
    std::string output()
    {
        std::string text;
        rewind(out);
        int ch;
        while ((ch = fgetc(out)) != EOF)
            text += (char)ch;
        return text;
    }
};

static ast_node_t* int_lit(int64_t n)
{
    ast_node_t* node = ast_new_int_lit(n, 1, 1);
    node->type = TYPE_INT;
    return node;
}

static ast_node_t* var_ref(const char* name, int id, type_t* type)
{
    ast_node_t* node = ast_new_var_ref(name, 1, 1);
    node->var_ref.id = id;
    node->type = type;
    return node;
}

static ast_node_t* binary(ast_binary_op_t op, ast_node_t* left, ast_node_t* right, type_t* type)
{
    ast_node_t* node = ast_new_binary(op, left, right, 1, 1);
    node->type = type;
    return node;
}

// Every literal is pushed in the IFJcode26 format, () is nil
TEST_F(CodegenExprTest, Literals)
{
    ast_node_t* d = ast_new_double_lit(1.5, 1, 1);
    ast_node_t* s = ast_new_string_lit("a b", 1, 1);
    ast_node_t* u = ast_new_unit_lit(1, 1);
    tree = int_lit(7);

    codegen_expr(codegen, tree);
    codegen_expr(codegen, d);
    codegen_expr(codegen, s);
    codegen_expr(codegen, u);
    EXPECT_EQ(output(), "PUSHS int@7\nPUSHS float@0x1.8p+0\nPUSHS string@a\\032b\nPUSHS nil@nil\n");

    ast_free(d);
    ast_free(s);
    ast_free(u);
}

// Variable is pushed from LF with its id
TEST_F(CodegenExprTest, Variable)
{
    tree = var_ref("x", 3, TYPE_INT);

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(), "PUSHS LF@x$3\n");
}

// a + 2 * 3 pushes a, 2, 3, then MULS and ADDS
TEST_F(CodegenExprTest, OperatorOrder)
{
    tree = binary(AST_OP_ADD, var_ref("a", 1, TYPE_INT),
        binary(AST_OP_MUL, int_lit(2), int_lit(3), TYPE_INT), TYPE_INT);

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(), "PUSHS LF@a$1\nPUSHS int@2\nPUSHS int@3\nMULS\nADDS\n");
}

// Int / Int is IDIVS, Double / Double is DIVS
TEST_F(CodegenExprTest, IntAndDoubleDivision)
{
    ast_node_t* d = binary(AST_OP_DIV, var_ref("x", 1, TYPE_DOUBLE), var_ref("y", 2, TYPE_DOUBLE), TYPE_DOUBLE);
    tree = binary(AST_OP_DIV, int_lit(7), int_lit(2), TYPE_INT);

    codegen_expr(codegen, tree);
    codegen_expr(codegen, d);
    EXPECT_EQ(output(), "PUSHS int@7\nPUSHS int@2\nIDIVS\nPUSHS LF@x$1\nPUSHS LF@y$2\nDIVS\n");

    ast_free(d);
}

// <=, >= and != are written as the opposite op and NOTS
TEST_F(CodegenExprTest, NegatedRelations)
{
    ast_node_t* ge = binary(AST_OP_GE, int_lit(1), int_lit(2), TYPE_BOOL);
    ast_node_t* ne = binary(AST_OP_NE, int_lit(1), int_lit(2), TYPE_BOOL);
    tree = binary(AST_OP_LE, int_lit(1), int_lit(2), TYPE_BOOL);

    codegen_expr(codegen, tree);
    codegen_expr(codegen, ge);
    codegen_expr(codegen, ne);
    EXPECT_EQ(output(),
        "PUSHS int@1\nPUSHS int@2\nGTS\nNOTS\n"
        "PUSHS int@1\nPUSHS int@2\nLTS\nNOTS\n"
        "PUSHS int@1\nPUSHS int@2\nEQS\nNOTS\n");

    ast_free(ge);
    ast_free(ne);
}

// String + uses the temp vars and CONCAT
TEST_F(CodegenExprTest, StringConcat)
{
    tree = binary(AST_OP_ADD, var_ref("s", 1, TYPE_STRING), var_ref("t", 2, TYPE_STRING), TYPE_STRING);

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(),
        "PUSHS LF@s$1\nPUSHS LF@t$2\n"
        "POPS GF@tmp$2\nPOPS GF@tmp$1\n"
        "CONCAT GF@tmp$1 GF@tmp$1 GF@tmp$2\nPUSHS GF@tmp$1\n");
}

// -x is multiplied by -1 of the right type
TEST_F(CodegenExprTest, UnaryMinus)
{
    ast_node_t* d = ast_new_unary(AST_OP_NEG, var_ref("y", 2, TYPE_DOUBLE), 1, 1);
    d->type = TYPE_DOUBLE;
    tree = ast_new_unary(AST_OP_NEG, var_ref("x", 1, TYPE_INT), 1, 1);
    tree->type = TYPE_INT;

    codegen_expr(codegen, tree);
    codegen_expr(codegen, d);
    EXPECT_EQ(output(), "PUSHS LF@x$1\nPUSHS int@-1\nMULS\nPUSHS LF@y$2\nPUSHS float@-0x1p+0\nMULS\n");

    ast_free(d);
}

// Conversion node from semantics gives INT2FLOATS
TEST_F(CodegenExprTest, Convert)
{
    tree = ast_new_convert(int_lit(5));

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(), "PUSHS int@5\nINT2FLOATS\n");
}

// Same setup for the statement tests
class CodegenStmtTest : public CodegenExprTest {
};

static ast_node_t* assign(const char* name, int id, ast_node_t* value)
{
    ast_node_t* node = ast_new_assign(name, value, 1, 1);
    node->assign.id = id;
    return node;
}

static ast_node_t* block(ast_node_t* stmt)
{
    ast_node_t* node = ast_new_block(1, 1);
    dyn_arr_append(&node->block.statements, stmt);
    return node;
}

// var x = 1 and x = x + 2 both end with POPS into the variable
TEST_F(CodegenStmtTest, VarDefAndAssign)
{
    ast_node_t* def = ast_new_var_def("x", false, nullptr, int_lit(1), 1, 1);
    def->var_def.id = 1;
    tree = block(def);
    dyn_arr_append(&tree->block.statements,
        assign("x", 1, binary(AST_OP_ADD, var_ref("x", 1, TYPE_INT), int_lit(2), TYPE_INT)));

    codegen_stmt(codegen, tree);
    EXPECT_EQ(output(),
        "PUSHS int@1\nPOPS LF@x$1\n"
        "PUSHS LF@x$1\nPUSHS int@2\nADDS\nPOPS LF@x$1\n");
}

// if jumps to else when the condition is false
TEST_F(CodegenStmtTest, IfElse)
{
    tree = ast_new_if(binary(AST_OP_LT, var_ref("a", 1, TYPE_INT), int_lit(2), TYPE_BOOL),
        block(assign("a", 1, int_lit(3))), block(assign("a", 1, int_lit(4))), 1, 1);

    codegen_stmt(codegen, tree);
    EXPECT_EQ(output(),
        "PUSHS LF@a$1\nPUSHS int@2\nLTS\n"
        "PUSHS bool@false\nJUMPIFEQS $1$else\n"
        "PUSHS int@3\nPOPS LF@a$1\nJUMP $1$end\n"
        "LABEL $1$else\n"
        "PUSHS int@4\nPOPS LF@a$1\n"
        "LABEL $1$end\n");
}

// while checks the condition first and jumps back after the body
TEST_F(CodegenStmtTest, While)
{
    tree = ast_new_while(binary(AST_OP_LT, var_ref("i", 1, TYPE_INT), int_lit(3), TYPE_BOOL),
        block(assign("i", 1, binary(AST_OP_ADD, var_ref("i", 1, TYPE_INT), int_lit(1), TYPE_INT))), 1, 1);

    codegen_stmt(codegen, tree);
    EXPECT_EQ(output(),
        "LABEL $1$while\n"
        "PUSHS LF@i$1\nPUSHS int@3\nLTS\n"
        "PUSHS bool@false\nJUMPIFEQS $1$end\n"
        "PUSHS LF@i$1\nPUSHS int@1\nADDS\nPOPS LF@i$1\n"
        "JUMP $1$while\n"
        "LABEL $1$end\n");
}

// Same setup for the call tests
class CodegenCallTest : public CodegenExprTest {
};

static ast_node_t* call(const char* name, bool is_builtin, type_t* type)
{
    ast_node_t* node = ast_new_call(name, 1, 1);
    node->call.is_builtin = is_builtin;
    node->type = type;
    return node;
}

// print writes the value and leaves Unit on the stack
TEST_F(CodegenCallTest, PrintValue)
{
    tree = call("print", true, TYPE_UNIT);
    dyn_arr_append(&tree->call.args, int_lit(5));

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(), "PUSHS int@5\nPOPS GF@tmp$1\nWRITE GF@tmp$1\nPUSHS nil@nil\n");
}

// print of Unit writes ()
TEST_F(CodegenCallTest, PrintUnit)
{
    ast_node_t* u = ast_new_unit_lit(1, 1);
    u->type = TYPE_UNIT;
    tree = call("print", true, TYPE_UNIT);
    dyn_arr_append(&tree->call.args, u);

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(), "PUSHS nil@nil\nPOPS GF@tmp$1\nWRITE string@()\nPUSHS nil@nil\n");
}

// built in gets its args in TF with the names from the spec
TEST_F(CodegenCallTest, BuiltinArgsInTF)
{
    tree = call("subStr", true, TYPE_STRING);
    dyn_arr_append(&tree->call.args, var_ref("s", 1, TYPE_STRING));
    dyn_arr_append(&tree->call.args, int_lit(1));
    dyn_arr_append(&tree->call.args, int_lit(3));

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(),
        "PUSHS LF@s$1\nPUSHS int@1\nPUSHS int@3\n"
        "CREATEFRAME\n"
        "DEFVAR TF@end$0\nPOPS TF@end$0\n"
        "DEFVAR TF@start$0\nPOPS TF@start$0\n"
        "DEFVAR TF@value$0\nPOPS TF@value$0\n"
        "CALL subStr\n");
}

// built in without args is only CREATEFRAME and CALL
TEST_F(CodegenCallTest, BuiltinWithoutArgs)
{
    tree = call("readInt", true, TYPE_INT);

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(), "CREATEFRAME\nCALL readInt\n");
}

// user function gets its args on the stack
TEST_F(CodegenCallTest, UserFunctionArgsOnStack)
{
    tree = call("foo", false, TYPE_INT);
    dyn_arr_append(&tree->call.args, int_lit(1));
    dyn_arr_append(&tree->call.args, var_ref("x", 2, TYPE_INT));

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(), "PUSHS int@1\nPUSHS LF@x$2\nCREATEFRAME\nCALL foo\n");
}
