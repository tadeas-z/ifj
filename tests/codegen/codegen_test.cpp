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
    EXPECT_EQ(output(), ".IFJcode26\nCREATEFRAME\nCALL $main\nJUMP $end\n");
}

// Program end writes the end label and exits with 0
TEST_F(CodegenTest, ProgramEndWritesExit)
{
    codegen_program_begin(codegen);
    codegen_program_end(codegen);
    EXPECT_EQ(output(), ".IFJcode26\nCREATEFRAME\nCALL $main\nJUMP $end\nLABEL $end\nEXIT int@0\n");
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

// MOVE writes every type of constant in the IFJcode26 format
TEST_F(CodegenTest, MoveConstants)
{
    var_t x = make_var(FRAME_LF, "x", 1);
    symb_t symb = {};

    codegen_move(codegen, x, make_int(-5));

    symb.type = S_FLOAT;
    symb.float_val = 1.5;
    codegen_move(codegen, x, symb);

    symb.type = S_BOOL;
    symb.bool_val = true;
    codegen_move(codegen, x, symb);

    symb.type = S_NIL;
    codegen_move(codegen, x, symb);

    codegen_move(codegen, x, make_string("ok"));

    EXPECT_EQ(output(),
        "MOVE LF@x$1 int@-5\n"
        "MOVE LF@x$1 float@0x1.8p+0\n"
        "MOVE LF@x$1 bool@true\n"
        "MOVE LF@x$1 nil@nil\n"
        "MOVE LF@x$1 string@ok\n");
}

// MOVE from one variable to another
TEST_F(CodegenTest, MoveVariable)
{
    symb_t symb = {};
    symb.type = S_VAR;
    symb.var = make_var(FRAME_LF, "y", 2);

    codegen_move(codegen, make_var(FRAME_TF, "x", 1), symb);
    EXPECT_EQ(output(), "MOVE TF@x$1 LF@y$2\n");
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
    codegen_move(codegen, param, make_int(3));
    codegen_call(codegen, "foo");
    EXPECT_EQ(output(), "CREATEFRAME\nDEFVAR TF@n$1\nMOVE TF@n$1 int@3\nCALL foo\n");
}
