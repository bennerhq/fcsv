/*
    This is BNF grammar for the filtering string:

        <expression> ::= <conditional> | <arithmetic>
        <conditional> ::= <boolean> "?" <expression> ":" <expression>
        <boolean> ::= <arithmetic> <relop> <arithmetic>
        <relop> ::= ">" | "<" | "=" | ">=" | "<=" | "!="

        <arithmetic> ::= <term> | <arithmetic> ("+" | "-") <term>
        <term> ::= <factor> | <term> ("*" | "/") <factor>
        <factor> ::= <number> | "(" <expression> ")" | <variable>
        <number> ::= <digit> | <number> <digit>
        <digit> ::= "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"
        <variable> ::= <letter> | <variable> <letter> | <variable> <digit>
        <letter> ::= 'a' | 'b' | 'c' | ... | 'z' | 'A' | 'B' | 'C' | ... | 'Z'
*/
#ifndef __EXPR_H__
#define __EXPR_H__

#include "exec.h"

const Instruction * parse_expression(const char *iexpr, const Variable *ivariables);

#endif /* __EXPR_H__ */
