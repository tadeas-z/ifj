// Implementation of the IFJ26 imperative language compiler
// lexer.c
// Author(s): Tadeáš Zonyga (xzonygt00)

#include "lexer.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char* data;
    size_t length, capacity;
} buffer_t;

static int digit(int c) { return c >= '0' && c <= '9'; }
static int letter(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static int id_start(int c) { return letter(c) || c == '_'; }
static int id_part(int c) { return id_start(c) || digit(c); }

void token_dispose(token_t* token)
{
    if (token) {
        free(token->lexeme);
        *token = (token_t) { 0 };
    }
}

lexer_status_t lexer_init(lexer_t* lexer, FILE* input)
{
    if (!lexer)
        return LEXER_INTERNAL_ERROR;
    *lexer = (lexer_t) { .input = input, .line = 1, .column = 1, .at_line_start = 1 };
    if (!input)
        return lexer->status = LEXER_INTERNAL_ERROR;
    lexer->current = fgetc(input);
    lexer->lookahead = fgetc(input);
    if (ferror(input))
        lexer->status = LEXER_INTERNAL_ERROR;
    return lexer->status;
}

static void advance(lexer_t* lexer)
{
    if (lexer->current == EOF)
        return;
    if (lexer->current == '\n') {
        ++lexer->line;
        lexer->column = 1;
    } else {
        ++lexer->column;
    }
    lexer->current = lexer->lookahead;
    lexer->lookahead = fgetc(lexer->input);
    if (ferror(lexer->input))
        lexer->status = LEXER_INTERNAL_ERROR;
}

static int take(lexer_t* lexer, buffer_t* buffer)
{
    if (buffer->length + 1 >= buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity * 2 : 32;
        if (buffer->capacity > SIZE_MAX / 2) {
            lexer->status = LEXER_INTERNAL_ERROR;
            return 0;
        }
        char* data = realloc(buffer->data, capacity);
        if (!data) {
            lexer->status = LEXER_INTERNAL_ERROR;
            return 0;
        }
        buffer->data = data;
        buffer->capacity = capacity;
    }
    buffer->data[buffer->length++] = (char)lexer->current;
    buffer->data[buffer->length] = '\0';
    advance(lexer);
    return lexer->status == LEXER_OK;
}

static lexer_status_t invalid(lexer_t* lexer)
{
    if (lexer->status == LEXER_OK)
        lexer->status = LEXER_ERROR;
    return lexer->status;
}

static token_type_t keyword(const char* text)
{
    static const struct {
        const char* text;
        token_type_t type;
    } words[] = {
        { "def", TOKEN_DEF }, { "do", TOKEN_DO }, { "Double", TOKEN_TYPE_DOUBLE },
        { "else", TOKEN_ELSE }, { "if", TOKEN_IF }, { "import", TOKEN_IMPORT },
        { "Int", TOKEN_TYPE_INT }, { "return", TOKEN_RETURN },
        { "String", TOKEN_TYPE_STRING }, { "then", TOKEN_THEN },
        { "Unit", TOKEN_TYPE_UNIT }, { "val", TOKEN_VAL }, { "var", TOKEN_VAR },
        { "while", TOKEN_WHILE }, { "Boolean", TOKEN_RESERVED_KEYWORD },
        { "case", TOKEN_RESERVED_KEYWORD }, { "class", TOKEN_RESERVED_KEYWORD },
        { "extension", TOKEN_RESERVED_KEYWORD }, { "for", TOKEN_RESERVED_KEYWORD },
        { "List", TOKEN_RESERVED_KEYWORD }, { "match", TOKEN_RESERVED_KEYWORD },
        { "type", TOKEN_RESERVED_KEYWORD }
    };
    for (size_t i = 0; i < sizeof words / sizeof words[0]; ++i)
        if (strcmp(text, words[i].text) == 0)
            return words[i].type;
    return TOKEN_IDENTIFIER;
}

/* Footnote 2 of the diagram: a Double without '_' and d/D must be a finite
 * binary64 value; the lexeme itself keeps its original spelling.
 */
static void double_in_range(lexer_t* lexer, const buffer_t* buffer)
{
    char* normalized = malloc(buffer->length + 1);
    if (!normalized) {
        lexer->status = LEXER_INTERNAL_ERROR;
        return;
    }
    size_t length = 0;
    for (size_t i = 0; i < buffer->length; ++i)
        if (buffer->data[i] != '_' && buffer->data[i] != 'd' && buffer->data[i] != 'D')
            normalized[length++] = buffer->data[i];
    normalized[length] = '\0';
    char* end;
    errno = 0;
    double value = strtod(normalized, &end);
    /* Nonzero subnormals are binary64 values, even when strtod reports
     * ERANGE. Overflow and underflow to zero are outside the range.
     */
    if (*end || !isfinite(value) || (errno == ERANGE && value == 0.0))
        invalid(lexer);
    free(normalized);
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt (summary): "Refactor the IFJ26 lexer to implement my complete
//          lexer automaton diagram with explicit enum states and switch
//          dispatch, one scanner per diagram section; consume only on
//          transitions, keep the next token's first character, keep layout
//          in skip_layout helpers and the public contract unchanged."
// Scope: scan_result_t, comment_t, all *_state_t enums and the scanners
//        scan_identifier, scan_main, scan_operator, scan_number,
//        scan_string, line_comment_body, block_comment_body, scan_slash,
//        scan_token.

/* Outcome of one run of the automaton from the start state s. A comment is
 * not a token: after p1/p2 the automaton returns to s (diagram edges p1 -> s
 * and p2 -> s), which lexer_next realises by running skip_layout again.
 */
typedef enum {
    SCAN_TOKEN,
    SCAN_LINE_COMMENT,
    SCAN_BLOCK_COMMENT,
    SCAN_ERROR
} scan_result_t;

/* Auxiliary data of p2: whether the comment contained LF and where. */
typedef struct {
    int multiline;
    size_t first_line, first_column;
} comment_t;

/* Every scanner follows the diagram's termination rule: when no transition
 * applies, an accepting state f returns its token and leaves current for the
 * next call, a non-accepting state q is LEXER_ERROR. Transitions consume
 * current through take(). EOF is never an ASCII character, so it never
 * matches a transition.
 */

/* ---- Identifiers, keywords and @main ---------------------------------- */

typedef enum {
    ID_UNDERSCORE, /* q1: a lone '_' is not an identifier yet */
    ID_NAME /* f1: identifier or keyword */
} id_state_t;

static scan_result_t scan_identifier(lexer_t* lexer, buffer_t* buffer, token_type_t* type)
{
    /* s -'_'-> q1, s -a..z+A..Z-> f1 */
    id_state_t state = lexer->current == '_' ? ID_UNDERSCORE : ID_NAME;
    if (!take(lexer, buffer))
        return SCAN_ERROR;
    for (;;) {
        switch (state) {
        case ID_UNDERSCORE:
            if (!id_part(lexer->current)) {
                invalid(lexer);
                return SCAN_ERROR;
            }
            state = ID_NAME;
            break;
        case ID_NAME:
            if (!id_part(lexer->current)) {
                /* Keywords are classified only for the complete name. */
                *type = keyword(buffer->data);
                return SCAN_TOKEN;
            }
            break;
        }
        if (!take(lexer, buffer))
            return SCAN_ERROR;
    }
}

typedef enum {
    MAIN_AT, /* q2: "@" */
    MAIN_M, /* q3: "@m" */
    MAIN_MA, /* q4: "@ma" */
    MAIN_MAI, /* q5: "@mai" */
    MAIN_DONE /* f2: "@main" */
} main_state_t;

static scan_result_t scan_main(lexer_t* lexer, buffer_t* buffer, token_type_t* type)
{
    main_state_t state = MAIN_AT; /* s -'@'-> q2 */
    if (!take(lexer, buffer))
        return SCAN_ERROR;
    for (;;) {
        int c = lexer->current;
        int expected = 0;
        switch (state) {
        case MAIN_AT:
            expected = 'm';
            break;
        case MAIN_M:
            expected = 'a';
            break;
        case MAIN_MA:
            expected = 'i';
            break;
        case MAIN_MAI:
            expected = 'n';
            break;
        case MAIN_DONE:
            /* Boundary of @main: a following name character is an error. */
            if (id_part(c)) {
                invalid(lexer);
                return SCAN_ERROR;
            }
            *type = TOKEN_MAIN;
            return SCAN_TOKEN;
        }
        if (c != expected) {
            invalid(lexer);
            return SCAN_ERROR;
        }
        state = (main_state_t)(state + 1);
        if (!take(lexer, buffer))
            return SCAN_ERROR;
    }
}

/* ---- Operators and delimiters ----------------------------------------- */

typedef enum {
    OP_SINGLE, /* f3..f10: ( ) : , ; + - * have no outgoing transition */
    OP_ASSIGN, /* f11: "=" */
    OP_LESS, /* f12: "<" */
    OP_GREATER, /* f13: ">" */
    OP_BANG /* q6: a lone "!" is an error */
} operator_state_t;

static scan_result_t scan_operator(lexer_t* lexer, buffer_t* buffer, token_type_t* type)
{
    operator_state_t state = OP_SINGLE;
    switch (lexer->current) { /* transitions from s */
    case '(':
        *type = TOKEN_LPAREN;
        break;
    case ')':
        *type = TOKEN_RPAREN;
        break;
    case ':':
        *type = TOKEN_COLON;
        break;
    case ',':
        *type = TOKEN_COMMA;
        break;
    case ';':
        *type = TOKEN_SEMICOLON;
        break;
    case '+':
        *type = TOKEN_PLUS;
        break;
    case '-':
        *type = TOKEN_MINUS;
        break;
    case '*':
        *type = TOKEN_STAR;
        break;
    case '=':
        state = OP_ASSIGN;
        *type = TOKEN_ASSIGN;
        break;
    case '<':
        state = OP_LESS;
        *type = TOKEN_LESS;
        break;
    case '>':
        state = OP_GREATER;
        *type = TOKEN_GREATER;
        break;
    case '!':
        state = OP_BANG;
        break;
    default:
        invalid(lexer);
        return SCAN_ERROR;
    }
    if (!take(lexer, buffer))
        return SCAN_ERROR;

    token_type_t with_equal = TOKEN_EOF;
    switch (state) {
    case OP_SINGLE:
        return SCAN_TOKEN;
    case OP_ASSIGN: /* f11 -'='-> f14 */
        with_equal = TOKEN_EQUAL;
        break;
    case OP_LESS: /* f12 -'='-> f15 */
        with_equal = TOKEN_LESS_EQUAL;
        break;
    case OP_GREATER: /* f13 -'='-> f16 */
        with_equal = TOKEN_GREATER_EQUAL;
        break;
    case OP_BANG: /* q6 -'='-> f17, otherwise error */
        if (lexer->current != '=') {
            invalid(lexer);
            return SCAN_ERROR;
        }
        with_equal = TOKEN_NOT_EQUAL;
        break;
    }
    if (lexer->current != '=')
        return SCAN_TOKEN; /* f11, f12, f13 accept */
    *type = with_equal; /* f14..f17 have no outgoing transition */
    return take(lexer, buffer) ? SCAN_TOKEN : SCAN_ERROR;
}

/* ---- Numbers and dot -------------------------------------------------- */

typedef enum {
    NUM_INT, /* f18: Int */
    NUM_INT_SEP, /* q7: '_' in Int, digit required */
    NUM_POINT, /* q8: '.' after Int, digit required */
    NUM_FRACTION, /* f19: Double with fraction */
    NUM_FRACTION_SEP, /* q9: '_' in fraction, digit required */
    NUM_EXP, /* q10: e/E, sign or digit required */
    NUM_EXP_SIGN, /* q11: exponent sign, digit required */
    NUM_EXPONENT, /* f20: Double with exponent */
    NUM_EXPONENT_SEP, /* q12: '_' in exponent, digit required */
    NUM_SUFFIX, /* f21: Double with d/D suffix */
    NUM_DOT /* f22: TOKEN_DOT, or the start of .5 */
} number_state_t;

/* Footnote 1 of the diagram: an accepted nonzero Int must not start with 0. */
static int integer_valid(const buffer_t* buffer)
{
    if (buffer->data[0] != '0')
        return 1;
    for (size_t i = 0; i < buffer->length; ++i)
        if (buffer->data[i] >= '1' && buffer->data[i] <= '9')
            return 0;
    return 1;
}

static scan_result_t accept_number(lexer_t* lexer, const buffer_t* buffer,
    number_state_t state, token_type_t* type)
{
    switch (state) {
    case NUM_INT:
        *type = TOKEN_INTEGER;
        if (!integer_valid(buffer)) {
            invalid(lexer);
            return SCAN_ERROR;
        }
        return SCAN_TOKEN;
    case NUM_DOT:
        *type = TOKEN_DOT;
        return SCAN_TOKEN;
    case NUM_FRACTION:
    case NUM_EXPONENT:
    case NUM_SUFFIX:
        *type = TOKEN_DOUBLE;
        double_in_range(lexer, buffer);
        return lexer->status == LEXER_OK ? SCAN_TOKEN : SCAN_ERROR;
    default: /* q7..q12 are not accepting */
        invalid(lexer);
        return SCAN_ERROR;
    }
}

static scan_result_t scan_number(lexer_t* lexer, buffer_t* buffer, token_type_t* type)
{
    /* s -0..9-> f18, s -'.'-> f22 */
    number_state_t state = digit(lexer->current) ? NUM_INT : NUM_DOT;
    if (!take(lexer, buffer))
        return SCAN_ERROR;
    for (;;) {
        int c = lexer->current;
        number_state_t next = state;
        int moved = 1;
        switch (state) {
        case NUM_INT:
            if (digit(c))
                next = NUM_INT;
            else if (c == '_')
                next = NUM_INT_SEP;
            else if (c == '.' && digit(lexer->lookahead))
                next = NUM_POINT; /* footnote 3: otherwise '.' is the next token */
            else if (c == 'e' || c == 'E')
                next = NUM_EXP;
            else
                moved = 0;
            break;
        case NUM_INT_SEP:
            moved = digit(c);
            next = NUM_INT;
            break;
        case NUM_POINT:
            moved = digit(c);
            next = NUM_FRACTION;
            break;
        case NUM_FRACTION:
            if (digit(c))
                next = NUM_FRACTION;
            else if (c == '_')
                next = NUM_FRACTION_SEP;
            else if (c == 'e' || c == 'E')
                next = NUM_EXP;
            else if (c == 'd' || c == 'D')
                next = NUM_SUFFIX;
            else
                moved = 0;
            break;
        case NUM_FRACTION_SEP:
            moved = digit(c);
            next = NUM_FRACTION;
            break;
        case NUM_EXP:
            if (c == '+' || c == '-')
                next = NUM_EXP_SIGN;
            else if (digit(c))
                next = NUM_EXPONENT;
            else
                moved = 0;
            break;
        case NUM_EXP_SIGN:
            moved = digit(c);
            next = NUM_EXPONENT;
            break;
        case NUM_EXPONENT:
            if (digit(c))
                next = NUM_EXPONENT;
            else if (c == '_')
                next = NUM_EXPONENT_SEP;
            else if (c == 'd' || c == 'D')
                next = NUM_SUFFIX;
            else
                moved = 0;
            break;
        case NUM_EXPONENT_SEP:
            moved = digit(c);
            next = NUM_EXPONENT;
            break;
        case NUM_SUFFIX:
            moved = 0;
            break;
        case NUM_DOT:
            moved = digit(c);
            next = NUM_FRACTION;
            break;
        }
        if (!moved)
            return accept_number(lexer, buffer, state, type);
        state = next;
        if (!take(lexer, buffer))
            return SCAN_ERROR;
    }
}

/* ---- Ordinary and triple-quoted strings ------------------------------- */

typedef enum {
    STR_OPEN, /* q13: first quote */
    STR_EMPTY, /* f23: "" */
    STR_BODY, /* q14: ordinary string content */
    STR_ESCAPE, /* q15: after '\' */
    STR_END, /* f24: closed ordinary string */
    RAW_BODY, /* q16: raw, no closing quote seen */
    RAW_QUOTE1, /* q17: raw, one closing quote */
    RAW_QUOTE2, /* q18: raw, two closing quotes */
    RAW_END /* f25: closed raw string */
} string_state_t;

/* Sigma of the string section is ASCII 0..127; EOF is not in Sigma. */
static int ascii(int c) { return c >= 0 && c <= 127; }

static scan_result_t scan_string(lexer_t* lexer, buffer_t* buffer)
{
    string_state_t state = STR_OPEN; /* s -'"'-> q13 */
    if (!take(lexer, buffer))
        return SCAN_ERROR;
    for (;;) {
        int c = lexer->current;
        int moved = ascii(c);
        string_state_t next = state;
        switch (state) {
        case STR_OPEN:
        case STR_BODY:
            if (c == '"')
                next = state == STR_OPEN ? STR_EMPTY : STR_END;
            else if (c == '\\')
                next = STR_ESCAPE;
            else if (c == '\n')
                moved = 0; /* a real LF is an error in an ordinary string */
            else
                next = STR_BODY;
            break;
        case STR_EMPTY:
            /* Return "" only after checking that current is not a third ". */
            moved = c == '"';
            next = RAW_BODY;
            break;
        case STR_ESCAPE:
            moved = c == '\\' || c == '"' || c == 'n' || c == 'r' || c == 't';
            next = STR_BODY;
            break;
        case RAW_BODY:
        case RAW_QUOTE1:
        case RAW_QUOTE2:
            /* LF and '\' are ordinary content of a raw string. */
            if (c != '"')
                next = RAW_BODY;
            else if (state == RAW_BODY)
                next = RAW_QUOTE1;
            else if (state == RAW_QUOTE1)
                next = RAW_QUOTE2;
            else
                next = RAW_END;
            break;
        case STR_END:
        case RAW_END:
            moved = 0;
            break;
        }
        if (!moved) {
            if (state == STR_EMPTY || state == STR_END || state == RAW_END)
                return SCAN_TOKEN; /* f23, f24, f25 */
            invalid(lexer); /* q13..q18, including EOF */
            return SCAN_ERROR;
        }
        state = next;
        if (!take(lexer, buffer))
            return SCAN_ERROR;
    }
}

/* ---- Comments (DFA extended with a depth counter) --------------------- */

/* p1: every character except LF stays in the comment. LF (the edge p1 -> s
 * with the line-end action) and EOF are left for the layout helpers.
 */
static void line_comment_body(lexer_t* lexer)
{
    while (lexer->status == LEXER_OK && lexer->current != '\n' && lexer->current != EOF)
        advance(lexer);
}

/* p2, entered after the opening slash and star with depth = 1. The pairs
 * slash-star and star-slash are read with lookahead and both characters
 * consumed, so they never overlap. EOF in p2 is an error.
 */
static void block_comment_body(lexer_t* lexer, comment_t* comment)
{
    size_t depth = 1;
    while (lexer->status == LEXER_OK) {
        if (lexer->current == EOF) {
            invalid(lexer);
            return;
        }
        if (lexer->current == '\n' && !comment->multiline) {
            comment->multiline = 1;
            comment->first_line = lexer->line;
            comment->first_column = lexer->column;
        }
        if (lexer->current == '/' && lexer->lookahead == '*') {
            if (depth == SIZE_MAX) {
                lexer->status = LEXER_INTERNAL_ERROR;
                return;
            }
            ++depth; /* p2 -slash-star-> p2 / ++depth */
            advance(lexer);
            advance(lexer);
        } else if (lexer->current == '*' && lexer->lookahead == '/') {
            advance(lexer);
            advance(lexer);
            if (--depth == 0)
                return; /* p2 -star-slash and depth = 1 / --depth-> s */
        } else {
            advance(lexer);
        }
    }
}

/* s -'/'-> f26: TOKEN_SLASH unless a comment follows. The slash is consumed
 * once; for a comment the caller discards the buffer.
 */
static scan_result_t scan_slash(lexer_t* lexer, buffer_t* buffer, token_type_t* type,
    comment_t* comment)
{
    if (!take(lexer, buffer))
        return SCAN_ERROR;
    switch (lexer->current) {
    case '/': /* f26 -'/'-> p1 */
        advance(lexer);
        line_comment_body(lexer);
        return lexer->status == LEXER_OK ? SCAN_LINE_COMMENT : SCAN_ERROR;
    case '*': /* f26 -'*' / depth = 1-> p2 */
        advance(lexer);
        block_comment_body(lexer, comment);
        return lexer->status == LEXER_OK ? SCAN_BLOCK_COMMENT : SCAN_ERROR;
    default:
        *type = TOKEN_SLASH;
        return SCAN_TOKEN;
    }
}

/* s: the first character selects the section of the automaton. Characters
 * without a transition from s are LEXER_ERROR.
 */
static scan_result_t scan_token(lexer_t* lexer, buffer_t* buffer, token_type_t* type,
    comment_t* comment)
{
    int c = lexer->current;
    if (id_start(c))
        return scan_identifier(lexer, buffer, type);
    if (digit(c) || c == '.')
        return scan_number(lexer, buffer, type);
    switch (c) {
    case '@':
        return scan_main(lexer, buffer, type);
    case '"':
        *type = TOKEN_STRING;
        return scan_string(lexer, buffer);
    case '/':
        return scan_slash(lexer, buffer, type, comment);
    case '(':
    case ')':
    case ':':
    case ',':
    case ';':
    case '+':
    case '-':
    case '*':
    case '=':
    case '<':
    case '>':
    case '!':
        return scan_operator(lexer, buffer, type);
    default:
        invalid(lexer);
        return SCAN_ERROR;
    }
}

static int horizontal_space(int c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v';
}

static void begin_physical_line(lexer_t* lexer)
{
    lexer->at_line_start = 1;
    lexer->physical_indentation = 0;
    lexer->leading_tab = 0;
    lexer->prefix_complete = 0;
}

static int newline_token(lexer_t* lexer, token_t* output, size_t line, size_t column)
{
    output->line = line;
    output->column = column;
    output->indentation = lexer->logical_indentation;
    output->lexeme = malloc(2);
    if (output->lexeme == NULL) {
        lexer->status = LEXER_INTERNAL_ERROR;
        return -1;
    }
    memcpy(output->lexeme, "\n", 2);
    output->length = 1;
    output->type = TOKEN_NEWLINE;
    lexer->logical_has_tokens = 0;
    return 1;
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt (summary): "Refactor skip_layout() without changing behavior: extract
//          horizontal whitespace with indentation tracking and // comment
//          skipping into small static helpers, simplify the multiline block
//          comment branch and keep the 1/0/-1 return contract."
// Scope: skip_horizontal_space, logical_line_end.

/* Spaces before the first token of a physical line count as indentation. */
static void skip_horizontal_space(lexer_t* lexer)
{
    while (lexer->status == LEXER_OK && horizontal_space(lexer->current)) {
        if (lexer->at_line_start && !lexer->prefix_complete) {
            if (lexer->current == ' ')
                ++lexer->physical_indentation;
            if (lexer->current == '\t')
                lexer->leading_tab = 1;
        }
        advance(lexer);
    }
}

/* After a physical line break: 1 emits a logical newline, 0 continues. */
static int logical_line_end(lexer_t* lexer, token_t* output, size_t line, size_t column)
{
    if (lexer->status != LEXER_OK)
        return -1;
    if (lexer->paren_depth == 0 && lexer->logical_has_tokens)
        return newline_token(lexer, output, line, column);
    return 0;
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt (summary): "Fix skip_layout() rejecting a single-line block comment
//          after a multiline block comment; accept whitespace and block
//          comments before LF/EOF, keep rejecting real tokens there."
// Scope: multiline_comment_tail (the // branch is covered by the block below).

/* Spec 1.1: after a multiline comment only whitespace may precede the line
 * end. A comment without LF counts as whitespace; one with LF is a line end
 * itself and the same rule then applies after it. Stops at LF/EOF, 0 or -1.
 */
static int multiline_comment_tail(lexer_t* lexer, token_t* output)
{
    for (;;) {
        skip_horizontal_space(lexer);
        if (lexer->status != LEXER_OK)
            return -1;
        if (lexer->current != '/' || lexer->lookahead != '*')
            break;
        comment_t ignored = { 0 };
        output->line = lexer->line;
        output->column = lexer->column;
        advance(lexer);
        advance(lexer);
        block_comment_body(lexer, &ignored);
        if (lexer->status != LEXER_OK)
            return -1;
    }
    // AI-generated using: Claude Code
    //              model: Claude Opus 5.5
    // Prompt (summary): "Refactor the IFJ26 lexer to my automaton diagram.
    //          Settled: a // comment after a multiline block comment is the
    //          expected line end itself and adds no second NEWLINE."
    // Scope: this // branch.
    if (lexer->current == '/' && lexer->lookahead == '/') {
        advance(lexer);
        advance(lexer);
        line_comment_body(lexer);
        if (lexer->status != LEXER_OK)
            return -1;
    }
    if (lexer->current != '\n' && lexer->current != EOF) {
        output->line = lexer->line;
        output->column = lexer->column;
        invalid(lexer);
        return -1;
    }
    return 0;
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt (summary): "Refactor the IFJ26 lexer to implement my complete
//          lexer automaton diagram with explicit enum states and switch
//          dispatch; keep indentation, logical lines and comment
//          coordination in skip_layout helpers. Settled: a leading tab is
//          an error only on a line with a real token, comment-only lines
//          are ignored."
// Scope: skip_layout, multiline_comment_end, accept_token, lexer_next.

/* Whitespace and LF before the automaton starts in s.
 * 1: emitted logical newline, 0: ready for s or EOF, -1: error.
 */
static int skip_layout(lexer_t* lexer, token_t* output)
{
    int result;
    while (lexer->status == LEXER_OK) {
        if (horizontal_space(lexer->current)) {
            skip_horizontal_space(lexer);
            continue;
        }
        if (lexer->current != '\n')
            return 0;
        size_t line = lexer->line, column = lexer->column;
        advance(lexer);
        begin_physical_line(lexer);
        if ((result = logical_line_end(lexer, output, line, column)) != 0)
            return result;
    }
    return -1;
}

/* A block comment with LF is one line end at its first LF. */
static int multiline_comment_end(lexer_t* lexer, token_t* output, const comment_t* comment)
{
    if (multiline_comment_tail(lexer, output) != 0)
        return -1;
    if (lexer->current == '\n') {
        advance(lexer);
        begin_physical_line(lexer);
    }
    return logical_line_end(lexer, output, comment->first_line, comment->first_column);
}

/* Layout rules and metadata for a real token; comments never reach here. */
static lexer_status_t accept_token(lexer_t* lexer, token_t* output, buffer_t* buffer,
    token_type_t type)
{
    if (lexer->at_line_start
        && (lexer->leading_tab
            || (lexer->logical_has_tokens
                && lexer->physical_indentation <= lexer->logical_indentation))) {
        free(buffer->data);
        return invalid(lexer);
    }
    if (type == TOKEN_LPAREN) {
        if (lexer->paren_depth == SIZE_MAX) {
            free(buffer->data);
            return lexer->status = LEXER_INTERNAL_ERROR;
        }
        ++lexer->paren_depth;
    } else if (type == TOKEN_RPAREN && lexer->paren_depth != 0) {
        --lexer->paren_depth;
    }
    output->starts_line = !lexer->logical_has_tokens;
    if (output->starts_line)
        lexer->logical_indentation = lexer->physical_indentation;
    output->indentation = lexer->logical_indentation;
    lexer->logical_has_tokens = 1;
    lexer->at_line_start = 0;
    output->type = type;
    output->lexeme = buffer->data;
    output->length = buffer->length;
    return LEXER_OK;
}

lexer_status_t lexer_next(lexer_t* lexer, token_t* output)
{
    if (!output)
        return LEXER_INTERNAL_ERROR;
    *output = (token_t) { 0 };
    if (!lexer)
        return LEXER_INTERNAL_ERROR;
    output->line = lexer->line;
    output->column = lexer->column;
    if (lexer->status != LEXER_OK)
        return lexer->status;
    if (!lexer->input)
        return lexer->status = LEXER_INTERNAL_ERROR;

    for (;;) {
        if (skip_layout(lexer, output) != 0)
            return lexer->status;
        output->line = lexer->line;
        output->column = lexer->column;
        if (lexer->current == EOF)
            return LEXER_OK; /* EOF in s */
        lexer->prefix_complete = 1;

        buffer_t buffer = { 0 };
        token_type_t type = TOKEN_EOF;
        comment_t comment = { 0 };
        scan_result_t result = scan_token(lexer, &buffer, &type, &comment);
        if (result == SCAN_TOKEN)
            return accept_token(lexer, output, &buffer, type);
        free(buffer.data);

        int line_end = 0;
        switch (result) {
        case SCAN_LINE_COMMENT:
            /* At LF skip_layout ends the line; at EOF the comment does. */
            if (lexer->current == EOF)
                line_end = logical_line_end(lexer, output, lexer->line, lexer->column);
            break;
        case SCAN_BLOCK_COMMENT:
            /* Without LF the comment separates tokens like whitespace. */
            if (comment.multiline)
                line_end = multiline_comment_end(lexer, output, &comment);
            break;
        default:
            return lexer->status;
        }
        if (line_end != 0)
            return lexer->status;
    }
}
