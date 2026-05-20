#pragma once
#ifndef MOBJECTS_H
#define MOBJECTS_H

#include "m.h"
#include "../utils/err.h"

#include <stdbool.h>
#include <malloc.h>
#include <stdlib.h>

typedef struct VM VM;

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
    C_NAN,
    C_NIL,
    C_INT,
    C_FLOAT,
    C_BOOLEAN,
    C_STRING,
} ConstantType;

typedef enum
{
    OBJ_STRING,
    OBJ_LIST,
    OBJ_DICTIONARY,
} ObjType;

typedef struct GCObject
{
    ObjType objType;

    bool marked;

    struct GCObject* next;
} GCObject;

typedef struct
{
    GCObject gc;

    int length;
    const char* chars;

    uint32_t hash;
} ObjString;


typedef struct Value
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

typedef struct
{
    GCObject gc;

    int length;
    int capacity;

    bool fixed;

    Value* values;
} ObjList;

typedef struct
{
    Value key;
    Value value;
} DictEntry;

typedef struct
{
    GCObject gc;

    int count;
    //int capacity;

    bool fixed;

    DictEntry* entries;
} ObjDict;

typedef struct
{
    ConstantType type;

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
} Constant;

typedef struct
{
    ObjString* key;
} StringEntry;

typedef struct
{
    int count;
    int capacity;

    StringEntry* entries;
} StringTable;

/* macros to create values */

/* macros to access values */
#define ttype(o)    ((o).type)
#define ivalue(o)   (o).i
#define fvalue(o)   (o).f
#define bvalue(o)   (o).b
inline const char* svalue(Value o);
inline Value listvalue(Value o, int pos);

//inline Value dictvalue(Value o, Value key)
//{
//    if (o.type == VAL_OBJ)
//    {
//        GCObject* obj = (GCObject*) (o.obj);
//
//        if (obj->objType == OBJ_DICTIONARY)
//        {
//            ObjDict* dict = (ObjDict*) obj;
//
//            for (int i = 0; i < dict->count; i++)
//            {
//                if (dict->entries[i].key)
//                    token_e
//            }
//        }
//    }
//}
//#define svalue(o)   (o).string.chars

inline int slenvalue(Value o);
inline int listlenvalue(Value o);
//#define slenvalue(o)    (o).string.length

/* macros of types */
#define ismnan(o)    (ttype(o) == VAL_NAN)
#define isnil(o)    (ttype(o) == VAL_NIL)
#define isint(o)    (ttype(o) == VAL_INT)
#define isfloat(o)    (ttype(o) == VAL_FLOAT)
#define isboolean(o)    (ttype(o) == VAL_BOOLEAN)
inline bool isstring(Value o);
inline bool islist(Value o);

//#define isstring(o)    (ttype(o) == VAL_STRING)
 
/* macros to set values */
#define setint(o, in)    (o).type = VAL_INT; (o).i = (in)
#define setfloat(o, fl)  (o).type = VAL_FLOAT; (o).f = (fl)
#define setboolean(o, bo)    (o).type = VAL_BOOLEAN; (o).b = (bo)
// string
ObjString* allocate_string(VM* vm, const char* text, int length);
// list
inline void set_list(Value* a, ObjList* list);
void set_list_element(VM* vm, Value* o, Value element, int pos, int line);
ObjList* allocate_list(VM* vm, int length, int capacity, bool fixed);
void resize_list(VM* vm, ObjList* list, int newCapacity, int line);
// dict
ObjDict* allocate_dict(VM* vm, int count, bool fixed);
void resize_dict(VM* vm, ObjDict* dict, int newCapacity, int line);
//inline void setstring(Value* o, const char* str, int len)
//{
//    ObjString* string = malloc(sizeof(ObjString));
//    if (string == NULL)
//    {
//        memoryCrash("Assign String");
//        exit(1);
//    }
//    string->gc.objType = OBJ_STRING;
//    string->gc.marked = false;
//    string->gc.next = NULL;
//    string->chars = str;
//    string->length = len;
//    string->hash = "";
//    
//    o->type = VAL_OBJ;
//    o->obj = (GCObject*) string;
//}

//inline void setlistvalue(Value* o, Value element)
//{
//    ObjList* list = (ObjList*) o->obj;
//
//    if (list->length > list->capacity)
//    {
//        if (list->fixed)
//            printf("Error fixed\n");
//        else
//
//        {
//            list->capacity = (list->capacity < 8) ? 8 : list->capacity * 2;
//            Value* newData = realloc(list->values, sizeof(Value) * list->capacity);
//            if (newData == NULL)
//            {
//                memoryCrash("List Realloc");
//                exit(1);
//            }
//
//            list->values = newData;
//        }
//    }
//    list->values[list->length++] = element;
//}
//#define setstring(o, str, len)  (o).type = VAL_STRING; (o).string.chars = (str); (o).string.length = (len)

#define setnil(o)   (o).type = VAL_NIL
#define setnan(o)   (o).type = VAL_NAN
#define settype(o, t)   (o).type = t
#define setobj(o1, o2)  o1 = o2

/* macro to check if a value is false */
#define isfalse(o)  (ismnan(o) || isnil(o) || (isboolean(o) && bvalue(o) == false))
void print_rvalue(Value v, bool newLine);
inline Value make_rnil();
inline Value make_rnan();
#endif
