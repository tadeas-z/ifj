
// Implementation of the IFJ26 imperative language compiler
// utils.h
// Author(s): Adam Zatloukal (xzatloa01)

#ifndef UTILS_H_
#define UTILS_H_

/**
 * @brief allocates memory for str and returns the copy (which lives on the heap)
 * @return copy of str that lives on the heap, returns NULL when memory allocation fails
 */
char* copy_string(const char* str);

#endif
