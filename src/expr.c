/**
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 43):
 *
 * GitHub Co-pilot and <jens@bennerhq.com> wrote this file.  As long as you 
 * retain this notice you can do whatever you want with this stuff. If we meet 
 * some day, and you think this stuff is worth it, you can buy me a beer in 
 * return.   
 *
 * /benner
 * ----------------------------------------------------------------------------
 */

/**
 * expr.c - Expression parser
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "../hdr/dmalloc.h"
#include "../hdr/exec.h"
#include "../hdr/expr.h"

#define IS_SPACE        " \t\n\r\v\f"
#define IS_INT          "0123456789"
#define IS_DOUBLE       IS_INT "."
#define IS_ALPHA        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_"
#define IS_ALPHA_INT    IS_ALPHA IS_INT

#define MAX_NAME_LEN    (32)
#define MAX_CODE_SIZE   (1024)

#define TOK_BASE        (1000)
#   define TOK_COLON       (TOK_BASE + 1)
#   define TOK_LPAREN      (TOK_BASE + 2)
#   define TOK_RPAREN      (TOK_BASE + 3)
#   define TOK_TRUE        (TOK_BASE + 4)
#   define TOK_FALSE       (TOK_BASE + 5)
#   define TOK_NUMBER      (TOK_BASE + 6)
#   define TOK_ID_NAME     (TOK_BASE + 7)
#   define TOK_VAR_IDX     (TOK_BASE + 8)
#   define TOK_VAR_STR     (TOK_BASE + 9)
#   define TOK_CONDITION   (TOK_BASE + 10) 
#   define TOK_END         (TOK_BASE + 11)

typedef struct {
    const char *expr;
    const char *expr_begin;
    const char *expr_end;
    
    const Variable *variables;
    
    Instruction *code; 
    int code_size;

    OpCode op;
    DataType type;
    union {
        char name[MAX_NAME_LEN];
        const char *str;
        double value;
    };
 } ParseState;

const struct {
    const char *str;
    OpCode op;
} op_symbols[] = {
    {.str = "",     .op = OP_NOP},
    {.str = "+",    .op = OP_ADD},
    {.str = "-",    .op = OP_SUB},
    {.str = "*",    .op = OP_MUL},
    {.str = "/",    .op = OP_DIV},
    {.str = "!=",   .op = OP_NEQ},
    {.str = "<=",   .op = OP_LE},
    {.str = ">=",   .op = OP_GE},
    {.str = "<",    .op = OP_LT},
    {.str = ">",    .op = OP_GT},
    {.str = "=",    .op = OP_EQ},
    {.str = "&",    .op = OP_AND},
    {.str = "|",    .op = OP_OR},
    {.str = "!",    .op = OP_NOT},
    {.str = "in",   .op = OP_IN_STR},
    {.str = "rin",  .op = OP_IN_REGEX_STR},
    {.str = "?",    .op = TOK_CONDITION},
    {.str = ":",    .op = TOK_COLON},
    {.str = "(",    .op = TOK_LPAREN},
    {.str = ")",    .op = TOK_RPAREN},
    {.str = "true", .op = TOK_TRUE},
    {.str = "false",.op = TOK_FALSE},
    {.str = "",     .op = TOK_END},
};

DataType parse_expr(ParseState *state);
DataType parse_term(ParseState *state);
DataType parse_factor(ParseState *state);
DataType parse_bool_expr(ParseState *state);
DataType parse_bool_term(ParseState *state);
DataType parse_bool_factor(ParseState *state);
DataType parse_rel_expr(ParseState *state);
DataType parse_arithmetic_expr(ParseState *state);
DataType parse_cond_expr(ParseState *state);

void parse_fatal(ParseState *state, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "\n\n*** ERROR: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "    Parsing: %s\n", state->expr_begin);
    fprintf(stderr, "            ");
    for (int i = 0; i < state->expr - state->expr_begin; i++) {
        fprintf(stderr, " ");
    }
    fprintf(stderr, "/^\\\n");
    va_end(args);

    exit(EXIT_FAILURE);
}

void set_token(ParseState *state, OpCode op, const char *match, int len) {
    const char *start = state->expr;
    if (match) {
        while (state->expr < state->expr_end && strchr(match, *state->expr)) {
            state->expr++;
        }
        len = state->expr - start;
    }
    else {
        state->expr += len;
    }

    if (len >= MAX_NAME_LEN - 1) {
        parse_fatal(state, "String too long %s\n", start);
    }
    char value[MAX_NAME_LEN];
    strncpy(value, start, len);
    value[len] = '\0';

    if (op == TOK_TRUE || op == TOK_FALSE) {
        state->type = VAR_NUMBER;
    }
    else if (op == TOK_NUMBER || op == TOK_VAR_IDX) {
        char *endptr;
        state->value = strtod(value, &endptr);
        if (*endptr != '\0') state->value = 0; // FIXME
        state->type = VAR_NUMBER;
    }
    else {
        strcpy(state->name, value);
        state->type = VAR_STRING;
    }
    state->op = op;
}

void next_token_str(ParseState *state, char find) {
    state->expr++;
    const char *start = state->expr;

    while (*state->expr != find && state->expr < state->expr_end) {
        state->expr++;
    }

    if (*state->expr != find) {
        parse_fatal(state, "'%s' Can't find trailing '%c'\n", state->expr_begin, find);
    }

    int len = state->expr - start;
    char *str = (char *) mem_malloc(len + 1);
    if (str == NULL) {
        parse_fatal(state, "Out of memory\n");
    }
    strncpy(str, start, len);
    state->op = TOK_VAR_STR;
    state->str = str;

    state->expr ++;
}

void next_token(ParseState *state) {
    while (state->expr < state->expr_end && strchr(IS_SPACE, *state->expr)) {
        state->expr++;
    }

    if (state->expr >= state->expr_end) {
        state->op = TOK_END;
        return;
    }

    if (*state->expr == '"') {
        next_token_str(state, '"');
        return;
    }

    if (*state->expr == '\'') {
        next_token_str(state, '\'');
        return;
    }

    if (*state->expr == '#') {
        state->expr++;
        set_token(state, TOK_VAR_IDX, IS_INT, 0);
        return;
    }

    if (strchr(IS_DOUBLE, *state->expr)) {
        set_token(state, TOK_NUMBER, IS_DOUBLE, 0);
        return;
    }

    for (int idx = 0; op_symbols[idx].op != TOK_END; idx++) {
        int len = strlen(op_symbols[idx].str);
        if (len && strncmp(state->expr, op_symbols[idx].str, len) == 0) {
            set_token(state, op_symbols[idx].op, NULL, len);
            return;
        }
    }

    if (strchr(IS_ALPHA, *state->expr)) {
        set_token(state, TOK_ID_NAME, IS_ALPHA_INT, 0);
        return;
    }

    parse_fatal(state, "Undefined symbol '%c'\n", *state->expr);
}

void emit_overflow(ParseState *state) {
    if (state->code_size >= MAX_CODE_SIZE - 1) {
        parse_fatal(state, "code array overflow\n");
    }
}

void emit(ParseState *state, OpCode op, double value, DataType type) {
    emit_overflow(state);

    state->code[state->code_size++] = (Instruction){
        .op = op, 
        .value = value, 
        .type = type
    };
}

void emit_str(ParseState *state, OpCode op, const char *str) {
    emit_overflow(state);

    state->code[state->code_size++] = (Instruction){
        .op = op, 
        .str = str, 
        .type = VAR_STRING
    };
}

void emit_type(ParseState *state, DataType data_type, OpCode op, DataType data_type_result) {
    switch (data_type) {
        case VAR_STRING:
            emit(state, OP_BASE_STR + (op - OP_BASE), 0, data_type_result);
            break;

        case VAR_NUMBER:
            emit(state, OP_BASE_NUM + (op - OP_BASE), 0, data_type_result);
            break;

        default:
            parse_fatal(state, "Unknown instruction type\n");
    }
}

DataType parse_expr(ParseState *state) {
    DataType data_type = VAR_UNKNOWN;

    if (state->op == TOK_TRUE || state->op == TOK_FALSE || 
        state->op == OP_NOT || state->op == TOK_LPAREN || 
        state->op == TOK_ID_NAME || state->op == TOK_VAR_IDX ||
        state->op == TOK_NUMBER || state->op == TOK_VAR_STR) {
        data_type = parse_cond_expr(state);
    } else {
        data_type = parse_arithmetic_expr(state);
    }

    return data_type;
}

DataType parse_arithmetic_expr(ParseState *state) {
    DataType data_type_left = parse_term(state);
    while (state->op == OP_ADD || state->op == OP_SUB) {
        OpCode op = state->op;

        next_token(state);
        DataType data_type_right = parse_term(state);

        if (data_type_left != data_type_right) {
            parse_fatal(state, "Mismatched types in arithmetic expression\n");
        }

        if (data_type_left == VAR_STRING) {
            emit_type(state, data_type_left, op, VAR_NUMBER);
        }
        else {
            emit_type(state, data_type_left, op, VAR_NUMBER);
        }
    }
    return data_type_left;
}

DataType parse_term(ParseState *state) {
    DataType data_type_left = parse_factor(state);

    while (state->op == OP_MUL || state->op == OP_DIV) {
        OpCode op = state->op;

        next_token(state);
        DataType data_type_right = parse_term(state);

        if (data_type_left == VAR_STRING) {
            if (op == OP_MUL) {
                if (data_type_right != VAR_NUMBER) {
                    parse_fatal(state, "Multiplay string must be number!\n");
                }
                emit(state, OP_MUL_STR, 0, VAR_STRING);
            }
            else {
                emit(state, OP_DIV_STR, 0, VAR_STRING);
            }
        }
        else {
            emit_type(state, data_type_left, op, VAR_STRING);
        }
    }

    return data_type_left;
}

DataType parse_factor(ParseState *state) {
    DataType data_type = VAR_UNKNOWN;

    switch (state->op) {
        case TOK_NUMBER:
            emit(state, OP_PUSH_NUM, state->value, VAR_NUMBER);
            next_token(state);
            data_type = VAR_NUMBER;
            break;

        case TOK_ID_NAME: {
            bool found = false;
            for (int i = 0; state->variables[i].type != VAR_END; i++) {
                if (strncmp(state->variables[i].name, state->name, strlen(state->name)) == 0) {
                    data_type = state->variables[i].type;
                    emit(state, OP_PUSH_VAR, i, data_type);
                    next_token(state);

                    found = true;
                    break;
                }
            }

            if (!found) {
                parse_fatal(state, "Undefined variable '%s'\n", state->name);
            }
            break;
        }

        case TOK_VAR_IDX:
            emit(state, OP_PUSH_VAR, state->value, VAR_NUMBER);
            next_token(state);
            data_type = VAR_NUMBER;
            break;

        case TOK_VAR_STR:
            emit_str(state, OP_PUSH_STR, state->str);
            next_token(state);
            data_type = VAR_STRING;
            break;

        case TOK_LPAREN:
            next_token(state);
            data_type = parse_expr(state);
            if (state->op != TOK_RPAREN) {
                parse_fatal(state, "Expected ')'\n");
            }
            next_token(state);
            break;

        default:
            parse_fatal(state, "Unknown token: %d\n", state->op);
            break;
    }

    return data_type;
}

DataType parse_bool(ParseState *state, OpCode op, DataType (*parse_bool_func)(ParseState *state)) {
    DataType data_type_left = parse_bool_func(state);
    while (state->op == op) {
        next_token(state);
        DataType data_type_right = parse_bool_func(state);

        if (data_type_left != data_type_right) {
            parse_fatal(state, "Mismatched types in boolean expression\n");
        }

        emit_type(state, data_type_left, op, VAR_NUMBER);
    }
    return data_type_left;
}

DataType parse_bool_expr(ParseState *state) {
    return parse_bool(state, OP_OR, parse_bool_term);
}

DataType parse_bool_term(ParseState *state) {
    return parse_bool(state, OP_AND, parse_bool_factor);
}

DataType parse_bool_factor(ParseState *state) {
    DataType data_type = VAR_UNKNOWN;

    switch (state->op) {
        case TOK_TRUE:
            emit(state, OP_PUSH_NUM, 1, VAR_NUMBER);
            next_token(state);
            data_type = VAR_NUMBER;
            break;

        case TOK_FALSE:
            emit(state, OP_PUSH_NUM, 0, VAR_NUMBER);
            next_token(state);
            data_type = VAR_NUMBER;
            break;

        case OP_NOT:
            next_token(state);
            data_type = parse_bool_factor(state);
            emit(state, OP_NOT, 0, VAR_NUMBER);
            break;

        case TOK_LPAREN:
            next_token(state);
            data_type = parse_expr(state);
            if (state->op != TOK_RPAREN) {
                parse_fatal(state, "Expected ')'\n");
            }
            next_token(state);
            break;

        default:
            data_type = parse_rel_expr(state);
            break;
    }

    return data_type;
}

DataType parse_rel_expr(ParseState *state) {
    DataType data_type_left = parse_arithmetic_expr(state);
    if (state->op == OP_EQ || state->op == OP_NEQ || 
        state->op == OP_LT || state->op == OP_GT || 
        state->op == OP_LE || state->op == OP_GE || 
        state->op == OP_IN_STR || state->op == OP_IN_REGEX_STR) {
        OpCode op = state->op;

        next_token(state);
        DataType data_type_right = parse_arithmetic_expr(state);

        if (op == OP_IN_STR || op == OP_IN_REGEX_STR) {
            if (data_type_left != VAR_STRING || data_type_right != VAR_STRING) {
                parse_fatal(state, "Mismatched types in 'in' expression\n");
            }
            emit(state, op, 0, VAR_NUMBER);
        }
        else {
            if (data_type_left != data_type_right) {
                parse_fatal(state, "Mismatched types in relational expression\n");
            }

            emit_type(state, data_type_left, op, VAR_NUMBER);
        }
        data_type_left = VAR_NUMBER;
    }

    return data_type_left;
}

DataType parse_cond_expr(ParseState *state) {
    DataType data_type = parse_bool_expr(state);
    if (state->op != TOK_CONDITION) {
        return data_type;
    }

    next_token(state);

    int code_false_branch = state->code_size;
    emit(state, OP_JPZ, 0, VAR_UNKNOWN);

    DataType data_type_true = parse_expr(state);   // Parse true branch

    int code_jump_end = state->code_size;
    emit(state, OP_JP, 0, VAR_UNKNOWN);

    if (state->op != TOK_COLON) {
        parse_fatal(state, "Expected ':' for conditional expression\n");
    }
    next_token(state); // Skip ':'

    int code_false = state->code_size;
    DataType data_type_false = parse_expr(state);   // Parse false branch

    if (data_type_true != data_type_false) {
        parse_fatal(state, "Mismatched types in condition expression\n");
    }

    state->code[code_false_branch].value = code_false;
    state->code[code_jump_end].value = state->code_size;

    return data_type_true;
}

void parse_cleaning(Instruction const *code) {
    if (code == NULL) {
        return;
    }

    for (const Instruction *ip = code; ip->op != OP_HALT; ip++) {
        if (ip->type == VAR_STRING && ip->op == OP_PUSH_STR) {
            mem_free((void *) ip->str);
        }
    }
    mem_free((void*) code);
}

const Instruction *parse_expression(const char *iexpr, const Variable *ivariables) {
    ParseState state = {
        .expr = iexpr,
        .expr_begin = iexpr,
        .expr_end = iexpr + strlen(iexpr),
        .variables = ivariables,
        .code = (Instruction *) mem_malloc(MAX_CODE_SIZE * sizeof(Instruction *)),
        .code_size = 0,
        .op = OP_NOP,
        .type = VAR_UNKNOWN
    };

    if (state.code == NULL) {
        parse_fatal(&state, "Out of memory\n");
    }

    next_token(&state);
    parse_expr(&state);

    emit(&state, OP_HALT, 0, VAR_UNKNOWN);

    return state.code;
}
