// Implementation of the IFJ26 imperative language compiler
// codegen.h
// Author(s): Marek Zachensky (xzachem00)

// Low lever IFJcode26 emitter (codegen.c) Each function writes one
// instruction with correctly formatter operands (variables, constants, labels).
// What is generate is decided by codegen_ast.c.

#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdbool.h>
#include <stdio.h>

typedef struct codegen codegen_t;

typedef enum {
    FRAME_GF,
    FRAME_LF,
    FRAME_TF
} frame_t;

// variable (LF@name$id)
typedef struct {
    frame_t frame;
    const char* name;
    int id;
} var_t;

typedef enum {
    S_VAR,
    S_INT,
    S_FLOAT,
    S_BOOL,
    S_NIL,
    S_STRING
} symb_type_t;

// variable or constant
typedef struct {
    symb_type_t type;
    var_t var;
    long long int_val;
    double float_val;
    bool bool_val;
    const char* string_val;
} symb_t;

// stack instructions for expressions
typedef enum {
    OP_ADDS,
    OP_SUBS,
    OP_MULS,
    OP_DIVS,
    OP_IDIVS,
    OP_LTS,
    OP_GTS,
    OP_EQS,
    OP_NOTS,
    OP_INT2FLOATS
} stack_op_t;

// OP var symb symb
typedef enum {
    OP_CONCAT,
    OP_GETCHAR,
    OP_STRI2INT
} binary_op_t;

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Edit and complete the Doxygen descriptions of the public
//          codegen functions in this header."
// Scope: doc comments of codegen_init, codegen_free, codegen_program_begin
//        and codegen_program_end only

/**
 * @brief Creates a code generator that writes IFJcode26 to the given stream.
 * @param output Stream for the generated code (typically stdout). Borrowed,
 *               the generator never closes it.
 * @return New generator, or NULL if allocation fails.
 */
codegen_t* codegen_init(FILE* output);

/**
 * @brief Frees the generator and all memory it owns.
 * @param codegen Generator to free; NULL is allowed and does nothing.
 * @note Does not close the output stream passed to codegen_init.
 */
void codegen_free(codegen_t* codegen);

/**
 * @brief Writes the program prologue.
 *
 * Emits the mandatory first line ".IFJcode26" followed by the code that
 * transfers control to the entry function (the one annotated with \@main).
 *
 * @param codegen Generator created by codegen_init.
 * @note Must be called exactly once, before any other code is generated.
 */
void codegen_program_begin(codegen_t* codegen);

/**
 * @brief Writes the program epilogue.
 *
 * Emits the code executed after the entry function returns, which ends
 * interpretation with exit code 0.
 *
 * @param codegen Generator created by codegen_init.
 * @note Must be called exactly once, after codegen_program_begin.
 */
void codegen_program_end(codegen_t* codegen);

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Write the Doxygen descriptions of the new public codegen
//          functions in this header."
// Prompt: "Remove the unused codegen_move and its description"
// Scope: doc comments of codegen_defvar, codegen_write,
//        codegen_func_begin, codegen_func_end, codegen_createframe
//        and codegen_call only

/**
 * @brief Writes DEFVAR for the variable.
 * @param codegen Generator created by codegen_init.
 * @param var Variable to define.
 */
void codegen_defvar(codegen_t* codegen, var_t var);

/**
 * @brief Writes WRITE, prints symb to standard output.
 * @param codegen Generator created by codegen_init.
 * @param symb Variable or constant to print.
 */
void codegen_write(codegen_t* codegen, symb_t symb);

/**
 * @brief Writes the start of a function: its label and PUSHFRAME.
 *
 * For the entry function the label used by codegen_program_begin is
 * written too.
 *
 * @param codegen Generator created by codegen_init.
 * @param name Function name from the IFJ26 source.
 * @param is_main True for the function annotated with \@main.
 */
void codegen_func_begin(codegen_t* codegen, const char* name, bool is_main);

/**
 * @brief Writes the end of a function: POPFRAME and RETURN.
 * @param codegen Generator created by codegen_init.
 */
void codegen_func_end(codegen_t* codegen);

/**
 * @brief Writes CREATEFRAME, a new empty TF for the next call.
 * @param codegen Generator created by codegen_init.
 */
void codegen_createframe(codegen_t* codegen);

/**
 * @brief Writes CALL to the function.
 * @param codegen Generator created by codegen_init.
 * @param name Function name from the IFJ26 source.
 */
void codegen_call(codegen_t* codegen, const char* name);

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Write the Doxygen descriptions of the new stack codegen
//          functions in this header."
// Scope: doc comments of codegen_pushs, codegen_pops and
//        codegen_stack_op only

/**
 * @brief Writes PUSHS, puts symb on the data stack.
 * @param codegen Generator created by codegen_init.
 * @param symb Variable or constant to push.
 */
void codegen_pushs(codegen_t* codegen, symb_t symb);

/**
 * @brief Writes POPS, moves the top of the data stack into var.
 * @param codegen Generator created by codegen_init.
 * @param var Target variable.
 */
void codegen_pops(codegen_t* codegen, var_t var);

/**
 * @brief Writes one stack instruction (ADDS, LTS, NOTS, ...).
 *
 * The operands are taken from the data stack and the result is pushed
 * back, so the operands must be pushed before with codegen_pushs.
 *
 * @param codegen Generator created by codegen_init.
 * @param op Instruction to write.
 */
void codegen_stack_op(codegen_t* codegen, stack_op_t op);

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Write the Doxygen descriptions of the new label and jump codegen
//          functions in this header."
// Scope: doc comments of codegen_new_label, codegen_label, codegen_jump
//        and codegen_jumpifeqs only

/**
 * @brief Gives a new label number, unique in the whole program.
 *
 * One number is used for all labels of one if or while, they differ
 * only by the suffix ($1$else, $1$end).
 *
 * @param codegen Generator created by codegen_init.
 * @return New number (1, 2, ...).
 */
int codegen_new_label(codegen_t* codegen);

/**
 * @brief Writes LABEL $num$suffix.
 * @param codegen Generator created by codegen_init.
 * @param num Number from codegen_new_label.
 * @param suffix Part of the if or while (else, end, ...).
 */
void codegen_label(codegen_t* codegen, int num, const char* suffix);

/**
 * @brief Writes JUMP to the label $num$suffix.
 * @param codegen Generator created by codegen_init.
 * @param num Number from codegen_new_label.
 * @param suffix Part of the if or while (else, end, ...).
 */
void codegen_jump(codegen_t* codegen, int num, const char* suffix);

/**
 * @brief Writes JUMPIFEQS to the label $num$suffix.
 *
 * Takes two values from the data stack and jumps if they are equal.
 * For a condition push its result and then bool@false, so the jump
 * is taken when the condition is false.
 *
 * @param codegen Generator created by codegen_init.
 * @param num Number from codegen_new_label.
 * @param suffix Part of the if or while (else, end, ...).
 */
void codegen_jumpifeqs(codegen_t* codegen, int num, const char* suffix);

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Write the Doxygen description of codegen_exit in this header."
// Scope: doc comment of codegen_exit only

/**
 * @brief Writes EXIT, the program ends with the given code.
 *
 * Used for runtime errors, for example a zero step in for (code 40).
 *
 * @param codegen Generator created by codegen_init.
 * @param code Exit code, 40 for a runtime error.
 */
void codegen_exit(codegen_t* codegen, int code);

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Write the Doxygen descriptions of the new string, conversion
//          and read codegen functions in this header."
// Prompt: "Remove the unused codegen_unary_op, unary_op_t and codegen_read
//          and their descriptions"
// Scope: doc comment of codegen_binary_op only

/**
 * @brief Writes an instruction with a variable and two symbols
 *        (CONCAT, GETCHAR, STRI2INT).
 * @param codegen Generator created by codegen_init.
 * @param op Instruction to write.
 * @param var Variable for the result.
 * @param symb1 First operand.
 * @param symb2 Second operand.
 */
void codegen_binary_op(codegen_t* codegen, binary_op_t op, var_t var, symb_t symb1, symb_t symb2);

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Write the Doxygen description of the new built-in functions
//          codegen function in this header."
// Prompt: "Add readBoolean to the description (BOOLTHEN)"
// Prompt: "Add $floor to the description (VALMEM)"
// Scope: doc comment of codegen_builtins only

/**
 * @brief Writes the code of all built-in functions (readLine, readInt,
 *        readDouble, readBoolean, toStr, toInt, strLength, subStr, ord, chr)
 *        and $floor for the VALMEM member x.floor.
 *
 * Every built-in is called like a normal function: CREATEFRAME, DEFVAR and
 * MOVE of the arguments into TF and CALL with its name. The arguments are
 * named TF@value$0, TF@start$0, TF@end$0, TF@index$0 and TF@code$0, as in
 * the IFJ26 specification. The result is left on the data stack. print is
 * not here, it is just WRITE.
 *
 * @param codegen Generator created by codegen_init.
 * @note Call it once, after codegen_program_begin, so the jump to $end
 *       goes over this code.
 */
void codegen_builtins(codegen_t* codegen);

#endif
