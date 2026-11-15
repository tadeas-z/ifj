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
