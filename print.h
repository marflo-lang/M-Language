#pragma once
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

//#include "print.h"

inline static void print_string(int dummy, ...)
{
    va_list args;
    va_start(args, dummy);

    const char* s = va_arg(args, const char*);
    printf("%s ", s ? s : "nil");
    
    va_end(args);
}

//static void print_token(Lexer* L, TokenArray* Tokens, int i)
/*static void print_token(Token token)
{
    printf("Type: %d '", token.type);
    if (token.type == M_V_INT)
        printf("int");
    else if (token.type == M_V_FLOAT)
        printf("float");
    else if (token.type == M_V_STRING)
        printf("String");
    else if (token.type == M_V_UNFINISHED_STRING)
        printf("Unfinished String");
    else if (token.type == M_ERROR)
        printf("ERROR");
    else if (token.type == M_EOF)
        printf("<eof>");
    else if (token.type >= 15)
        printf("Operand");
    else if (token.type == M_V_IDENTIFIER)
        printf("identifier");
    else
        printf("Keyword");

    printf("', Value: ");
    /*for (int j = 0; j < token.length; j++)
    {
        int p = token.location.begin.offset + j;
        printf("%c", L->src[p]);
    }*/

    /*for (int j = token.location.begin.offset; j <= token.location.end.offset; j++)
    {
        //printf("%c", L->src[j]);
    }*-/

    printf(", Length: %d", token.length);

    printf(", Offset: begin %d -> end %d", token.location.begin.offset, token.location.end.offset);

    printf("\n");
}*/

#define print_without_end(x)    _Generic((x), \
int: printf("%d ", x), \
double: printf("%g ", x), \
char: printf("%c ", x), \
char*: print_string(0, x), \
const char*: print_string(0, x), \
bool: printf("%s ", x ? "true" : "false"), \
default: printf("indefinido "))
/*Token: print_token(x), \*/

#define print_with_end(x)   print_without_end(x); printf("\n")

#define print(...) \
do \
{ \
 FOR_EACH(print_without_end, __VA_ARGS__);\
printf("\n"); \
} while (0)

#define FOR_EACH_1(m, x) m(x)
#define FOR_EACH_2(m, x, y)  m(x); m(y)
#define FOR_EACH_3(m, x, y, z)   m(x); m(y); m(z)
#define FOR_EACH_4(m, x, y, z, a)    m(x); m(y); m(z); m(a)
#define FOR_EACH_5(m, x, y, z, a, b) m(x); m(y); m(z); m(a); m(b)
#define GET_MACRO(_1, _2, _3, _4, _5, NAME, ...)    NAME
#define FOR_EACH(m, ...) GET_MACRO(__VA_ARGS__, FOR_EACH_5, FOR_EACH_4, FOR_EACH_3, FOR_EACH_2, FOR_EACH_1) (m, __VA_ARGS__)




