#pragma once

#include <stdbool.h>
#include "m.h"

//typedef enum
//{
//    NaN,
//    NIL,
//    INT,
//    FLOAT,
//    BOOLEAN,
//    STRING
//} RValueType;
//
//typedef struct
//{
//    int length;
//    char* text;
//}String;
//
//typedef union
//{
//    int i;
//    double f;
//    bool b;
//    String* s;
//}Data;
//
//typedef struct
//{
//    RValueType type;
//    Data data;
//} RValue;

/* macros of types */
#define isnan(o)    (ttype(o) == NaN)
#define isnil(o)    (ttype(o) == NIL)
#define isint(o)    (ttype(o) == INT)
#define isfloat(o)    (ttype(o) == FLOAT)
#define isboolean(o)    (ttype(o) == BOOLEAN)
#define isstring(o)    (ttype(o) == STRING)
 
/* macros to access values */
#define ttype(o)    ((o)->type)
#define ivalue(o)   (o)->data.i
#define fvalue(o)   (o)->data.f
#define bvalue(o)   (o)->data.b
#define svalue(o)   &(o)->data.s

#define isfalse(o)  (isnan(o) || isnil(o) || isboolean(o) && bvalue(o) == false)


