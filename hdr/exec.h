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
 * exec.h -- header file for exec.c
 */
#ifndef __EXEC_H__
#define __EXEC_H__

#include <time.h>

#define OpCode      int
#define OP_PUSH_NUM (0)
#define OP_PUSH_VAR (1)
#define OP_PUSH_STR (2)
#define OP_ADD      (3)
#define OP_SUB      (4)
#define OP_MUL      (5)
#define OP_DIV      (6)
#define OP_NEQ      (7)
#define OP_LE       (8)
#define OP_GE       (9)
#define OP_LT       (10)
#define OP_GT       (11)
#define OP_EQ       (12)
#define OP_AND      (13)
#define OP_OR       (14)
#define OP_NOT      (15)
#define OP_JP       (16)
#define OP_JPZ      (17)
#define OP_NOP      (18)
#define OP_EQ_STR   (19)
#define OP_HALT     (20)

#define VAR_BASE    (100)
#define VAR_NUMBER  (VAR_BASE + 1)
#define VAR_STRING  (VAR_BASE + 2)
#define VAR_DATE    (VAR_BASE + 3)
#define VAR_UNKNOWN (VAR_BASE + 4)
#define VAR_END     (VAR_BASE + 5)

#define DATE_FORMAT     "%Y-%m-%dT%H:%M:%S"

typedef struct {
    OpCode op;
    union {
        double value;
        const char *str;
    };
} Instruction;

typedef struct {
    OpCode type;
    char *name;
    union {
        const char *str;
        double value;
        struct tm tm;
    };
} Variable;

void print_code(const Instruction *code);
double execute_code(const Instruction *code, const Variable *variables);

#endif /* __EXEC_H__ */
