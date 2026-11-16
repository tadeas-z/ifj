// Implementation of the IFJ26 imperative language compiler
// parser_expression.c
// Author(s): Dusan Popovic (xpopovd00)

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "parser_expression.h"
#include "parser_stack.h"
#include "ast.h"
#include "lexer.h"
#include "comp_error.h"

bool stack_err_flag = false;
//macro relation_error, syntatic error
#define R_ERR ' ' 	

/*
---------------------------------------------------------------------------------
|    Stack function definition                               				    | 
---------------------------------------------------------------------------------
*/

static void stack_init (tstack_t *s){
	s->top = NULL;
}

static bool push (tstack_t *s, tdata_t d) {
	tselem_t *temp = (tselem_t * ) malloc(sizeof(tselem_t));
	if(temp == NULL){
		stack_err_flag = true;
		return false;
	}
	else{
		temp->data = d;
		temp->next = s->top;
		s->top = temp;
		return true;
	}
}

static void pop (tstack_t *s){
	if (s->top != NULL){
		tselem_t *temp = s->top;
		s->top = s->top->next;
		free (temp);
	}
}

static tdata_t top (tstack_t *s) {
	if(s->top != NULL)
		return s->top->data;
	stack_err_flag = true;
	tdata_t stack_is_empity = {SYMBOL_TERMINAL, 0, false, NULL};
	return stack_is_empity;
}

static tselem_t* get_top_terminal (tstack_t *s){
    if(s == NULL || s->top == NULL)
            return NULL;
    tselem_t *temp = s->top;
    while (temp != NULL){
        if(temp->data.symbol_type == SYMBOL_TERMINAL)
            return temp;
        temp = temp->next;
    }
    return NULL;
}

static void stack_free(tstack_t *s){
    if(s != NULL && s->top != NULL){
        tselem_t *temp = s->top;
        while (temp != NULL){
            tselem_t *next = temp->next;
            if(temp->data.ast_node != NULL)
                ast_free (temp->data.ast_node);
            free(temp);
			temp = next;
        }
        s->top = NULL;
    }
}
//end stack function definition

static int64_t parser_int_literal (const char *lexeme){
	if(lexeme == NULL)
		return 0;

	char buf[128];
	size_t j = 0;
	for(size_t i = 0; lexeme[i] != '\0' && j < sizeof(buf) - 1; i++){
		if(lexeme[i] != '_')
			buf[j++] = lexeme[i];
	}

	buf[j] = '\0';
	int base = 10;
	if(buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X'))
		base = 16;
	return strtoll(buf, NULL, base);
}

static double parser_double_literal (const char *lexeme){
	if(lexeme == NULL)
		return 0;

	char buf[128];
	size_t j = 0;
	for(size_t i = 0; lexeme[i] != '\0' && j < sizeof(buf) - 1; i++){
		if(lexeme[i] != '_')
			buf[j++] = lexeme[i];
	}

	buf[j] = '\0';
	return strtod(buf, NULL);
}

static char *decode_string_literal (const char *lexeme){
	if(lexeme == NULL){
		char *empity = (char*) malloc(1);
		if(empity != NULL)
			empity[0] = '\0';
		return empity;
	}
	size_t len = strlen(lexeme);

	//Multi-line string (""" ... """")
	if(len >= 6 && strncmp(lexeme, "\"\"\"", 3) == 0 && strncmp(lexeme + len -3, "\"\"\"", 3) == 0){
		size_t inner_len = len - 6;
		char *out = (char*) malloc(inner_len +1);
		if(out == NULL)
			return NULL;
		memcpy(out, lexeme + 3, inner_len);
		out[inner_len] = '\0';
		return out;
	}

	//Single-line string (" ... ")
	size_t start = (len >= 1 && lexeme[0] == '"') ? 1 : 0;
	size_t end = (len >= 2 && lexeme[len-1] == '"') ? len - 1 : len;

	char *out = (char*) malloc((end - start) + 1);
	if(out == NULL) 
		return NULL;

	size_t j = 0;
	for(size_t i = start; i < end; i++){
		if(lexeme[i] == '\\' && (i + 1) < end){
			i++;
			switch(lexeme[i]){
				case 'n': out[j++] = '\n'; 
				break;
				case 'r': out[j++] = '\r'; 
				break;
				case 't': out[j++] = '\t';
				break;
				case '"': out[j++] = '"';
				break;
				case '\\': out[j++] = '\\'; 
				break;
				default:
					out[j++] = lexeme[i];
					break;  
			}
		}
		else
			out[j++] = lexeme[i];
	}
	out[j] = '\0';
	return out; 
} 

const char precedence_table [10] [10] = {
		/*	  	 +		 -		 *		 /		 (			 )		 $		id		rel		u-		*/
/*	+	*/{		'>',	'>',	'<',	'<',	'<',		'>',	'>',	'<',	'>',	'<'		},
/*	-	*/{		'>',	'>',	'<',	'<',	'<',		'>',	'>',	'<',	'>',	'<'		},
/*	*	*/{		'>',	'>',	'>',	'>',	'<',		'>',	'>',	'<',	'>',	'<'		},
/*	/	*/{		'>',	'>',	'>',	'>',	'<',		'>',	'>',	'<',	'>',	'<'		},
/*	(	*/{		'<',	'<',	'<',	'<',	'<',		'=',	R_ERR,	'<',	'<',	'<'		},
/*	)	*/{		'>',	'>',	'>',	'>',	R_ERR,		'>',	'>',	R_ERR,	'>',	R_ERR	},
/*	$	*/{		'<',	'<',	'<',	'<',	'<',		R_ERR,	R_ERR,	'<',	'<',	'<'		},		
/*	id	*/{		'>',	'>',	'>',	'>',	R_ERR, 		'>', 	'>', 	R_ERR,	'>',	R_ERR	},
/*	rel	*/{		'<',	'<',	'<',	'<',	'<',		'>',	'>',	'<',	'>',	'<'		},
/*	u-	*/{		'>',   	'>',   	'>',   	'>',   	'<',   		'>',   	'>',   	'<',   	'>',   	'<' 	}
};

typedef enum {
	index_add = 0,
	index_sub = 1,
	index_mul = 2,
	index_div = 3,
	index_left_par = 4,
	index_right_par = 5,
	index_dollar = 6,
	index_id = 7,
	index_rel = 8,
	index_uminus = 9
}PrecedenceIndex;

int get_precedence_index (token_type_t t, bool is_unary){
	if (t == TOKEN_PLUS) 
		return index_add;
	else if (t == TOKEN_MINUS) 
		return is_unary ? index_uminus : index_sub;
	else if (t == TOKEN_STAR) 
		return index_mul;
	else if (t == TOKEN_SLASH) 
		return index_div;
	else if (t == TOKEN_LPAREN) 
		return index_left_par;
	else if (t == TOKEN_RPAREN) 
		return index_right_par;
	else if (t == TOKEN_EOF ||  t == TOKEN_NEWLINE || t ==TOKEN_SEMICOLON || t == TOKEN_COMMA || 
			t == TOKEN_THEN || t == TOKEN_DO || t== TOKEN_ELSE || t== TOKEN_COLON)
		return index_dollar;
	else if (t == TOKEN_IDENTIFIER || t == TOKEN_INTEGER || t == TOKEN_DOUBLE || t == TOKEN_STRING)
		return index_id;
	else if (t == TOKEN_EQUAL || t == TOKEN_NOT_EQUAL || t == TOKEN_LESS || 
		t == TOKEN_LESS_EQUAL || t == TOKEN_GREATER || t == TOKEN_GREATER_EQUAL)
		return index_rel;
	else
		return -1;
}

static bool is_operand (token_type_t token) {
	if(token == TOKEN_IDENTIFIER || token == TOKEN_INTEGER ||
		token == TOKEN_DOUBLE || token == TOKEN_STRING){
			return true;
	}
	return false;
}

static void free_control (tdata_t* control, int count){
	for(int i = 0; i < count; i++){
		if (control[i].ast_node != NULL){
			ast_free(control[i].ast_node);
			control[i].ast_node = NULL;
		}
	}	
}

int reduce_rules (tstack_t *stack, size_t line, size_t col){
    tdata_t control[5];
    int count_control = 0;
	bool mark_found = false;
	
    while (stack->top != NULL){
        tdata_t temp = top(stack);
        pop(stack);

		if (temp.shift_mark){
            temp.shift_mark = false;
			if(!(push (stack, temp))){
				if(temp.ast_node != NULL)
					ast_free(temp.ast_node);
				free_control (control, count_control);
				return ERR_INTERNAL;
			}
			mark_found = true;
			break; 
		}
		
        control[count_control++] = temp;
        if(count_control > 3){
            free_control (control, count_control);
			return ERR_SYNTAX;    	  
		}
	}
    tdata_t non_terminal = {SYMBOL_NON_TERMINAL, TOKEN_EOF, false, NULL};
    if (!mark_found || count_control == 0){
        free_control(control, count_control);
		return ERR_SYNTAX;
	}
	
    //rule E -> id 
    if(count_control == 1 && control[0].symbol_type == SYMBOL_TERMINAL && 
		is_operand (control[0].token_type)){
		
            non_terminal.ast_node = control[0].ast_node;
            if (!push(stack, non_terminal)){
				free_control (control, count_control);
				return ERR_INTERNAL;
			}
			return ERR_OK;
    	}
    	if (count_control == 2){
			// 1) E -> ()
			if(control[1].symbol_type == SYMBOL_TERMINAL && control[1].token_type == TOKEN_LPAREN &&
				control[0].symbol_type == SYMBOL_TERMINAL && control[0].token_type == TOKEN_RPAREN){
				
				ast_node_t *unit_node = ast_new_unit_lit(line, col);
				if(unit_node == NULL) {
					free_control(control, count_control);
					return ERR_INTERNAL;
				}	
				non_terminal.ast_node = unit_node;
				if(!push(stack, non_terminal)){
					ast_free(unit_node);
					return ERR_INTERNAL;
				}
				return ERR_OK;
			}

			// 2) E -> - E
			if(control[1].symbol_type == SYMBOL_TERMINAL && control[1].token_type == TOKEN_MINUS &&
				control[0].symbol_type == SYMBOL_NON_TERMINAL){

				ast_node_t *unary_node = ast_new_unary(AST_OP_NEG, control[0].ast_node, line, col);
				if(unary_node == NULL) {
					free_control(control, count_control);
					return ERR_INTERNAL;
				}	
				non_terminal.ast_node = unary_node;
				if(!push(stack, non_terminal)){
					ast_free(unary_node);
					return ERR_INTERNAL;
				}
				return ERR_OK;
			}
		}

		if(count_control == 3){
			// 3) E -> ( E )
			if(control[2].token_type == TOKEN_LPAREN && 
				control[1].symbol_type == SYMBOL_NON_TERMINAL && 
				control[0].token_type == TOKEN_RPAREN){
				
				non_terminal.ast_node = control[1].ast_node;
				if (!(push(stack, non_terminal))){ 
					free_control (control, count_control);
					return ERR_INTERNAL;
				}
				return ERR_OK;
			}  
			
			//E -> E op E, op={+,-,*,/}
			if(control[2].symbol_type == SYMBOL_NON_TERMINAL && 
				control[1].symbol_type == SYMBOL_TERMINAL && 
				control[0].symbol_type == SYMBOL_NON_TERMINAL){
				
				ast_binary_op_t op;
				switch(control[1].token_type){
					case TOKEN_PLUS: op = AST_OP_ADD;
					break;
					case TOKEN_MINUS: op = AST_OP_SUB;
					break;
					case TOKEN_STAR: op = AST_OP_MUL;
					break;
					case TOKEN_SLASH: op = AST_OP_DIV;
					break;
					case TOKEN_EQUAL: op = AST_OP_EQ;
					break;
					case TOKEN_NOT_EQUAL: op = AST_OP_NE;
					break;
					case TOKEN_LESS: op = AST_OP_LT;
					break;
					case TOKEN_LESS_EQUAL: op = AST_OP_LE;
					break;
					case TOKEN_GREATER: op = AST_OP_GT;
					break;
					case TOKEN_GREATER_EQUAL: op = AST_OP_GE;
					break;
					default:
						free_control(control, count_control);
						return ERR_SYNTAX;
				}

				ast_node_t *bin_node = ast_new_binary(op, control[2].ast_node, control[0].ast_node, line, col);
				if(bin_node == NULL){
					free_control (control, count_control);
					return ERR_INTERNAL;
				}
				
				non_terminal.ast_node = bin_node;
				if (!(push(stack, non_terminal))){ 
					ast_free (bin_node);
					return ERR_INTERNAL;
				}
				return ERR_OK;
        	}
		}
		free_control (control, count_control);
		return ERR_SYNTAX;
}

static int advance_shared_token(lexer_t *lexer, token_t *current_token) {
    token_dispose(current_token);
    if (lexer_next(lexer, current_token) != ERR_OK) {
        return ERR_LEXICAL;
    }
    return ERR_OK;
}

/*
---------------------------------------------------------------------------------
|    Main precedence expression parser                              			| 
---------------------------------------------------------------------------------
*/

int parser_expression(lexer_t *lexer, token_t *current_token, ast_node_t **out_node) {
    if(lexer == NULL || current_token == NULL || out_node == NULL)
		return ERR_INTERNAL;
	
	*out_node = NULL;
	stack_err_flag = false;

	tstack_t stack;
    stack_init(&stack);

    tdata_t dollar = {SYMBOL_TERMINAL, TOKEN_EOF, false, NULL};
    if (!push(&stack, dollar))
		return ERR_INTERNAL;

    bool end_parsing = false;
	bool expect_operand = true;

    while(!(end_parsing)){
        tselem_t *terminal_top = get_top_terminal(&stack);
        if (terminal_top == NULL){
            stack_free(&stack);
            return ERR_SYNTAX;
        }
		if(expect_operand && current_token->type == TOKEN_IDENTIFIER){
			size_t id_line = current_token->line;
			size_t id_col = current_token->column;
			size_t name_len = strlen(current_token->lexeme) + 1;
			char *id_name = (char*) malloc(name_len);
			if(id_name == NULL){
				stack_free(&stack);
				return ERR_INTERNAL;
			}
			memcpy(id_name, current_token->lexeme, name_len);

			int adv_err = advance_shared_token(lexer, current_token);
			if(adv_err != ERR_OK){
				free(id_name);
				stack_free(&stack);
				return adv_err;
			}

			ast_node_t *operand_node = NULL;

			//call function(arg1, arg2, ...)
			if(current_token->type == TOKEN_LPAREN){
				operand_node = ast_new_call(id_name, id_line, id_col);
				free(id_name);
				if(operand_node == NULL){
					stack_free(&stack);
					return ERR_INTERNAL;
				}

				adv_err = advance_shared_token(lexer, current_token);
				if(adv_err != ERR_OK){
					ast_free(operand_node);
					stack_free(&stack);
					return adv_err;
				}

				if(current_token->type != TOKEN_RPAREN){
					while(1){
						while(current_token->type == TOKEN_NEWLINE){
							adv_err = advance_shared_token(lexer, current_token);
							if(adv_err != ERR_OK){
								ast_free(operand_node);
								stack_free(&stack);
								return adv_err;
							}
						}

						ast_node_t *arg_node = NULL;
						int arg_err = parser_expression(lexer, current_token, &arg_node);
						if(arg_err != ERR_OK){
							ast_free(operand_node);
							stack_free(&stack);
							return arg_err;
						}

						if (!dyn_arr_append(&operand_node->call.args, arg_node)) {
							ast_free(arg_node);
							ast_free(operand_node);
							stack_free(&stack);
							return ERR_INTERNAL;
                        }

						if(current_token->type == TOKEN_COMMA) {
							adv_err = advance_shared_token(lexer, current_token);
							if(adv_err != ERR_OK){
								ast_free(operand_node);
								stack_free(&stack);
								return adv_err;
							}

							while(current_token->type == TOKEN_NEWLINE){
								adv_err = advance_shared_token(lexer, current_token);
                                if (adv_err != ERR_OK) {
                                    ast_free(operand_node);
                                    stack_free(&stack);
                                    return adv_err;
                                }
							}

							if(current_token->type == TOKEN_RPAREN)
								break;
							continue; 
						}
						break;
					}
				}

				if(current_token->type != TOKEN_RPAREN) {
					ast_free(operand_node);
					stack_free(&stack);
					return ERR_SYNTAX;
				}

				adv_err = advance_shared_token(lexer, current_token);
                if (adv_err != ERR_OK) {
                    ast_free(operand_node);
                    stack_free(&stack);
                    return adv_err;
                }
			}

			else{
				operand_node = ast_new_var_ref(id_name, id_line, id_col);
				free(id_name);
				if(operand_node == NULL){
					stack_free(&stack);
					return ERR_INTERNAL;
				}
			}
			
			bool top_is_unary = (terminal_top->data.token_type == TOKEN_MINUS &&
                                 (terminal_top->next == NULL || terminal_top->next->data.symbol_type == SYMBOL_TERMINAL));
			int row = get_precedence_index(terminal_top->data.token_type, top_is_unary);
			if (row < 0 || row >= 10 || precedence_table[row][index_id] != '<') {
                ast_free(operand_node);
                stack_free(&stack);
                return ERR_SYNTAX;
            }

			terminal_top->data.shift_mark = true;
            tdata_t data = {
                .symbol_type = SYMBOL_TERMINAL,
                .token_type = TOKEN_IDENTIFIER,
                .shift_mark = false,
                .ast_node = operand_node
            };
            if (!push(&stack, data)) {
                ast_free(operand_node);
                stack_free(&stack);
                return ERR_INTERNAL;
            }

            expect_operand = false;
            continue;
		}

		bool input_is_unary = (current_token->type == TOKEN_MINUS && expect_operand);
        bool top_is_unary = (terminal_top->data.token_type == TOKEN_MINUS &&
                             (terminal_top->next == NULL || terminal_top->next->data.symbol_type == SYMBOL_TERMINAL));

        int row = get_precedence_index (terminal_top->data.token_type, top_is_unary);
        int col = get_precedence_index (current_token->type, input_is_unary);

        if(row < 0 || row >= 10 || col < 0 || col >=  10){
            stack_free (&stack);
            return ERR_SYNTAX;
        }
        char rel = precedence_table[row][col];
        switch (rel){
            case '=': {
                tdata_t data = {SYMBOL_TERMINAL, current_token->type, false, NULL};
                if (!(push(&stack, data))){ 
					stack_free(&stack);
					return ERR_INTERNAL;
				}
				expect_operand = false;
                int adv_err = advance_shared_token(lexer, current_token);
                if (adv_err != ERR_OK) {
                    stack_free(&stack);
                    return adv_err;
                }
                break;
            }

            case '>': {
				size_t token_line = current_token->line;
                size_t token_col = current_token->column;
				int r_rules = reduce_rules(&stack, token_line, token_col); 
                if(r_rules != 0){
                    stack_free (&stack);
                    return r_rules;
                }
                tselem_t *first_elem = stack.top;
                
                if(first_elem != NULL && 
                    first_elem->data.symbol_type == SYMBOL_NON_TERMINAL &&
                    first_elem->next != NULL && 
                    first_elem->next->data.token_type == TOKEN_EOF &&
                    col == index_dollar)
                {
                    if(out_node != NULL){
                        *out_node = first_elem->data.ast_node;
                        first_elem->data.ast_node = NULL; 
                    }
                    end_parsing = true;
                }
				break;
            }
            
            case '<': {
                terminal_top->data.shift_mark = true;
                ast_node_t *leaf_node = NULL;
				size_t token_line = current_token->line;
                size_t token_col = current_token->column;
				token_type_t token_type = current_token->type;
                
				if (token_type == TOKEN_INTEGER) {
					int64_t val = parser_int_literal(current_token->lexeme);
					leaf_node = ast_new_int_lit(val, token_line, token_col);
					if (leaf_node == NULL) {
						stack_free(&stack);
						return ERR_INTERNAL;
					}
					expect_operand = false;
				} 
				else if (token_type == TOKEN_DOUBLE) {
					double val = parser_double_literal(current_token->lexeme);
					leaf_node = ast_new_double_lit(val, token_line, token_col);
					if (leaf_node == NULL) {
						stack_free(&stack);
						return ERR_INTERNAL;
					}
					expect_operand = false;
				}
				else if (token_type == TOKEN_STRING) {
					char *decoded = decode_string_literal(current_token->lexeme);
					if (decoded == NULL) {
						stack_free(&stack);
						return ERR_INTERNAL;
					}
					leaf_node = ast_new_string_lit(decoded, token_line, token_col);
					free(decoded);

					if (leaf_node == NULL) {
						stack_free(&stack);
						return ERR_INTERNAL;
					}
					expect_operand = false;
				}
				else{
					expect_operand = true;
				}
				
				tdata_t data = {
					.symbol_type = SYMBOL_TERMINAL,
					.token_type = token_type,
					.shift_mark = false,
					.ast_node = leaf_node
				};

				if (!push(&stack, data)) {
					if (leaf_node != NULL) 
						ast_free(leaf_node);
					stack_free(&stack);
					return ERR_INTERNAL;
				}

				int adv_err = advance_shared_token(lexer, current_token);
				if(adv_err != ERR_OK){
					stack_free(&stack);
					return adv_err;
				}
				break;
			}	
			case R_ERR:
			default:
				stack_free(&stack);
				return ERR_SYNTAX;
		}
	}

    stack_free(&stack);
    return ERR_OK;
}
