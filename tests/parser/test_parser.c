// Implementation of the IFJ26 imperative language compiler
// test_parser.c
// Author(s): Dušan Popović (xpopovd00)

// AI-generated using: Gemini
//                model: Gemini 4 HYDROGEN
// Prompt (summary): "Generate unit tests for the IFJ26 parser covering valid and
//                    invalid syntax, declarations, control flow, and trailing commas."
// Scope: test_parser.c 

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "ast.h"
#include "comp_error.h"
#include "lexer.h"
#include "parser.h"

static int total_tests = 0;
static int passed_tests = 0;

static int parse_program_str(const char *src, ast_node_t **out_ast) {
    FILE *f = tmpfile();
    assert(f != NULL);
    fputs(src, f);
    rewind(f);

    lexer_t lexer;
    lexer_init(&lexer, f);

    int err = parser_main(&lexer, out_ast);

    lexer_free(&lexer);
    fclose(f);
    return err;
}

static void check_program(const char *name, const char *src, int expected_err) {
    total_tests++;
    ast_node_t *ast = NULL;
    int actual = parse_program_str(src, &ast);

    if (actual == expected_err) {
        if (expected_err == ERR_OK) {
            assert(ast != NULL);
        }
        passed_tests++;
        printf("[PASS] %s\n", name);
    } else {
        printf("[FAIL] %s -> got %d, expected %d\n", name, actual, expected_err);
    }

    if (ast != NULL) {
        ast_free(ast);
    }
}

int main(void) {
    /* --- Valid programs (ERR_OK) --- */
    check_program("Minimal program with @main",
                  "import ifj26.*\n"
                  "@main\n"
                  "def main() = 1\n",
                  ERR_OK);

    check_program("var/val declarations with and without explicit types",
                  "import ifj26.*\n"
                  "@main\n"
                  "def main() : Unit =\n"
                  "    val a : Int = 10\n"
                  "    var b = 20.5\n"
                  "    val s : String = \"text\"\n"
                  "    var u : Unit = ()\n",
                  ERR_OK);

    check_program("Assignment and standalone function call statement",
                  "import ifj26.*\n"
                  "def helper(x : Int) : Int = x + 1\n"
                  "@main\n"
                  "def main() : Unit =\n"
                  "    var x = 1\n"
                  "    x = helper(x)\n"
                  "    helper(x + 2)\n",
                  ERR_OK);

    check_program("Branching with if / else if / else",
                  "import ifj26.*\n"
                  "@main\n"
                  "def main() : Unit =\n"
                  "    val x = 5\n"
                  "    if x < 0 then\n"
                  "        print(\"neg\")\n"
                  "    else if x == 0 then\n"
                  "        print(\"zero\")\n"
                  "    else\n"
                  "        print(\"pos\")\n",
                  ERR_OK);

    check_program("While loop and return statements",
                  "import ifj26.*\n"
                  "def loop(n : Int) : Int =\n"
                  "    var i = 0\n"
                  "    while i < n do\n"
                  "        i = i + 1\n"
                  "    return i\n"
                  "@main\n"
                  "def main() : Unit =\n"
                  "    loop(3)\n"
                  "    return\n",
                  ERR_OK);

    check_program("Multi-line parameters with trailing comma",
                  "import ifj26.*\n"
                  "def f(\n"
                  "    a : Int,\n"
                  "    b : Double,\n"
                  "    ) : Int = a\n"
                  "@main\n"
                  "def main() = f(1, 2.0)\n",
                  ERR_OK);

    /* --- Invalid programs (ERR_SYNTAX) --- */
    check_program("Missing import ifj26.* prolog",
                  "@main\n"
                  "def main() = 1\n",
                  ERR_SYNTAX);

    check_program("Wrong module name in prolog",
                  "import ifj25.*\n"
                  "@main\n"
                  "def main() = 1\n",
                  ERR_SYNTAX);

    check_program("Trailing comma in parameters without newline",
                  "import ifj26.*\n"
                  "def g(a: Int,) = a\n"
                  "@main\n"
                  "def main() = g(1)\n",
                  ERR_SYNTAX);

    check_program("If statement missing 'then'",
                  "import ifj26.*\n"
                  "@main\n"
                  "def main() =\n"
                  "    if 1 < 2\n"
                  "        print(\"x\")\n",
                  ERR_SYNTAX);

    check_program("While loop missing 'do'",
                  "import ifj26.*\n"
                  "@main\n"
                  "def main() =\n"
                  "    while 1 < 2\n"
                  "        print(\"x\")\n",
                  ERR_SYNTAX);

    check_program("Empty block under if",
                  "import ifj26.*\n"
                  "@main\n"
                  "def main() =\n"
                  "    if 1 < 2 then\n"
                  "    val x = 5\n",
                  ERR_SYNTAX);

    check_program("Statement starting with an operator",
                  "import ifj26.*\n"
                  "@main\n"
                  "def main() =\n"
                  "    + 5\n",
                  ERR_SYNTAX);

    printf("\nParser test results: %d / %d passed.\n", passed_tests, total_tests);
    return (passed_tests == total_tests) ? 0 : 1;
}
