// Implementation of the IFJ26 imperative language compiler
// codegen.h
// Author(s): Marek Zachensky (xzachem00)

#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>

typedef struct codegen codegen_t;

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

#endif
