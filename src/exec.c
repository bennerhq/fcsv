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
 * exec.c - Execution of the code
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include "../hdr/dmalloc.h"
#include "../hdr/exec.h"
#include "../hdr/expr.h"

#define MAX_STACK_SIZE      (1024)
#define STACK_SIZE_BUFFER   (10)

Variable stack[MAX_STACK_SIZE];

const char *op_names[] = {
    "NOP",
    "PUSH %d",
    "PUSH %s [%d]",
    "PUSH '%s'",
    "IN$",
    "JP   %03X",
    "JPZ  %03X",
    "HALT",

    "ADD",  "SUB",  "MUL",  "DIV",  "NEQ",  "LE",  "GE",  "LT",  "GT",  "EQ",  "AND",  "OR",  "NOT",
    "ADD#", "SUB#", "MUL#", "DIV#", "NEQ#", "LE#", "GE#", "LT#", "GT#", "EQ#", "AND#", "OR#", "NOT#",
    "ADD$", "SUB$", "MUL$", "DIV$", "NEQ$", "LE$", "GE$", "LT$", "GT$", "EQ$", "AND$", "OR$", "NOT$",
    "IN$",  "XIN$"
};

void print_instruction(const Instruction *instr, const Variable *variables) {
    const char *fmt = op_names[instr->op];

    if (instr->op == OP_PUSH_VAR) {
        printf(fmt, variables[(int) instr->value].name, (int) instr->value);
    }
    else if (strstr(fmt, "%d") || strstr(fmt, "%03X")) {
        printf(fmt, (int) instr->value);
    } 
    else if (strstr(fmt, "%s")) {
        printf(fmt, instr->str);
    } 
    else {
        printf("%s", fmt);
    }

    printf("\n");
}

void print_code(const Instruction *code, const Variable *variables) {
    const Instruction *ip = code - 1;
    do {
        ip ++;

        printf("0x%03X\t", (int) (ip - code));
        print_instruction(ip, variables);
     } while (ip->op != OP_HALT);
}

void print_stack(Variable *stack, Variable *sp) {
    for (Variable *vp = stack; vp < sp; vp++) {
        printf("%d: %d ", (int) (vp - stack), (int) vp->type);
        switch (vp->type) {
            case VAR_NUMBER:
                printf("%f\n", vp->value);
                break;

            case VAR_STRING:
                printf("%s\n", vp->str);
                break;

            case VAR_DATETIME:
                {
                    char buffer[20];
                    strftime(buffer, sizeof(buffer), DATE_FORMAT, &vp->datetime);
                    printf("%s\n", buffer);
                }
                break;
            default:
                printf("Unknown type\n");
                break;
        }
    }
}

int strregex(const char *str, const char *pattern) {
    regex_t regex;
    int ret;

    ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret) return 0;

    ret = regexec(&regex, str, 0, NULL, 0);
    regfree(&regex);

    // Return 1 if match found, 0 otherwise
    return !ret;
}

Variable* execute_code_datatype(const Instruction *code, const Variable *variables) {
    Variable* sp = stack;
//print_code(code, variables);
    for (const Instruction *ip = code; ip->op != OP_HALT; ip++) {
        if ((sp - stack) + STACK_SIZE_BUFFER >= MAX_STACK_SIZE) {
            fprintf(stderr, "Error: Stack overflow!\n");
            exit(EXIT_FAILURE);
        }

        int op = ip->op;
re_type:
        switch (op) {
            case OP_NOP:
                break;
            case OP_JP:
                ip = code + (int) ip->value - 1;
                break;
            case OP_JPZ:
                if (!(*--sp).value) {
                    ip = code + (int) ip->value - 1;
                }
                break;
            case OP_PUSH_VAR:
                (*sp++) = variables[(int) ip->value];
                break;
            case OP_PUSH_NUM:
                sp->type = VAR_NUMBER;
                sp->value = ip->value;
                sp++;
                break;
            case OP_PUSH_STR:
                sp->type = VAR_STRING;
                sp->str = ip->str;
                sp++;
                break;

            // Dynamic type
            case OP_ADD: case OP_SUB: case OP_MUL:
            case OP_DIV: case OP_EQ:  case OP_NEQ:
            case OP_LT:  case OP_GT:  case OP_LE:
            case OP_GE:  case OP_AND: case OP_OR: 
            case OP_NOT:
                op = (sp->type == VAR_NUMBER) ? OP_BASE_NUM + (op - OP_BASE) : OP_BASE_STR + (op - OP_BASE);
                goto re_type;

            // Number type
            case OP_ADD_NUM:
                sp--;
                sp[-1].value += sp[0].value;
                break;
            case OP_SUB_NUM:
                sp--;
                sp[-1].value -= sp[0].value;
                break;
            case OP_MUL_NUM:
                sp--;
                sp[-1].value *= sp[0].value;
                break;
            case OP_DIV_NUM:
                sp--;
                if (sp[0].value == 0) {
                    fprintf(stderr, "Division by zero!\n");
                    exit(EXIT_FAILURE);
                }
                sp[-1].value /= sp[0].value;
                break;
            case OP_EQ_NUM:
                sp --;
                sp[-1].value = (sp[-1].value == sp[0].value);
                break;
            case OP_NEQ_NUM:
                sp --;
                sp[-1].value = (sp[-1].value != sp[0].value);
                break;
            case OP_LT_NUM:
                sp --;
                sp[-1].value = (sp[-1].value < sp[0].value);
                break;
            case OP_GT_NUM:
                sp --;
                sp[-1].value = (sp[-1].value > sp[0].value);
                break;
            case OP_LE_NUM:
                sp --;
                sp[-1].value = (sp[-1].value <= sp[0].value);
                break;
            case OP_GE_NUM:
                sp --;
                sp[-1].value = (sp[-1].value >= sp[0].value);
                break;
            case OP_AND_NUM:
                sp--;
                sp[-1].value = (sp[-1].value && sp[0].value);
                break;
            case OP_OR_NUM:
                sp--;
                sp[-1].value = (sp[-1].value || sp[0].value);
                break;
            case OP_NOT_NUM:
                sp[-1].value = !sp[-1].value;
                break;

            // String type
            case OP_EQ_STR:
                sp--;
                sp[-1].type = VAR_NUMBER;
                sp[-1].value = strncmp(sp[-1].str, sp[0].str, strlen(sp[0].str)) == 0;
                break;
            case OP_ADD_STR:
                {
                    sp--;
                    size_t len1 = strlen(sp[-1].str);
                    size_t len2 = strlen(sp[0].str);
                    char *result = (char *) mem_malloc(len1 + len2 + 1);
                    if (result == NULL) {
                        fprintf(stderr, "Out of memory\n");
                        exit(EXIT_FAILURE);
                    }
                    strcpy(result, sp[-1].str);
                    strcat(result, sp[0].str);

                    if (sp[-1].is_dynamic) mem_free((void *) sp[-1].str);

                    sp[-1].str = result;
                    sp[-1].is_dynamic = true;
                }
                break;
            case OP_SUB_STR:
                {
                    sp --;
                    char *pos = strstr(sp[-1].str, sp[0].str);
                    if (pos) {
                        size_t len1 = strlen(sp[-1].str);
                        size_t len2 = strlen(sp[0].str);
                        memmove(pos, pos + len2, len1 - len2 + 1);
                    }
                }
                break;
            case OP_MUL_STR:
                {
                    sp--;
                    int repeat = (int) sp[0].value;
                    size_t len = strlen(sp[-1].str);
                    char *result = (char *) mem_malloc(len * repeat + 1);
                    if (result == NULL) {
                        fprintf(stderr, "Out of memory\n");
                        exit(EXIT_FAILURE);
                    }

                    result[0] = '\0';
                    for (int i = 0; i < repeat; i++) {
                        strcat(result, sp[-1].str);
                    }

                    if (sp[-1].is_dynamic) mem_free((void *) sp[-1].str);

                    sp[-1].str = result;
                    sp[-1].is_dynamic = true;
                }
                break;
            case OP_DIV_STR:
                {
                    sp--;
                    char *pos = strstr(sp[-1].str, sp[0].str);
                    if (pos) {
                        *pos = '\0';
                    }
                }
                break;
            case OP_NEQ_STR:
                sp--;
                sp[-1].type = VAR_NUMBER;
                sp[-1].value = strncmp(sp[-1].str, sp[0].str, strlen(sp[0].str)) != 0;
                break;
            case OP_LE_STR:
                sp--;
                sp[-1].type = VAR_NUMBER;
                sp[-1].value = strncmp(sp[-1].str, sp[0].str, strlen(sp[0].str)) <= 0;
                break;
            case OP_GE_STR:
                sp--;
                sp[-1].type = VAR_NUMBER;
                sp[-1].value = strncmp(sp[-1].str, sp[0].str, strlen(sp[0].str)) >= 0;
                break;
            case OP_LT_STR:
                sp--;
                sp[-1].type = VAR_NUMBER;
                sp[-1].value = strncmp(sp[-1].str, sp[0].str, strlen(sp[0].str)) < 0;
                break;
            case OP_GT_STR:
                sp--;
                sp[-1].type = VAR_NUMBER;
                sp[-1].value = strncmp(sp[-1].str, sp[0].str, strlen(sp[0].str)) > 0;
                break;
            case OP_AND_STR:
                sp--;
                sp[-1].type = VAR_NUMBER;
                sp[-1].value = (strlen(sp[-1].str) > 0 && strlen(sp[0].str) > 0);
                break;
            case OP_OR_STR:
                sp--;
                sp[-1].type = VAR_NUMBER;
                sp[-1].value = (strlen(sp[-1].str) > 0 || strlen(sp[0].str) > 0);
                break;
            case OP_NOT_STR:
                sp[-1].type = VAR_NUMBER;
                sp[-1].value = (strlen(sp[-1].str) == 0);
                break;
            case OP_IN_STR:
                sp--;
                sp[-1].type = VAR_NUMBER;
                sp[-1].value = (strlen(sp[-1].str) > 0) && (strstr(sp[0].str, sp[-1].str) != 0);
                break;
            case OP_IN_REGEX_STR:
                sp--;
                sp[-1].type = VAR_NUMBER;
                sp[-1].value = (strlen(sp[-1].str) > 0) && (strregex(sp[0].str, sp[-1].str) != 0);
                break;

            default:
                fprintf(stderr, "Error: Unknown op code %d!\n", op);
                exit(EXIT_FAILURE);
        }
    }

    sp --;
    if (sp != stack) {
        fprintf(stderr, "Error: No results!\n");
        exit(EXIT_FAILURE);
    }

    return sp;
}

double execute_code(const Instruction *code, const Variable *variables) {
    Variable *result = execute_code_datatype(code, variables);

    double value = 0;

    switch (result->type) {
        case VAR_NUMBER:
            value = result->value;
            break;

        case VAR_STRING:
            value = strlen(result->str) > 0;
            if (result->is_dynamic) mem_free((void *) result->str);
            break;

        case VAR_DATETIME:
            value = 0;
            break;

        default:
            fprintf(stderr, "Result type is unknown!\n");
            exit(EXIT_FAILURE);
    }

    return value;
}
