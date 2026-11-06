#include "semantic.h"
#include "comp_error.h"
#include "dynamic_array.h"
#include "symtable.h"

typedef struct {
    const char* name;
    type_t* ret_type;
    size_t param_cnt;
    type_t* param_types[3]; // subStr has 3 params, nothing has more
    const char* param_names[3]; // Not really needed but useful for cleaner debug
} builtin_desc_t;

/**
 *  Helper used to convert between error codes
 *  st_result_t (symbol table error codes) -> error_code_t (global compiler error codes)
 */
static error_code_t convert_st_error_code(st_result_t st_error_code)
{
    switch (st_error_code) {
    // Successful insertion
    case ST_OK:
        return ERR_OK;

    // Redefinition
    case ST_EXISTS:
        return ERR_SEM_REDEF;

    // Memory allocation failure - internal error
    case ST_NOMEM:
        return ERR_INTERNAL;

    // Invalid param was passed
    case ST_INVALID:
        return ERR_INTERNAL;

    default:
        return ERR_INTERNAL;
    }
}

/**
 * Inserts all the builtin functions into the symbol table
 * and set all the symbol values.
 * Returs st_result_t and needs to be converted upstream to error_code_t using
 * convert_st_error_code
 */
static st_result_t insert_builtin_functions(symtable_t* symtable)
{
    symbol_t* func_sym;
    st_result_t err_code;

    // Built in function - see section 4.0 of the specs
    const builtin_desc_t BUILTINS[] = {
        { "print", TYPE_UNIT, 1, { TYPE_ANY }, { "value" } },
        { "readLine", TYPE_STRING, 0, { 0 }, { NULL } },
        { "readInt", TYPE_INT, 0, { 0 }, { NULL } },
        { "readDouble", TYPE_DOUBLE, 0, { 0 }, { NULL } },
        { "toStr", TYPE_STRING, 1, { TYPE_ANY }, { "value" } },
        { "toInt", TYPE_INT, 1, { TYPE_DOUBLE }, { "value" } },
        { "strLength", TYPE_INT, 1, { TYPE_STRING }, { "value" } },
        { "subStr", TYPE_STRING, 3, { TYPE_STRING, TYPE_INT, TYPE_INT }, { "value", "start", "end" } },
        { "ord", TYPE_INT, 2, { TYPE_STRING, TYPE_INT }, { "value", "index" } },
        { "chr", TYPE_STRING, 1, { TYPE_INT }, { "code" } },
    };

    const size_t builtin_cnt = sizeof(BUILTINS) / sizeof(BUILTINS[0]);

    for (size_t func_i = 0; func_i < builtin_cnt; func_i++) {
        const builtin_desc_t* func_desc = &BUILTINS[func_i];

        err_code = symtable_insert_func(symtable, func_desc->name, &func_sym);

        if (err_code != ST_OK)
            return err_code;

        // Set symbol properties
        func_sym->type = func_desc->ret_type;
        func_sym->builtin = true;
        func_sym->is_main = false;
        func_sym->sema_marker = SEMA_DONE;

        // Sets symbol properties - param_names, param_types, param_count
        for (size_t param_i = 0; param_i < func_desc->param_cnt; param_i++) {
            err_code = symtable_func_add_param(func_sym, func_desc->param_names[param_i], func_desc->param_types[param_i]);

            if (err_code != ST_OK)
                return err_code;
        }
    }
    return ST_OK;
}

static error_code_t collect_function_signatures(sema_ctx_t* semantic_ctx)
{
    // Validate params
    if (semantic_ctx == NULL || semantic_ctx->program == NULL)
        return ERR_INTERNAL;

    // Helper variables
    symtable_t* symtable = semantic_ctx->symbol_table;
    symbol_t* func_sym;
    error_code_t err_code;

    // Get function list handle
    ast_node_t* program = semantic_ctx->program;
    dyn_arr_t* function_list = &program->program.func_list;

    // Loop over every function in the program
    for (size_t func_i = 0; func_i < function_list->count; func_i++) {
        ast_node_t* func_node = dyn_arr_get(function_list, func_i);
        if (func_node == NULL)
            return ERR_INTERNAL;

        ast_func_def_t* func_def = &func_node->func_def;

        // Insert the function into the symbol table
        err_code = convert_st_error_code(
            symtable_insert_func(symtable, func_def->name, &func_sym));
        if (err_code != ERR_OK)
            return err_code;

        // Fill the symbol (symtable value = ast node value)
        func_sym->decl = func_node;
        func_sym->is_main = func_def->is_main;
        func_sym->type = func_def->return_type;
        func_sym->ret_inferred = (func_def->return_type == NULL);

        // Function params
        dyn_arr_t* ast_params = &func_def->params;
        ast_param_t* ast_param;

        // Go over every param in the AST, get it and add it to the symtable
        for (size_t param_i = 0; param_i < ast_params->count; param_i++) {
            ast_param = dyn_arr_get(ast_params, param_i);
            if (ast_param == NULL)
                return ERR_INTERNAL;

            err_code = convert_st_error_code(
                symtable_func_add_param(func_sym, ast_param->name, ast_param->type));
            if (err_code != ERR_OK)
                return err_code;
        }

        // Check for @main
        if (func_def->is_main == true) {
            // There is already a main function
            if (semantic_ctx->main_func != NULL)
                return ERR_SEM_REDEF;

            // This is the first main function
            semantic_ctx->main_func = func_node;

            // Main function cant have params
            if (ast_params->count > 0)
                return ERR_SEM_CALL;

            // @main must return Unit
            if (func_def->return_type != NULL && func_def->return_type->kind != T_UNIT)
                return ERR_SEM_TYPE;
        }
    }

    // No @main exists
    if (semantic_ctx->main_func == NULL)
        return ERR_SEM_UNDEF;

    return ERR_OK;
}

error_code_t semantic_analyze(ast_node_t* program)
{
    // Check input param validity
    if (program == NULL || program->kind != AST_PROGRAM)
        return ERR_INTERNAL;

    // Context struct for
    sema_ctx_t semantic_ctx = { .program = program };

    // -----------------------------------------------------------
    //  PASS 0: insert builtin functions to symtable->global
    // -----------------------------------------------------------
    semantic_ctx.symbol_table = symtable_new();
    if (semantic_ctx.symbol_table == NULL)
        return ERR_INTERNAL;

    // Insert builtin functions into the symbol table & check error code
    st_result_t st_err_code = insert_builtin_functions(semantic_ctx.symbol_table);

    error_code_t err_code = convert_st_error_code(st_err_code);
    if (err_code != ERR_OK) {
        symtable_free(semantic_ctx.symbol_table);
        return err_code;
    }

    // -----------------------------------------------------------
    //  PASS 1: insert user functions & check @main semantics
    // -----------------------------------------------------------
    err_code = collect_function_signatures(&semantic_ctx);
    if (err_code != ERR_OK) {
        symtable_free(semantic_ctx.symbol_table);
        return err_code;
    }

    // -----------------------------------------------------------
    //  PASS 2
    // -----------------------------------------------------------

    symtable_free(semantic_ctx.symbol_table);

    return ERR_OK;
}
