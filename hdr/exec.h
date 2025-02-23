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

#define DATE_FORMAT "%Y-%m-%dT%H:%M:%S"

#define OP_NOP          (0)
#define OP_PUSH_NUM     (1)
#define OP_PUSH_VAR     (2)
#define OP_PUSH_STR     (3)
#define OP_JP           (4)
#define OP_JPZ          (5)
#define OP_HALT         (6)

#define OP_BASE         (7)
#define OP_ADD          (OP_BASE + 0)
#define OP_SUB          (OP_BASE + 1)
#define OP_MUL          (OP_BASE + 2)
#define OP_DIV          (OP_BASE + 3)
#define OP_NEQ          (OP_BASE + 4)
#define OP_LE           (OP_BASE + 5)
#define OP_GE           (OP_BASE + 6)
#define OP_LT           (OP_BASE + 7)
#define OP_GT           (OP_BASE + 8)
#define OP_EQ           (OP_BASE + 9)
#define OP_AND          (OP_BASE + 10)
#define OP_OR           (OP_BASE + 11)
#define OP_NOT          (OP_BASE + 12)

#define OP_BASE_NUM     (20)
#define OP_ADD_NUM      (OP_BASE_NUM + 0)
#define OP_SUB_NUM      (OP_BASE_NUM + 1)
#define OP_MUL_NUM      (OP_BASE_NUM + 2)
#define OP_DIV_NUM      (OP_BASE_NUM + 3)
#define OP_NEQ_NUM      (OP_BASE_NUM + 4)
#define OP_LE_NUM       (OP_BASE_NUM + 5)
#define OP_GE_NUM       (OP_BASE_NUM + 6)
#define OP_LT_NUM       (OP_BASE_NUM + 7)
#define OP_GT_NUM       (OP_BASE_NUM + 8)
#define OP_EQ_NUM       (OP_BASE_NUM + 9)
#define OP_AND_NUM      (OP_BASE_NUM + 10)
#define OP_OR_NUM       (OP_BASE_NUM + 11)
#define OP_NOT_NUM      (OP_BASE_NUM + 12)

#define OP_BASE_STR     (33)
#define OP_ADD_STR      (OP_BASE_STR + 0)
#define OP_SUB_STR      (OP_BASE_STR + 1)
#define OP_MUL_STR      (OP_BASE_STR + 2)
#define OP_DIV_STR      (OP_BASE_STR + 3)
#define OP_NEQ_STR      (OP_BASE_STR + 4)
#define OP_LE_STR       (OP_BASE_STR + 5)
#define OP_GE_STR       (OP_BASE_STR + 6)
#define OP_LT_STR       (OP_BASE_STR + 7)
#define OP_GT_STR       (OP_BASE_STR + 8)
#define OP_EQ_STR       (OP_BASE_STR + 9)
#define OP_AND_STR      (OP_BASE_STR + 10)
#define OP_OR_STR       (OP_BASE_STR + 11)
#define OP_NOT_STR      (OP_BASE_STR + 12)

#define VAR_BASE        (100)
#define VAR_NUMBER      (VAR_BASE + 0)
#define VAR_STRING      (VAR_BASE + 1)
#define VAR_DATETIME    (VAR_BASE + 2)
#define VAR_IDX         (VAR_BASE + 3)
#define VAR_UNKNOWN     (VAR_BASE + 4)
#define VAR_END         (VAR_BASE + 5)

typedef int OpCode;
typedef int DataType;

typedef struct {
    OpCode op;
    DataType type;
    union {
        const char *str;
        double value;  
    };
} Instruction;

typedef struct {
    DataType type;
    char *name;
    union {
        const char *str;
        double value;
        struct tm datetime;
    };
} Variable;

void print_code(const Instruction *code, const Variable *variables);
double execute_code(const Instruction *code, const Variable *variables);

#endif /* __EXEC_H__ */
