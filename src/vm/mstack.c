
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "mstack.h"
#include "vm.h"
#include "mobjects.h"
#include "merror.h"

const char* M_TypesName[] = {
    "void",
    "nil",
    "int",
    "float",
    "boolean",
    "string",
    "list",
    "dictionary",
    "closure"
};

/*
* Stack Api
*/

#define abs_idx(idx) stack_gettop(vm) + idx + 1;

#define check_stack(vm, size) \
    if ((vm)->top + (size) > (vm)->frame->top && !stack_checkstack(vm, (size))) \
    { \
         m_error(vm, MEMORY_ERROR, "Stack Overflow; too many elements in the stack");\
    }

#define stack_opt(vm, func, arg, def) (stack_isvoidornil(vm, (arg)) ? (def) : func(vm, arg))

static Value* index2value(VM* vm, int idx)
{
    if (idx > 0)
    {
        api_check(idx <= vm->frame->top - vm->base);
        Value* value = vm->base + (idx - 1);
        if (value >= vm->top)
            return m_void_obj;
        else
            return value;
    }
    else
    {
        api_check(idx != 0 && -idx <= vm->top - vm->base);
        Value* value = vm->top + idx;
        return value;
    }
}

void stack_checkstackR(VM* vm, int space, const char* msg)
{
    if (!stack_checkstack(vm, space))
        m_error(vm, MEMORY_ERROR, "stack overflow (%s)", msg);
}

void stack_checktype(VM* vm, int arg, int tag)
{
    if (stack_gettype(vm, arg) != tag)
        m_tagerror(vm, arg, tag);
}

void stack_checkany(VM* vm, int arg)
{
    if (stack_gettype(vm, arg) == VAL_VOID)
    {
        const char* format = "missing argument #%d";
        char result[512];
        snprintf(result, sizeof(result), format, arg);
        m_argerror(vm, arg, result);
    }
}

int stack_absindex(VM* vm, int idx)
{
    return idx >= 0 ? idx : abs_idx(idx);
}

const char* stack_checklenstring(VM* vm, int arg, size_t* len)
{
    const char* s = stack_tolenstring(vm, arg, len);
    if (!s)
        m_tagerror(vm, arg, OBJ_STRING);
    return s;
}

const char* stack_optlenstring(VM* vm, int arg, const char* def, size_t* len)
{
    if (stack_isvoidornil(vm, arg))
    {
        if (len)
            *len = (def ? strlen(def) : 0);
        return def;
    }
    else
        return stack_checklenstring(vm, arg, len);
}

int64_t stack_checkint(VM* vm, int arg)
{
    bool isint;
    int64_t i = stack_toint(vm, arg, &isint);
    if (!isint)
        m_tagerror(vm, arg, VAL_INT);
    return i;
}

int64_t stack_optint(VM* vm, int arg, int64_t def)
{
    return stack_opt(vm, stack_checkint, arg, def);
}

ObjDict* stack_checkdict(VM* vm, int arg)
{
    ObjDict* d = stack_todict(vm, arg);
    if (!d) // d == NULL
        m_tagerror(vm, arg, OBJ_DICTIONARY);
    return d;
}

ObjList* stack_checklist(VM* vm, int arg)
{
    ObjList* l = stack_tolist(vm, arg);
    if (!l) // l == NULL
        m_tagerror(vm, arg, OBJ_LIST);
    return l;
}

bool stack_checkboolean(VM* vm, int arg)
{
    // This check specifically for boolean values, ignoring
    // all other thruthy or falsy values. If you want to allow
    // truthy/falsy values, you can use stack_toboolean instead.
    if (!stack_isboolean(vm, arg))
        m_tagerror(vm, arg, VAL_BOOLEAN);
    return stack_toboolean(vm, arg);
}

bool stack_optboolean(VM* vm, int arg, bool def)
{
    return stack_opt(vm, stack_checkboolean, arg, def);
}

double stack_checkfloat(VM* vm, int arg)
{
    bool isfloat;
    double f = stack_tofloat(vm, arg, &isfloat);
    if (!isfloat)
        m_tagerror(vm, arg, VAL_FLOAT);
    return f;
}

double stack_optfloat(VM* vm, int arg, double def)
{
    return stack_opt(vm, stack_checkfloat, arg, def);
}

/*
* Stack Manipulation
*/

extern inline int stack_gettop(VM* vm)
{
    return cast(int, vm->top - vm->base);
}

void stack_settop(VM* vm, int idx)
{
    if (idx > 0)
    {
        check_stack(vm, idx - cast(int, vm->top - vm->base));
        while (vm->top < vm->base + idx)
            setvoid(vm->top++);
        vm->top = vm->base + idx;
    }
    else
    {
        api_check(-(idx + 1) <= (vm->top - vm->base));
        vm->top += idx + 1;
    }
}

void stack_remove(VM* vm, int idx)
{
    Value* v = index2value(vm, idx);
    while (++v < vm->top)
        setobjr(v - 1, v);
    vm->top--;
}

void stack_insert(VM* vm, int idx)
{
    Value* v = index2value(vm, idx);
    for (Value* q = vm->top; q > v; q--)
        setobjr(q, q - 1);
    setobjr(v, vm->top);
}

void stack_newDict(VM* vm)
{
    stack_pushdict(vm, allocate_dict(vm, 1, false));
}

void stack_newList(VM* vm, int length)
{
    stack_pushlist(vm, allocate_list(vm, length, length, false));
}

void stack_setlist(VM* vm, int listpos, int keypos)
{
    Value* o = index2value(vm, listpos);
    api_check(islist(o));
    if (olistvalue(o)->fixed)
        m_error(vm, RESIZE_LIST_ERROR, "cannot resize a fixed list");

    setobjr(listvalue(o, keypos), vm->top - 1);
}

void stack_register(VM* vm, const char* name, const M_Reg* registry)
{
    if (name)
    {
        stack_getglobal(vm, name);
        if (!stack_isvoidornil(vm, -1))
            m_error(vm, INTERNAL_ERROR, "global variable '%s' already exists", name);
        stack_pop(vm, 1);
        stack_newDict(vm);
        stack_setglobal(vm, name);
        vm->top++;
    }

    for (; registry->name; registry++)
    {
        stack_pushcclosure(vm, registry->func, registry->name, 0, vm->gd);
        stack_setfield(vm, -2, registry->name);
    }
    stack_pop(vm, 1); // remove library
}

void stack_getglobal(VM* vm, const char* name)
{
    Value v;
    setstring(&v, allocate_string(vm, name, strlen(name)));
    stack_pushvalue(vm, get_dict_value(vm, vm->gd, &v));
}

void stack_setglobal(VM* vm, const char* name)
{
    Value v;
    setstring(&v, allocate_string(vm, name, strlen(name)));
    set_dict_key_value(vm, vm->gd, &v, vm->top - 1);
    vm->top--;
}

void stack_setfield(VM* vm, int idx, const char* name)
{
    Value* v = index2value(vm, idx);
    Value key;
    setstring(&key, allocate_string(vm, name, strlen(name)));
    set_dict_key_value(vm, cast(ObjDict*, v->obj), &key, vm->top - 1);
    vm->top--;
}

void stack_getfield(VM* vm, int idx, const char* name)
{
    Value* v = index2value(vm, idx);
    Value key;
    setstring(&key, allocate_string(vm, name, strlen(name)));
    stack_pushvalue(vm, get_dict_value(vm, cast(ObjDict*, v->obj), &key));
}

void stack_geti(VM* vm, int idx, int n)
{
    Value* l = index2value(vm, idx);
    api_check(islist(l));
    setobjr(vm->top, listvalue(l, n));
    incr_top(vm);
}

void stack_seti(VM* vm, int idx, int n)
{
    Value* l = index2value(vm, idx);
    api_check(islist(l));
    ObjList* list = olistvalue(l);
    ////if (list->readonly)
    //if (n > list->capacity && list->fixed)
    //    m_error(vm, RESIZE_LIST_ERROR, "list is fixed");
    if (list->readonly)
        m_error(vm, RESIZE_LIST_ERROR, "list is readonly");

    if (n >= list->capacity)
    {
        if (list->fixed)
            m_error(vm, RESIZE_LIST_ERROR, "list is fixed");

        resize_list(vm, list, list->capacity < n / 2 ? list->capacity * 2 : list->capacity + n);
    }
    // we cannot use set_list_element here because if n is greater than current length, it will throw an error
    //set_list_element(vm, list, *vm->top, n);
    list->values[n] = *(vm->top - 1);
    list->length++;
    vm->top--;
}

size_t stack_objlen(VM* vm, int idx)
{
    stack_checkany(vm, stack_absindex(vm, idx));
    Value* v = index2value(vm, idx);
    if (isstring(v))
    {
        return slenvalue(v);
    }
    else if (islist(v))
    {
        return cast(size_t, listlenvalue(v));
    }
    else if (isdict(v))
    {
        return cast(size_t, dictlenvalue(v));
    }
    else
    {
        m_typeerror(vm, stack_absindex(vm, idx), "list, string or dict");
        return 0; // to avoid warnings
    }
}

void stack_pushvalue(VM* vm, Value* o)
{
    check_stack(vm, 1);
    setobjr(vm->top, o);
    incr_top(vm);
}

void stack_pushidxvalue(VM* vm, int idx)
{
    check_stack(vm, 1);
    Value* v = index2value(vm, idx);
    setobjr(vm->top, v);
    incr_top(vm);
}

int stack_gettype(VM* vm, int idx)
{
    Value* v = index2value(vm, idx);
    return ttype(v) == VAL_OBJ ? gcttype(v) : ttype(v);
}

const char* stack_gettypename(VM* vm, int idx)
{
    Value* v = index2value(vm, idx);
    return ttype(v) < VAL_OBJ ? M_TypesName[ttype(v)] : M_TypesName[gcttype(v)-1];
}

const char* stack_typename(VM* vm, int tag)
{
    return tag < VAL_OBJ ? M_TypesName[tag] : M_TypesName[tag - 1];
}

bool stack_isvoidornil(VM* vm, int idx)
{
    Value* v = index2value(vm, idx);
    return isvoid(v) || isnil(v);
}

bool stack_isCclosure(VM* vm, int idx)
{
    Value* v = index2value(vm, idx);
    return isCclosure(v);
}

bool stack_isMclosure(VM* vm, int idx)
{
    Value* v = index2value(vm, idx);
    return isMclosure(v);
}

bool stack_isint(VM* vm, int idx)
{
    int t = stack_gettype(vm, idx);
    return t == VAL_INT;
}

bool stack_isfloat(VM* vm, int idx)
{
    int t = stack_gettype(vm, idx);
    return t == VAL_FLOAT;
}

bool stack_isnumber(VM* vm, int idx)
{
    int t = stack_gettype(vm, idx);
    return t == VAL_FLOAT || t == VAL_INT;
}

bool stack_isboolean(VM* vm, int idx)
{
    int t = stack_gettype(vm, idx);
    return t == VAL_BOOLEAN;
}

bool stack_isstring(VM* vm, int idx)
{
    Value* v = index2value(vm, idx);
    int t = stack_gettype(vm, idx);
    return t == VAL_OBJ && v->obj->objType == OBJ_STRING;
}

bool stack_islist(VM* vm, int idx)
{
    Value* v = index2value(vm, idx);
    int t = stack_gettype(vm, idx);
    return t == VAL_OBJ && v->obj->objType == OBJ_LIST;
}

bool stack_isdict(VM* vm, int idx)
{
    Value* v = index2value(vm, idx);
    int t = stack_gettype(vm, idx);
    return t == VAL_OBJ && v->obj->objType == OBJ_DICTIONARY;
}

int64_t stack_toint(VM* vm, int idx, bool* isint)
{
    Value n;
    const Value* o = index2value(vm, idx);
    if (toint(o, &n))
    {
        if (isint)
            *isint = true;
        return ivalue(o);
    }
    else
    {
        if (isint)
            *isint = false;
        return 0;
    }
}

double stack_tofloat(VM* vm, int idx, bool* isfloat)
{
    Value n;
    const Value* o = index2value(vm, idx);
    if (tofloat(o, &n))
    {
        if (isfloat)
            *isfloat = true;
        return fvalue(o);
    }
    else
    {
        if (isfloat)
            *isfloat = false;
        return 0.0;
    }
}

void stack_tonumber(VM* vm, int idx, int64_t* i, double* f, bool* isint, bool* isfloat)
{
    if (i)
        *i = stack_toint(vm, idx, isint);
    if (isint && *isint || i) // We assume it will always return a non-zero number
    {
        if (isfloat)
            *isfloat = false;
        if (f)
            *f = 0.0;
        return;
    }
    *f = stack_tofloat(vm, idx, isfloat);
    if (isint)
        *isint = false;
}

bool stack_toboolean(VM* vm, int idx)
{
    const Value* o = index2value(vm, idx);
    return !isfalse(o);
}

const char* stack_tolenstring(VM* vm, int idx, size_t* len)
{
    Value* o = index2value(vm, idx);
    if (!isstring(o))
    {
        if (!vmutils_tryToString(vm, o))
        {
            if (len)
                *len = 0;
            return NULL;
        }
    }
    if (len)
        *len = cast(ObjString*, o->obj)->length;
    return svalue(o);
}

const char* stack_ptolenstring(VM* vm, int idx, size_t* len)
{
    // A call to '__string' will go here once we add inheritance and meta-methods for list/dict/function/etc.
    switch (stack_gettype(vm, idx))
    {
        case VAL_VOID:
            stack_pushstring(vm, "void");
            break;

        case VAL_NIL:
            stack_pushstring(vm, "nil");
            break;
        case VAL_INT:
        {
            char str[48];
            int64_t num = stack_toint(vm, idx, NULL);
            size_t len = snprintf(str, sizeof(str), "%" PRId64, num);
            if (len < 0) return NULL;
            str[len] = '\0';

            stack_pushlenstring(vm, str, len);
            break;
        }
        case VAL_FLOAT:
        {
            char str[48];
            double num = stack_tofloat(vm, idx, NULL);
            // Si quieres que 5.0 se vea como "5.0" y no como "5"
            size_t len;
            if (floor(num) == num)
                len = snprintf(str, sizeof(str), "%.1f", num);
            else
                len = snprintf(str, sizeof(str), "%.17g", num);

            if (len < 0) return NULL;
            str[len] = '\0';

            stack_pushlenstring(vm, str, len);
            break;
        }

        case VAL_BOOLEAN:
            stack_pushstring(vm, stack_toboolean(vm, idx) ? "true" : "false");
            break;

        case OBJ_STRING:
            stack_pushidxvalue(vm, idx);
            break;

        case OBJ_LIST:
        {
            m_StrBuf buff;
            m_buffinitsize(vm, &buff, 64);
            m_buffaddchar(&buff, '[');
            ObjList* list = stack_tolist(vm, idx);
            int n = list->length;

            for (int i = 0; i < n; ++i)
            {
                if (islist(&list->values[i]) && list == list->values[i].obj) //recursive, the same list?
                {
                    stack_pushstring(vm, "<recursive>");
                }
                else
                {
                    stack_pushvalue(vm, &list->values[i]);
                }
                size_t len2;
                const char* s2 = stack_ptolenstring(vm, -1, &len2);
                //m_buffaddlenstring(vm, s2, len2); // why m_buffaddlenstring is not working?
                m_buffaddstring(&buff, s2);
                if (i < n - 1)
                    m_buffaddstring(&buff, ", ");
            }

            m_buffaddchar(&buff, ']');
            m_buffpushresult(&buff);
            break;
        }

        default:
        {
            const void* ptr = cast(void*, index2value(vm, idx));
            unsigned long long dir = cast(uintptr_t, ptr);
            stack_pushfstring(vm, "%s: 0x%016llx", stack_gettypename(vm, idx), dir);
            break;
        }
    }

    return stack_tolenstring(vm, -1, len);
}

MBuiltinFn stack_tocclosure(VM* vm, int idx)
{
    Value* o = index2value(vm, idx);
    return (isCclosure(o)) ? NULL : cast(MBuiltinFn, closurevalue(o)->C.funcC);
}

ObjList* stack_tolist(VM* vm, int idx)
{
    Value* o = index2value(vm, idx);
    return (islist(o)) ? cast(ObjList*, o->obj) : NULL;
}

ObjDict* stack_todict(VM* vm, int idx)
{
    Value* o = index2value(vm, idx);
    return (isdict(o)) ? cast(ObjDict*, o->obj) : NULL;
}

// push functions C -> stack

void stack_pushvoid(VM* vm)
{
    check_stack(vm, 1);
    setvoid(vm->top);
    incr_top(vm);
}

void stack_pushnil(VM* vm)
{
    check_stack(vm, 1);
    setnil(vm->top);
    incr_top(vm);
}

void stack_pushint(VM* vm, int64_t i)
{
    check_stack(vm, 1);
    setint(vm->top, i);
    incr_top(vm);
}

void stack_pushfloat(VM* vm, double f)
{
    check_stack(vm, 1);
    setfloat(vm->top, f);
    incr_top(vm);
}

void stack_pushnumber(VM* vm, double f, int64_t i, bool isint)
{
    check_stack(vm, 1);
    if (isint)
        setint(vm->top, i)
    else
        setfloat(vm->top, f);
    incr_top(vm);
}

void stack_pushlenstring(VM* vm, const char* s, size_t len)
{
    api_check(s != NULL);
    check_stack(vm, 1);
    setstring(vm->top, allocate_string(vm, s, len));
    incr_top(vm);
}

void stack_pushstring(VM* vm, const char* s)
{
    if (s == NULL)
    {
        stack_pushvoid(vm);
    }
    else
        stack_pushlenstring(vm, s, strlen(s));
}

void stack_pushostring(VM* vm, ObjString* s)
{
    stack_pushlenstring(vm, s->chars, s->length);
}

const char* stack_pushvfstring(VM* vm, const char* format, va_list argp)
{
    char result[512];
    vsnprintf(result, sizeof(result), format, argp);
    stack_pushstring(vm, result);
    return svalue(vm->top - 1);
}

const char* stack_pushfstring(VM* vm, const char* format, ...)
{
    va_list argp;
    va_start(argp, format);
    const char* r = stack_pushvfstring(vm, format, argp);
    va_end(argp);
    return r;
}

void stack_pushcclosure(VM* vm, MBuiltinFn fn, const char* name, int nupvals, ObjDict* env)
{
    api_check(fn != NULL);
    api_check(nupvals >= 0);
    api_check(name != NULL);
    check_stack(vm, 1);
    api_check(cast(int, vm->top - vm->base) >= nupvals);
    Closure* cl = allocate_Cclosure(vm, nupvals, env);
    cl->C.funcC = fn;
    cl->C.name = name;
    cl->env = env;
    vm->top -= nupvals;
    while (nupvals--)
        setobjr(&cl->C.upvals[nupvals], vm->top + nupvals);
    setclosure(vm->top, cl);
    incr_top(vm);
}

void stack_pushboolean(VM* vm, bool b)
{
    check_stack(vm, 1);
    setboolean(vm->top, b != false); // ensure that true is 1
    incr_top(vm);
}

void stack_pushlist(VM* vm, ObjList* list)
{
    api_check(list != NULL);
    check_stack(vm, 1);
    set_list(vm->top, list);
    incr_top(vm);
}

void stack_pushdict(VM* vm, ObjDict* dict)
{
    api_check(dict != NULL);
    check_stack(vm, 1);
    set_dict(vm->top, dict);
    incr_top(vm);
}

void stack_concat(VM* vm, int total)
{
    api_check(total >= 0);
    api_check(cast(int, vm->top - vm->base) >= total);
    if (total >= 2)
    {
        vmutils_concat(vm, total, cast(int, vm->top - vm->base) - 1);
        vm->top -= (total - 1);
    }
    else if (total == 0)
    {
        stack_pushstring(vm, "");
    }
    // else total == 1, do nothing
}

static size_t append(VM* vm, ObjString* buf, size_t bufsize, size_t offset, const char* data)
{
    size_t size = strlen(data);
    if (offset + size >= bufsize)
    {
        size_t newbufsize = bufsize * 2 < (offset + size) ? bufsize * 2 : bufsize + offset + size < (SIZE_MAX / 2) ? bufsize + offset : SIZE_MAX - 1;
        ObjString* oldbuf = buf;
        ObjString* newbuf = bufstart_string(vm, newbufsize);
        size = offset + size >= newbuf->length ? newbuf->length - offset - 1 : size;
        memcpy(newbuf->chars, oldbuf->chars, offset);
        buf = newbuf;
    }

    memcpy(buf->chars + offset, data, size);
    return offset + size;
}

void stack_traceback(VM* vm, const char* msg, int level)
{
    api_check(level >= 0);
    
    ObjString* buf = bufstart_string(vm, 4096);
    size_t offset = 0;

    if (msg)
    {
        offset = append(vm, buf, buf->length, offset, msg);
        offset = append(vm, buf, buf->length, offset, "\n");
    }

    m_debug md;
    for (; m_getinfo(vm, level, "sln", &md); ++level)
    {
        if (md.source)
            offset = append(vm, buf, buf->length, offset, md.short_src);

        if (md.currentline > 0)
        {
            char line[32];
            snprintf(line, sizeof(line), " :%lld", md.currentline);

            offset = append(vm, buf, buf->length, offset, line);
        }

        if (md.name)
        {
            offset = append(vm, buf, buf->length, offset, " function ");
            offset = append(vm, buf, buf->length, offset, md.name);
        }

        offset = append(vm, buf, buf->length, offset, "\n");
    }

    M_ASSERT(offset < buf->length);
    buf->chars[offset] = '\0';

    stack_pushostring(vm, buf);
}

/*
* Realloc & Alloc Stack
*/

#define MAX_STACK_SIZE (cast(int, 1024 / sizeof(Value)) * 1024 * 1024)

extern inline bool stack_checkstacklimit(VM* vm, int space_needed)
{
    return (((uint8_t*)vm->stack_last - (uint8_t*)vm->top) <= (space_needed * (int)sizeof(Value)));
}

#define reallocstack(vm, realsize) \
    { \
        Value* ns = realloc((vm)->stack, (realsize)); \
        if (ns == NULL) \
            m_error((vm), MEMORY_ERROR, "Error trying to reallocate stack"); \
        (vm)->stack = ns; \
    }

#define reallocframes(vm, realsize) \
    { \
        CallFrame* nf = realloc((vm)->frames, (realsize)); \
        if (nf == NULL) \
            m_error((vm), MEMORY_ERROR, "Error trying to reallocate call stack"); \
        (vm)->frames = nf; \
    }

static void correcstack(VM* vm, Value* oldstack)
{
    vm->top = (vm->top - oldstack) + vm->stack;

    for (CallFrame* frame = vm->frames; frame <= vm->frame; frame++)
    {
        frame->top = (frame->top - oldstack) + vm->stack;
        frame->registers = (frame->registers - oldstack) + vm->stack;
        frame->closure = (frame->closure - oldstack) + vm->stack;
    }

    vm->base = (vm->base - oldstack) + vm->stack;
}

static void performcall(VM* vm, Value* func, int nresults)
{
    if (vmutils_precall(vm, func, nresults) == 1)
    {
        vm->frame->flags |= M_CALLFRAME_RETURN; // vm_execute will stop after returning from this frame

        vm_execute(vm);
    }
}

void stack_callFunc(VM* vm, Value* func, int nresults)
{
    if (++vm->nestedCcalls >= M_MAXCCALLS)
        stack_checkCstack(vm);

    ptrdiff_t funcoffset = savestack(vm, func);

    performcall(vm, func, nresults);

    if (nresults != -1)
        vm->top = restorestack(vm, funcoffset) + nresults;

    vm->nestedCcalls--;
}

void stack_callCFunc(VM* vm, int idx, int nresults)
{
    Value* func = index2value(vm, idx);
    stack_callFunc(vm, func, nresults);
}

void stack_call(VM* vm, int nargs, int nresults)
{
    api_check(nargs >= 0);
    api_check(nresults >= -1);
    api_check(nargs <= (vm->top - vm->base));

    if (nresults > nargs + 1)
        checkstack(vm, nresults - (nargs + 1));

    Value* func = vm->top - (nargs + 1);

    stack_callFunc(vm, func, nresults);

    if (nresults == -1 && vm->top >= vm->frame->top)
        vm->frame->top = vm->top;
}

void stack_reallocstack(VM* vm, size_t new_capacity)
{
    if (new_capacity > MAX_STACK_SIZE)
        m_error(vm, MEMORY_ERROR, "stack overflow");
    Value* oldstack = vm->stack;
    size_t realsize = new_capacity + EXTRA_STACK;
    M_ASSERT(vm->stack_last - vm->stack == vm->stack_size - EXTRA_STACK);
    reallocstack(vm, realsize);
    Value* newstack = vm->stack;
    for (size_t i = vm->stack_size; i < realsize; i++)
        setvoid(newstack + i);
    vm->stack_size = realsize;
    vm->stack_last = newstack + new_capacity;
    correcstack(vm, oldstack);
}

extern inline size_t get_grow_stack(VM* vm, int n)
{
    return n <= vm->stack_size ? vm->stack_size * 2 : vm->stack_size + n;
}

/**
* @param vm It's the runtime vm execution
* @param space_needed It's the space that we need in the stack
* @return Return true if we have free space in the stack for the space needed, otherwise false
*/
bool stack_checkstack(VM* vm, int space_needed)
{
    api_check(space_needed >= 0);
    if (space_needed > M_MAXSTACK || (vm->top - vm->base + space_needed) > M_MAXSTACK)
        return false;
    else if (space_needed > 0)
    {
        checkstack(vm, space_needed);
    }

    expandstacklimit(vm, vm->top + space_needed);
    return true;
}

void stack_reallocFrames(VM* vm, size_t new_capacity)
{
    CallFrame* oldframes = vm->frames;
    reallocframes(vm, new_capacity);
    vm->frame_count = new_capacity;
    vm->frame = (vm->frame - oldframes) + vm->frames;
    vm->frame_last = vm->frames + new_capacity - 1;
}

CallFrame* stack_growFrames(VM* vm)
{
    const size_t hardlimit = M_MAXCALLS + (M_MAXCALLS >> 3);

    if (vm->frame_count >= hardlimit)
        m_error(vm, MEMORY_ERROR, "stack overflow in error handling");

    size_t request = vm->frame_count * 2;

    stack_reallocFrames(vm, vm->frame_count >= M_MAXCALLS ? hardlimit : request < M_MAXCALLS ? request : M_MAXCALLS);

    if (vm->frame_count > M_MAXCALLS)
        m_error(vm, MEMORY_ERROR, "stack overflow");

    return ++vm->frame;
}

void stack_checkCstack(VM* vm)
{
    const int hardlimit = M_MAXCCALLS + (M_MAXCCALLS >> 3);
    if (vm->nestedCcalls == M_MAXCCALLS)
        m_error(vm, MEMORY_ERROR, "C stack overflow");
    else if (vm->nestedCcalls >= hardlimit)
        m_error(vm, MEMORY_ERROR, "C stack overflow in error handling");
}

#define BASIC_STACK_SIZE (2 * MIN_STACK)

void stack_init(VM* vm)
{
    // Initialize CallFrame array
    vm->frames = cast(CallFrame*, calloc(8, sizeof(CallFrame)));
    if (vm->frames == NULL)
    {
        memoryCrash("Memory Error to allocate CallFrame array");
        exit(1);
    }
    vm->frame = vm->frames;
    vm->frame_count = 8;
    vm->frame_last = vm->frames + vm->frame_count - 1;

    // Initialize Stack array
    vm->stack = cast(Value*, calloc(BASIC_STACK_SIZE + EXTRA_STACK, sizeof(Value)));
    if (vm->stack == NULL)
    {
        memoryCrash("Memory Error to allocate Stack array");
        exit(1);
    }
    vm->stack_size = BASIC_STACK_SIZE + EXTRA_STACK;
    vm->top = vm->stack;
    vm->stack_last = vm->stack + (vm->stack_size - EXTRA_STACK);

    // Initialize first frame
    vm->frame->closure = vm->top;
    vm->base = vm->frame->registers = ++vm->top;
    vm->frame->top = vm->top + MIN_STACK;
}
