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
// Prompt: "Create tests using google-tests for return and expression statement"
// Prompt: "Create tests using google-tests for codegen_func"
// Prompt: "Create tests using google-tests for codegen_program"
// Prompt: "Create tests using google-tests for calls inside expressions (FUNEXP)"
// Prompt: "Create tests using google-tests for BOOLTHEN (true, false, !, && and ||,
//          if without else, else if)"
// Prompt: "Create tests using google-tests for unknown nodes (exit code 99)"
// Prompt: "Create tests using google-tests for VALMEM members (length, toString,
//          toInt, floor and chains)"
// Prompt: "Create tests using google-tests for CONTROL (if as expression,
//          else if value, for with to, until, by, guards, locals)"
// Prompt: "Add a test for if as expression with an empty block"
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

// Same setup for the return tests
class CodegenReturnTest : public CodegenExprTest {
};

// return pushes the value and leaves the function
TEST_F(CodegenReturnTest, ReturnValue)
{
    tree = ast_new_return(var_ref("x", 1, TYPE_INT), 1, 1);

    codegen_stmt(codegen, tree);
    EXPECT_EQ(output(), "PUSHS LF@x$1\nPOPFRAME\nRETURN\n");
}

// bare return gives Unit
TEST_F(CodegenReturnTest, ReturnUnit)
{
    tree = ast_new_return(nullptr, 1, 1);

    codegen_stmt(codegen, tree);
    EXPECT_EQ(output(), "PUSHS nil@nil\nPOPFRAME\nRETURN\n");
}

// value of the expression statement is thrown away
TEST_F(CodegenReturnTest, ExprStmt)
{
    ast_node_t* c = call("print", true, TYPE_UNIT);
    dyn_arr_append(&c->call.args, int_lit(7));
    tree = ast_new_expr_stmt(c, 1, 1);

    codegen_stmt(codegen, tree);
    EXPECT_EQ(output(),
        "PUSHS int@7\nPOPS GF@tmp$1\nWRITE GF@tmp$1\nPUSHS nil@nil\n"
        "POPS GF@tmp$1\n");
}

// Same setup for the function tests
class CodegenFuncTest : public CodegenExprTest {
};

static ast_node_t* func(const char* name, bool is_main, type_t* type, ast_node_t* body)
{
    ast_node_t* node = ast_new_func_def(name, is_main, 1, 1);
    node->func_def.return_type = type;
    node->func_def.body = body;
    return node;
}

// params are popped from the stack in reverse, result is the last expression
TEST_F(CodegenFuncTest, ParamsAndLastExpr)
{
    tree = func("add", false, TYPE_INT,
        block(ast_new_expr_stmt(binary(AST_OP_ADD, var_ref("a", 1, TYPE_INT), var_ref("b", 2, TYPE_INT), TYPE_INT), 1, 1)));
    ast_func_def_add_param(tree, "a", TYPE_INT);
    ast_func_def_add_param(tree, "b", TYPE_INT);
    ((ast_param_t*)dyn_arr_get(&tree->func_def.params, 0))->id = 1;
    ((ast_param_t*)dyn_arr_get(&tree->func_def.params, 1))->id = 2;

    codegen_func(codegen, tree);
    EXPECT_EQ(output(),
        "LABEL add\nPUSHFRAME\n"
        "DEFVAR LF@b$2\nPOPS LF@b$2\n"
        "DEFVAR LF@a$1\nPOPS LF@a$1\n"
        "PUSHS LF@a$1\nPUSHS LF@b$2\nADDS\n"
        "POPFRAME\nRETURN\n");
}

// locals are defined at the start, Unit function returns ()
TEST_F(CodegenFuncTest, LocalsAndUnit)
{
    ast_node_t* def = ast_new_var_def("x", false, nullptr, int_lit(1), 1, 1);
    def->var_def.id = 1;
    tree = func("main", true, TYPE_UNIT, block(def));
    dyn_arr_append(&tree->func_def.locals, def);

    codegen_func(codegen, tree);
    EXPECT_EQ(output(),
        "LABEL $main\nLABEL main\nPUSHFRAME\n"
        "DEFVAR LF@x$1\n"
        "PUSHS int@1\nPOPS LF@x$1\n"
        "PUSHS nil@nil\nPOPFRAME\nRETURN\n");
}

// return in a Unit function computes the value but returns ()
TEST_F(CodegenFuncTest, ReturnInUnitFunc)
{
    tree = func("f", false, TYPE_UNIT, block(ast_new_return(int_lit(5), 1, 1)));

    codegen_func(codegen, tree);
    EXPECT_EQ(output(),
        "LABEL f\nPUSHFRAME\n"
        "PUSHS int@5\nPOPS GF@tmp$1\nPUSHS nil@nil\nPOPFRAME\nRETURN\n"
        "PUSHS nil@nil\nPOPFRAME\nRETURN\n");
}

// Same setup for the program tests
class CodegenProgramTest : public CodegenExprTest {
};

// program starts with the header, then functions, built ins and the end
TEST_F(CodegenProgramTest, WholeProgram)
{
    tree = ast_new_program();
    dyn_arr_append(&tree->program.func_list, func("main", true, TYPE_UNIT, ast_new_block(1, 1)));

    codegen_program(codegen, tree);
    std::string text = output();
    std::string start = ".IFJcode26\n"
                        "DEFVAR GF@tmp$1\nDEFVAR GF@tmp$2\n"
                        "CREATEFRAME\nCALL $main\nJUMP $end\n"
                        "LABEL $main\nLABEL main\nPUSHFRAME\n"
                        "PUSHS nil@nil\nPOPFRAME\nRETURN\n"
                        "LABEL readLine\n";
    std::string end = "LABEL $end\nEXIT int@0\n";
    EXPECT_EQ(text.substr(0, start.size()), start);
    EXPECT_EQ(text.substr(text.size() - end.size()), end);
}

// Same setup for the FUNEXP tests
class CodegenFunexpTest : public CodegenExprTest {
};

// call result is used like any other value in the expression
TEST_F(CodegenFunexpTest, CallInBinary)
{
    tree = binary(AST_OP_MUL, int_lit(2), call("readInt", true, TYPE_INT), TYPE_INT);

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(), "PUSHS int@2\nCREATEFRAME\nCALL readInt\nMULS\n");
}

// inner call is done first, its result is the arg of the outer call
TEST_F(CodegenFunexpTest, NestedBuiltinCalls)
{
    ast_node_t* inner = call("strLength", true, TYPE_INT);
    dyn_arr_append(&inner->call.args, var_ref("s", 1, TYPE_STRING));
    tree = call("toStr", true, TYPE_STRING);
    dyn_arr_append(&tree->call.args, inner);

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(),
        "PUSHS LF@s$1\nCREATEFRAME\nDEFVAR TF@value$0\nPOPS TF@value$0\nCALL strLength\n"
        "CREATEFRAME\nDEFVAR TF@value$0\nPOPS TF@value$0\nCALL toStr\n");
}

// args are done left to right, an arg can be an expression with a call
TEST_F(CodegenFunexpTest, UserCallWithExprArgs)
{
    ast_node_t* inner = call("foo", false, TYPE_INT);
    dyn_arr_append(&inner->call.args, int_lit(1));
    tree = call("bar", false, TYPE_INT);
    dyn_arr_append(&tree->call.args, binary(AST_OP_ADD, inner, int_lit(2), TYPE_INT));
    dyn_arr_append(&tree->call.args, var_ref("x", 1, TYPE_INT));

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(),
        "PUSHS int@1\nCREATEFRAME\nCALL foo\nPUSHS int@2\nADDS\n"
        "PUSHS LF@x$1\nCREATEFRAME\nCALL bar\n");
}

// Same setup for the BOOLTHEN tests
class CodegenBoolthenTest : public CodegenExprTest {
};

static ast_node_t* bool_lit(bool value)
{
    ast_node_t* node = ast_new_bool_lit(value, 1, 1);
    node->type = TYPE_BOOL;
    return node;
}

static ast_node_t* not_op(ast_node_t* operand)
{
    ast_node_t* node = ast_new_unary(AST_OP_NOT, operand, 1, 1);
    node->type = TYPE_BOOL;
    return node;
}

// true and false are pushed as bool@
TEST_F(CodegenBoolthenTest, BoolLiterals)
{
    ast_node_t* f = bool_lit(false);
    tree = bool_lit(true);

    codegen_expr(codegen, tree);
    codegen_expr(codegen, f);
    EXPECT_EQ(output(), "PUSHS bool@true\nPUSHS bool@false\n");

    ast_free(f);
}

// !b is the operand and NOTS
TEST_F(CodegenBoolthenTest, Not)
{
    tree = not_op(var_ref("b", 1, TYPE_BOOL));

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(), "PUSHS LF@b$1\nNOTS\n");
}

// a && b, b is skipped when a is false
TEST_F(CodegenBoolthenTest, AndShortCircuit)
{
    tree = binary(AST_OP_AND, var_ref("a", 1, TYPE_BOOL), var_ref("b", 2, TYPE_BOOL), TYPE_BOOL);

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(),
        "PUSHS LF@a$1\nPUSHS bool@false\nJUMPIFEQS $1$skip\n"
        "PUSHS LF@b$2\nJUMP $1$end\n"
        "LABEL $1$skip\nPUSHS bool@false\n"
        "LABEL $1$end\n");
}

// a || b, b is skipped when a is true
TEST_F(CodegenBoolthenTest, OrShortCircuit)
{
    tree = binary(AST_OP_OR, var_ref("a", 1, TYPE_BOOL), var_ref("b", 2, TYPE_BOOL), TYPE_BOOL);

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(),
        "PUSHS LF@a$1\nPUSHS bool@true\nJUMPIFEQS $1$skip\n"
        "PUSHS LF@b$2\nJUMP $1$end\n"
        "LABEL $1$skip\nPUSHS bool@true\n"
        "LABEL $1$end\n");
}

// a || b && c, inner && gets its own labels
TEST_F(CodegenBoolthenTest, NestedAndOr)
{
    ast_node_t* inner = binary(AST_OP_AND, var_ref("b", 2, TYPE_BOOL), var_ref("c", 3, TYPE_BOOL), TYPE_BOOL);
    tree = binary(AST_OP_OR, var_ref("a", 1, TYPE_BOOL), inner, TYPE_BOOL);

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(),
        "PUSHS LF@a$1\nPUSHS bool@true\nJUMPIFEQS $1$skip\n"
        "PUSHS LF@b$2\nPUSHS bool@false\nJUMPIFEQS $2$skip\n"
        "PUSHS LF@c$3\nJUMP $2$end\n"
        "LABEL $2$skip\nPUSHS bool@false\n"
        "LABEL $2$end\n"
        "JUMP $1$end\n"
        "LABEL $1$skip\nPUSHS bool@true\n"
        "LABEL $1$end\n");
}

// if without else, nothing is written after the else label
TEST_F(CodegenBoolthenTest, IfWithoutElse)
{
    tree = ast_new_if(var_ref("b", 1, TYPE_BOOL), block(assign("a", 2, int_lit(1))), nullptr, 1, 1);

    codegen_stmt(codegen, tree);
    EXPECT_EQ(output(),
        "PUSHS LF@b$1\n"
        "PUSHS bool@false\nJUMPIFEQS $1$else\n"
        "PUSHS int@1\nPOPS LF@a$2\nJUMP $1$end\n"
        "LABEL $1$else\n"
        "LABEL $1$end\n");
}

// else if is an if inside the else block
TEST_F(CodegenBoolthenTest, ElseIf)
{
    ast_node_t* inner = ast_new_if(var_ref("c", 2, TYPE_BOOL), block(assign("a", 3, int_lit(2))), nullptr, 1, 1);
    tree = ast_new_if(var_ref("b", 1, TYPE_BOOL), block(assign("a", 3, int_lit(1))), block(inner), 1, 1);

    codegen_stmt(codegen, tree);
    EXPECT_EQ(output(),
        "PUSHS LF@b$1\n"
        "PUSHS bool@false\nJUMPIFEQS $1$else\n"
        "PUSHS int@1\nPOPS LF@a$3\nJUMP $1$end\n"
        "LABEL $1$else\n"
        "PUSHS LF@c$2\n"
        "PUSHS bool@false\nJUMPIFEQS $2$else\n"
        "PUSHS int@2\nPOPS LF@a$3\nJUMP $2$end\n"
        "LABEL $2$else\n"
        "LABEL $2$end\n"
        "LABEL $1$end\n");
}

// Same setup for the unknown node tests
class CodegenUnknownTest : public CodegenExprTest {
};

// block is not an expression, codegen ends with 99
TEST_F(CodegenUnknownTest, UnknownExpressionExits)
{
    tree = ast_new_block(1, 1);

    EXPECT_EXIT(codegen_expr(codegen, tree), ::testing::ExitedWithCode(99), "unknown expression");
}

// literal is not a statement, codegen ends with 99
TEST_F(CodegenUnknownTest, UnknownStatementExits)
{
    tree = int_lit(1);

    EXPECT_EXIT(codegen_stmt(codegen, tree), ::testing::ExitedWithCode(99), "unknown statement");
}

// Same setup for the VALMEM tests
class CodegenValmemTest : public CodegenExprTest {
};

static ast_node_t* member(const char* name, ast_node_t* receiver, type_t* type)
{
    ast_node_t* node = ast_new_member(name, receiver, 1, 1);
    node->type = type;
    return node;
}

// s.length is strLength(s)
TEST_F(CodegenValmemTest, Length)
{
    tree = member("length", var_ref("s", 1, TYPE_STRING), TYPE_INT);

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(), "PUSHS LF@s$1\nCREATEFRAME\nDEFVAR TF@value$0\nPOPS TF@value$0\nCALL strLength\n");
}

// x.toString is toStr(x)
TEST_F(CodegenValmemTest, ToString)
{
    tree = member("toString", var_ref("x", 1, TYPE_INT), TYPE_STRING);

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(), "PUSHS LF@x$1\nCREATEFRAME\nDEFVAR TF@value$0\nPOPS TF@value$0\nCALL toStr\n");
}

// d.toInt is toInt(d)
TEST_F(CodegenValmemTest, ToInt)
{
    tree = member("toInt", var_ref("d", 1, TYPE_DOUBLE), TYPE_INT);

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(), "PUSHS LF@d$1\nCREATEFRAME\nDEFVAR TF@value$0\nPOPS TF@value$0\nCALL toInt\n");
}

// d.floor calls $floor, so a user function floor is not hit
TEST_F(CodegenValmemTest, Floor)
{
    tree = member("floor", var_ref("d", 1, TYPE_DOUBLE), TYPE_DOUBLE);

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(), "PUSHS LF@d$1\nCREATEFRAME\nDEFVAR TF@value$0\nPOPS TF@value$0\nCALL $floor\n");
}

// d.floor.toInt, inner member is done first
TEST_F(CodegenValmemTest, Chain)
{
    tree = member("toInt", member("floor", var_ref("d", 1, TYPE_DOUBLE), TYPE_DOUBLE), TYPE_INT);

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(),
        "PUSHS LF@d$1\nCREATEFRAME\nDEFVAR TF@value$0\nPOPS TF@value$0\nCALL $floor\n"
        "CREATEFRAME\nDEFVAR TF@value$0\nPOPS TF@value$0\nCALL toInt\n");
}

// Same setup for the CONTROL tests
class CodegenControlTest : public CodegenExprTest {
};

static ast_node_t* for_loop(ast_node_t* from, ast_node_t* to, ast_node_t* step, bool inclusive, ast_node_t* body)
{
    ast_node_t* node = ast_new_for("i", from, to, step, inclusive, 1, 1);
    node->for_stmt.var_id = 1;
    node->for_stmt.end_id = 2;
    if (step != nullptr)
        node->for_stmt.step_id = 3;
    node->for_stmt.body = body;
    return node;
}

// if as expression leaves the value of the branch on the stack
TEST_F(CodegenControlTest, IfExpression)
{
    tree = ast_new_if(var_ref("b", 1, TYPE_BOOL), block(ast_new_expr_stmt(int_lit(1), 1, 1)),
        block(ast_new_expr_stmt(int_lit(2), 1, 1)), 1, 1);
    tree->type = TYPE_INT;

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(),
        "PUSHS LF@b$1\nPUSHS bool@false\nJUMPIFEQS $1$else\n"
        "PUSHS int@1\nJUMP $1$end\n"
        "LABEL $1$else\n"
        "PUSHS int@2\n"
        "LABEL $1$end\n");
}

// else if is an if in the else block, it gives the value too
TEST_F(CodegenControlTest, IfExpressionElseIf)
{
    ast_node_t* inner = ast_new_if(var_ref("c", 2, TYPE_BOOL), block(ast_new_expr_stmt(int_lit(2), 1, 1)),
        block(ast_new_expr_stmt(int_lit(3), 1, 1)), 1, 1);
    tree = ast_new_if(var_ref("b", 1, TYPE_BOOL), block(ast_new_expr_stmt(int_lit(1), 1, 1)), block(inner), 1, 1);

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(),
        "PUSHS LF@b$1\nPUSHS bool@false\nJUMPIFEQS $1$else\n"
        "PUSHS int@1\nJUMP $1$end\n"
        "LABEL $1$else\n"
        "PUSHS LF@c$2\nPUSHS bool@false\nJUMPIFEQS $2$else\n"
        "PUSHS int@2\nJUMP $2$end\n"
        "LABEL $2$else\n"
        "PUSHS int@3\n"
        "LABEL $2$end\n"
        "LABEL $1$end\n");
}

// empty block gives ()
TEST_F(CodegenControlTest, IfExpressionEmptyBlock)
{
    tree = ast_new_if(var_ref("b", 1, TYPE_BOOL), ast_new_block(1, 1), ast_new_block(1, 1), 1, 1);
    tree->type = TYPE_UNIT;

    codegen_expr(codegen, tree);
    EXPECT_EQ(output(),
        "PUSHS LF@b$1\nPUSHS bool@false\nJUMPIFEQS $1$else\n"
        "PUSHS nil@nil\nJUMP $1$end\n"
        "LABEL $1$else\n"
        "PUSHS nil@nil\n"
        "LABEL $1$end\n");
}

// for i <- 1 to 3, no by means step 1 and no check of the step
TEST_F(CodegenControlTest, ForTo)
{
    tree = for_loop(int_lit(1), int_lit(3), nullptr, true, block(assign("a", 4, var_ref("i", 1, TYPE_INT))));

    codegen_stmt(codegen, tree);
    EXPECT_EQ(output(),
        "PUSHS int@1\nPOPS LF@i$1\n"
        "PUSHS int@3\nPOPS LF@end$2\n"
        "LABEL $1$for\n"
        "PUSHS LF@i$1\nPUSHS LF@end$2\nGTS\nNOTS\nPUSHS bool@false\nJUMPIFEQS $1$end\n"
        "LABEL $1$body\n"
        "PUSHS LF@i$1\nPOPS LF@a$4\n"
        "LABEL $1$next\n"
        "PUSHS LF@i$1\nPUSHS int@1\nADDS\nPOPS LF@i$1\n"
        "JUMP $1$for\n"
        "LABEL $1$end\n");
}

// until stops before the end
TEST_F(CodegenControlTest, ForUntil)
{
    tree = for_loop(int_lit(0), int_lit(3), nullptr, false, block(assign("a", 4, var_ref("i", 1, TYPE_INT))));

    codegen_stmt(codegen, tree);
    std::string text = output();
    EXPECT_NE(text.find("PUSHS LF@i$1\nPUSHS LF@end$2\nLTS\nPUSHS bool@false\nJUMPIFEQS $1$end\n"), std::string::npos);
}

// by: step is stored once, 0 ends with 40, minus step goes down
TEST_F(CodegenControlTest, ForWithStep)
{
    tree = for_loop(int_lit(5), int_lit(1), var_ref("s", 5, TYPE_INT), true, block(assign("a", 4, var_ref("i", 1, TYPE_INT))));

    codegen_stmt(codegen, tree);
    EXPECT_EQ(output(),
        "PUSHS int@5\nPOPS LF@i$1\n"
        "PUSHS int@1\nPOPS LF@end$2\n"
        "PUSHS LF@s$5\nPOPS LF@step$3\n"
        "PUSHS LF@step$3\nPUSHS int@0\nEQS\nPUSHS bool@false\nJUMPIFEQS $1$for\n"
        "EXIT int@40\n"
        "LABEL $1$for\n"
        "PUSHS LF@step$3\nPUSHS int@0\nLTS\nPUSHS bool@true\nJUMPIFEQS $1$down\n"
        "PUSHS LF@i$1\nPUSHS LF@end$2\nGTS\nNOTS\nPUSHS bool@false\nJUMPIFEQS $1$end\n"
        "JUMP $1$body\n"
        "LABEL $1$down\n"
        "PUSHS LF@i$1\nPUSHS LF@end$2\nLTS\nNOTS\nPUSHS bool@false\nJUMPIFEQS $1$end\n"
        "LABEL $1$body\n"
        "PUSHS LF@i$1\nPOPS LF@a$4\n"
        "LABEL $1$next\n"
        "PUSHS LF@i$1\nPUSHS LF@step$3\nADDS\nPOPS LF@i$1\n"
        "JUMP $1$for\n"
        "LABEL $1$end\n");
}

// a false guard skips the body and goes to the next i
TEST_F(CodegenControlTest, ForGuard)
{
    tree = for_loop(int_lit(1), int_lit(3), nullptr, true, block(assign("a", 4, var_ref("i", 1, TYPE_INT))));
    dyn_arr_append(&tree->for_stmt.guards, var_ref("g", 6, TYPE_BOOL));

    codegen_stmt(codegen, tree);
    std::string text = output();
    EXPECT_NE(text.find("LABEL $1$body\nPUSHS LF@g$6\nPUSHS bool@false\nJUMPIFEQS $1$next\n"), std::string::npos);
}

// locals of a for are the loop variable, end and step
TEST_F(CodegenControlTest, ForLocals)
{
    ast_node_t* loop = for_loop(int_lit(1), int_lit(3), int_lit(1), true, block(assign("a", 4, var_ref("i", 1, TYPE_INT))));
    tree = func("main", true, TYPE_UNIT, block(loop));
    dyn_arr_append(&tree->func_def.locals, loop);

    codegen_func(codegen, tree);
    std::string text = output();
    EXPECT_NE(text.find("PUSHFRAME\nDEFVAR LF@i$1\nDEFVAR LF@end$2\nDEFVAR LF@step$3\n"), std::string::npos);
}

// function ends with an if, the value of the branch is the result
TEST_F(CodegenControlTest, FuncEndsWithIf)
{
    ast_node_t* last = ast_new_if(var_ref("b", 1, TYPE_BOOL), block(ast_new_expr_stmt(int_lit(1), 1, 1)),
        block(ast_new_expr_stmt(int_lit(2), 1, 1)), 1, 1);
    tree = func("f", false, TYPE_INT, block(last));

    codegen_func(codegen, tree);
    EXPECT_EQ(output(),
        "LABEL f\nPUSHFRAME\n"
        "PUSHS LF@b$1\nPUSHS bool@false\nJUMPIFEQS $1$else\n"
        "PUSHS int@1\nJUMP $1$end\n"
        "LABEL $1$else\n"
        "PUSHS int@2\n"
        "LABEL $1$end\n"
        "POPFRAME\nRETURN\n");
}
