// Implementation of the IFJ26 imperative language compiler
// codegen.h
// Author(s): Marek Zachensky (xzachem00)

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
// Scope: doc comments of codegen_defvar, codegen_move, codegen_write,
//        codegen_func_begin, codegen_func_end, codegen_createframe
//        and codegen_call only

/**
 * @brief Writes DEFVAR for the variable.
 * @param codegen Generator created by codegen_init.
 * @param var Variable to define.
 */
void codegen_defvar(codegen_t* codegen, var_t var);

/**
 * @brief Writes MOVE, copies symb into var.
 * @param codegen Generator created by codegen_init.
 * @param var Target variable.
 * @param symb Variable or constant to copy.
 */
void codegen_move(codegen_t* codegen, var_t var, symb_t symb);

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

#endif
