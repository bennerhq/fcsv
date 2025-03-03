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


void var_print(const Variable * var);


DataType parse_expr();
DataType parse_term();
DataType parse_factor();
DataType parse_bool_expr();
DataType parse_bool_term();
DataType parse_bool_factor();
DataType parse_rel_expr();
DataType parse_arithmetic_expr();
DataType parse_cond_expr();

#define IS_SPACE        " \t\n\r\v\f"
#define IS_INT          "0123456789"
#define IS_DOUBLE       IS_INT "."
#define IS_ALPHA        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_"
#define IS_ALPHA_INT    IS_ALPHA IS_INT

#define MAX_STR_SIZE    (32)
#define MAX_CODE_SIZE   (1024)

#define TOK_BASE        (1000)
#define TOK_COLON       (TOK_BASE + 1)
#define TOK_LPAREN      (TOK_BASE + 2)
#define TOK_RPAREN      (TOK_BASE + 3)
#define TOK_TRUE        (TOK_BASE + 4)
#define TOK_FALSE       (TOK_BASE + 5)
#define TOK_NUMBER      (TOK_BASE + 6)
#define TOK_ID_NAME     (TOK_BASE + 7)
#define TOK_VAR_IDX     (TOK_BASE + 8)
#define TOK_VAR_STR     (TOK_BASE + 9)
#define TOK_CONDITION   (TOK_BASE + 10) 
#define TOK_END         (TOK_BASE + 11)

typedef struct {
    OpCode op;
    DataType type;
    union {
        char name[MAX_STR_SIZE];
        const char *str;
        double value;
    };
} Token;
Token token;

static const char *expr;
static const char *expr_begin;
static const char *expr_end;

static const Variable *variables;

static Instruction *code = NULL; 
static int code_size;

const Token op_symbols[] = {
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

void parse_fatal(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "*** ERROR: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "    Parsing: %s\n", expr_begin);
    va_end(args);

    exit(EXIT_FAILURE);
}

void set_token(OpCode op, const char *match, int len) {
    const char *start = expr;
    if (match) {
        while (expr < expr_end && strchr(match, *expr)) {
            expr++;
        }
        len = expr - start;
    }
    else {
        expr += len;
    }
    if (len >= MAX_STR_SIZE - 1) {
        parse_fatal("String too long %s\n", start);
    }

    char value[MAX_STR_SIZE];
    strncpy(value, start, len);
    value[len] = '\0';

    if (op == TOK_TRUE || op == TOK_FALSE) {
        token.type = VAR_NUMBER;
    }
    else if (op == TOK_NUMBER || op == TOK_VAR_IDX) {
        char *endptr;
        token.value = strtod(value, &endptr);
        if (*endptr != '\0') token.value = 0; // FIXME
        token.type = VAR_NUMBER;
    }
    else {
        strcpy(token.name, value);
        token.type = VAR_STRING;
    }
    token.op = op;
}

void next_token_str(char find) {
    expr++;
    const char *start = expr;

    while (*expr != find && expr < expr_end) {
        expr++;
    }

    if (*expr != find) {
        parse_fatal("'%s' Can't find trailing '%c'\n", expr_begin, find);
    }

    int len = expr - start;
    char *str = (char *) mem_malloc(len + 1);
    if (str == NULL) {
        parse_fatal("Out of memory\n");
    }
    strncpy(str, start, len);
    token.op = TOK_VAR_STR;
    token.str = str;

    expr ++;
}

void next_token() {
    while (expr < expr_end && strchr(IS_SPACE, *expr)) {
        expr++;
    }

    if (expr >= expr_end) {
        token.op = TOK_END;
        return;
    }

    if (*expr == '"') {
        next_token_str('"');
        return;
    }

    if (*expr == '\'') {
        next_token_str('\'');
        return;
    }

    if (*expr == '#') {
        expr++;
        set_token(TOK_VAR_IDX, IS_INT, 0);
        return;
    }

    if (strchr(IS_DOUBLE, *expr)) {
        set_token(TOK_NUMBER, IS_DOUBLE, 0);
        return;
    }

    for (const Token *item = op_symbols; item++;) {
        if (item->op == TOK_END) break;

        int len = strlen(item->str);
        if (len && strncmp(expr, item->str, len) == 0) {
            set_token(item->op, NULL, len);
            return;
        }
    }

    if (strchr(IS_ALPHA, *expr)) {
        set_token(TOK_ID_NAME, IS_ALPHA_INT, 0);
        return;
    }

    parse_fatal("Undefined symbol '%c'\n", *expr);
}

void emit_overflow() {
    if (code_size >= MAX_CODE_SIZE - 1) {
        parse_fatal("code array overflow\n");
    }
}

void emit(OpCode op, double value, DataType type) {
    emit_overflow();
    code[code_size++] = (Instruction){
        .op = op, 
        .value = value, 
        .type = type
    };
}

void emit_str(OpCode op, const char *str) {
    emit_overflow();
    code[code_size++] = (Instruction){
        .op = op, 
        .str = str, 
        .type = VAR_STRING
    };
}

void emit_type(DataType data_type, OpCode op, DataType data_type_result) {
    switch (data_type) {
        case VAR_STRING:
            emit(OP_BASE_STR + (op - OP_BASE), 0, data_type_result);
            break;

        case VAR_NUMBER:
            emit(OP_BASE_NUM + (op - OP_BASE), 0, data_type_result);
            break;

        default:
            parse_fatal("Unknown instruction type\n");
    }
}

DataType parse_expr() {
    DataType data_type = VAR_UNKNOWN;

    if (token.op == TOK_TRUE || token.op == TOK_FALSE || 
        token.op == OP_NOT || token.op == TOK_LPAREN || 
        token.op == TOK_ID_NAME || token.op == TOK_VAR_IDX ||
        token.op == TOK_NUMBER || token.op == TOK_VAR_STR) {
        data_type = parse_cond_expr();
    } else {
        data_type = parse_arithmetic_expr();
    }

    return data_type;
}

DataType parse_arithmetic_expr() {
    DataType data_type_left = parse_term();
    while (token.op == OP_ADD || token.op == OP_SUB) {
        OpCode op = token.op;

        next_token();
        DataType data_type_right = parse_term();

        if (data_type_left != data_type_right) {
            parse_fatal("Mismatched types in arithmetic expression\n");
        }

        if (data_type_left == VAR_STRING) {
            emit_type(data_type_left, op, VAR_NUMBER);
        }
        else {
            emit_type(data_type_left, op, VAR_NUMBER);
        }
    }
    return data_type_left;
}

DataType parse_term() {
    DataType data_type_left = parse_factor();

    while (token.op == OP_MUL || token.op == OP_DIV) {
        OpCode op = token.op;

        next_token();
        DataType data_type_right = parse_term();

        if (data_type_left == VAR_STRING) {
            if (op == OP_MUL) {
                if (data_type_right != VAR_NUMBER) {
                    parse_fatal("Multiplay string must be number!\n");
                }
                emit(OP_MUL_STR, 0, VAR_STRING);
            }
            else {
                emit(OP_DIV_STR, 0, VAR_STRING);
            }
        }
        else {
            emit_type(data_type_left, op, VAR_STRING);
        }
    }

    return data_type_left;
}

DataType parse_factor() {
    DataType data_type = VAR_UNKNOWN;

    switch (token.op) {
        case TOK_NUMBER:
            emit(OP_PUSH_NUM, token.value, VAR_NUMBER);
            next_token();
            data_type = VAR_NUMBER;
            break;

        case TOK_ID_NAME: {
            bool found = false;
            for (int i = 0; variables[i].type != VAR_END; i++) {
                if (strncmp(variables[i].name, token.name, strlen(token.name)) == 0) {
                    data_type = variables[i].type;
                    emit(OP_PUSH_VAR, i, data_type);
                    next_token();

                    found = true;
                    break;
                }
            }

            if (!found) {
                parse_fatal("Undefined variable '%s'\n", token.name);
            }
            break;
        }

        case TOK_VAR_IDX:
            emit(OP_PUSH_VAR, token.value, VAR_NUMBER);
            next_token();
            data_type = VAR_NUMBER;
            break;

        case TOK_VAR_STR:
            emit_str(OP_PUSH_STR, token.str);
            next_token();
            data_type = VAR_STRING;
            break;

        case TOK_LPAREN:
            next_token();
            data_type = parse_expr();
            if (token.op != TOK_RPAREN) {
                parse_fatal("Expected ')'\n");
            }
            next_token();
            break;

        default:
            parse_fatal("Unknown token: %d\n", token.op);
            break;
    }

    return data_type;
}

DataType parse_bool(OpCode op, DataType (*parse_bool_func)()) {
    DataType data_type_left = parse_bool_func();
    while (token.op == op) {
        next_token();
        DataType data_type_right = parse_bool_func();

        if (data_type_left != data_type_right) {
            parse_fatal("Mismatched types in boolean expression\n");
        }

        emit_type(data_type_left, op, VAR_NUMBER);
    }
    return data_type_left;
}

DataType parse_bool_expr() {
    return parse_bool(OP_OR, parse_bool_term);
}

DataType parse_bool_term() {
    return parse_bool(OP_AND, parse_bool_factor);
}

DataType parse_bool_factor() {
    DataType data_type = VAR_UNKNOWN;

    switch (token.op) {
        case TOK_TRUE:
            emit(OP_PUSH_NUM, 1, VAR_NUMBER);
            next_token();
            data_type = VAR_NUMBER;
            break;

        case TOK_FALSE:
            emit(OP_PUSH_NUM, 0, VAR_NUMBER);
            next_token();
            data_type = VAR_NUMBER;
            break;

        case OP_NOT:
            next_token();
            data_type = parse_bool_factor();
            emit(OP_NOT, 0, VAR_NUMBER);
            break;

        case TOK_LPAREN:
            next_token();
            data_type = parse_expr();
            if (token.op != TOK_RPAREN) {
                parse_fatal("Expected ')'\n");
            }
            next_token();
            break;

        default:
            data_type = parse_rel_expr();
            break;
    }

    return data_type;
}

DataType parse_rel_expr() {
    DataType data_type_left = parse_arithmetic_expr();
    if (token.op == OP_EQ || token.op == OP_NEQ || 
        token.op == OP_LT || token.op == OP_GT || 
        token.op == OP_LE || token.op == OP_GE || 
        token.op == OP_IN_STR || token.op == OP_IN_REGEX_STR) {
        OpCode op = token.op;

        next_token();
        DataType data_type_right = parse_arithmetic_expr();

        if (op == OP_IN_STR || op == OP_IN_REGEX_STR) {
            if (data_type_left != VAR_STRING || data_type_right != VAR_STRING) {
                parse_fatal("Mismatched types in 'in' expression\n");
            }
            emit(op, 0, VAR_NUMBER);
        }
        else {
            if (data_type_left != data_type_right) {
                parse_fatal("Mismatched types in relational expression\n");
            }

            emit_type(data_type_left, op, VAR_NUMBER);
        }
        data_type_left = VAR_NUMBER;
    }

    return data_type_left;
}

DataType parse_cond_expr() {
    DataType data_type = parse_bool_expr();
    if (token.op != TOK_CONDITION) {
        return data_type;
    }

    next_token();

    int code_false_branch = code_size;
    emit(OP_JPZ, 0, VAR_UNKNOWN);

    DataType data_type_true = parse_expr();   // Parse true branch

    int code_jump_end = code_size;
    emit(OP_JP, 0, VAR_UNKNOWN);

    if (token.op != TOK_COLON) {
        parse_fatal("Expected ':' for conditional expression\n");
    }
    next_token(); // Skip ':'

    int code_false = code_size;
    DataType data_type_false = parse_expr();   // Parse false branch

    if (data_type_true != data_type_false) {
        parse_fatal("Mismatched types in condition expression\n");
    }

    code[code_false_branch].value = code_false;
    code[code_jump_end].value = code_size;

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
    expr = iexpr;
    variables = ivariables;

    code = (Instruction *) mem_malloc(MAX_CODE_SIZE * sizeof(Instruction *));
    if (code == NULL) {
        parse_fatal("Out of memory\n");
    }

    code_size = 0;
    expr_begin = expr;
    expr_end = expr + strlen(expr);

    next_token();
    parse_expr();

    emit(OP_HALT, 0, VAR_UNKNOWN);

    return code;
}
