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

static error_code_t check_function(sema_ctx_t* semantic_ctx, ast_node_t* ast_func_def);
static error_code_t check_block(sema_ctx_t* semantic_ctx, ast_node_t* ast_block);
//static error_code_t check_statement(sema_ctx_t* semantic_ctx, ast_node_t* ast_statement);
//static error_code_t check_expression(sema_ctx_t* semantic_ctx, ast_node_t* ast_expression);
//static error_code_t check_call(sema_ctx_t* semantic_ctx, ast_node_t* ast_call);

// ----------------------------------------
//  Helpers
// ----------------------------------------

/**
 *  @brief Helper used to convert between error codes
 *  @note st_result_t (symbol table error codes) -> error_code_t (global compiler error codes)
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
 * @brief Seatches for a variable symbol in the symtable
 * @returns the symbol when the variable belongs to the current function
 *  else returns NULL.
 * @note it is the callers job to propagate this error as ERR_SEM_UNDEF
 */
static symbol_t* lookup_var_in_function(sema_ctx_t* semantic_ctx, const char* name)
{
    int found_depth;

    symbol_t* var_symbol = symtable_lookup_var_ex(semantic_ctx->symbol_table, name, &found_depth);

    // Not found
    if (var_symbol == NULL)
        return NULL; // Its the callers job to set ERR_SEM_UNDEF from the NULL

    // Found but it belongs to another function
    if (found_depth < semantic_ctx->base_depth)
        return NULL;

    return var_symbol;
}

// ----------------------------------------
//  PASS 0 (builtin functions) & PASS 1 (insert functions into symtable)
// ----------------------------------------

/**
 * @brief Inserts all the builtin functions into the symbol table
 *  and set all the symbol values like return type and params.
 * @returns st_result_t and needs to be converted upstream to error_code_t using
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


/**
 * @brief Goes over every function in the program and inserts it into the symbol table
 * @note  Also fills the meta data of the symbol like:
 *  decl - ptr to ast_node from whom this symbol was created
 *  is_main - read from ast
 *  return_type - read from ast
 *  ret_inferred - true when return_type == NULL. This means PASS2 has to set the return type
 *  based on the type of the functions body
 *  params - copies param info from ast into symtable
 * @note Also handles @main checks (check the bottom of the function)
 *
 */
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

// ----------------------------------------
//  PASS 2 - type and other rules checks
// ----------------------------------------

/**
 * @brief Loops over every function in program->func_list
 * @note Only checks the function if it hasnt been checked already.
 *  Checking for example the main function can trigger a check on another function
 *  the check here prevents checking the latter function twice.
 * @note check_function_bodies -> check_function -> check_block -> ...
 */
static error_code_t check_function_bodies(sema_ctx_t* semantic_ctx)
{
    if (semantic_ctx == NULL)
        return ERR_INTERNAL;

    // Helpers
    ast_program_t* program = &semantic_ctx->program->program;
    symtable_t* symbol_table = semantic_ctx->symbol_table;
    error_code_t err_code;

    // Loop over func_list
    for (size_t func_i = 0; func_i < program->func_list.count; func_i++) {
        // Get func from AST
        ast_node_t* ast_func_def = dyn_arr_get(&program->func_list, func_i);
        if (ast_func_def == NULL)
            return ERR_INTERNAL;

        // Find the function in the symbol table
        symbol_t* func_sym = symtable_lookup_func(symbol_table, ast_func_def->func_def.name);
        if (func_sym == NULL)
            return ERR_INTERNAL;

        // Check the body only if it wasnt checked already - because checking one function can mean having to check multiple others
        if (func_sym->sema_marker == SEMA_UNCHECKED) {
            err_code = check_function(semantic_ctx, ast_func_def);
            if (err_code != ERR_OK)
                return err_code;
        }
    }
    return ERR_OK;
}

/**
 * @brief PUSHES symtable SCOPE for params, INSERTS them there, SETS
 *  unique ID in the AST based on symtable id, CHECKS functions BODY
 *  and SETS return_type and does SEMANTIC CHECKS ON RETURN_TYPE
 * @note also sets sema_marker in the symbol, CONTEXT is updated internally but UPON RETURN
 *  IT CONTAINS THE SAME CONTEXT AS WHEN THE FUNCTION WAS CALLED
 * @return CAN RETURN every erro from error_code_t - check implementation
 */
static error_code_t check_function(sema_ctx_t* semantic_ctx, ast_node_t* ast_func_def)
{
    if (semantic_ctx == NULL || ast_func_def == NULL)
        return ERR_INTERNAL;

    // Helper variables
    symtable_t* symbol_table = semantic_ctx->symbol_table;
    ast_func_def_t* ast_func_def_data = &ast_func_def->func_def;
    error_code_t err_code;

    // Find the symbol
    symbol_t* func_sym = symtable_lookup_func(symbol_table, ast_func_def_data->name);
    if (func_sym == NULL)
        return ERR_INTERNAL;

    // When THIS function is called from within ITS body - we know the type isnt known yet
    func_sym->sema_marker = SEMA_IN_PROGRESS;

    // Save callers state and update context
    sema_ctx_t callers_state = {
        .current_func = semantic_ctx->current_func, // When called by check_function_bodies - NULL. When called by check_call - the func being checked
        .base_depth = semantic_ctx->base_depth // When called by check_function_bodies - 0. When called by check_call - depth of the function being checked
    };

    semantic_ctx->current_func = ast_func_def;

    // Param scope - params and local variables live in different scopes
    // This simulates real Scala behaviour
    err_code = convert_st_error_code(
        symtable_push_scope(symbol_table));
    if (err_code != ERR_OK)
        return err_code;

    // This boundry is used by lookup_var_in_function - if var name is found
    // below this scope it is NOT defined in THIS function
    semantic_ctx->base_depth = symtable_depth(symbol_table);

    // Insert params to the currrent scope
    dyn_arr_t* params = &ast_func_def_data->params;

    for (size_t param_i = 0; param_i < params->count; param_i++) {
        ast_param_t* param = dyn_arr_get(params, param_i);
        symbol_t* param_sym;

        // Symbol table insertion
        err_code = convert_st_error_code(
            symtable_insert_var(
                symbol_table, param->name, param->type, true, &param_sym));
        if (err_code != ERR_OK)
            return err_code;

        param->id = param_sym->id; // Copy unique id to the AST
    }

    // Check the function body
    err_code = check_block(semantic_ctx, ast_func_def_data->body);
    if (err_code != ERR_OK)
        return err_code;

    // Set the inferred return type - can be set because check_body set the return type of the body
    if (func_sym->ret_inferred == true) {       // ret_inferred is set by the parser in pass 1 when the return type of the function is NULL
        ast_func_def_data->return_type = ast_func_def_data->body->type;
        func_sym->type = ast_func_def_data->body->type;
    }

    // Functions cant return booleans
    if (ast_func_def_data->return_type->kind == T_BOOL)
        return ERR_SEM_TYPE;

    // TODO explicit type -> return rules

    // @main has to return Unit
    if (ast_func_def_data->is_main && ast_func_def_data->return_type->kind != T_UNIT)
        return ERR_SEM_TYPE;

    // Cleanup
    symtable_pop_scope(symbol_table);
    semantic_ctx->current_func = callers_state.current_func;
    semantic_ctx->base_depth = callers_state.base_depth;
    func_sym->sema_marker = SEMA_DONE;

    return ERR_OK;
}

static error_code_t check_block(sema_ctx_t* semantic_ctx, ast_node_t* ast_block) {
    if (semantic_ctx == NULL || ast_block == NULL)
        return ERR_INTERNAL;

    lookup_var_in_function(semantic_ctx, "name");

    return ERR_OK;
}

// ----------------------------------------
//  Top level semantics function
// ----------------------------------------

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

    err_code = check_function_bodies(&semantic_ctx);
    if (err_code != ERR_OK) {
        symtable_free(semantic_ctx.symbol_table);
        return err_code;
    }

    symtable_free(semantic_ctx.symbol_table);

    return ERR_OK;
}
