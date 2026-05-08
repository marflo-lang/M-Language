#pragma once

#include <stdbool.h>
#include "m.h"

typedef enum
{
    VAL_NAN,
    VAL_NIL,
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_BOOLEAN
} CValueType;

typedef struct
{
    CValueType type;

    union
    {
        int i;
        double f;
        bool b;

        struct
        {
            const char* chars;
            int length;
        } string;
    };
} Value;

/* macros to access values */
#define ttype(o)    ((o).type)
#define ivalue(o)   (o).i
#define fvalue(o)   (o).f
#define bvalue(o)   (o).b
#define svalue(o)   (o).string.chars
#define slenvalue(o)    (o).string.length

/* macros of types */
#define ismnan(o)    (ttype(o) == VAL_NAN)
#define isnil(o)    (ttype(o) == VAL_NIL)
#define isint(o)    (ttype(o) == VAL_INT)
#define isfloat(o)    (ttype(o) == VAL_FLOAT)
#define isboolean(o)    (ttype(o) == VAL_BOOLEAN)
#define isstring(o)    (ttype(o) == VAL_STRING)
 
/* macros to set values */
#define setint(o, in)    (o).type = VAL_INT; (o).i = (in)
#define setfloat(o, fl)  (o).type = VAL_FLOAT; (o).f = (fl)
#define setboolean(o, bo)    (o).type = VAL_BOOLEAN; (o).b = (bo)
#define setstring(o, str, len)  (o).type = VAL_STRING; (o).string.chars = (str); (o).string.length = (len)
#define setnil(o)   (o).type = VAL_NIL
#define setnan(o)   (o).type = VAL_NAN
#define settype(o, t)   (o).type = t
#define setobj(o1, o2)  o1 = o2

/* macro to check if a value is false */
#define isfalse(o)  (ismnan(o) || isnil(o) || (isboolean(o) && bvalue(o) == false))


