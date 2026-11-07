#include "semantic.h"
#include "comp_error.h"
#include "dynamic_array.h"
#include "symtable.h"

#define TRY(call)                       \
    do {                                \
        error_code_t try_err_ = (call); \
        if (try_err_ != ERR_OK)         \
            return try_err_;            \
    } while (0)

#define TRY_ST(call) TRY(convert_st_error_code(call))

typedef struct {
    const char* name;
    type_t* ret_type;
    size_t param_cnt;
    type_t* param_types[3]; // subStr has 3 params, nothing has more
    const char* param_names[3]; // Not really needed but useful for cleaner debug
} builtin_desc_t;

static error_code_t check_function(sema_ctx_t* semantic_ctx, ast_node_t* ast_func_def);
static error_code_t check_block(sema_ctx_t* semantic_ctx, ast_node_t* ast_block);
static error_code_t check_statement(sema_ctx_t* semantic_ctx, ast_node_t* ast_statement);
static error_code_t check_expression(sema_ctx_t* semantic_ctx, ast_node_t* ast_expression);
// static error_code_t check_call(sema_ctx_t* semantic_ctx, ast_node_t* ast_call);

static error_code_t check_var_def(sema_ctx_t *semantic_ctx, ast_node_t* ast_statement);
static error_code_t check_assign(sema_ctx_t *semantic_ctx, ast_node_t* ast_statement);
static error_code_t check_return(sema_ctx_t *semantic_ctx, ast_node_t* ast_statement);
static error_code_t check_if(sema_ctx_t *semantic_ctx, ast_node_t* ast_statement);
static error_code_t check_while(sema_ctx_t *semantic_ctx, ast_node_t* ast_statement);

// ========================================
//  Helpers
// ========================================

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

// returns the last statement of a AST_BLOCK
static ast_node_t* last_statement(ast_node_t* body) {
    if (body == NULL)
        return NULL;

    dyn_arr_t *block_statements = &body->block.statements;

    if (block_statements->count > 0)
        return dyn_arr_get(block_statements, block_statements->count - 1);

    return NULL;
}

// True when the last statemen of a block is return, else false
static bool block_ends_with_return(ast_node_t* block) {
    ast_node_t* last_stmt = last_statement(block);
    if (last_stmt == NULL)
        return false;

    if (last_stmt->kind == AST_RETURN)
        return true;

    if (last_stmt->kind == AST_IF)
        return block_ends_with_return(last_stmt->if_stmt.then_block) &&
        block_ends_with_return(last_stmt->if_stmt.else_block);

    return false;
}

static bool block_ends_with_unit(ast_node_t* block) {
    ast_node_t* last_stmt = last_statement(block);

    // Empty block -> Unit
    if (last_stmt == NULL)
        return true;

    if (last_stmt->kind == AST_IF)
        return block_ends_with_unit(last_stmt->if_stmt.then_block) &&
        block_ends_with_unit(last_stmt->if_stmt.else_block);

    // Otherwise last_stmp->type is Unit
    return last_stmt->type->kind == T_UNIT;
}

// ========================================
//  PASS 0 (builtin functions) & PASS 1 (insert functions into symtable)
// ========================================

/**
 * @brief Inserts all the builtin functions into the symbol table
 *  and set all the symbol values like return type and params.
 * @returns st_result_t and needs to be converted upstream to error_code_t using
 * convert_st_error_code
 */
static error_code_t insert_builtin_functions(symtable_t* symtable)
{
    symbol_t* func_sym;

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
        {"readBoolean", TYPE_BOOL, 0, {0}, {NULL}}
    };

    const size_t builtin_cnt = sizeof(BUILTINS) / sizeof(BUILTINS[0]);

    for (size_t func_i = 0; func_i < builtin_cnt; func_i++) {
        const builtin_desc_t* func_desc = &BUILTINS[func_i];

        TRY_ST(symtable_insert_func(symtable, func_desc->name, &func_sym));

        // Set symbol properties
        func_sym->type = func_desc->ret_type;
        func_sym->builtin = true;
        func_sym->is_main = false;
        func_sym->sema_marker = SEMA_DONE;

        // Sets symbol properties - param_names, param_types, param_count
        for (size_t param_i = 0; param_i < func_desc->param_cnt; param_i++)
            TRY_ST(symtable_func_add_param(func_sym, func_desc->param_names[param_i], func_desc->param_types[param_i]));
    }
    return ERR_OK;
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

    // Get function list handle
    ast_node_t* program = semantic_ctx->program;
    dyn_arr_t* function_list = &program->program.func_list;

    // Loop over every function in the program
    for (size_t func_i = 0; func_i < function_list->count; func_i++) {
        ast_node_t* func_node = dyn_arr_get(function_list, func_i);

        ast_func_def_t* func_def = &func_node->func_def;

        // Insert the function into the symbol table
        TRY_ST(symtable_insert_func(symtable, func_def->name, &func_sym));

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

            TRY_ST(symtable_func_add_param(func_sym, ast_param->name, ast_param->type));
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

// ========================================
//  PASS 2 - type and other rules checks
// ========================================

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

    // Loop over func_list
    for (size_t func_i = 0; func_i < program->func_list.count; func_i++) {
        // Get func from AST
        ast_node_t* ast_func_def = dyn_arr_get(&program->func_list, func_i);

        // Find the function in the symbol table
        symbol_t* func_sym = symtable_lookup_func(symbol_table, ast_func_def->func_def.name);
        if (func_sym == NULL)
            return ERR_INTERNAL;

        // Check the body only if it wasnt checked already - because checking one function can mean having to check multiple others
        if (func_sym->sema_marker == SEMA_UNCHECKED) {
            TRY(check_function(semantic_ctx, ast_func_def));
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
    TRY_ST(symtable_push_scope(symbol_table));

    // This boundry is used by lookup_var_in_function - if var name is found
    // below this scope it is NOT defined in THIS function
    semantic_ctx->base_depth = symtable_depth(symbol_table);

    // Insert params to the currrent scope
    dyn_arr_t* params = &ast_func_def_data->params;

    for (size_t param_i = 0; param_i < params->count; param_i++) {
        ast_param_t* param = dyn_arr_get(params, param_i);
        symbol_t* param_sym;

        // Symbol table insertion
        TRY_ST(symtable_insert_var(symbol_table, param->name, param->type, true, &param_sym));

        param->id = param_sym->id; // Copy unique id to the AST
    }

    // Check the function body
    TRY(check_block(semantic_ctx, ast_func_def_data->body));

    // Section 2.3 (rule 3)
    // check_statement always sets an if to unit, here we check whats inside each branch
    ast_node_t* last_stmt = last_statement(ast_func_def_data->body);
    if (last_stmt != NULL) {
        if (last_stmt->kind == AST_IF) {

            // If function return type is explicitly Unit it doesnt matter what each branch returns
            // because it is discarded and the function returns Unit
            if (!func_sym->ret_inferred && ast_func_def_data->return_type->kind == T_UNIT)
                ;

            // Each path ends with return and
            // check_return already checked if each branches return type is compatible with the functions return type
            else if (block_ends_with_return(ast_func_def_data->body))
                ast_func_def_data->body->type = ast_func_def_data->return_type;

            else if (func_sym->ret_inferred && block_ends_with_unit(ast_func_def_data->body))
                ast_func_def_data->body->type = TYPE_UNIT;
            else
                return ERR_SEM_TYPE;
        }
    }

    // Set the inferred return type - can be set because check_body set the return type of the body
    if (func_sym->ret_inferred == true) { // ret_inferred is set by the parser in pass 1 when the return type of the function is NULL
        ast_func_def_data->return_type = ast_func_def_data->body->type;
        func_sym->type = ast_func_def_data->body->type;
    }

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

static error_code_t check_block(sema_ctx_t* semantic_ctx, ast_node_t* ast_block)
{
    if (semantic_ctx == NULL || ast_block == NULL)
        return ERR_INTERNAL;

    // Helper variables
    dyn_arr_t* block_statements = &ast_block->block.statements;

    // Push a scope for the current block
    TRY_ST(symtable_push_scope(semantic_ctx->symbol_table));

    // Loop over the statements in the block
    for (size_t statement_i = 0; statement_i < block_statements->count; statement_i++) {
        ast_node_t* statement = dyn_arr_get(block_statements, statement_i);

        // Do semantic checks on every statement
        TRY(check_statement(semantic_ctx, statement));
    }

    // Get the last statement and set the return type of the block to the type of the last statement
    ast_node_t *last_stmt = last_statement(block_statements);
    if (last_stmt == NULL)
        ast_block->type = TYPE_UNIT;
    else
        ast_block->type = last_stmt->type;


    // Pop the scope - the block is already analyzed
    TRY_ST(symtable_pop_scope(semantic_ctx->symbol_table));

    return ERR_OK;
}

// ========================================
//  Statements
// ========================================

static error_code_t check_statement(sema_ctx_t* semantic_ctx, ast_node_t* ast_statement)
{
    if (semantic_ctx == NULL || ast_statement == NULL)
        return ERR_INTERNAL;

    switch (ast_statement->kind) {
    case AST_VAR_DEF:
        TRY(check_var_def(semantic_ctx, ast_statement));
        ast_statement->type = TYPE_UNIT;        // What is the type if this stmt is the last
        break;

    case AST_ASSIGN:
        TRY(check_assign(semantic_ctx, ast_statement));
        ast_statement->type = TYPE_UNIT;
        break;

    case AST_RETURN:
        TRY(check_return(semantic_ctx, ast_statement));
        ast_statement->type = semantic_ctx->current_func->func_def.return_type;
        break;

    case AST_IF:
        TRY(check_if(semantic_ctx, ast_statement));
        ast_statement->type = TYPE_UNIT;
        break;

    case AST_WHILE:
        TRY(check_while(semantic_ctx, ast_statement));
        ast_statement->type = TYPE_UNIT;
        break;

    case AST_EXPR_STMT:
        TRY(check_expression(semantic_ctx, ast_statement->expr_stmt.expr));
        ast_statement->type = ast_statement->expr_stmt.expr->type;
        break;

    default:
        return ERR_INTERNAL;
    }

    return ERR_OK;
}

static error_code_t check_var_def(sema_ctx_t *semantic_ctx, ast_node_t* ast_statement) {
    ast_var_def_t *ast_var_def = &ast_statement->var_def;

    // val x = x + 1
    // check_expression looks up an x that already exists and uses its symbol, then we return and insert the new x into the symtable
    // if we were to insert the new x first, then call check_expression the lookup there would find the new x
    // For correct shadowing we use the first option
    TRY(check_expression(semantic_ctx ,ast_var_def->init)); // ast_var_def->init->type is now known

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
    error_code_t err_code = insert_builtin_functions(semantic_ctx.symbol_table);

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
