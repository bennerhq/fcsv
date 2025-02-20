/**
 * exec.c - Execution of the code
 */
 #include <stdio.h>
 #include <stdlib.h>

#include "../hdr/exec.h"
#include "../hdr/expr.h"

#define MAX_STACK_SIZE      (1024)
#define STACK_SIZE_BUFFER   (10)

const Variable *variables;

const char *op_names[] = {
    "PUSH %d",
    "PUSH VAR %d",
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
    "HALT"
};

void print_instruction(const Instruction instr) {
    printf(op_names[instr.op], instr.value);

    if (instr.op == OP_PUSH_VAR) {
        printf(" %s", variables[instr.value].name);
    }

    printf("\n");
}

void print_code(const Instruction *code) {
    for (const Instruction *ip = code; ip->op != OP_HALT; ip++) {
        printf("%03d\t", (int) (ip - code));
        print_instruction(*ip);
    }
}

double execute_code(const Instruction *code, const Variable *ivariables) {
    variables = ivariables;

    double stack[MAX_STACK_SIZE];
    double *sp = stack;

    for (const Instruction *ip = code; ip->op != OP_HALT; ip++) {
        if ((sp - stack) + STACK_SIZE_BUFFER >= MAX_STACK_SIZE) {
            fprintf(stderr, "Stack overflow!\n");
            exit(EXIT_FAILURE);
        }

        switch (ip->op) {
            case OP_NOP:
                break;
            case OP_PUSH_NUM:
                *sp++ = ip->value;
                break;
            case OP_PUSH_VAR:
                *sp++ = variables[ip->value].value;
                break;
            case OP_ADD:
                sp--;
                sp[-1] += sp[0];
                break;
            case OP_SUB:
                sp--;
                sp[-1] -= sp[0];
                break;
            case OP_MUL:
                sp--;
                sp[-1] *= sp[0];
                break;
            case OP_DIV:
                sp--;
                if (sp[0] == 0) {
                    fprintf(stderr, "Division by zero!\n");
                    exit(EXIT_FAILURE);
                }
                sp[-1] /= sp[0];
                break;
            case OP_EQ:
                sp--;
                sp[-1] = (sp[-1] == sp[0]);
                break;
            case OP_NEQ:
                sp--;
                sp[-1] = (sp[-1] != sp[0]);
                break;
            case OP_LT:
                sp--;
                sp[-1] = (sp[-1] < sp[0]);
                break;
            case OP_GT:
                sp--;
                sp[-1] = (sp[-1] > sp[0]);
                break;
            case OP_LE:
                sp--;
                sp[-1] = (sp[-1] <= sp[0]);
                break;
            case OP_GE:
                sp--;
                sp[-1] = (sp[-1] >= sp[0]);
                break;
            case OP_AND:
                sp--;
                sp[-1] = (sp[-1] && sp[0]);
                break;
            case OP_OR:
                sp--;
                sp[-1] = (sp[-1] || sp[0]);
                break;
            case OP_NOT:
                sp[-1] = !sp[-1];
                break;
            case OP_JPZ:
                if (!*--sp) {
                    ip = code + ip->value - 1;
                }
                break;
            case OP_JP:
                ip = code + ip->value - 1;
                break;
            default:
                fprintf(stderr, "Unknown op code!");
                exit(EXIT_FAILURE);
        }
    }

    if (sp != stack + 1) {
        fprintf(stderr, "No results!");
        exit(EXIT_FAILURE);
    }

    sp--;
    return *sp;
}
