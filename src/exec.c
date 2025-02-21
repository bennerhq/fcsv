/**
 * exec.c - Execution of the code
 */
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>

#include "../hdr/exec.h"
#include "../hdr/expr.h"

#define MAX_STACK_SIZE      (1024)
#define STACK_SIZE_BUFFER   (10)

const Variable *variables;

const char *op_names[] = {
    "PUSH %d",
    "PUSH VAR %d",
    "PUSH STR \"%s\"",
    "ADD",
    "SUB",
    "MUL",
    "DIV",
    "NEQ",
    "LE",
    "GE",
    "LT",
    "GT",
    "EQ",
    "AND",
    "OR",
    "NOT",
    "JP %d",
    "JPZ %d",
    "NOP",
    "EQ_STR",
    "HALT"
};

void print_instruction(const Instruction *instr) {
    const char *fmt = op_names[instr->op];
    if (strstr(fmt, "%d")) {
        printf(fmt, (int) instr->value);
    } else if (strstr(fmt, "%s")) {
        printf(fmt, instr->str);
    } else {
        printf("%s", fmt);
    }

    if (instr->op == OP_PUSH_VAR) {
        printf(" '%s'", variables[(int) instr->value].name);
    }

    printf("\n");
}

void print_code(const Instruction *code) {
    for (const Instruction *ip = code; ip->op != OP_HALT; ip++) {
        printf("%03d\t", (int) (ip - code));
        print_instruction(ip);
    }
}

#define OP_VAR_TYPE(REL_OP)                                         \
    sp --;                                                          \
    if (sp[-1].type == VAR_STRING && sp[0].type == VAR_STRING) {    \
        sp[-1].value = strncmp(sp[-1].str, sp[0].str, strlen(sp[0].str)) REL_OP 0;      \
    }                                                               \
    else {                                                          \
        sp[-1].value = (sp[-1].value REL_OP sp[0].value);           \
    }                                                               \
    sp[-1].type = VAR_NUMBER;

double execute_code(const Instruction *code, const Variable *ivariables) {
    variables = ivariables;

    Variable stack[MAX_STACK_SIZE];
    Variable *sp = stack;

    for (const Instruction *ip = code; ip->op != OP_HALT; ip++) {
        if ((sp - stack) + STACK_SIZE_BUFFER >= MAX_STACK_SIZE) {
            fprintf(stderr, "Stack overflow!\n");
            exit(EXIT_FAILURE);
        }

        switch (ip->op) {
            case OP_NOP:
                break;
            case OP_PUSH_NUM:
                (*sp).type = VAR_NUMBER;
                (*sp).value = ip->value;
                sp++;
                break;
            case OP_PUSH_VAR:
                (*sp++) = variables[(int) ip->value];
                break;
            case OP_PUSH_STR:
                (*sp).type = VAR_STRING;
                (*sp).str = ip->str;
                sp++;
                break;
            case OP_ADD:
                sp--;
                sp[-1].value += sp[0].value;
                break;
            case OP_SUB:
                sp--;
                sp[-1].value -= sp[0].value;
                break;
            case OP_MUL:
                sp--;
                sp[-1].value *= sp[0].value;
                break;
            case OP_DIV:
                sp--;
                if (sp[0].value == 0) {
                    fprintf(stderr, "Division by zero!\n");
                    exit(EXIT_FAILURE);
                }
                sp[-1].value /= sp[0].value;
                break;
            case OP_EQ:
                OP_VAR_TYPE(==);
                break;
            case OP_NEQ:
                OP_VAR_TYPE(!=);
                break;
            case OP_LT:
                OP_VAR_TYPE(<);
                break;
            case OP_GT:
                OP_VAR_TYPE(>);
                break;
            case OP_LE:
                OP_VAR_TYPE(<=);
                break;
            case OP_GE:
                OP_VAR_TYPE(>=);
                break;
            case OP_AND:
                sp--;
                sp[-1].value = (sp[-1].value && sp[0].value);
                break;
            case OP_OR:
                sp--;
                sp[-1].value = (sp[-1].value || sp[0].value);
                break;
            case OP_NOT:
                sp[-1].value = !sp[-1].value;
                break;
            case OP_JPZ:
                if (!(*--sp).value) {
                    ip = code + (int) ip->value - 1;
                }
                break;
            case OP_JP:
                ip = code + (int) ip->value - 1;
                break;
            case OP_EQ_STR:
                sp--;
                sp[-1].type = VAR_NUMBER;
                sp[-1].value = strcmp(sp[-1].str, sp[0].str) == 0;
                break;
                
            default:
                fprintf(stderr, "Unknown op code!");
                exit(EXIT_FAILURE);
        }
    }

    if (sp != stack + 1) {
        fprintf(stderr, "No results!\n");
        exit(EXIT_FAILURE);
    }

    return (--sp)->value != 0;
}
