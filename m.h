#pragma once

#define M_VERSION   0.1
#define DEBUG   1

#define CLI_DEBUG   0
#define LEXER_DEBUG 1
#define PARSER_DEBUG    1
#define AST_DETALLADO   1
#define COMPILER_DEBUG  1

/* basic types */
typedef enum /*ValueType*/
{
    M_NaN,
    M_NIL,
    M_BOOLEAN,
    M_INT,
    M_FLOAT,
    M_STRING
}ValueType;

/* minimum stack to C functions */
#define MIN_STACK 20

#define cast(t, expr)   ((t) (expr))

