/**
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 43):
 *
 * <jens@bennerhq.com> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   
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

#include "../hdr/exec.h"
#include "../hdr/expr.h"

DataType parse_expr();
DataType parse_term();
DataType parse_factor();
DataType parse_bool_expr();
DataType parse_bool_term();
DataType parse_bool_factor();
DataType parse_rel_expr();
DataType parse_arithmetic_expr();
DataType parse_cond_expr();

#define IS_SPACE        " \t\n"
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

Instruction *code = NULL; 
int code_size;

const Variable *variables;

const char *expr;
const char *expr_end;

const Token op_symbols[] = {
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
    {.str = "?",    .op = TOK_CONDITION},
    {.str = ":",    .op = TOK_COLON},
    {.str = "(",    .op = TOK_LPAREN},
    {.str = ")",    .op = TOK_RPAREN},
    {.str = "true", .op = TOK_TRUE},
    {.str = "false",.op = TOK_FALSE},
    {.str = "",     .op = TOK_END},
};

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
        fprintf(stderr, "Error: String too long %s\n", start);
        exit(EXIT_FAILURE);
    }

    char value[MAX_STR_SIZE];
    strncpy(value, start, len);
    value[len] = '\0';

    if (op == TOK_NUMBER || op == TOK_VAR_IDX) {
        char *endptr;
        token.value = strtod(value, &endptr);
        if (*endptr != '\0') token.value = 0; // FIXME
        token.type = VAR_NUMBER;
    }
    else if (op == TOK_TRUE || op == TOK_FALSE) {
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
        fprintf(stderr, "Error: Can't find trailing '%c'\n", find);
        exit(EXIT_FAILURE);
    }

    int len = expr - start;
    char *str = (char *) malloc(len + 1);
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
        if (strncmp(expr, item->str, len) == 0) {
            set_token(item->op, NULL, len);
            return;
        }
    }

    if (strchr(IS_ALPHA, *expr)) {
        set_token(TOK_ID_NAME, IS_ALPHA_INT, 0);
        return;
    }

    fprintf(stderr, "Error: Undefined symbol '%c'\n", *expr);
    exit(EXIT_FAILURE);
}

void emit_overflow() {
    if (code_size >= MAX_CODE_SIZE - 1) {
        fprintf(stderr, "Error: code array overflow\n");
        exit(EXIT_FAILURE);
    }
}

void emit(OpCode op, double value, DataType type) {
    emit_overflow();
    code[code_size++] = (Instruction){.op = op, .value = value, .type = type};
    code[code_size].op = OP_HALT;
}

void emit_str(OpCode op, const char *str, DataType data_type) {
    emit_overflow();
    code[code_size++] = (Instruction){.op = op, .str = str, .type = data_type};
    code[code_size].op = OP_HALT;
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
            fprintf(stderr, "Unknown instruction type\n");
            exit(EXIT_FAILURE);
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
            fprintf(stderr, "Type error: Mismatched types in arithmetic expression\n");
            exit(EXIT_FAILURE);
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
                    fprintf(stderr, "Type error: Multiplay string with number!\n");
                    exit(EXIT_FAILURE);
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
            int var_index = -1;
            for (int i = 0; variables[i].type != VAR_END; i++) {
                if (strncmp(variables[i].name, token.name, strlen(token.name)) == 0) {
                    var_index = i;
                    break;
                }
            }
            if (var_index != -1) {
                data_type = variables[var_index].type;
                emit(OP_PUSH_VAR, var_index, data_type);
            } else {
                fprintf(stderr, "Error: Undefined variable '%s'\n", token.name);
                exit(EXIT_FAILURE);
            }
            next_token();
            break;
        }

        case TOK_VAR_IDX:
            emit(OP_PUSH_VAR, token.value, VAR_NUMBER);
            next_token();
            data_type = VAR_NUMBER;
            break;

        case TOK_VAR_STR:
            emit_str(OP_PUSH_STR, token.str, VAR_STRING);
            next_token();
            data_type = VAR_STRING;
            break;

        case TOK_LPAREN:
            next_token();
            data_type = parse_expr();
            if (token.op != TOK_RPAREN) {
                fprintf(stderr, "Error: Expected ')'\n");
                exit(EXIT_FAILURE);
            }
            next_token();
            break;

        default:
            printf("*** ERROR: UPS.... token.op: %d\n", token.op);
            break;
    }

    return data_type;
}

DataType parse_bool_expr() {
    DataType data_type_left = parse_bool_term();
    while (token.op == OP_OR) {
        next_token();
        DataType data_type_right = parse_bool_term();
        if (data_type_left != data_type_right) {
            fprintf(stderr, "Type error: Mismatched types in boolean expression\n");
            exit(EXIT_FAILURE);
        }
        emit_type(data_type_left, OP_OR, VAR_NUMBER);
    }
    return data_type_left;
}

DataType parse_bool_term() {
    DataType data_type_left = parse_bool_factor();
    while (token.op == OP_AND) {
        next_token();
        DataType data_type_right = parse_bool_factor();

        if (data_type_left != data_type_right) {
            fprintf(stderr, "Type error: Mismatched types in boolean term\n");
            exit(EXIT_FAILURE);
        }

        emit_type(data_type_left, OP_AND, VAR_NUMBER);
        data_type_left = VAR_NUMBER;
    }
    return data_type_left;
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
                fprintf(stderr, "Error: Expected ')'\n");
                exit(EXIT_FAILURE);
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
        token.op == OP_LE || token.op == OP_GE) {
        OpCode op = token.op;

        next_token();
        DataType data_type_right = parse_arithmetic_expr();

        if (data_type_left != data_type_right) {
            fprintf(stderr, "Type error: Mismatched types in relational expression\n");
            exit(EXIT_FAILURE);
        }

        emit_type(data_type_left, op, VAR_NUMBER);
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
        fprintf(stderr, "Error: Expected ':' for conditional expression\n");
        exit(EXIT_FAILURE);
    }
    next_token(); // Skip ':'

    int code_false = code_size;
    DataType data_type_false = parse_expr();   // Parse false branch

    if (data_type_true != data_type_false) {
        fprintf(stderr, "Type error: Mismatched types in condition expression\n");
        exit(EXIT_FAILURE);
    }

    code[code_false_branch].value = code_false;
    code[code_jump_end].value = code_size;

    return data_type_true;
}

const Instruction * parse_expression(const char *iexpr, const Variable *ivariables) {
    expr = iexpr;
    variables = ivariables;

    if (code != NULL) {
        for (const Instruction *ip = code; ip->op != OP_HALT; ip++) {
            if (ip->op == OP_PUSH_STR) {
                free((void *) ip->str);
            }
        }
        free(code);
    }
    code = (Instruction *) malloc(MAX_CODE_SIZE * sizeof(Instruction *));
    if (code == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        exit(EXIT_FAILURE);
    }

    code_size = 0;
    expr_end = expr + strlen(expr);

    next_token();
    parse_expr();

    emit(OP_HALT, 0, VAR_UNKNOWN);

    return code;
}
