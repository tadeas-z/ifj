// Implementation of the IFJ26 imperative language compiler
// utils.c
// Author(s): Adam Zatloukal (xzatloa01)

#include "utils.h"
#include <stdlib.h>
#include <string.h>

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Implement the copy_string helper"
// Scope: copy_string only

char* copy_string(const char* str)
{
    size_t size = strlen(str) + 1;
    char* copy = malloc(size);
    if (copy == NULL)
        return NULL;
    memcpy(copy, str, size);
    return copy;
}
