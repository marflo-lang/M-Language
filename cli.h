#pragma once
#include <string.h>
#include <stdint.h>
#include "m.h"

typedef struct
{
    int typechecker_level;
    int optimizer_level;
    char* script_path;
}Config;

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

