#pragma once
#include <stdio.h>
#include <stdbool.h>

//#include "print.h"

//inline void print_string(char* s)
//{
#define print_string(s) printf("%s", s ? s : "nil")
//}

#define print_without_end(x)    _Generic((x), \
int: printf("%d", x), \
char*: print_string(x), \
char: printf("%c", x), \
bool: printf("%s", x ? "true" : "false"), \
double: printf("%d", x), \
default: printf("%p", &x))

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




