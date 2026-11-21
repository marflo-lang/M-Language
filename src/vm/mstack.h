#pragma once

#include "mobjects.h"
#include "mdebug.h"

typedef struct VM VM;
typedef struct CallFrame CallFrame;

typedef struct M_Reg
{
    const char* name;
    MBuiltinFn func;
} M_Reg;

/*
* Stack Api
*/

#define api_check(x)    M_ASSERT(x)

#define incr_top(vm) \
    { \
        api_check(vm->top < vm->frame->top); \
        stack_checkstack(vm, 1); \
        vm->top++; \
    }

#define update_top(vm, ntop) \
    { \
        api_check(ntop >= vm->base && p <= L->frame->top); \
        vm->top = ntop; \
    }

#define expandstacklimit(vm, t) \
    { \
        if (vm->frame->top < t) \
            vm->frame->top = t; \
    }

#define checkstack(vm, n) \
    if (stack_checkstacklimit(vm, n)) \
        stack_reallocstack(vm, get_grow_stack(vm, n))

#define savestack(vm, p) \
    check_expr((p) >= vm->stack && (p) < vm->stack + vm->stack_size, cast(uint8_t*, p) - cast(uint8_t*, vm->stack))

#define restorestack(vm, n) \
    check_expr((n) >= 0 && cast(size_t, n) <= sizeof(Value) * vm->stack_size, cast(Value*, cast(uint8_t*, vm->stack) + (n)))

#define saveframe(vm, frame) \
    check_expr((frame) >= vm->frames && (frame) < vm->frames + vm->frame_count, cast(uint8_t*, frame) - cast(uint8_t*, vm->frames))

#define restoreframe(vm, n) \
    check_expr((n) >= 0 && cast(size_t, n) <= sizeof(CallFrame) * vm->frame_count, cast(CallFrame*, cast(uint8_t*, vm->frames) + (n)))

#define incr_frame(vm) ((vm->frame == vm->frame_last) ? stack_growFrames(vm) : ++vm->frame)

#define stack_argcheck(vm, cond, arg, extramsg) ((void)((cond) ? (void)0 : m_argerror(vm, arg, extramsg)))
#define stack_argexpected(vm, cond, arg, tname) ((void)((cond) ? (void)0 : m_typeerror(vm, arg, tname)))
void stack_checkstackR(VM * vm, int space, const char* msg);
void stack_checktype(VM* vm, int arg, int tag);
void stack_checkany(VM* vm, int arg);
int stack_absindex(VM* vm, int idx);
#define stack_checkstring(vm, arg) stack_checklenstring(vm, (arg), NULL)
const char* stack_checklenstring(VM* vm, int arg, size_t* len);
#define stack_optstring(vm, arg, def) stack_optlenstring(vm, (arg), (def), NULL)
const char* stack_optlenstring(VM* vm, int arg, const char* def, size_t* len);
int64_t stack_checkint(VM* vm, int arg);
int64_t stack_optint(VM* vm, int arg, int64_t def);
ObjDict* stack_checkdict(VM* vm, int arg);
ObjList* stack_checklist(VM* vm, int arg);
bool stack_checkboolean(VM* vm, int arg);
bool stack_optboolean(VM* vm, int arg, bool def);
double stack_checkfloat(VM* vm, int arg);
double stack_optfloat(VM* vm, int arg, double def);

/*
* Stack Manipulation
*/

// get functions stack -> C

#define stack_pop(vm, n) stack_settop(vm, -(n)-1)
inline int stack_gettop(VM * vm);
void stack_settop(VM* vm, int idx);
void stack_remove(VM* vm, int idx);
void stack_newDict(VM* vm);
void stack_newList(VM* vm, int length);
void stack_insert(VM* vm, int idx);
void stack_register(VM* vm, const char* name, const M_Reg* registry);
void stack_getglobal(VM* vm, const char* name);
void stack_setglobal(VM* vm, const char* name);
void stack_setfield(VM* vm, int idx, const char* name);
void stack_getfield(VM* vm, int idx, const char* name);
void stack_geti(VM* vm, int idx, int n);
void stack_seti(VM* vm, int idx, int n);
size_t stack_objlen(VM* vm, int idx);
void stack_setlist(VM* vm, int listpos, int keypos);
void stack_pushvalue(VM* vm, Value* o);
void stack_pushidxvalue(VM* vm, int idx);
int stack_gettype(VM* vm, int idx);
const char* stack_gettypename(VM* vm, int idx);
const char* stack_typename(VM* vm, int tag);
bool stack_isvoidornil(VM * vm, int idx);
bool stack_isCclosure(VM* vm, int idx);
bool stack_isMclosure(VM* vm, int idx);
bool stack_isint(VM* vm, int idx);
bool stack_isfloat(VM* vm, int idx);
bool stack_isnumber(VM * vm, int idx);
bool stack_isboolean(VM* vm, int idx);
bool stack_isstring(VM * vm, int idx);
bool stack_islist(VM * vm, int idx);
bool stack_isdict(VM * vm, int idx);
int64_t stack_toint(VM* vm, int idx, bool* isint);
double stack_tofloat(VM* vm, int idx, bool* isfloat);
void stack_tonumber(VM * vm, int idx, int64_t * i, double* f, bool* isint, bool* isfloat);
bool stack_toboolean(VM* vm, int idx);
const char* stack_tolenstring(VM* vm, int idx, size_t* len);
#define stack_tostring(vm, idx) stack_tolenstring(vm, idx, NULL)
const char* stack_ptolenstring(VM * vm, int idx, size_t * len);
MBuiltinFn stack_tocclosure(VM* vm, int idx);
ObjList* stack_tolist(VM* vm, int idx);
ObjDict* stack_todict(VM* vm, int idx);

// push functions C -> stack

void stack_pushvoid(VM* vm);
void stack_pushnil(VM* vm);
void stack_pushint(VM* vm, int64_t i);
void stack_pushfloat(VM* vm, double f);
void stack_pushnumber(VM * vm, double f, int64_t i, bool isint);
void stack_pushlenstring(VM* vm, const char* s, size_t len);
void stack_pushstring(VM* vm, const char* s);
void stack_pushostring(VM * vm, ObjString * s);
const char* stack_pushvfstring(VM* vm, const char* format, va_list argp);
const char* stack_pushfstring(VM* vm, const char* format, ...);
void stack_pushcclosure(VM* vm, MBuiltinFn fn, const char* name, int nupvals, ObjDict* env);
void stack_pushboolean(VM* vm, bool b);
void stack_pushlist(VM * vm, ObjList * list);
void stack_pushdict(VM* vm, ObjDict* dict);
void stack_concat(VM * vm, int total);
bool stack_getupvalue(VM* vm, int funcidx, int n);
bool stack_setupvalue(VM* vm, int funcidx, int n);
void stack_traceback(VM * vm, const char* msg, int level);

/*
* Realloc & Alloc Stack
*/

inline size_t get_grow_stack(VM* vm, int n);
void stack_reallocstack(VM * vm, size_t new_capacity);
void stack_checkCstack(VM* vm);
CallFrame* stack_growFrames(VM* vm);
void stack_reallocFrames(VM* vm, size_t new_capacity);
bool stack_checkstack(VM* vm, int space_needed);
inline bool stack_checkstacklimit(VM* vm, int space_needed);
void stack_callFunc(VM* vm, Value* func, int nresults);
void stack_callCFunc(VM* vm, int idx, int nresults);
void stack_call(VM* vm, int nargs, int nresults);
void stack_init(VM* vm);
