// Implementation of the IFJ26 imperative language compiler
// dynamic_array.c
// Author(s): Adam Zatloukal (xzatloa01)

#include "dynamic_array.h"
#include <stdbool.h>
#include <stdlib.h>

bool dyn_arr_init(dyn_arr_t* ptr)
{
    if (ptr == NULL)
        return false;

    // Default values, allocate an array of pointers
    ptr->capacity = 0;
    ptr->count = 0;
    ptr->items = malloc(DEFAULT_CAPACITY * sizeof(void*));
    if (ptr->items == NULL)
        return false;

    ptr->capacity = DEFAULT_CAPACITY;
    return true;
}

bool dyn_arr_append(dyn_arr_t* ptr, void* item)
{
    if (ptr == NULL || item == NULL)
        return false;

    // Double the size when the new item cannot fit
    if (ptr->count + 1 > ptr->capacity) {
        size_t new_capacity = ptr->capacity == 0 ? DEFAULT_CAPACITY : ptr->capacity * 2;

        void* new_ptr = realloc(ptr->items, new_capacity * sizeof(void*));
        if (new_ptr == NULL)
            return false;

        ptr->capacity = new_capacity;
        ptr->items = new_ptr;
    }

    // Append at count then increment
    ptr->items[ptr->count++] = item;
    return true;
}

void* dyn_arr_get(const dyn_arr_t* ptr, size_t i)
{
    if (ptr == NULL || i >= ptr->count)
        return NULL;
    return ptr->items[i];
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "implement dyn_arr_free"
// Scope: dyn_arr_free only

void dyn_arr_free(dyn_arr_t* ptr, void (*free_item)(void*))
{
    if (ptr == NULL)
        return;

    // Free the items first if the array owns them
    if (free_item != NULL) {
        for (size_t i = 0; i < ptr->count; i++)
            free_item(ptr->items[i]);
    }

    // Free the array of pointers and leave the struct empty
    free(ptr->items);
    ptr->items = NULL;
    ptr->count = 0;
    ptr->capacity = 0;
}
