#pragma once

#include "m.h"
#include "mconfig.h"
#include "mdebug.h"
#include "../utils/err.h"

#include <stdbool.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct VM VM;
typedef struct Chunk Chunk;

typedef enum
{
    VAL_VOID,
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
    OBJ_STRING = VAL_OBJ + 1,
    OBJ_LIST,
    OBJ_DICTIONARY,
    OBJ_CLOSURE
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

    size_t length;
    uint32_t hash;

    char chars[];
} ObjString;


typedef struct Value
{
    CValueType type;

    union
    {
        int64_t i;
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

    /* this means the capacity is fixed, but the content can still be modified */
    bool fixed;
    /* this means the content is read-only, but the capacity can still be modified */
    bool readonly;

    Value* values;
} ObjList;

typedef struct
{
    uint32_t hash;
    /*
    * In the key, to know if a key is empty or deleted, we
    * will use void and nil; void means empty, nil means deleted,
    * thus saving us from creating enums with deadkey or tombstone.
    */
    Value key;
    Value value;
} DictEntry;

typedef struct
{
    GCObject gc;

    int count;
    int capacity;

    /* this means the capacity is fixed, but the content can still be modified */
    bool fixed;
    /* this means the content is read-only, but the capacity can still be modified */
    bool readonly;
    bool safeenv;

    DictEntry* entries;
} ObjDict;

typedef int (*MBuiltinFn)(VM* vm);

typedef struct
{
    GCObject gc;
    bool isC;
    int stack_size;
    size_t upvalue_count;
    /*
    * In the future, when we add threads, each thread will have
    * its own env and the closure will point to the env of the thread
    * it belongs to.
    */
    ObjDict* env; // environment for the closure (global dictionary)

    union
    {
        struct
        {
            MBuiltinFn funcC;
            const char* name;
            Value upvals[];
        } C;

        struct
        {
            Chunk* chunk;
            Value uprefs[];
        } M;
    };
} Closure;

typedef struct
{
    ConstantType type;

    union
    {
        int64_t i;
        double f;
        bool b;
        struct
        {
            const char* chars;
            size_t length;
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

typedef struct m_StrBuf
{
    char* p;
    char* end;
    VM* vm;
    ObjString* storage;
    char buffer[512];
} m_StrBuf;

#define m_buffdata(buf) ((buf)->storage ? (buf)->storage->chars : (buf)->buffer)
#define m_bufflen(buf)  ((buf)->storage ? (buf)->p - (buf)->storage->chars : (buf)->p - (buf)->buffer)

void m_bufferinit(VM* vm, m_StrBuf* buf);
char* m_buffinitsize(VM* vm, m_StrBuf* buf, size_t size);
char* m_prepbuffsize(m_StrBuf* buf, size_t size);
void m_buffaddlenstring(m_StrBuf* buf, const char* s, size_t len);
#define m_buffaddstring(buf, s) m_buffaddlenstring((buf), (s), strlen(s))
void m_buffaddchar(m_StrBuf* buf, char c);
void m_buffaddvalue(m_StrBuf* buf);
void m_buffaddvalueany(m_StrBuf* buf, int idx);
void m_buffpushresult(m_StrBuf* buf);

/* macros to create values */

/* macros to create values */

/* macros to access values */
#define ttype(o)    ((o)->type)
#define gcttype(o)  ((o)->obj->objType)
#define ivalue(o)   (o)->i
#define fvalue(o)   (o)->f
#define nvalue(o)   (isint(o) ? ivalue(o) : fvalue(o))
#define bvalue(o)   (o)->b
#define orefvalue(o)    (o)->obj
inline const char* svalue(Value* o);
inline ObjString* osvalue(const Value* o);
inline Value* listvalue(Value* o, int pos);
inline Closure* closurevalue(Value* o);
inline size_t slenvalue(Value* o);
inline int listlenvalue(Value* o);
inline size_t dictlenvalue(Value* o);

/* macros of types */
#define isvoid(o)    (ttype(o) == VAL_VOID)
#define isnil(o)    (ttype(o) == VAL_NIL)
#define isint(o)    (ttype(o) == VAL_INT)
#define isfloat(o)    (ttype(o) == VAL_FLOAT)
#define isnumber(o)     (isint(o) || isfloat(o))
#define isboolean(o)    (ttype(o) == VAL_BOOLEAN)
#define isobject(o)     (ttype(o) == VAL_OBJ)
inline bool isstring(const Value* o);
inline bool islist(const Value* o);
inline bool isdict(const Value* o);
inline bool isclosure(const Value* o);
#define isCclosure(o) (isclosure(o) && cast(Closure*, o->obj)->isC)
#define isMclosure(o) (isclosure(o) && !cast(Closure*, o->obj)->isC)

/* macros to set values */
#define setint(o, in)    \
    { \
        (o)->type = VAL_INT; \
        (o)->i = (in); \
    }

#define setfloat(o, fl)  { (o)->type = VAL_FLOAT; (o)->f = (fl); }
#define setboolean(o, bo)    { (o)->type = VAL_BOOLEAN; (o)->b = (bo); }
// string
ObjString* allocate_string(VM* vm, const char* text, size_t length);
ObjString* bufstart_string(VM* vm, size_t length);
ObjString* bufend_string(VM* vm, ObjString* string);
#define setstring(o, s) \
    { \
        (o)->type = VAL_OBJ; \
        (o)->obj = cast(GCObject*, s); \
    }
// list
inline void set_list(Value* a, ObjList* list);
ObjList* copy_list(VM* vm, ObjList* listToCopy, bool deep);
void set_list_element(VM* vm, ObjList* list, Value element, int pos);
ObjList* allocate_list(VM* vm, int length, int capacity, bool fixed);
void resize_list(VM* vm, ObjList* list, int newCapacity);
inline ObjList* olistvalue(Value* o);
void setfixedlist(VM* vm, ObjList* list, int lenght);
void setreadonlylist(VM* vm, ObjList* list);
// dict
inline void set_dict(Value* a, ObjDict* dict);
void set_dict_key_value(VM* vm, ObjDict* dict, Value* key, Value* value);
ObjDict* copy_dict(VM* vm, ObjDict* dictToCopy, bool deep);
Value* get_dict_value(VM* vm, ObjDict* dict, Value* key);
ObjDict* allocate_dict(VM* vm, int count, bool fixed);
bool remove_dict_key_value(VM* vm, ObjDict* dict, const Value* key, Value* val_out);
void resize_dict(VM* vm, ObjDict* dict, int newCapacity);
// function
Closure* allocate_Mclosure(VM* vm, int numupvals, ObjDict* env, Chunk* chunk);
Closure* allocate_Cclosure(VM* vm, int numupvals, ObjDict* env);
inline void setclosure(Value* o, Closure* clo);

#define setnil(o)   (o)->type = VAL_NIL
#define setvoid(o)   (o)->type = VAL_VOID
#define settype(o, t)   (o)->type = t
#define setobj(o1, o2)  (o1) = (o2)
#define setobjr(o1, o2) \
    { \
        const Value* c_o2 = (o2); \
        Value* c_o1 = (o1); \
        *c_o1 = *c_o2; \
    }

/* macro to check if a value is false */
#define isfalse(o)  (isvoid(o) || isnil(o) || (isboolean(o) && bvalue(o) == false))
void print_rvalue(Value* v, bool newLine);
inline Value* make_rnil();
inline Value* make_rnan();
const char* getValueTypeName(Value* v);

extern const Value m_nil_obj_;
extern const Value m_void_obj_;

#define m_nil_obj   (&m_nil_obj_)
#define m_void_obj  (&m_void_obj_)
