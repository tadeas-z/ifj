// Implementation of the IFJ26 imperative language compiler
// error.h
// Author(s): Adam Zatloukal (xzatloa01)

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Create the error enum according to the assignment rules"
// Scope: error_code_t

#ifndef ERROR_H
#define ERROR_H

// Compiler exit codes (IFJ26 specification, section 6).
// When an error fits several categories, the more specific code wins.
typedef enum {
    ERR_OK = 0,
    ERR_LEXICAL = 1, // malformed lexeme, comment or indentation
    ERR_SYNTAX = 2, // bad syntax, missing prolog, disallowed construct, broken block structure
    ERR_SEM_UNDEF = 3, // undefined function, variable or type; missing @main function
    ERR_SEM_REDEF = 4, // redefinition of function, variable or type; multiple @main functions
    ERR_SEM_CALL = 5, // wrong argument count or type in a call; @main function with params
    ERR_SEM_TYPE = 6, // type incompatibility or failed type inference
    ERR_SEM_ASSIGN = 7, // write to an immutable variable (val) or function parameter
    ERR_SEM_OTHER = 10, // other semantic errors (disallowed return, non-exhaustive match, zero range step)
    ERR_RUNTIME = 40, // not returned by the compiler, generated code exits with it via EXIT
    ERR_INTERNAL = 99 // internal compiler error (e.g. allocation failure)
} error_code_t;

#endif
