// Implementation of the IFJ26 imperative language compiler
// parser.h
// Author(s): Dušan Popović (xpopovd00)

/*  */

#ifndef IFJ_PARSER_H
#define IFJ_PARSER_H

#include "error.h"      //compiler exit
#include "parser_expression.h"
#include "symtable.h"
#include "types.h"

int parser_program ();

#endif
