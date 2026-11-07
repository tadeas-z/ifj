#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "parser.h"
#include "parser_stack.h"
#include "astree.h"
#include "lexer.h"

bool stack_err_flag = false;
//macro relation_error, syntatic error
#define R_ERR ' ' 	

//stack function definition
static int stack_err (void){
	fprintf(stderr, "! WOARNING ! -> The program done some illegal operation. \n");
	stack_err_flag = true;
    return 2;
}

static void stack_init (tstack_t *s){
	s->top = NULL;
}

static void push (tstack_t *s, tdata_t d) {
	tselem_t *temp = (tselem_t * ) malloc(sizeof(tselem_t));
	if(temp != NULL){
		temp->data = d;
		temp->next = s->top;
		s->top = temp;
	}
	else
		stack_err();
}

static void pop (tstack_t *s){
	tselem_t *temp;
	if (s->top != NULL){
		temp = s->top;
		s->top = s->top->next;
		free (temp);
	}
}

static tdata_t top (tstack_t *s) {
	if(s->top != NULL)
		return (s->top->data);
	else{
		stack_err();
		tdata_t stack_is_empity = {SYMBOL_TERMINAL, 0, false, NULL};
		return stack_is_empity;
	}
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
            temp->next = next;
            free(temp);
        }
        s->top = NULL;
    }
}
//end stack function definition
const char precedence_table [9] [9] = {
		/*	  	 +		 -		 *		 /		 (			 )		 $		id		rel			*/
/*	+	*/{		'>',	'>',	'<',	'<',	'<',		'>',	'>',	'<',	'>'			},
/*	-	*/{		'>',	'>',	'<',	'<',	'<',		'>',	'>',	'<',	'>'			},
/*	*	*/{		'>',	'>',	'>',	'>',	'<',		'>',	'>',	'<',	'>'			},
/*	/	*/{		'>',	'>',	'>',	'>',	'<',		'>',	'>',	'<',	'>'			},
/*	(	*/{		'<',	'<',	'<',	'<',	'<',		'=',	R_ERR,	'<',	'<'			},
/*	)	*/{		'>',	'>',	'>',	'>',	R_ERR,		'>',	'>',	R_ERR,	'>'			},
/*	$	*/{		'<',	'<',	'<',	'<',	'<',		R_ERR,	R_ERR,	'<',	'<'			},		
/*	id	*/{		'>',	'>',	'>',	'>',	R_ERR, 		'>', 	'>', 	R_ERR,	'>'			},
/*	rel	*/{		'<',	'<',	'<',	'<',	'<',		'>',	'>',	'<',	'>'			}
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
}PrecedenceIndex;

static int get_precedence_index (token_type_t t){
	if (t == TOKEN_PLUS) 
		return index_add;
	else if (t == TOKEN_MINUS) 
		return index_sub;
	else if (t == TOKEN_STAR) 
		return index_mul;
	else if (t == TOKEN_SLASH) 
		return index_div;
	else if (t == TOKEN_LPAREN) 
		return index_left_par;
	else if (t == TOKEN_RPAREN) 
		return index_right_par;
	else if (t == TOKEN_EOF || t== TOKEN_NEWLINE || t==TOKEN_SEMICOLON || t== TOKEN_COMMA || 
			t == TOKEN_THEN || t== TOKEN_DO || t== TOKEN_ELSE || t== TOKEN_COLON)
		return index_dollar;
	else if (t== TOKEN_IDENTIFIER || t== TOKEN_INTEGER || t== TOKEN_DOUBLE)
		return index_id;
	else if (t == TOKEN_EQUAL || t == TOKEN_NOT_EQUAL || t == TOKEN_LESS || 
		t==TOKEN_LESS_EQUAL || t == TOKEN_GREATER || t == TOKEN_GREATER_EQUAL)
		return index_rel;
	else
		return -1;
}

static bool reduce_rules (tstack_t *stack){
    tdata_t control[5];
    int count_control = 0;
    while (stack->top != NULL){
        tdata_t temp = top(stack);
        pop(stack);

		if (temp.shift_mark){
            temp.shift_mark = false;
			push (stack, temp);
			break; 
		}
		
        control[count_control++] = temp;
        if(count_control > 3)
            return false;    	  
						  //_______________________________|shift_|________
    }                     //|symbol_type        |token_type|mark  |ast_node|
    tdata_t non_terminal = {SYMBOL_NON_TERMINAL, TOKEN_EOF, false, NULL};
    if (count_control == 0)
        return false;
    //rule E -> id 
    if(count_control == 1 && control[0].symbol_type == SYMBOL_TERMINAL){
            non_terminal.ast_node = control[0].ast_node;
            push(stack, non_terminal);
            return true;
    }
    if (count_control == 3){
        //pravidlo E -> ( E )
        if(control[2].token_type == TOKEN_LPAREN && 
			control[1].symbol_type == SYMBOL_NON_TERMINAL && 
			control[0].token_type == TOKEN_RPAREN){
			
            non_terminal.ast_node = control[1].ast_node;
            push(stack, non_terminal);
            return true;
        }  
		
        //pravidlo E -> E a E, a={+,-,*,/}
        if(control[2].symbol_type == SYMBOL_NON_TERMINAL && 
			control[1].symbol_type == SYMBOL_TERMINAL && 
			control[0].symbol_type == SYMBOL_NON_TERMINAL){
			
            ast_node_t *bin_node = (ast_node_t *) malloc(sizeof(ast_node_t));
            if(bin_node == NULL)
                return false;
            bin_node->kind = AST_BINARY;
            bin_node->binary.left = control[2].ast_node;
            bin_node->binary.right = control[0].ast_node;

            switch(control[1].token_type){
                case TOKEN_PLUS: bin_node->binary.op = AST_OP_ADD;
                break;
                case TOKEN_MINUS: bin_node->binary.op = AST_OP_SUB;
                break;
                case TOKEN_STAR: bin_node->binary.op = AST_OP_MUL;
                break;
                case TOKEN_SLASH: bin_node->binary.op = AST_OP_DIV;
                break;
				case TOKEN_EQUAL: bin_node->binary.op = AST_OP_EQ;
				break;
				case TOKEN_NOT_EQUAL: bin_node->binary.op = AST_OP_NE;
				break;
				case TOKEN_LESS: bin_node->binary.op = AST_OP_LT;
				break;
				case TOKEN_LESS_EQUAL: bin_node->binary.op = AST_OP_LE;
				break;
				case TOKEN_GREATER: bin_node->binary.op = AST_OP_GT;
				break;
				case TOKEN_GREATER_EQUAL: bin_node->binary.op = AST_OP_GTE;
				break;
				
                default:
                    free(bin_node);
                    return false;
            }
            non_terminal.ast_node = bin_node;
            push(stack, non_terminal);
            return true;
        }
    }
    return false;
}

// AI-generated using: Gemini
//               model: Gemini 3.6 flesh
// Prompt (summary): "Implement scaleton of parse_expression() function for precedence 
//           parser using shift-reduce algorithm, precedence matrix, stack operations,
//           and AST node creation for literals and variables."
// Scope: parse_expression(modified by xpopovd00), get_next_token(modified by xpopovd00)
// More details on: ifj-private/src/parser/parser_skeleton_notes.txt

static token_t get_next_token (lexer_t *lexer){
    token_t token;
    lexer_status_t status = lexer_next (lexer, &token);
    if (status != LEXER_OK || status != LEXER_ERROR || status != EOF)
        return token;
}

int parse_expression(lexer_t *lexer, ast_node_t **out_node) {
    tstack_t stack;
    stack_init(&stack);

    tdata_t dollar = {SYMBOL_TERMINAL, TOKEN_EOF, false, NULL};
    push(&stack, dollar);
    bool end_parsing = false;
    token_t input = get_next_token(lexer);

    while(!(end_parsing)){
        tselem_t *terminal_top = get_top_terminal(&stack);
        if (terminal_top == NULL){
            stack_free(&stack);
            return 2;
        }
        int row = get_precedence_index (terminal_top->data.token_type);
        int col = get_precedence_index (input.type);

        if(row < 0 || row >= 9 || col < 0 || col >=  9){
            stack_free (&stack);
            return 2;
        }
        char rel = precedence_table[row][col];
        switch (rel){
            case '=': {
                tdata_t data = {SYMBOL_TERMINAL, input.type, false, NULL};
                push (&stack, data);
                input = get_next_token (lexer);
            }
            break;
            case '>': {
                if(reduce_rules(&stack) == false){
                    stack_free (&stack);
                    return 2;
                }
                tselem_t *first_elem = stack.top;
                
                if(first_elem != NULL && 
                    first_elem->data.symbol_type == SYMBOL_NON_TERMINAL &&
                    first_elem->next != NULL && 
                    first_elem->next->data.token_type == TOKEN_EOF &&
                    col == index_dollar)
                {
                    if(out_node != NULL)
                        *out_node = first_elem->data.ast_node;
                    end_parsing = true;
                }
            }
            break;

            case '<': {
                terminal_top->data.shift_mark = true;
                ast_node_t *leaf_node = NULL;
                
                if(input.type == TOKEN_INTEGER || input.type == TOKEN_DOUBLE || 
                    input.type == TOKEN_STRING || input.type == TOKEN_IDENTIFIER)
                {
                    leaf_node = (ast_node_t *) malloc(sizeof(ast_node_t));
                    if(leaf_node != NULL){    
                        if (input.type == TOKEN_INTEGER) {
                            leaf_node->kind = AST_INT_LIT;
                            leaf_node->int_lit.value = atoll(input.lexeme ? input.lexeme : "0");
                        } 
                        else if (input.type == TOKEN_DOUBLE) {
                            leaf_node->kind = AST_DOUBLE_LIT;
                            leaf_node->double_lit.value = atof(input.lexeme ? input.lexeme : "0.0");
                        }
                        else if (input.type == TOKEN_STRING) {
                            leaf_node->kind = AST_STRING_LIT;
                            leaf_node->string_lit.value = input.lexeme;
                        }
                        else if (input.type == TOKEN_IDENTIFIER) {
                            leaf_node->kind = AST_VAR_REF;
                            leaf_node->var_ref.name = input.lexeme;
                        }
                    }
                }
        
            tdata_t data = {
                    .symbol_type = SYMBOL_TERMINAL,
                    .token_type = input.type,
                    .shift_mark = false,
                    .ast_node = leaf_node
                };
            push(&stack, data);
            input = get_next_token(lexer);
            
            break;
            case R_ERR:
            default:
                stack_free(&stack);
                return 2;
            }
        }
    }
    stack_free(&stack);
    return 0;
}
