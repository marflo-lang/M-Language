#pragma once

#include "common.h"

#include <setjmp.h>
#include <inttypes.h>
#include <stdbool.h>

typedef enum RErrorType {
    /**
    * This must always be 0 and no other value can be 0,
    * since setjmp() compares the result to 0, so any other
    * error that has 0 will be considered as not being an error.
    */
    NONE_ERROR = 0,
    INVALID_OPERANDS_ERROR,
    TYPES_ERROR,
    ARGUMENT_ERROR,
    ARITHMETIC_ERROR,
    MEMORY_ERROR,
    INTERNAL_ERROR,
    ASSIGNMENT_ERROR,
    RESIZE_LIST_ERROR,
    RESIZE_DICT_ERROR,
    INDEX_OUT_OF_BOUNDS_ERROR,
    ATTEMPED_TO_INDEX_NO_COLLECTION_ERROR,
    INDEX_ERROR,
    INVALID_KEY_TYPE_ERROR,
    ASSERTION_ERROR,
    CALL_NO_FUNCTION,
    BITS_ERROR,
    DEBUG_ERROR,
    CUSTOM_ERROR,

    /**
    * This is just to validate the limit in debug mode
    * and ensure it doesn't go out of range.
    */
    LIMIT_ERROR,
} RErrorType;

typedef struct ErrorHandler
{
    jmp_buf buffer;
    struct ErrorHandler* previous;
    int status;
} ErrorHandler;

typedef struct m_debug
{
    const char* name;       // (n)
    const char* what;       // (s)  'M', 'C', 'main'
    const char* source;     // (s)  fullpath like 'carp1/carp2/arch.m'
    const char* short_src;  // (s)  only file name like 'arch.m'
    size_t linedefined;     // (s)
    size_t currentline;     // (l)
    uint8_t nupvals;        // (u)
    uint8_t nparamps;       // (a)
    uint8_t isvararg;       // (a)
} m_debug;

typedef struct VM VM;

typedef void(*PMfunc)(VM* vm, void* ud);

// Runtime errors
M_NORETURN void m_throw_error(VM* vm, RErrorType type);
M_NORETURN void m_error(VM* vm, RErrorType type, const char* format, ...);
bool m_getinfo(VM* vm, int level, const char* what, m_debug* md);
void m_where(VM* vm, int level);
int m_runprotected(VM* vm, PMfunc func, void* ud);
M_NORETURN void m_argerror(VM* vm, int arg, const char* extramsg);
M_NORETURN void m_typeerror(VM* vm, int arg, const char* tname);
M_NORETURN void m_tagerror(VM* vm, int arg, int tag);
const char* m_traceback(VM* vm);
int m_resume(VM* vm, int nargs);
