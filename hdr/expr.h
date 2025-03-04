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
 * expr.h -- header file for exec.c
 */

/**
    This is BNF grammar for the filtering string:
 
        <expression> ::= <conditional> | <arithmetic>
        <conditional> ::= <boolean> "?" <expression> ":" <expression> 
        <boolean> ::= <arithmetic> <relop> <arithmetic> | <boolean> <logop> <boolean>
        <relop> ::= ">" | "<" | "=" | ">=" | "<=" | "!=" | "in" | "rin"
        <logop> ::= "&" | "|"

        <arithmetic> ::= <term> | <arithmetic> ("+" | "-") <term>
        <term> ::= <factor> | <term> ("*" | "/") <factor>
        <factor> ::= <number> | "(" <expression> ")" | <variable> | "!" <factor> | "up" <factor> | "dn" <factor>
        <number> ::= <digit> | <number> <digit>
        <digit> ::= "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"
        <variable> ::= <letter> | <variable> <letter> | <variable> <digit>
        <letter> ::= 'a' | 'b' | 'c' | ... | 'z' | 'A' | 'B' | 'C' | ... | 'Z'
*/
#ifndef __EXPR_H__
#define __EXPR_H__

#include "exec.h"

const Variable *parse_expression(const char *iexpr, const Variable *ivariables);
void parse_cleaning(Variable const *code);

#endif /* __EXPR_H__ */
