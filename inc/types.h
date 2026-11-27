// Implementation of the IFJ26 imperative language compiler
// types.h
// Author(s): Adam Zatloukal (xzatloa01)

// Basic tyoes (Int, Double, String, Bool, Unit and Any (builtin only)) exist only once
// as shared constants referenced by the AST and the symbol table.
// type_compatible() decides if a value of one type can be used where another type is expected
// (including the implicit Int -> Double conversion).

#ifndef TYPES_H_
#define TYPES_H_

#include <stdbool.h>

typedef enum {
    T_INT, // basic, without data
    T_DOUBLE,
    T_STRING,
    T_UNIT,
    T_BOOL,
    T_ANY,
    T_UNION, // composite, with data - EXTENSIONS ONLY
    T_LIST,
    T_FUNC,
    T_CLASS
} type_kind_t;

typedef struct type {
    type_kind_t kind;
    // will be added based on extensions
} type_t;

extern type_t *const TYPE_INT, *const TYPE_DOUBLE, *const TYPE_STRING,
                                                       *const TYPE_UNIT, *const TYPE_BOOL, *const TYPE_ANY;

// ========================================
//  Type checks
// ========================================

/**
 * @brief Is type a and type b the same?
 */
bool type_equal(const type_t* a, const type_t* b);

/**
 * @brief can a value of source be used where target is expected?
 * @return true when both types are the same, or if target is T_ANY or
 *         when trying to assign an int to a double
 *         else false
 */
bool type_compatible(const type_t* target, const type_t* source);

#endif
