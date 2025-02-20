/**
 * exec.h -- header file for exec.c
 */
#ifndef __EXEC_H__
#define __EXEC_H__

#define OpCode      int
#define OP_PUSH_NUM (0)
#define OP_PUSH_VAR (1)
#define OP_ADD      (2)
#define OP_SUB      (3)
#define OP_MUL      (4)
#define OP_DIV      (5)
#define OP_NEQ      (6)
#define OP_LE       (7)
#define OP_GE       (8)
#define OP_LT       (9)
#define OP_GT       (10)
#define OP_EQ       (11)
#define OP_AND      (12)
#define OP_OR       (13)
#define OP_NOT      (14)
#define OP_JP       (15)
#define OP_JPZ      (16)
#define OP_NOP      (17)
#define OP_HALT     (18)

typedef struct {
    OpCode op;
    int value;
} Instruction;

typedef enum {
    VAR_NUMBER,
    VAR_STRING,
    VAR_UNKNOWN,
    VAR_END
} VariableType;

typedef struct {
    VariableType type;
    char *name;
    union {
        char *string;
        double value;
    };
} Variable;

void print_code(const Instruction *code);
double execute_code(const Instruction *code, const Variable *variables);

#endif /* __EXEC_H__ */
