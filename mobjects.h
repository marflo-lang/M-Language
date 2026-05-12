#pragma once

#include <stdbool.h>
#include <malloc.h>
#include <stdlib.h>
#include "m.h"
#include "err.h"

typedef enum
{
    VAL_NAN,
    VAL_NIL,
    VAL_INT,
    VAL_FLOAT,
    VAL_BOOLEAN,
    VAL_OBJ,
} CValueType;

typedef enum
{
    OBJ_STRING,
    OBJ_LIST,
    OBJ_DICTIONARY
} ObjType;

typedef struct GCObject
{
    ObjType objType;

    bool market;

    struct GCObject* next;
} GCObject;

typedef struct
{
    GCObject gc;

    int length;
    const char* chars;

    uint32_t hash;
} ObjString;

typedef struct
{
    CValueType type;

    union
    {
        int i;
        double f;
        bool b;
        GCObject* obj;
    };
} Value;

/* macros to access values */
#define ttype(o)    ((o).type)
#define ivalue(o)   (o).i
#define fvalue(o)   (o).f
#define bvalue(o)   (o).b
inline const char* svalue(Value o)
{
    if (o.type == VAL_OBJ)
    {
        GCObject* obj = (GCObject*)(o.obj);

        if (obj->objType == OBJ_STRING)
        {
            ObjString* string = (ObjString*)obj;

            return string->chars;
        }
    }
}
//#define svalue(o)   (o).string.chars

inline size_t slenvalue(Value o)
{
    if (o.type == VAL_OBJ)
    {
        GCObject* obj = (GCObject*)(o.obj);

        if (obj->objType == OBJ_STRING)
        {
            ObjString* string = (ObjString*)obj;

            return string->length;
        }
    }
}
//#define slenvalue(o)    (o).string.length

/* macros of types */
#define ismnan(o)    (ttype(o) == VAL_NAN)
#define isnil(o)    (ttype(o) == VAL_NIL)
#define isint(o)    (ttype(o) == VAL_INT)
#define isfloat(o)    (ttype(o) == VAL_FLOAT)
#define isboolean(o)    (ttype(o) == VAL_BOOLEAN)
inline bool isstring(Value o)
{
    if (o.type == VAL_OBJ)
    {
        GCObject* obj = (GCObject*)(o.obj);

        if (obj->objType == OBJ_STRING)
        {
            return true;
        }
    }

    return false;
}
//#define isstring(o)    (ttype(o) == VAL_STRING)
 
/* macros to set values */
#define setint(o, in)    (o).type = VAL_INT; (o).i = (in)
#define setfloat(o, fl)  (o).type = VAL_FLOAT; (o).f = (fl)
#define setboolean(o, bo)    (o).type = VAL_BOOLEAN; (o).b = (bo)
inline void setstring(Value* o, const char* str, int len)
{
    ObjString* string = malloc(sizeof(ObjString));
    if (string == NULL)
    {
        memoryCrash("Assign String");
        exit(1);
    }
    string->gc.objType = OBJ_STRING;
    string->gc.market = false;
    string->gc.next = NULL;
    string->chars = str;
    string->length = len;
    string->hash = "";
    
    o->type = VAL_OBJ;
    o->obj = (GCObject*) string;
}
//#define setstring(o, str, len)  (o).type = VAL_STRING; (o).string.chars = (str); (o).string.length = (len)
#define setnil(o)   (o).type = VAL_NIL
#define setnan(o)   (o).type = VAL_NAN
#define settype(o, t)   (o).type = t
#define setobj(o1, o2)  o1 = o2

/* macro to check if a value is false */
#define isfalse(o)  (ismnan(o) || isnil(o) || (isboolean(o) && bvalue(o) == false))


