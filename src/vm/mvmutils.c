
#include <inttypes.h>
#include <math.h>

#include "mobjects.h"
#include "mstack.h"
#include "vm.h"


static bool try_parse_string_number(const char* chars, size_t length, MNumType* type, int64_t* iout, double* fout)
{
    char* buffer = malloc(length + 1);
    if (buffer == NULL)
    {
        memoryCrash("VM");
        exit(1);
    }
    memcpy(buffer, chars, length);
    buffer[length] = '\0';

    char* dot = memchr(buffer, '.', length);

    if (dot != NULL)
    {
        if (!fout)
            return false;

        errno = 0;
        char* endptr;
        double num = strtod(buffer, &endptr);

        bool ok = (endptr != buffer && *endptr == '\0' && errno == 0);

        if (ok)
        {
            *type = M_TYPE_FLOAT;
            *fout = num;
        }
        

        free(buffer);
        return ok;
    }
    else
    {
        if (!iout)
            return false;

        errno = 0;
        char* endptr;
        int64_t num = strtoll(buffer, &endptr, 10);

        bool ok = (endptr != buffer && *endptr == '\0' && errno == 0);
        if (ok)
        {
            *type = M_TYPE_INT;
            *iout = num;
        }


        free(buffer);
        return ok;
    }
}

static bool tryToNumber(const Value* v, MNumType* type, int64_t* iout, double* fout)
{
    if (isint(v) && iout)
    {
        *type = M_TYPE_INT;
        *iout = ivalue(v);
        return true;
    }
    else if (isfloat(v) && fout)
    {
        *type = M_TYPE_FLOAT;
        *fout = fvalue(v);
        return true;
    }
    else if (isint(v) && fout)
    {
        *type = M_TYPE_FLOAT;
        *fout = (double)ivalue(v);
        return true;
    }
    else if (isfloat(v) && iout)
    {
        *type = M_TYPE_INT;
        *iout = (int64_t)fvalue(v);
        return true;
    }
    else if (isstring(v) && (iout || fout))
    {
        return try_parse_string_number(svalue(v), slenvalue(v), type, iout, fout);
    }

    return false;
}


Value* vmutils_tryToInt(const Value* obj, Value* n)
{
    int64_t num;
    int t;
    if (isint(obj))
        return obj;
    else if (tryToNumber(obj, &t, &num, NULL))
    {
        M_ASSERT(t == M_TYPE_INT);
        setint(n, num);
        return n;
    }
    else
        return NULL;
}

Value* vmutils_tryToFloat(const Value* obj, Value* n)
{
    double num;
    int t;
    if (isfloat(obj))
        return obj;
    else if (tryToNumber(obj, &t, NULL, &num))
    {
        M_ASSERT(t == M_TYPE_FLOAT);
        setfloat(n, num);
        return n;
    }
    else
        return NULL;
}

Value* vmutils_tryToNumber(const Value* obj, Value* n)
{
    int64_t iout;
    double fout;
    int t;
    if (isint(obj))
        return obj;
    else if (isfloat(obj))
        return obj;
    else if (tryToNumber(obj, &t, &iout, &fout))
    {
        M_ASSERT(t == M_TYPE_INT || t == M_TYPE_FLOAT);
        if (t == M_TYPE_INT)
        {
            setint(n, iout);
        }
        else if (t == M_TYPE_FLOAT)
        {
            setfloat(n, fout);
        }
        else
        {
            M_UNREACHABLE();
            return NULL;
        }

        return n;
    }
    else
        return NULL;
}

bool vmutils_tryToString(VM* vm, Value* obj)
{
    if (isstring(obj)) return true;

    if (isnumber(obj)) // isnumber() es equivalente a (isint() || isfloat())
    {
        char str[48];
        if (isint(obj))
        {
            int64_t num = ivalue(obj);
            size_t len = snprintf(str, sizeof(str), "%" PRId64, ivalue(obj));
            if (len < 0) return false;

            str[len] = '\0';
            setstring(obj, allocate_string(vm, str, len));
            return true;
        }
        else
        {
            double num = fvalue(obj);
            // Si quieres que 5.0 se vea como "5.0" y no como "5"
            size_t len;
            if (floor(num) == num)
                len = snprintf(str, sizeof(str), "%.1f", num);
            else
                len = snprintf(str, sizeof(str), "%.17g", num);

            if (len < 0) return false;

            str[len] = '\0';
            setstring(obj, allocate_string(vm, str, len));
            return true;
        }
    }
    else
        return false;
}

bool vmutils_valueEquals(VM* vm, const Value* v1, const Value* v2)
{
    if (ttype(v1) == ttype(v2))
    {
        if (isboolean(v1))
        {
            return bvalue(v1) == bvalue(v2);
        }
        else if (isfloat(v1))
        {
            return fvalue(v1) == fvalue(v2);
        }
        else if (isint(v1))
        {
            return ivalue(v1) == ivalue(v2);
        }
        else if (isstring(v1))
        {
            return v1->obj == v2->obj;

        }
        else if (isnil(v1))
        {
            return true;
        }
        else if (isvoid(v1))
        {
            return true;
        }
        else
        {
            //unknownType(vm, chunk->lines[i], ttype(rb));
            //m_throw_error(vm, INTERNAL_ERROR, line, "Unknown Type; Internal Enum '%d'", ttype(v1));
            m_error(vm, INTERNAL_ERROR, "Unknown Type; Internal Enum '%d'", ttype(v1));
        }
    }
    //else
    //{
    // here we will call to the '__equals' metamethod when we implement inheritance and metamethods
    return false; // not else to avoid warnings
    //}
}

bool vmutils_lessthan(VM* vm, const Value* v1, const Value* v2)
{
    if (ttype(v1) == ttype(v2))
    {
        if (isfloat(v1))
        {
            return fvalue(v1) < fvalue(v2);
        }
        else if (isint(v1))
        {
            return ivalue(v1) < ivalue(v2);
        }
        else if (isstring(v1))
        {
            return (vmutils_strcmp(osvalue(v1), osvalue(v2)) < 0);

        }
        else
            m_error(vm, INVALID_OPERANDS_ERROR, "Invalid operands '%s' and '%s' for operator '<'", getValueTypeName(v1), getValueTypeName(v2));
    }
    else
    {
        m_error(vm, INVALID_OPERANDS_ERROR, "Invalid operands '%s' and '%s' for operator '<'", getValueTypeName(v1), getValueTypeName(v2));
    }
    return false; // to avoid warnings
}

bool vmutils_lessOrEquals(VM* vm, const Value* v1, const Value* v2)
{
    if (ttype(v1) == ttype(v2))
    {
        if (isfloat(v1))
        {
            return fvalue(v1) <= fvalue(v2);
        }
        else if (isint(v1))
        {
            return ivalue(v1) <= ivalue(v2);
        }
        else if (isstring(v1))
        {
            return (vmutils_strcmp(osvalue(v1), osvalue(v2)) <= 0);
        }
        else
            m_error(vm, INTERNAL_ERROR, "Unknown Type; Internal Enum '%d'", ttype(v1));
    }
    else
    {
        m_error(vm, INVALID_OPERANDS_ERROR, "Invalid operands '%s' and '%s' for operator '<='", getValueTypeName(v1), getValueTypeName(v2));
    }
    return false; // to avoid warinigs
}

/*
* this function compare 2 strings caracter by caracter on ASCII format
* return < 0 if left string is smaller than right string
* returns 0 if both strings are equals
* return > 0 if right string is smaller than left string
*/
int vmutils_strcmp(ObjString* leftS, ObjString* rightS)
{
    /* Since the strings are interning, we compare the references, because if they are equal they will have the same reference */
    if (leftS == rightS)
        return 0;

    const char* left = leftS->chars;
    const char* right = rightS->chars;

    size_t lengthL = leftS->length;
    size_t lengthR = rightS->length;
    size_t lengthMin = lengthL < lengthR ? lengthL : lengthR;

    int res = memcmp(left, right, lengthMin); /* We compare the strings byte by byte */

    if (res != 0) /* if res is 0, it means strings are equals */
        return res;
    /* We compare if they have the same size, or else we compare if left is smaller than right */
    return lengthL == lengthR ? 0 : lengthL < lengthR ? -1 : 1;
}

#define MAXSIZE (1 << 30)

void vmutils_concat(VM* vm, int total, int last)
{
    // concat simbol '<>'
    do
    {
        Value* top = vm->base + last + 1;
        int n = 2;

        if (!(isstring(top - 2) || (isint(top - 2) || isfloat(top - 2)) || !isstring(top - 1)))
        {
            // here will be inheritance of the __tostring or __concat metamethod, but for now we will just throw an error
            m_error(vm, INVALID_OPERANDS_ERROR, "Invalid operands '%s' and '%s' for operator '<>'", getValueTypeName(top - 2), getValueTypeName(top - 1));
        }
        else if (slenvalue(top - 1) == 0)
        {
            (void)tostring(vm, top - 2);
        }
        else
        {
            size_t totalLength = slenvalue(top - 2) + slenvalue(top - 1);
            char* buffer;
            // collect total length of the new string
            for (n = 1; n < total && tostring(vm, top - n - 1); n++)
            {
                size_t l = slenvalue(top - n - 1);
                if (l > MAXSIZE)
                    m_error(vm, INTERNAL_ERROR, "string length overflow");
                totalLength += l;
            }
            char buf[512];
            ObjString* s = NULL;

            if (totalLength < 512)
            {
                buffer = buf;
            }
            else
            {
                s = bufstart_string(vm, totalLength);
                buffer = s->chars;
            }

            totalLength = 0;
            for (int i = n; i > 0; i--)
            {
                // concat all string
                size_t l = slenvalue(top - i);
                memcpy(buffer + totalLength, svalue(top - i), l);
                totalLength += l;
            }

            if (totalLength < 512)
            {
                setstring(top - n, allocate_string(vm, buffer, totalLength));
            }
            else
            {
                setstring(top - n, bufend_string(vm, s));
            }
        }
        total -= n - 1; // got 'n' strings to create 1 new
        last -= n - 1;
    }
    while (total > 1); // repeat until 1 string result left
}

int vmutils_precall(VM* vm, Value* func, int nresults)
{
    if (!isclosure(func))
        m_error(vm, CALL_NO_FUNCTION, "cannot call a no function");

    Closure* clos = closurevalue(func);

    CallFrame* frame = incr_frame(vm);
    frame->closure = func;
    frame->registers = func + 1;
    frame->top = vm->top + clos->stack_size;
    frame->pc = NULL;
    frame->expected_returns = nresults;
    
    vm->base = frame->registers;
    // Note: vm->top is assigned externally

    checkstack(vm, clos->stack_size);
    M_ASSERT(frame->top < vm->stack_last);

    if (!clos->isC)
    {
        Chunk* chunk = clos->M.chunk;

        
        Value* arg_init = vm->top;
        Value* arg_end = vm->base + chunk->parameter_count;
        while (arg_init < arg_end)
            setvoid(arg_init++); // complete missing arguments
        vm->top = frame->top;

        frame->pc = chunk->instructions;

        return 1;
    }
    else
    {
        MBuiltinFn f = clos->C.funcC;
        int n = f(vm);

        // frame is our frame, framep is our parent
        CallFrame* frame = vm->frame;
        CallFrame* framep = frame - 1;

        Value* res = frame->closure;
        Value* val_init = vm->top - n;
        Value* val_end = vm->top;
        
        int i;
        for (i = nresults; i != 0 && val_init < val_end; i--)
            setobjr(res++, val_init++);
        while (i-- > 0)
            setvoid(res++);
        
        vm->frame = framep;
        vm->base = framep->registers;
        vm->top = res;

        return 2;
    }
}
