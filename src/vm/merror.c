
#include <stdarg.h>

#include "mstack.h"
#include "merror.h"
#include "vm.h"

M_NORETURN void m_throw_error(VM* vm, RErrorType type)
{
    ErrorHandler* h;
    if (h = vm->current_handler)
    {
        h->status = type;
        longjmp(h->buffer, type);
    }

    printf("There is no error handler.\n");
    abort();
}

#define pcR(pc, c)  ((pc) && ((pc) != (c)->instructions) ? cast(size_t, (pc) - (c)->instructions) - 1 : 0)

static size_t currentpc(VM* vm, CallFrame* frame)
{
    return pcR(frame->pc, cast(Closure*, frame->closure->obj)->M.chunk);
}

static size_t currentline(VM* vm, CallFrame* frame)
{
    Chunk* chu = cast(Closure*, frame->closure->obj)->M.chunk;

    return chu->lines[currentpc(vm, frame)];
}

static const char* getfuncname(Closure* cl)
{
    if (cl->isC)
    {
        if (cl->C.name)
            return cl->C.name;
    }
    else
    {
        Chunk* chu = cl->M.chunk;

        if (chu->name)
            return chu->name;
    }

    return NULL;
}

static const char* currentfuncname(VM* vm)
{
    Closure* cl = vm->frame > vm->frames ? closurevalue(vm->frame->closure) : NULL;
    const char* name = cl && cl->isC ? cl->C.name : NULL;
    return name;
}

static Closure* auxgetinfo(VM* vm, const char* what, m_debug* md, Closure* cl, CallFrame* frame)
{
    Closure* clos = NULL;
    for (; *what; what++)
    {
        switch (*what)
        {
            case 's':
            {
                if (cl->isC)
                {
                    md->source = "[C]";
                    md->what = "C";
                    md->linedefined = 0;
                    md->short_src = "[C]";
                }
                else
                {
                    const char* source = cl->M.chunk->source;
                    md->source = source;
                    md->what = "M";
                    md->linedefined = 0;
                    md->short_src = cl->M.chunk->source;
                }
                continue;
            }

            case 'l':
            {
                if (frame)
                {
                    md->currentline = isMclosure(frame->closure) ? currentline(vm, frame) : 0;
                }
                else
                {
                    md->linedefined = 0;
                }

                continue;
            }

            case 'u':
            {
                md->nupvals = cl->upvalue_count;
                continue;
            }

            case 'a':
            {
                if (cl->isC)
                {
                    md->isvararg = true;
                    md->nparamps = 0;
                }
                else
                {
                    md->isvararg = false;
                    md->nparamps = cl->M.chunk->parameter_count;
                }

                continue;
            }

            case 'n':
            {
                md->name = frame ? getfuncname(frame->closure->obj) : getfuncname(cl);
                continue;
            }

            case 'f':
            {
                clos = cl;
                continue;
            }

            default:
                M_UNREACHABLE();
        }
    }

    return clos;
}

bool m_getinfo(VM* vm, int level, const char* what, m_debug* md)
{
    Closure* cl = NULL;
    CallFrame* frame = NULL;
    if (level < 0)
    {
        // element has to be in the stack
        if (-level > cast(int, vm->top - vm->base))
            return false;

        Value* fn = vm->top + level;

        if (!isclosure(fn))
            return false;

        cl = closurevalue(fn);
    }
    else if (cast(unsigned, level) < cast(unsigned, vm->frame - vm->frames))
    {
        frame = vm->frame - level;
        M_ASSERT(isclosure(frame->closure));
        cl = closurevalue(frame->closure);
    }

    if (cl)
    {
        Closure* fcl = NULL;
        if (fcl = auxgetinfo(vm, what, md, cl, frame))
        {
            setclosure(vm->top, fcl);
            incr_top(vm);
        }
    }

    return cl ? true : false;
}

void m_where(VM* vm, int level)
{
    m_debug md;
    if (m_getinfo(vm, level, "sl", &md) && md.currentline > 0)
    {
        stack_pushfstring(vm, "%s:%d: ", md.short_src, md.currentline);
        return;
    }

    checkstack(vm, 1);
    stack_pushstring(vm, "");
}

static void m_error_type(VM* vm, RErrorType type)
{
    M_ASSERT(type < LIMIT_ERROR);

    switch (type)
    {
    case INVALID_OPERANDS_ERROR:
        stack_pushstring(vm, "Invalid Operands Error: ");
        return;
    case TYPES_ERROR:
        stack_pushstring(vm, "Type Error: ");
        return;
    case ARGUMENT_ERROR:
        stack_pushstring(vm, "Argument Error: ");
        return;
    case ARITHMETIC_ERROR:
        stack_pushstring(vm, "Arithmetic Error: ");
        return;
    case MEMORY_ERROR:
        stack_pushstring(vm, "Memory Error: ");
        return;
    case INTERNAL_ERROR:
        stack_pushstring(vm, "Internal Error: ");
        return;
    case ASSIGNMENT_ERROR:
        stack_pushstring(vm, "Assignment Error: ");
        return;
    case RESIZE_LIST_ERROR:
        stack_pushstring(vm, "Resize List Error: ");
        return;
    case RESIZE_DICT_ERROR:
        stack_pushstring(vm, "Resize Dictionary Error: ");
        return;
    case INDEX_OUT_OF_BOUNDS_ERROR:
        stack_pushstring(vm, "Index out of bounds Error: ");
        return;
    case ATTEMPED_TO_INDEX_NO_COLLECTION_ERROR:
        stack_pushstring(vm, "Attemped to index no collection Error: ");
        return;
    case INDEX_ERROR:
        stack_pushstring(vm, "Index Error");
        return;
    case INVALID_KEY_TYPE_ERROR:
        stack_pushstring(vm, "Invalid key type Error: ");
        return;
    case ASSERTION_ERROR:
        stack_pushstring(vm, "Assertion Error: ");
        return;
    case CALL_NO_FUNCTION:
        stack_pushstring(vm, "Call No Function Error: ");
        return;
    case BITS_ERROR:
        stack_pushstring(vm, "Bits Error: ");
        return;
    case DEBUG_ERROR:
        stack_pushstring(vm, "Debug Error: ");
        return;
    case CUSTOM_ERROR:
        stack_pushstring(vm, "Custom Error: ");
        return;
    default:
        M_UNREACHABLE();
        stack_pushfstring(vm, "Unknown Type Error %d: ", type);
        return;
    }
}

M_NORETURN void m_error(VM* vm, RErrorType type,const char* format, ...)
{
    M_ASSERT(type > NONE_ERROR && type < LIMIT_ERROR);
    va_list argp;
    va_start(argp, format);
    m_where(vm, 1);
    m_error_type(vm, type);
    stack_pushvfstring(vm, format, argp);
    va_end(argp);
    stack_concat(vm, 3);
    m_throw_error(vm, type);
}

M_NORETURN void m_argerror(VM* vm, int arg, const char* extramsg)
{
    const char* funcname = currentfuncname(vm);
    if (funcname)
        m_error(vm, TYPES_ERROR, "invalid argument #%d to '%s' (%s)", arg, funcname, extramsg);
    else
        m_error(vm, TYPES_ERROR, "invalid argument #%d (%s)", arg, extramsg);
}

M_NORETURN void m_typeerror(VM* vm, int arg, const char* tname)
{
    const char* funcname = currentfuncname(vm);
    const char* objname = stack_gettypename(vm, arg);

    if (funcname)
        m_error(vm, TYPES_ERROR, "invalid argument #%d to '%s' (%s expected, got %s)", arg, funcname, tname, objname);
    else
        m_error(vm, TYPES_ERROR, "invalid argument #%d (%s expected, got %s)", arg, tname, objname);
}

M_NORETURN void m_tagerror(VM* vm, int arg, int tag)
{
    m_typeerror(vm, arg, stack_typename(vm, tag));
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

#ifdef DEBUG
const char* m_traceback(VM* vm)
{
    m_StrBuf buf;
    //m_buffinitsize(vm, &buf, 4096);
    m_bufferinit(vm, &buf);

    m_debug md;
    for (size_t level = 0; m_getinfo(vm, level, "sln", &md); ++level)
    {
        if (md.source)
            m_buffaddstring(&buf, md.short_src);

        if (md.currentline > 0)
        {
            char line[32];
            int l = snprintf(line, sizeof(line), " :%"PRId64, md.currentline);
            line[l >= 31 ? 31 : l] = '\0';

            m_buffaddstring(&buf, line);
        }

        if (md.name)
        {
            m_buffaddstring(&buf, " function ");
            m_buffaddstring(&buf, md.name);
        }

        m_buffaddstring(&buf, "\n");
    }

    m_buffpushresult(&buf);
    return stack_tostring(vm, -1);
}

#else

const char* m_traceback(VM* vm)
{
    ObjString* buf = bufstart_string(vm, 4096);
    size_t offset = 0;

    m_debug md;
    for (int level = 0; m_getinfo(vm, level, "sln", &md); ++level)
    {
        if (md.source)
            offset =  append(vm, buf, buf->length, offset, md.short_src);

        if (md.currentline > 0)
        {
            char line[32];
            snprintf(line, sizeof(line), " :%d", md.currentline);

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

    return buf->chars;
}

#endif

int m_runprotected(VM* vm, PMfunc func, void* ud)
{
    ErrorHandler h;
    h.status = NONE_ERROR;
    h.previous = vm->current_handler;
    vm->current_handler = &h;
    if (setjmp(h.buffer) == 0)
        func(vm, ud);

    vm->current_handler = h.previous;
    return h.status;
}

static int resume_start(VM* vm, int nargs)
{
    api_check(nargs >= 0);
    api_check(vm->top - vm->base >= nargs);

    if (vm->frame != vm->frames)
    {
        vm->top -= nargs;
        setstring(vm->top, allocate_string(vm, "cannot resume a non-main function", 33));
        incr_top(vm);
        return 1;
    }

    vm->nestedCcalls = 0;
    return 0;
}

static void resume_continue(VM* vm)
{
    while (vm->frame > vm->frames)
    {
        Closure* clo = closurevalue(vm->frame->closure);

        if (clo->isC)
        {
            m_error(vm, INTERNAL_ERROR, "cannot resume a C function");
        }
        else
        {
            vm_execute(vm);
        }
    }
}

static int resume_finish(VM* vm, int status)
{
    // C call count base was set to an incremented value of C call count in resume, so we decrement here
    vm->nestedCcalls--;
    if (status)
    {
        setobjr(vm->top, vm->top - 1); // error message on current top
        vm->top++;
        vm->frame->top = vm->top;
    }
    else
    {
        expandstacklimit(vm, vm->top);
    }
    return status;
}

static void resume(VM* vm, void* ud)
{
    Value* first_arg = cast(Value*, ud);
    M_ASSERT(vm->frame == vm->frames && first_arg >= vm->base);
    
    int precallresult = vmutils_precall(vm, first_arg - 1, -1);

    // C function is completed, exit
    if (precallresult != 1)
        return;

    // mark to not return past the current CallFrame
    vm->frame->flags |= M_CALLFRAME_RETURN;

    // run continuations from stack
    resume_continue(vm);
}

int m_resume(VM* vm, int nargs)
{
    int starterr = 0;
    if (starterr = resume_start(vm, nargs))
        return starterr;

    int status = m_runprotected(vm, resume, vm->top - nargs);

    int result = resume_finish(vm, status);

    return result;
}

//void invalidOperandsError(VM* vm, int line, const char* op, const char* type1, const char* type2)
//{
//    vm_runtime_raise(vm, INVALID_OPERANDS_ERROR, line,
//    "TypeError: Invalid operands '%s' and '%s' for operator '%s'", 
//     type1 != NULL ? type1 : "", 
//     type2 != NULL ? type2 : "",
//     op);
//}
//
//void typeError(VM* vm, int line, const char* type1, const char* type2)
//{
//    vm_runtime_raise(vm, TYPES_ERROR, line,
//    "TypeError: Expected '%s', but got '%s'", 
//    type1, type2);
//}
//
//void arithmeticError(VM* vm, int line)
//{
//    vm_runtime_raise(vm, ARITHMETIC_ERROR, line,
//    "ArithmeticError: Division By Zero");
//}
//
//void memoryError(VM* vm, int line)
//{
//    vm_runtime_raise(vm, MEMORY_ERROR, line,
//    "MemoryError: An error occurred in memory allocation");
//}
//
//void unknownType(VM* vm, int line, int type)
//{
//    vm_runtime_raise(vm, INTERNAL_ERROR, line,
//    "InternalError: Unknown Type; Number Enum Type %d", type);
//}
//
//void cannotAddElementNotList(VM* vm, int line, const char* type)
//{
//    vm_runtime_raise(vm, ASSIGNMENT_ERROR, line,
//    "AssignmentError: Cannot assign an item to a non-list; Got type %s", type);
//}
//
//void cannotResizeList(VM* vm, int line)
//{
//    vm_runtime_raise(vm, RESIZE_LIST_ERROR, line,
//    "ResizeListError: Cannot resize a fixed list");
//}
//
//void cannotResizeDict(VM* vm, int line)
//{
//    vm_runtime_raise(vm, RESIZE_DICT_ERROR, line,
//    "ResizeDictError: Cannot resize a fixed Dictionary");
//}
//
//void resizeFractured(VM* vm, int line, const char* complement)
//{
//    vm_runtime_raise(vm, INTERNAL_ERROR, line,
//    "InternalError: Something was wrong when try to resize the %s", complement);
//}
//
//void indexoutofbound(VM* vm, int line, int pos, int capacity)
//{
//    vm_runtime_raise(vm, INDEX_OUT_OF_BOUNDS_ERROR, line,
//    "IndexOutOfBoundsError: Tried to index position %d when length is %d", pos, capacity);
//}
//
//void attempedToIndexNoCollection(VM* vm, int line, const char* type)
//{
//    vm_runtime_raise(vm, ATTEMPED_TO_INDEX_NO_COLLECTION_ERROR, line,
//    "AttempedToIndexNoCollection: Attemped to index %s instead of collection", type);
//}
//
//void indexError(VM* vm, int line, const char* expected, const char* got)
//{
//    vm_runtime_raise(vm, INDEX_ERROR, line,
//    "IndexError: Expected index type '%s', but got '%s'", expected, got);
//}
//
//void invalidKeyType(VM* vm, int line, const char* expected, const char* got)
//{
//    vm_runtime_raise(vm, INVALID_KEY_TYPE_ERROR, line,
//    "InvalidKeyTypeError: Expected types '%s' to dict key, but got type '%s'", expected, got);
//}

