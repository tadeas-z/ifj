// Implementation of the IFJ26 imperative language compiler
// types.c
// Author(s): Adam Zatloukal (xzatloa01)

#include "types.h"
#include <stdbool.h>
#include <stdio.h>

static type_t INT_INST = { .kind = T_INT };
static type_t DOUBLE_INST = { .kind = T_DOUBLE };
static type_t STRING_INST = { .kind = T_STRING };
static type_t UNIT_INST = { .kind = T_UNIT };
static type_t BOOL_INST = { .kind = T_BOOL };
static type_t ANY_INST = { .kind = T_ANY };

/* CAN be accessed outside of this file */
type_t* const TYPE_INT = &INT_INST;
type_t* const TYPE_DOUBLE = &DOUBLE_INST;
type_t* const TYPE_STRING = &STRING_INST;
type_t* const TYPE_UNIT = &UNIT_INST;
type_t* const TYPE_BOOL = &BOOL_INST;
type_t* const TYPE_ANY = &ANY_INST;

// ========================================
//  Type checks
// ========================================

bool type_equal(const type_t* a, const type_t* b)
{
    if (a == NULL || b == NULL)
        return false;

    return a->kind == b->kind; // Might need to be expanded as type_t struct grows with extensions
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Implement the type_compatible function. return true only when:
//          type is T_ANY OR both types match OR we are assigning T_INT to
//          T_DOUBLE. Also count in any other edge cases"

bool type_compatible(const type_t* target, const type_t* source)
{
    if (target == NULL || source == NULL)
        return false;
    if (target->kind == T_ANY)
        return true;
    if (type_equal(target, source))
        return true;
    if (target->kind == T_DOUBLE && source->kind == T_INT)
        return true;
    return false;
}
