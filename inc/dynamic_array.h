// Implementation of the IFJ26 imperative language compiler
// dynamic_array.h
// Author(s): Adam Zatloukal (xzatloa01)

// items ──► [ ptr0 | ptr1 | ptr2 | ... ]   (capacity slots, count used)
//              │      │      │
//              ▼      ▼      ▼
//           node A  node B  node C        (allocated separately by the caller)


#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H

#include <stddef.h>
#include <stdbool.h>

#define DEFAULT_CAPACITY 32

typedef struct {
    void** items;
    size_t count, capacity;
} dyn_arr_t;

/**
 * @brief Initiates the dynamic array with defualt values, allocates memory
 * @param ptr pointer to an dyn_arr_t
 * @return false on invalid ptr or memory allocation failure
 *         else true
 */
bool  dyn_arr_init(dyn_arr_t* ptr);

/**
 * @brief appends item to the end of the dynamic array
 * @param ptr dynamic array handle
 * @param item pointer to the element you want to append
 * @return flase on invalid params, memory allocation failure
 *          else true
 */
bool  dyn_arr_append(dyn_arr_t* ptr, void* item);

/**
 * @brief returns a pointer to an element inside of the dynamic array
 * @param ptr dynamic array handle
 * @param i access index (starts with 0)
 * @return element pointer
 */
void* dyn_arr_get(const dyn_arr_t* ptr, size_t i);

/**
 * @brief free the entire dynamic array
 * @param ptr dynamic array handle
 * @paragraph free_item function pointer - this function will be used to free the
 *      elements inside the array. For example if the array stores int pointers normal free is passed
 *      if the array stores you own complex data type pointers pass the function that frees said datatype
 */
void  dyn_arr_free(dyn_arr_t* ptr, void (*free_item)(void*));

#endif
