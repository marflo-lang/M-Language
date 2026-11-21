#pragma once

#include "m.h"
#include "common.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#if (defined(DEBUG) && DEBUG == 1) && !defined(NDEBUG)
#define M_ASSERT_ENEABLED
#endif 

extern int (*assert_handler)(const char* expression, const char* file, int line, const char* func);

//int assert_call_handler(const char* expression, const char* file, int line, const char* func);
M_NOINLINE inline int assert_call_handler(const char* expression, const char* file, int line, const char* func)
{
    if (assert_handler)
        return assert_handler(expression, file, line, func);

    return 1;
}

#ifdef M_ASSERT_ENEABLED

#define M_ASSERT(expr) ((void)(!!(expr) || (assert_call_handler(#expr, __FILE__, __LINE__, __FUNCTION__) && (__debugbreak(), 0))))

#define M_UNREACHABLE(...) (assert_call_handler("" __VA_ARGS__, __FILE__, __LINE__, __func__), __debugbreak())

#define M_NOT_IMPLEMENTED() assert_call_handler("Not implemented", __FILE__, __LINE__, __func__)

#define M_TODO(msg) assert_call_handler("TODO: " msg, __FILE__, __LINE__, __func__)

#else
#define M_ASSERT(expr)    (sizeof(!!(expr)))
#define M_UNREACHABLE(...)    ((void)0)
#define M_NOT_IMPLEMENTED()    ((void)0)
#define M_TODO(msg)    (sizeof(!!(msg)))
#endif

#define check_expr(cond, expr)  (M_ASSERT(cond), expr)
