// Implementation of the IFJ26 imperative language compiler
// codegen_test.cpp
// Author(s): Marek Zachensky (xzachem00)

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "write gtest tests for codegen_program_begin, codegen_program_end
//          and codegen_free(NULL)"
// Scope: tests for program begin, program end and codegen_free

extern "C" {
#include "codegen.h"
}

#include <gtest/gtest.h>

#include <cstdio>
#include <string>

// Fresh generator writing to a tmpfile for every test
class CodegenTest : public ::testing::Test {
protected:
    FILE* out = nullptr;
    codegen_t* codegen = nullptr;

    void SetUp() override
    {
        out = tmpfile();
        ASSERT_NE(out, nullptr);
        codegen = codegen_init(out);
        ASSERT_NE(codegen, nullptr);
    }

    void TearDown() override
    {
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

// Program begin writes the header, creates TF and calls $main
TEST_F(CodegenTest, ProgramBeginWritesHeader)
{
    codegen_program_begin(codegen);
    EXPECT_EQ(output(), ".IFJcode26\nDEFVAR GF@tmp$1\nDEFVAR GF@tmp$2\nCREATEFRAME\nCALL $main\nJUMP $end\n");
}

// Program end writes the end label and exits with 0
TEST_F(CodegenTest, ProgramEndWritesExit)
{
    codegen_program_begin(codegen);
    codegen_program_end(codegen);
    EXPECT_EQ(output(), ".IFJcode26\nDEFVAR GF@tmp$1\nDEFVAR GF@tmp$2\nCREATEFRAME\nCALL $main\nJUMP $end\nLABEL $end\nEXIT int@0\n");
}

// Free of NULL does nothing
TEST(CodegenFreeTest, FreeNullIsOk)
{
    codegen_free(nullptr);
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "write gtest tests for codegen_defvar, codegen_move, codegen_write,
//          codegen_func_begin, codegen_func_end, codegen_createframe
//          and codegen_call"
// Prompt: "codegen_move is removed, test the constants with codegen_write
//          and use POPS in the call test"
// Scope: make_var, make_int, make_string and all tests below

static var_t make_var(frame_t frame, const char* name, int id)
{
    var_t var = { frame, name, id };
    return var;
}

static symb_t make_int(long long n)
{
    symb_t symb = {};
    symb.type = S_INT;
    symb.int_val = n;
    return symb;
}

static symb_t make_string(const char* s)
{
    symb_t symb = {};
    symb.type = S_STRING;
    symb.string_val = s;
    return symb;
}

// DEFVAR uses the right frame and the name$id format
TEST_F(CodegenTest, DefvarInAllFrames)
{
    codegen_defvar(codegen, make_var(FRAME_GF, "a", 1));
    codegen_defvar(codegen, make_var(FRAME_LF, "b", 2));
    codegen_defvar(codegen, make_var(FRAME_TF, "c", 3));
    EXPECT_EQ(output(), "DEFVAR GF@a$1\nDEFVAR LF@b$2\nDEFVAR TF@c$3\n");
}

// WRITE writes every type of constant in the IFJcode26 format
TEST_F(CodegenTest, WriteConstants)
{
    symb_t symb = {};

    codegen_write(codegen, make_int(-5));

    symb.type = S_FLOAT;
    symb.float_val = 1.5;
    codegen_write(codegen, symb);

    symb.type = S_BOOL;
    symb.bool_val = true;
    codegen_write(codegen, symb);

    symb.type = S_NIL;
    codegen_write(codegen, symb);

    codegen_write(codegen, make_string("ok"));

    EXPECT_EQ(output(),
        "WRITE int@-5\n"
        "WRITE float@0x1.8p+0\n"
        "WRITE bool@true\n"
        "WRITE nil@nil\n"
        "WRITE string@ok\n");
}

// WRITE escapes space, #, backslash and newline in strings
TEST_F(CodegenTest, WriteStringWithEscapes)
{
    codegen_write(codegen, make_string("a b#c\\d\n"));
    EXPECT_EQ(output(), "WRITE string@a\\032b\\035c\\092d\\010\n");
}

// Normal function has only its own label
TEST_F(CodegenTest, FuncBeginAndEnd)
{
    codegen_func_begin(codegen, "foo", false);
    codegen_func_end(codegen);
    EXPECT_EQ(output(), "LABEL foo\nPUSHFRAME\nPOPFRAME\nRETURN\n");
}

// Entry function gets also the $main label
TEST_F(CodegenTest, MainFuncGetsMainLabel)
{
    codegen_func_begin(codegen, "run", true);
    EXPECT_EQ(output(), "LABEL $main\nLABEL run\nPUSHFRAME\n");
}

// Function named end does not clash with the $end label
TEST_F(CodegenTest, FuncNamedEndHasNoDollar)
{
    codegen_func_begin(codegen, "end", false);
    EXPECT_EQ(output(), "LABEL end\nPUSHFRAME\n");
}

// Call with one parameter in TF
TEST_F(CodegenTest, CallWithParameter)
{
    var_t param = make_var(FRAME_TF, "n", 1);

    codegen_createframe(codegen);
    codegen_defvar(codegen, param);
    codegen_pops(codegen, param);
    codegen_call(codegen, "foo");
    EXPECT_EQ(output(), "CREATEFRAME\nDEFVAR TF@n$1\nPOPS TF@n$1\nCALL foo\n");
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Now add tests for the stack functions: PUSHS of a constant and
//          a variable, POPS, every stack_op_t gives the right instruction
//          and a whole expression 1 + 2 * 3 in the right order"
// Scope: CodegenStackTest and all tests below

// Same fixture, separate suite name for the stack tests
class CodegenStackTest : public CodegenTest { };

// PUSHS works for constants and variables
TEST_F(CodegenStackTest, PushsConstantAndVariable)
{
    symb_t symb = {};
    symb.type = S_VAR;
    symb.var = make_var(FRAME_LF, "a", 1);

    codegen_pushs(codegen, make_int(7));
    codegen_pushs(codegen, symb);
    EXPECT_EQ(output(), "PUSHS int@7\nPUSHS LF@a$1\n");
}

// POPS writes the target variable
TEST_F(CodegenStackTest, PopsVariable)
{
    codegen_pops(codegen, make_var(FRAME_LF, "r", 2));
    EXPECT_EQ(output(), "POPS LF@r$2\n");
}

// Every op is written with its own name
TEST_F(CodegenStackTest, AllOpsHaveRightName)
{
    codegen_stack_op(codegen, OP_ADDS);
    codegen_stack_op(codegen, OP_SUBS);
    codegen_stack_op(codegen, OP_MULS);
    codegen_stack_op(codegen, OP_DIVS);
    codegen_stack_op(codegen, OP_IDIVS);
    codegen_stack_op(codegen, OP_LTS);
    codegen_stack_op(codegen, OP_GTS);
    codegen_stack_op(codegen, OP_EQS);
    codegen_stack_op(codegen, OP_NOTS);
    codegen_stack_op(codegen, OP_INT2FLOATS);
    EXPECT_EQ(output(),
        "ADDS\nSUBS\nMULS\nDIVS\nIDIVS\n"
        "LTS\nGTS\nEQS\nNOTS\nINT2FLOATS\n");
}

// 1 + 2 * 3 pushes the operands first, then MULS and ADDS
TEST_F(CodegenStackTest, WholeExpression)
{
    codegen_pushs(codegen, make_int(1));
    codegen_pushs(codegen, make_int(2));
    codegen_pushs(codegen, make_int(3));
    codegen_stack_op(codegen, OP_MULS);
    codegen_stack_op(codegen, OP_ADDS);
    codegen_pops(codegen, make_var(FRAME_LF, "x", 1));
    EXPECT_EQ(output(),
        "PUSHS int@1\nPUSHS int@2\nPUSHS int@3\n"
        "MULS\nADDS\nPOPS LF@x$1\n");
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Add tests for the label and jump functions: new label numbers
//          go up, LABEL, JUMP and JUMPIFEQS write $num$suffix and a whole
//          if-else in the right order"
// Scope: CodegenLabelTest and all tests below

// Same fixture, separate suite name for the label tests
class CodegenLabelTest : public CodegenTest { };

// Every call gives the next number
TEST_F(CodegenLabelTest, NewLabelNumbersGoUp)
{
    EXPECT_EQ(codegen_new_label(codegen), 1);
    EXPECT_EQ(codegen_new_label(codegen), 2);
    EXPECT_EQ(codegen_new_label(codegen), 3);
}

// LABEL, JUMP and JUMPIFEQS use the $num$suffix format
TEST_F(CodegenLabelTest, LabelAndJumps)
{
    codegen_label(codegen, 1, "end");
    codegen_jump(codegen, 1, "end");
    codegen_jumpifeqs(codegen, 1, "end");
    EXPECT_EQ(output(), "LABEL $1$end\nJUMP $1$end\nJUMPIFEQS $1$end\n");
}

// if a < 3 then ... else ... jumps to else when the condition is false
TEST_F(CodegenLabelTest, WholeIfElse)
{
    symb_t a = {};
    a.type = S_VAR;
    a.var = make_var(FRAME_LF, "a", 1);
    symb_t f = {};
    f.type = S_BOOL;
    f.bool_val = false;

    int n = codegen_new_label(codegen);
    codegen_pushs(codegen, a);
    codegen_pushs(codegen, make_int(3));
    codegen_stack_op(codegen, OP_LTS);
    codegen_pushs(codegen, f);
    codegen_jumpifeqs(codegen, n, "else");
    codegen_write(codegen, make_string("then"));
    codegen_jump(codegen, n, "end");
    codegen_label(codegen, n, "else");
    codegen_write(codegen, make_string("else"));
    codegen_label(codegen, n, "end");
    EXPECT_EQ(output(),
        "PUSHS LF@a$1\nPUSHS int@3\nLTS\nPUSHS bool@false\n"
        "JUMPIFEQS $1$else\n"
        "WRITE string@then\nJUMP $1$end\n"
        "LABEL $1$else\nWRITE string@else\n"
        "LABEL $1$end\n");
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Add tests for the string, conversion and read functions: every
//          unary_op_t and binary_op_t gives the right instruction with its
//          operands and READ writes the type"
// Prompt: "Remove the tests of the removed codegen_unary_op and codegen_read"
// Scope: CodegenStringTest and all tests below

// Same fixture, separate suite name for the string tests
class CodegenStringTest : public CodegenTest { };

// Every binary op is written with its name, var and both symbs
TEST_F(CodegenStringTest, AllBinaryOps)
{
    var_t r = make_var(FRAME_LF, "r", 1);

    codegen_binary_op(codegen, OP_CONCAT, r, make_string("a"), make_string("b"));
    codegen_binary_op(codegen, OP_GETCHAR, r, make_string("abc"), make_int(1));
    codegen_binary_op(codegen, OP_STRI2INT, r, make_string("abc"), make_int(0));
    EXPECT_EQ(output(),
        "CONCAT LF@r$1 string@a string@b\n"
        "GETCHAR LF@r$1 string@abc int@1\n"
        "STRI2INT LF@r$1 string@abc int@0\n");
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Add tests for codegen_builtins: every built-in has its label
//          and PUSHFRAME and every built-in ends with RETURN"
// Prompt: "Add tests for readBoolean and toStr of a Boolean (BOOLTHEN)"
// Prompt: "Add $floor to the label test (VALMEM)"
// Prompt: "Add a test for codegen_exit"
// Scope: CodegenBuiltinTest and all tests below

// Same fixture, separate suite name for the built-in tests
class CodegenBuiltinTest : public CodegenTest { };

// Every built-in starts with its label and PUSHFRAME
TEST_F(CodegenBuiltinTest, AllBuiltinsHaveLabel)
{
    codegen_builtins(codegen);
    std::string text = output();

    const char* names[] = { "readLine", "readInt", "readDouble", "readBoolean", "toStr", "toInt", "$floor",
        "strLength", "subStr", "ord", "chr" };
    for (const char* name : names) {
        std::string label = std::string("LABEL ") + name + "\nPUSHFRAME\n";
        EXPECT_NE(text.find(label), std::string::npos) << name;
    }
}

// Code ends with RETURN, so nothing runs after the last built-in
TEST_F(CodegenBuiltinTest, EndsWithReturn)
{
    codegen_builtins(codegen);
    std::string text = output();
    std::string end = "RETURN\n";

    ASSERT_GE(text.size(), end.size());
    EXPECT_EQ(text.substr(text.size() - end.size()), end);
}

// readBoolean reads bool, EOF gives false
TEST_F(CodegenBuiltinTest, ReadBoolean)
{
    codegen_builtins(codegen);
    std::string text = output();

    EXPECT_NE(text.find("READ LF@r$0 bool\n"), std::string::npos);
    EXPECT_NE(text.find("MOVE LF@r$0 bool@false\n"), std::string::npos);
}

// toStr turns a Boolean into "true" or "false"
TEST_F(CodegenBuiltinTest, ToStrBool)
{
    codegen_builtins(codegen);
    std::string text = output();

    EXPECT_NE(text.find("JUMPIFEQ $toStr$bool LF@r$0 string@bool\n"), std::string::npos);
    EXPECT_NE(text.find("MOVE LF@r$0 string@true\n"), std::string::npos);
    EXPECT_NE(text.find("MOVE LF@r$0 string@false\n"), std::string::npos);
}

// EXIT with the given code
TEST_F(CodegenBuiltinTest, Exit)
{
    codegen_exit(codegen, 40);
    EXPECT_EQ(output(), "EXIT int@40\n");
}
