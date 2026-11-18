// Implementation of the IFJ26 imperative language compiler
// semantic.c
// Author(s): Adam Zatloukal (xzatloa01)

// call semantic_analyze to perform smenatics analysis.
// Pass 0 adds every builtin function into the symbol table,
// Pass 1 goes through every function and adds it into the symbol table.
// Pass2 goes through the body of each function and performs analysis -
// different rule checks, type checks, type conversions, type compatibility,
// type inferrence + adds useful information into the ast for codegen.

#include "semantic.h"
#include "comp_error.h"
#include "dynamic_array.h"
#include "symtable.h"
#include <string.h>

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
static error_code_t check_call(sema_ctx_t* semantic_ctx, ast_node_t* ast_call);

static error_code_t check_var_def(sema_ctx_t* semantic_ctx, ast_node_t* ast_statement);
static error_code_t check_assign(sema_ctx_t* semantic_ctx, ast_node_t* ast_statement);
static error_code_t check_return(sema_ctx_t* semantic_ctx, ast_node_t* ast_statement);
static error_code_t check_if(sema_ctx_t* semantic_ctx, ast_node_t* ast_statement);
static error_code_t check_while(sema_ctx_t* semantic_ctx, ast_node_t* ast_statement);
static error_code_t check_for(sema_ctx_t* semantic_ctx, ast_node_t* ast_statement);

static error_code_t check_unary(sema_ctx_t* semantic_ctx, ast_node_t* ast_expression);
static error_code_t check_binary(sema_ctx_t* semantic_ctx, ast_node_t* ast_expression);

static error_code_t check_member(sema_ctx_t* semantic_ctx, ast_node_t* ast_member);

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
static ast_node_t* last_statement(ast_node_t* body)
{
    if (body == NULL)
        return NULL;

    dyn_arr_t* block_statements = &body->block.statements;

    if (block_statements->count > 0)
        return dyn_arr_get(block_statements, block_statements->count - 1);

    return NULL;
}

// True when the last statemen of a block is return, else false
static bool block_ends_with_return(ast_node_t* block)
{
    ast_node_t* last_stmt = last_statement(block);
    if (last_stmt == NULL)
        return false;

    if (last_stmt->kind == AST_RETURN)
        return true;

    if (last_stmt->kind == AST_IF)
        return block_ends_with_return(last_stmt->if_stmt.then_block) && block_ends_with_return(last_stmt->if_stmt.else_block);

    return false;
}

static bool block_ends_with_unit(ast_node_t* block)
{
    ast_node_t* last_stmt = last_statement(block);

    // Empty block -> Unit
    if (last_stmt == NULL)
        return true;

    if (last_stmt->kind == AST_IF)
        return block_ends_with_unit(last_stmt->if_stmt.then_block) && block_ends_with_unit(last_stmt->if_stmt.else_block);

    // Otherwise last_stmp->type is Unit
    return last_stmt->type->kind == T_UNIT;
}

/**
 * @brief Checks type compatibility + Implicit conversion: Int -> Double
 * @param slot *slot is the ptr to the expression node itslef (type is already checked)
 * @param target type thats expected at this place
 * @param mismatch_err what to return if types dont fit
 */
static error_code_t coerce(ast_node_t** slot, type_t* target, error_code_t mismatch_err)
{
    // Get the source type (type of the expression)
    type_t* source = (*slot)->type;
    if (source == NULL) // If its NULL semantics forgot to set a type -> ERR_INTERNAL
        return ERR_INTERNAL;

    // If the 2 types dont fit return mismatch
    if (!type_compatible(target, source))
        return mismatch_err;

    // Convert Int -> Double
    if (source->kind == T_INT && target->kind == T_DOUBLE) {
        ast_node_t* ast_convert_node = ast_new_convert(*slot); // CONVERT --> INT_LIT 0 (copies the pointer into .operand)
        if (ast_convert_node == NULL)
            return ERR_INTERNAL;

        // Make the pointer that pointed to the expression node point to the CONVERSION node
        // var_def->init --> CONVERT --> INT_LIT 0
        (*slot) = ast_convert_node;
    }

    return ERR_OK;
}

static error_code_t coerce_block_result(ast_node_t* block, type_t* target)
{
    ast_node_t* last_stmt = last_statement(block);

    // Empty block returns Unit
    if (last_stmt == NULL)
        return target->kind == T_UNIT ? ERR_OK : ERR_SEM_TYPE;

    switch (last_stmt->kind) {
    case AST_EXPR_STMT:
        TRY(coerce(&last_stmt->expr_stmt.expr, target, ERR_SEM_TYPE));
        last_stmt->type = target;
        block->type = target;
        break;

    case AST_IF:
        TRY(coerce_block_result(last_stmt->if_stmt.then_block, target));
        TRY(coerce_block_result(last_stmt->if_stmt.else_block, target));
        // Both branches now produce target
        last_stmt->type = target;
        block->type = target;
        break;

    // Branch is skipped - check_return already checked the types
    case AST_RETURN:
        return ERR_OK;

    // var def, assign, while, for
    default:
        if (!type_equal(last_stmt->type, target))
            return ERR_SEM_TYPE;
        break;
    }
    return ERR_OK;
}

static bool is_numeric(type_t* ast_operand_type)
{
    if (ast_operand_type == NULL)
        return false;

    if (ast_operand_type->kind == T_INT || ast_operand_type->kind == T_DOUBLE)
        return true;

    return false;
}

static error_code_t unify_numeric(ast_node_t* ast_binary)
{
    type_t* left_type = ast_binary->binary.left->type;
    type_t* right_type = ast_binary->binary.right->type;

    if (left_type->kind == T_DOUBLE || right_type->kind == T_DOUBLE) {
        TRY(coerce(&ast_binary->binary.left, TYPE_DOUBLE, ERR_SEM_TYPE));
        TRY(coerce(&ast_binary->binary.right, TYPE_DOUBLE, ERR_SEM_TYPE));
    }
    return ERR_OK;
}

// Checks if provided literal is 0 or -0
static bool is_zero_literal(ast_node_t* node)
{
    if (node->kind == AST_INT_LIT && node->int_lit.value == 0)
        return true;

    else if (node->kind == AST_UNARY && node->unary.op == AST_OP_NEG)
        return is_zero_literal(node->unary.operand);

    else
        return false;
}

// Needed for recursion
static type_t* branch_type(ast_node_t* block);

static type_t* if_value_type(ast_node_t* if_node)
{
    // else is mandatory if the if is an expression (CONTROL)
    if (if_node->if_stmt.else_block == NULL)
        return NULL;

    // Get the result type of each branch
    // When both branches contain return nothing is set as the result
    bool then_skip = block_ends_with_return(if_node->if_stmt.then_block);
    bool else_skip = block_ends_with_return(if_node->if_stmt.else_block);

    // Both branches are skipped -> if never produces a value
    if (then_skip && else_skip)
        return NULL;

    // Get the type of each branch that isnt skipped
    type_t* then_type = NULL;
    type_t* else_type = NULL;

    if (then_skip == false)
        // Indirectly recursive
        then_type = branch_type(if_node->if_stmt.then_block);

    if (else_skip == false)
        else_type = branch_type(if_node->if_stmt.else_block);

    // NULL from recursion (branch_type) ->
    // a branch has no value (missing else, mismatched types or all paths return)
    if ((!then_skip && then_type == NULL) || (!else_skip && else_type == NULL))
        return NULL;

    // Return the type based on what branch was skipped (if any)
    if (then_skip)
        return else_type;
    else if (else_skip)
        return then_type;
    else if (type_equal(else_type, then_type))
        return then_type;
    // Both types are numeric but atleast one isnt INT -> must be DOUBLE
    else if (is_numeric(then_type) && is_numeric(else_type))
        return TYPE_DOUBLE;
    // String+Int, ...
    else
        return NULL;
}

static type_t* branch_type(ast_node_t* block)
{
    ast_node_t* last_stmt = last_statement(block);

    if (last_stmt == NULL)
        return TYPE_UNIT;
    else if (last_stmt->kind == AST_IF)
        return if_value_type(last_stmt);
    else
        return last_stmt->type;
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
        { "readBoolean", TYPE_BOOL, 0, { 0 }, { NULL } }
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

    // Used for call recursion detection
    // When we enter a function, create a frame .caller poitns to the pervious frame
    // and we make the context point to the new frame the result is: (diagram is when we are in fn3)
    // |───[fn1 frame]◀────[fn2 frame]◀────[fn3 frame]
    //                                          ▲
    //                                          |
    //                              [context]───
    sema_frame_t frame = { .func_sym = func_sym, .caller = semantic_ctx->call_chain };
    semantic_ctx->call_chain = &frame; // Push to the call chain

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
    type_t* type;
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

            else if ((type = if_value_type(last_stmt)) != NULL) {
                // ret_inferred -> ifs type becomes the funcions type
                // Explicit return type -> every branch must fit the type
                type_t* target = func_sym->ret_inferred ? type : ast_func_def_data->return_type;

                // Last stmt of the body is the if stmt -> this checks the block
                // compatibility with the func ret type or throws an error
                TRY(coerce_block_result(ast_func_def_data->body, target));

                // If coerce didnt thrown an error it means types are good -> set the type of the func body
                ast_func_def_data->body->type = target;
            }

            // If function return type was not specified and every path in the if returns Unit
            // infer the type as Unit
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

    // Return type is explicit
    // skip when last_stmt is IF since that is already covered
    if (!func_sym->ret_inferred && func_sym->type->kind != T_UNIT && (last_stmt == NULL || last_stmt->kind != AST_IF)) {
        if (last_stmt != NULL && last_stmt->kind == AST_EXPR_STMT) {
            TRY(coerce(&last_stmt->expr_stmt.expr, func_sym->type, ERR_SEM_TYPE));
            last_stmt->type = func_sym->type;
            ast_func_def_data->body->type = func_sym->type;

        } else if (!type_compatible(func_sym->type, ast_func_def_data->body->type))
            return ERR_SEM_TYPE;
    }

    // @main has to return Unit
    if (ast_func_def_data->is_main && ast_func_def_data->return_type->kind != T_UNIT)
        return ERR_SEM_TYPE;

    // Cleanup

    // For fn3:
    // |───[fn1 frame]◀────[fn2 frame]◀────[fn3 frame] (destoyed when check_function returns)
    //                          ▲
    //                          |
    //              [context]───
    semantic_ctx->call_chain = frame.caller; // Pop from the call chain

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
    ast_node_t* last_stmt = last_statement(ast_block);
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

// Statement switch

/**
 * @paragraph Calls check_* function based on statement->type and sets the type of the statement
 *  in case it is the last statement in a block and it is needed as a return value (type)
 */
static error_code_t check_statement(sema_ctx_t* semantic_ctx, ast_node_t* ast_statement)
{
    if (semantic_ctx == NULL || ast_statement == NULL)
        return ERR_INTERNAL;

    switch (ast_statement->kind) {
    case AST_VAR_DEF:
        TRY(check_var_def(semantic_ctx, ast_statement));
        ast_statement->type = TYPE_UNIT; // What is the type if this stmt is the last
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

    case AST_FOR:
        TRY(check_for(semantic_ctx, ast_statement));
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

// ----------------------------------------
//  Specific statements
// ----------------------------------------

/**
 * @paragraph Does the following checks:
 *  checks the return type of the init expression,
 *  when var has NO specified type tries to set the type to the type of the init expression,
 * @paragraph Fills info into the ast (id, adds the var as a local to the current function)
 *  Also inserts the var into the symbol table
 */
static error_code_t check_var_def(sema_ctx_t* semantic_ctx, ast_node_t* ast_statement)
{
    // Helper variables
    ast_var_def_t* ast_var_def = &ast_statement->var_def;
    symtable_t* symbol_table = semantic_ctx->symbol_table;

    // Check the initilizer

    // val x = x + 1
    // check_expression looks up an x that already exists and uses its symbol, then we return and insert the new x into the symtable
    // if we were to insert the new x first, then call check_expression the lookup there would find the new x
    // For correct shadowing we use the first option
    TRY(check_expression(semantic_ctx, ast_var_def->init)); // ast_var_def->init->type is now known

    // Determine variables type
    type_t* var_type;

    if (ast_var_def->declared_type != NULL) {
        // Var type was specified
        TRY(coerce(&ast_var_def->init, ast_var_def->declared_type, ERR_SEM_TYPE)); // Convert init expr to double if needed
        var_type = ast_var_def->declared_type;
    } else {
        // Type is inferred
        var_type = ast_var_def->init->type;
    }

    // Insert into current scope (symtable)
    symbol_t* var_sym;
    TRY_ST(symtable_insert_var(symbol_table, ast_var_def->name, var_type,
        ast_var_def->is_const, &var_sym));

    // Copy uniqe id from symtable to ast
    ast_var_def->id = var_sym->id;

    // Append the variable to the func_def.locals array
    bool err_code = dyn_arr_append(&semantic_ctx->current_func->func_def.locals, ast_statement);
    if (err_code == false)
        return ERR_INTERNAL;

    // Type of the statement is already set by check_stetement
    return ERR_OK;
}

/**
 * @paragraph Does the following checks:
 *  assignment to a variable that doest exist -> ERR_SEM_UNDEF,
 *  assignment to a constant variable (val) -> ERR_SEM_UNSIGNED,
 *  check if the type of left and right side of the assign match (maybe convert),
 */
static error_code_t check_assign(sema_ctx_t* semantic_ctx, ast_node_t* ast_statement)
{
    symbol_t* var_sym = lookup_var_in_function(semantic_ctx, ast_statement->assign.name);

    // Variable doesnt exist (could also be ERR_SEM_ASSIGN)
    if (var_sym == NULL)
        return ERR_SEM_UNDEF;

    // You cannot assign to val - aslo covers trying to assign to func params
    if (var_sym->is_const)
        return ERR_SEM_ASSIGN;

    // Check right side of assign
    TRY(check_expression(semantic_ctx, ast_statement->assign.value));

    // Now we know type of the right hand side -> check if it matches with the left side
    // Type of var cannot change during runtime
    TRY(coerce(&ast_statement->assign.value, var_sym->type, ERR_SEM_TYPE));

    ast_statement->assign.id = var_sym->id;

    // Statement type is set in check_statement to Unit
    return ERR_OK;
}

/**
 * @paragraph Does the following checks:
 *  Type of the condition must be T_BOOL
 *  calls check_block on the body of the cycle
 */
static error_code_t check_while(sema_ctx_t* semantic_ctx, ast_node_t* ast_statement)
{
    // Check the condition + cond must be bool
    TRY(check_expression(semantic_ctx, ast_statement->while_stmt.cond));
    if (ast_statement->while_stmt.cond->type->kind != T_BOOL)
        return ERR_SEM_TYPE;

    // Check the body of the cycle
    TRY(check_block(semantic_ctx, ast_statement->while_stmt.body));

    // Type of this statement is Unit - set in check_statement
    return ERR_OK;
}

static error_code_t check_for(sema_ctx_t* semantic_ctx, ast_node_t* ast_statement)
{
    // Helpers
    ast_for_t* for_stmt = &ast_statement->for_stmt;
    bool is_step = false;

    // Check loop variables
    TRY(check_expression(semantic_ctx, for_stmt->from));
    TRY(check_expression(semantic_ctx, for_stmt->to));

    // From and to must be T_INT
    if ((for_stmt->from->type->kind != T_INT) || (for_stmt->to->type->kind != T_INT))
        return ERR_SEM_TYPE;

    // step can be omitted
    if (for_stmt->step != NULL) {
        is_step = true;

        TRY(check_expression(semantic_ctx, for_stmt->step));
        // Must be INT
        if (for_stmt->step->type->kind != T_INT)
            return ERR_SEM_TYPE;

        // step cant be 0 -> other checks are done during runtime
        if (is_zero_literal(for_stmt->step))
            return ERR_SEM_OTHER; // Might be a different error
    }

    // Insert the loop vat, $end and $step into the symbol table
    TRY_ST(symtable_push_scope(semantic_ctx->symbol_table));

    symbol_t* var_sym;
    symbol_t* end_sym;
    symbol_t* step_sym;

    // You cannot assign to loop variables -> ERR_SEM_ASSIGN
    TRY_ST(symtable_insert_var(semantic_ctx->symbol_table, for_stmt->var_name, TYPE_INT, true, &var_sym));
    TRY_ST(symtable_insert_var(semantic_ctx->symbol_table, "$end", TYPE_INT, true, &end_sym));
    if (is_step) {
        TRY_ST(symtable_insert_var(semantic_ctx->symbol_table, "$step", TYPE_INT, true, &step_sym));
        for_stmt->step_id = step_sym->id;
    }

    for_stmt->var_id = var_sym->id;
    for_stmt->end_id = end_sym->id;

    // Append for node into func_def.locals
    bool res = dyn_arr_append(&semantic_ctx->current_func->func_def.locals, ast_statement);
    if (res == false)
        return ERR_INTERNAL;

    // Check each guard
    dyn_arr_t* guards = &for_stmt->guards;

    for (size_t guard_i = 0; guard_i < guards->count; guard_i++) {
        ast_node_t* guard = dyn_arr_get(guards, guard_i);
        TRY(check_expression(semantic_ctx, guard));
        // Each guard must be T_BOOL
        if (guard->type->kind != T_BOOL)
            return ERR_SEM_TYPE;
    }

    // Check the body of the for loop
    TRY(check_block(semantic_ctx, for_stmt->body));

    // Pop the for loop var symtable scope
    TRY_ST(symtable_pop_scope(semantic_ctx->symbol_table));

    return ERR_OK;
}

/**
 * @paragraph Does the following checks:
 *  Type of the condition must be T_BOOL
 *  Calls check_block on the body of every branch
 */
static error_code_t check_if(sema_ctx_t* semantic_ctx, ast_node_t* ast_statement)
{
    // Check the condition + cond must be bool
    TRY(check_expression(semantic_ctx, ast_statement->if_stmt.cond));
    if (ast_statement->if_stmt.cond->type->kind != T_BOOL)
        return ERR_SEM_TYPE;

    // Check each the body of each branch
    TRY(check_block(semantic_ctx, ast_statement->if_stmt.then_block));
    // Boolthen -> else_block can be NULL
    if (ast_statement->if_stmt.else_block != NULL)
        TRY(check_block(semantic_ctx, ast_statement->if_stmt.else_block));

    // Type of this statement is Unit - set in check_statement
    return ERR_OK;
}

/**
 * @paragraph Does the following checks:
 *  If there is a return statement (there is because we got into this function) the function
 *  MUST have an explicit reutrn type,
 *  When return contains a return value check if the type of tha value is compatible with the
 *  functions return type - if function return type is T_UNIT we dont check (it is discarded),
 *  When there is no return value and function return type is T_UNIT thats fine, if its not T_UNIT
 *  There is nothing to return but the function needs something returned -> ERR_SEM_OTHER (10)
 */
static error_code_t check_return(sema_ctx_t* semantic_ctx, ast_node_t* ast_statement)
{
    ast_return_t* ast_return_stmt = &ast_statement->return_stmt;
    ast_func_def_t* ast_func_def = &semantic_ctx->current_func->func_def;

    // If body contains return the function MUST have an explicit return type

    // No return type written (ast_func_def->return_type is NULL while the
    // func is being checked - it is set only at the end of check_function)
    // We dont even need to lookup the symbol to check ret_inferred
    if (ast_func_def->return_type == NULL)
        return ERR_SEM_TYPE;

    // We have a return value
    if (ast_return_stmt->value != NULL) {
        TRY(check_expression(semantic_ctx, ast_return_stmt->value));

        // Now we know the type of the return value
        if (ast_func_def->return_type->kind == T_UNIT)
            ; // Return value is discarded when function returns Unit
        else
            // Check if the return type and function return type is compatible and maybe do a conversion
            TRY(coerce(&ast_return_stmt->value, ast_func_def->return_type, ERR_SEM_TYPE));

        // There is no return value
    } else {
        // Unit function discards the return value anyways
        if (ast_func_def->return_type->kind == T_UNIT)
            ;
        // The function has a (non Unit) return type but we are not returning anything
        else
            return ERR_SEM_OTHER; // Could also be ERR_SEM_TYPE (6)
    }

    return ERR_OK;
}

// ========================================
//  Expressions
// ========================================

// Expression switch

static error_code_t check_expression(sema_ctx_t* semantic_ctx, ast_node_t* ast_expression)
{
    switch (ast_expression->kind) {
    case AST_INT_LIT:
        ast_expression->type = TYPE_INT;
        break;

    case AST_DOUBLE_LIT:
        ast_expression->type = TYPE_DOUBLE;
        break;

    case AST_STRING_LIT:
        ast_expression->type = TYPE_STRING;
        break;

    case AST_UNIT_LIT:
        ast_expression->type = TYPE_UNIT;
        break;

    case AST_BOOL_LIT:
        ast_expression->type = TYPE_BOOL;
        break;

    case AST_VAR_REF: {
        symbol_t* var_sym = lookup_var_in_function(semantic_ctx, ast_expression->var_ref.name);
        if (var_sym == NULL)
            return ERR_SEM_UNDEF;
        ast_expression->type = var_sym->type;
        ast_expression->var_ref.id = var_sym->id;
        break;
    }

    case AST_UNARY:
        TRY(check_unary(semantic_ctx, ast_expression));
        break;

    case AST_BINARY:
        TRY(check_binary(semantic_ctx, ast_expression));
        break;

    case AST_CALL:
        TRY(check_call(semantic_ctx, ast_expression));
        break;

    case AST_MEMBER:
        TRY(check_member(semantic_ctx, ast_expression));
        break;

    // if expressions - CONTROL
    case AST_IF: {
        TRY(check_if(semantic_ctx, ast_expression)); // Checks the condition and each branch
        type_t* type = if_value_type(ast_expression);
        if (type == NULL)
            return ERR_SEM_TYPE;

        // Look at the last stmt of each block and check types
        TRY(coerce_block_result(ast_expression->if_stmt.then_block, type));
        TRY(coerce_block_result(ast_expression->if_stmt.else_block, type));

        ast_expression->type = type;
        break;
    }

    default:
        return ERR_INTERNAL;
    }
    return ERR_OK;
}

// ----------------------------------------
//  Specific expressions
// ----------------------------------------

static error_code_t check_unary(sema_ctx_t* semantic_ctx, ast_node_t* ast_expression)
{
    TRY(check_expression(semantic_ctx, ast_expression->unary.operand));
    // Now we know ast_expression ast_expression->unary.operand->type

    switch (ast_expression->unary.op) {
    // Negative value
    case AST_OP_NEG:
        if (!is_numeric(ast_expression->unary.operand->type))
            return ERR_SEM_TYPE;
        break;

    // Boolean NOT
    case AST_OP_NOT:
        if (ast_expression->unary.operand->type->kind != T_BOOL)
            return ERR_SEM_TYPE;
        break;

    default:
        return ERR_INTERNAL;
    }

    ast_expression->type = ast_expression->unary.operand->type;
    return ERR_OK;
}

static error_code_t check_binary(sema_ctx_t* semantic_ctx, ast_node_t* ast_expression)
{
    // Checks both children, save their types
    TRY(check_expression(semantic_ctx, ast_expression->binary.left));
    TRY(check_expression(semantic_ctx, ast_expression->binary.right));
    type_t* left_type = ast_expression->binary.left->type;
    type_t* right_type = ast_expression->binary.right->type;

    type_t* result = NULL;

    switch (ast_expression->binary.op) {
    // Arithmetic operators
    case AST_OP_ADD:
        // Concatenation
        if (left_type->kind == T_STRING && right_type->kind == T_STRING) {
            result = TYPE_STRING;
            break;
        }

        // fallthrough
    case AST_OP_SUB:
    case AST_OP_MUL:
    case AST_OP_DIV:
        // These apply for + - * /
        // If at least one side is not numeric -> ERR_SEM_TYPE
        if (!is_numeric(left_type) || !is_numeric(right_type))
            return ERR_SEM_TYPE;

        // Both sides are T_INT -> expression result is T_INT
        if (left_type->kind == T_INT && right_type->kind == T_INT)
            result = TYPE_INT;

        // At least one side is T_DOUBLE -> convert both sides to T_DOUBLE & check
        // Result expression type is also T_DOUBLE
        else {
            TRY(unify_numeric(ast_expression));
            result = TYPE_DOUBLE;
        }
        break;

    // Relational operators
    case AST_OP_LT:
    case AST_OP_LE:
    case AST_OP_GT:
    case AST_OP_GE:
        // Both sides must be string or both sides must be numerical, else - ERROR
        if (!(left_type->kind == T_STRING && right_type->kind == T_STRING) && !(is_numeric(left_type) && is_numeric(right_type)))
            return ERR_SEM_TYPE;

        // Both sides are strings - OK
        // Both sides are numbers - OK
        // Both INT - OK, At least one DOUBLE -> convert
        TRY(unify_numeric(ast_expression));

        // Relational operators always return T_BOOL
        result = TYPE_BOOL;
        break;

    // Equality
    case AST_OP_EQ:
    case AST_OP_NE:
        // Both sides must be numerric or both sides must match type, otherwise -> ERROR
        if (!((is_numeric(left_type) && is_numeric(right_type)) || (left_type->kind == right_type->kind)))
            return ERR_SEM_TYPE;

        // Both are numeric -> might need conversion - OK
        // Both INT - OK, Atlest one DOUBLE -> convert
        // Both types match - OK
        TRY(unify_numeric(ast_expression));

        result = TYPE_BOOL;
        break;

    // Boolean operators
    case AST_OP_AND:
    case AST_OP_OR:
        // Both sides must be Booleans
        if (!(left_type->kind == T_BOOL && right_type->kind == T_BOOL))
            return ERR_SEM_TYPE;

        result = TYPE_BOOL;
        break;

    default:
        return ERR_INTERNAL;
    }

    ast_expression->type = result;
    return ERR_OK;
}

// ========================================
//  Function calls
// ========================================

static error_code_t check_call(sema_ctx_t* semantic_ctx, ast_node_t* ast_call)
{
    // Helper variables
    symtable_t* symbol_table = semantic_ctx->symbol_table;
    ast_call_t* ast_call_data = &ast_call->call;

    // Find the function in the symbol table
    symbol_t* func_sym = symtable_lookup_func(symbol_table, ast_call_data->name);
    if (func_sym == NULL)
        return ERR_SEM_UNDEF;

    ast_call_data->is_builtin = func_sym->builtin;

    // Lazy checks
    if (func_sym->sema_marker == SEMA_UNCHECKED)
        TRY(check_function(semantic_ctx, func_sym->decl));

    // This means the funciton is recursive -> must have explicit return type
    // Also covers indirect recursion
    else if (func_sym->sema_marker == SEMA_IN_PROGRESS) {
        // Currently we are in the innermost function (see call chain diagram in check_function)
        for (sema_frame_t* frame = semantic_ctx->call_chain; frame != NULL; frame = frame->caller) {
            if (frame->func_sym->ret_inferred)
                return ERR_SEM_TYPE;

            if (frame->func_sym == func_sym)
                break;
        }
    }

    // Arguments
    // arg count must match the func signature
    if ((size_t)func_sym->param_count != ast_call_data->args.count)
        return ERR_SEM_CALL;

    // Check each arg
    for (size_t arg_i = 0; arg_i < (size_t)func_sym->param_count; arg_i++) {
        ast_node_t* ast_arg = dyn_arr_get(&ast_call_data->args, arg_i); // !!! LOCAL COPY
        type_t* expected_param_type = func_sym->param_types[arg_i];

        TRY(check_expression(semantic_ctx, ast_arg));
        // Now we know ast_arg->type
        // check type mismatch between ast param and the func signature param type
        TRY(coerce(&ast_arg, expected_param_type, ERR_SEM_CALL));
        ast_call_data->args.items[arg_i] = ast_arg; // needed since ast_param is a copy of the pointer
    }

    // Calls return type is the same as the functions return type
    ast_call->type = func_sym->type;
    return ERR_OK;
}

// -----------------------------------------------------------
//  VALMEM extension
// -----------------------------------------------------------

typedef struct {
    const char* name;
    type_kind_t receiver;
    type_t* result;
} member_decl_t;

static error_code_t check_member(sema_ctx_t* semantic_ctx, ast_node_t* ast_member)
{
    // Local, because TYPE_* cannot be used in a static initializer
    const member_decl_t MEMBERS[] = {
        { "toString", T_ANY, TYPE_STRING },
        { "toInt", T_DOUBLE, TYPE_INT },
        { "floor", T_DOUBLE, TYPE_DOUBLE },
        { "length", T_STRING, TYPE_INT },
    };
    const size_t member_cnt = sizeof(MEMBERS) / sizeof(MEMBERS[0]);

    TRY(check_expression(semantic_ctx, ast_member->member.receiver));
    // Now we know ast_member->member.receiver->type

    // Find the member
    const member_decl_t* found = NULL;

    for (size_t i = 0; i < member_cnt; i++) {
        if (strcmp(MEMBERS[i].name, ast_member->member.name) == 0) {
            found = &MEMBERS[i];
            break;
        }
    }

    if (found == NULL)
        return ERR_SEM_UNDEF; // Might be different error

    // Check types
    if (found->receiver != T_ANY && ast_member->member.receiver->type->kind != found->receiver)
        return ERR_SEM_TYPE;

    ast_member->type = found->result;
    return ERR_OK;
}

// ========================================
//  Top level semantics function
// ========================================

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
