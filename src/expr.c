/**
 * expr.c - Expression parser
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../hdr/exec.h"
#include "../hdr/expr.h"

void parse_expr();
void parse_term();
void parse_factor();
void parse_bool_expr();
void parse_bool_term();
void parse_bool_factor();
void parse_rel_expr();
void parse_arithmetic_expr();
void parse_cond_expr();

Instruction *code = NULL; 
int code_size;

#define MAX_CODE_SIZE   (1024)

#define TO_COLON    (OP_HALT + 1)
#define TO_LPAREN   (OP_HALT + 2)
#define TO_RPAREN   (OP_HALT + 3)
#define TO_TRUE     (OP_HALT + 4)
#define TO_FALSE    (OP_HALT + 5)
#define TO_NUMBER   (OP_HALT + 6)
#define TO_ID_NAME  (OP_HALT + 7)
#define TO_ID_IDX   (OP_HALT + 8)
#define TO_END      (OP_HALT + 9)

typedef struct {
    OpCode op;
    char *str;
} Token;

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
    {.str = "?",    .op = OP_JPZ},
    {.str = ":",    .op = TO_COLON},
    {.str = "(",    .op = TO_LPAREN},
    {.str = ")",    .op = TO_RPAREN},
    {.str = "true", .op = TO_TRUE},
    {.str = "false",.op = TO_FALSE},
    {.str = NULL,   .op = TO_END},
};

const Variable *variables;
const char *expr;
Token token = {.str = NULL, .op = TO_END};

void emit(OpCode op, int value) {
    if (code_size + 1 >= MAX_CODE_SIZE) {
        fprintf(stderr, "Error: code array overflow\n");
        exit(EXIT_FAILURE);
	}

    code[code_size].op = op;
    code[code_size].value = value;
    code_size ++;
}

void cleanToken() {
    if (token.str != NULL) {
        free(token.str);
        token.str = NULL;
    }
}

Token next_token() {
    cleanToken();

    while (isspace(*expr)) {
        expr++;
    }

    if (*expr == '\0') {
        return (Token){TO_END, 0};
    }

    if (*expr == '#') {
        expr++;
        const char *start = expr;
        while (isdigit(*expr)) {
            expr++;
        }
        return (Token){TO_ID_IDX, strndup(start, expr - start)};
    }

    if (isdigit(*expr)) {
        const char *start = expr;
        while (isdigit(*expr)) {
            expr++;
        }
        return (Token){TO_NUMBER, strndup(start, expr - start)};
    }

    for (int i = 0; op_symbols[i].str; i++) {
        const Token* item = &op_symbols[i];
        if (strlen(item->str) && strncmp(expr, item->str, strlen(item->str)) == 0) {
            expr += strlen(item->str);
            return (Token){item->op, strndup(item->str, strlen(item->str))};
        }
    }

    if (isalpha(*expr)) {
        const char *start = expr;
        while (isalnum(*expr) || *expr == '_') {
            expr++;
        }
        char *identifier = strndup(start, expr - start);
        return (Token){TO_ID_NAME, identifier};
    }

    fprintf(stderr, "Error: Undefined valuebol '%s'\n", strndup(expr++, 1));
    exit(EXIT_FAILURE);
}

void parse_expr() {
    if (token.op == TO_TRUE || token.op == TO_FALSE || 
        token.op == OP_NOT || token.op == TO_LPAREN || 
        token.op == TO_ID_NAME || token.op == TO_ID_IDX ||
        token.op == TO_NUMBER) {
        parse_cond_expr();
    } else {
        parse_arithmetic_expr();
    }
}

void parse_arithmetic_expr() {
    parse_term();
    while (token.op == OP_ADD || token.op == OP_SUB) {
        OpCode op = token.op;
        token = next_token();
        parse_term();
        emit(op, 0);
    }
}

void parse_term() {
    parse_factor();
    while (token.op == OP_MUL || token.op == OP_DIV) {
        OpCode op = token.op;
        token = next_token();
        parse_factor();
        emit(op, 0);
    }
}

void parse_factor() {
    switch (token.op) {
        case TO_NUMBER:
            emit(OP_PUSH_NUM, atoi(token.str));
            token = next_token();
            break;

        case TO_ID_NAME: {
            int var_index = -1;
            for (int i = 0; variables[i].type != VAR_END; i++) {
                if (strncmp(variables[i].name, token.str, strlen(token.str)) == 0) {
                    var_index = i;
                    break;
                }
            }
            if (var_index != -1) {
                emit(OP_PUSH_VAR, var_index);
            } else {
                fprintf(stderr, "Error: Undefined variable '%s'\n", token.str);
                exit(EXIT_FAILURE);
            }
            token = next_token();
            break;
        }

        case TO_ID_IDX:
            emit(OP_PUSH_VAR, atoi(token.str));
            token = next_token();
            break;

        case TO_LPAREN:
            token = next_token();
            parse_expr();
            if (token.op != TO_RPAREN) {
                fprintf(stderr, "Error: Expected ')'\n");
                exit(EXIT_FAILURE);
            }
            token = next_token();
            break;

        default:
            break;
    }
}

void parse_bool_expr() {
    parse_bool_term();
    while (token.op == OP_OR) {
        token = next_token();
        parse_bool_term();
        emit(OP_OR, 0);
    }
}

void parse_bool_term() {
    parse_bool_factor();
    while (token.op == OP_AND) {
        token = next_token();
        parse_bool_factor();
        emit(OP_AND, 0);
    }
}

void parse_bool_factor() {
    switch (token.op) {
        case TO_TRUE:
            emit(OP_PUSH_NUM, 1);
            token = next_token();
            break;

        case TO_FALSE:
            emit(OP_PUSH_NUM, 0);
            token = next_token();
            break;

        case OP_NOT:
            token = next_token();
            parse_bool_factor();
            emit(OP_NOT, 0);
            break;

        case TO_LPAREN:
            token = next_token();
            parse_expr();
            if (token.op != TO_RPAREN) {
                fprintf(stderr, "Error: Expected ')'\n");
                exit(EXIT_FAILURE);
            }
            token = next_token();
            break;

        default:
            parse_rel_expr();
            break;
    }
}

void parse_rel_expr() {
    parse_arithmetic_expr();
    if (token.op == OP_EQ || token.op == OP_NEQ || 
        token.op == OP_LT || token.op == OP_GT || 
        token.op == OP_LE || token.op == OP_GE) {
        OpCode op = token.op;
        token = next_token();
        parse_arithmetic_expr();
        emit(op, 0);
    }
}

void parse_cond_expr() {
    parse_bool_expr();
    if (token.op == OP_JPZ) {
        token = next_token();
        int code_false_branch = code_size;
        emit(OP_JPZ, 0);
        parse_expr();   // Parse true branch
        int code_jump_end = code_size;
        emit(OP_JP, 0);

        if (token.op != TO_COLON) {
            fprintf(stderr, "Error: Expected ':' for conditional expression\n");
            exit(EXIT_FAILURE);
        }
        int code_false = code_size;

        token = next_token(); // Skip ':'
        parse_expr();   // Parse false branch

        code[code_false_branch].value = code_false;
        code[code_jump_end].value = code_size;
    }
}

const Instruction * parse_expression(const char *iexpr, const Variable *ivariables) {
    expr = iexpr;
    variables = ivariables;

    if (code != NULL) {
        free(code);
    }
    code = (Instruction *) malloc(MAX_CODE_SIZE * sizeof(Instruction *));
    if (code == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        exit(EXIT_FAILURE);
    }

    code_size = 0;
    token = next_token();

    parse_expr();

    emit(OP_HALT, 0);

    cleanToken();

    return code;
}
