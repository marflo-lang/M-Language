#pragma once

#include "m.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    M_ANALYZE,
    M_RUN,
    M_TEST
} M_State;

typedef struct
{
    char** paths;
    size_t count;
    size_t capacity;
} TestList;

typedef struct
{
    int typechecker_level;
    int optimizer_level;
    char* script_path;
    M_State state;
    TestList list;
} Config;

typedef struct
{
    char* src;
    size_t length;
} Source;


#ifdef _MSC_VER
    #define strtok_custom strtok_s
    #define sscanf_custom sscanf_s
#else
    #define strtok_custom strtok
    #define sscanf_custom sscanf
#endif

