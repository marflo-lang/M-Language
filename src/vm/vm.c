
#include "../utils/err.h"
#include "../backend/codegen.h"
#include "vm.h"

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <malloc.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h> 
#include <locale.h>
#include <stdarg.h>

/* --- Frames and errors --- */

/**/
static int vm_instruction_index(CallFrame* frame, Instruction* pc)
{
    ptrdiff_t off = pc - frame->chunk->instructions;
    if (off < 0)
        return 0;
    if (off >= frame->chunk->actual_instruction)
        return frame->chunk->actual_instruction - 1;
    return (int) off;
}

static int vm_line_at_pc(CallFrame* frame, Instruction* pc)
{
    int idx = vm_instruction_index(frame, pc);
    return frame->chunk->lines[idx];
}

/* init the registers from a new frame on NaN */
static void vm_init_frame_registers(Value* base, int count)
{
    for (int i = 0; i < count; i++)
        setnan(base[i]);
}

/* return the current line where vm is */
int vm_current_line(VM* vm)
{
    if (vm->frame_count == 0)
        return vm->error.line;
    CallFrame* frame = &vm->frames[vm->frame_count - 1];
    if (frame->ip <= frame->chunk->instructions)
        return frame->chunk->lines[0];
    return vm_line_at_pc(frame, frame->ip - 1);
}

/* return the frame name; this is necessary for trace stack */
const char* vm_frame_name(VM* vm, int frame_index)
{
    CallFrame* frame = &vm->frames[frame_index];
    if (frame->func_name != NULL)
        return frame->func_name;
    if (frame->chunk->name != NULL)
        return frame->chunk->name;
    if (frame_index == 0)
        return vm->name != NULL ? vm->name : "<script>";
    return "<anonymous>";
}

/* return the frame source; this is necessary for trace stack */
const char* vm_frame_source(VM* vm, int frame_index)
{
    CallFrame* frame = &vm->frames[frame_index];
    if (frame->chunk->source != NULL)
        return frame->chunk->source;
    if (frame_index == 0 && vm->name != NULL)
        return vm->name;
    return vm->name != NULL ? vm->name : "<unknown>";
}

/* clear the error stack; this will be necessary when implemend try-catch and catch the error */
void vm_runtime_clear(VM* vm)
{
    vm->error.has_error = false;
    vm->error.message[0] = '\0';
    vm->error.line = 0;
    vm->error.type = INTERNAL_ERROR;
}

/* print the runtime error with stack trace if the vm->frame_count > 0 */
void vm_runtime_report(VM* vm)
{
    if (!vm->error.has_error)
        return;

    const char* error_source = vm->name;
    if (vm->frame_count > 0)
        error_source = vm_frame_source(vm, vm->frame_count - 1);

    printf("\033[1;31m");
    printf("Runtime Error at %s:%d:", error_source, vm->error.line);
    printf("\033[0m");
    printf("\033[31m");
    printf(" %s\n", vm->error.message);
    printf("\033[0m");

    if (vm->frame_count == 0)
        return;

    printf("\033[3;36m");
    printf("Stack trace (most recent call last):\n");
    for (int frame_index = vm->frame_count - 1; frame_index >= 0; frame_index--)
    {
        CallFrame* frame = &vm->frames[frame_index];
        /* pendiente revisar el else (: frame->chunk->instructions) no es int*/
        int line = (frame_index == vm->frame_count - 1)
            ? vm->error.line
            : vm_line_at_pc(frame, frame->ip > frame->chunk->instructions
                ? frame->ip - 1
                : frame->chunk->instructions);

        printf("  at %s (%s):%d\n", vm_frame_name(vm, frame_index), vm_frame_source(vm, frame_index), line);

        if (frame_index > 0 && frame->call_line > 0)
            printf("    called from %s (%s):%d\n", vm_frame_name(vm, frame_index - 1), vm_frame_source(vm, frame_index - 1), frame->call_line);
    }
        printf("\033[0m");
}

/* create a new frame on the top of CallFrame */
bool vm_push_frame(VM* vm, Chunk* chunk, int call_line, const char* func_name, int result_base)
{
    if (vm->frame_count >= MAX_FRAMES)
    {
        vm_runtime_raise(vm, INTERNAL_ERROR, call_line > 0 ? call_line : VM_LINE(vm), "Call stack overflow: too many nested functions (max %d)", MAX_FRAMES);
        return false;
    }

    ptrdiff_t used = vm->stack_top - vm->stack;
    if (used + chunk->register_capacity > M_MAXSTACK)
    {
        vm_runtime_raise(vm, INTERNAL_ERROR, call_line > 0 ? call_line : VM_LINE(vm), "Stack overflow: not enough register slots (max %d)", M_MAXSTACK);
        return false;
    }

    CallFrame* frame = &vm->frames[vm->frame_count++];
    frame->chunk = chunk;
    frame->ip = chunk->instructions;
    frame->registers = vm->stack_top;
    frame->slot_count = chunk->register_capacity;
    frame->call_line = call_line;
    frame->func_name = func_name;
    frame->result_base = result_base;

    vm_init_frame_registers(frame->registers, frame->slot_count);
    vm->stack_top += frame->slot_count;
    return true;
}

/* close the most recent frame */
void vm_pop_frame(VM* vm)
{
    if (vm->frame_count == 0)
        return;
    CallFrame* frame = &vm->frames[vm->frame_count - 1];
    vm->stack_top = frame->registers;
    vm->frame_count--;
    // falta poner 0 a todo lo que se quitó
}

/* 
* caller is the function when call the new frame
* callee is the new frame
* arg_base is the register number when start to copy the register
* argc is the count of register to copy
*/
static void vm_copy_call_args(CallFrame* caller , CallFrame* callee, int arg_base, int argc)
{
    int param_slots = callee->chunk->parameter_count;
    if (param_slots < 0)
        param_slots = 0;
    
    int copy = argc;
    if (copy > param_slots)
        copy = param_slots;
    if (copy > callee->slot_count)
        copy = callee->slot_count;

    for (int i = 0; i < copy; i++)
    {
        int src = arg_base + i;
        if (src < 0 || src >= caller->slot_count)
            break;
        callee->registers[i] = caller->registers[src];
    }
}

/* this function starts a new M function */
bool vm_begin_call(VM* vm, Chunk* callee, int call_line, const char* func_name, int result_base, int arg_base, int argc)
{
    CallFrame* caller = NULL;
    if (vm->frame_count > 0)
        caller = &vm->frames[vm->frame_count - 1];

    if (!vm_push_frame(vm, callee, call_line, func_name, result_base))
        return false;

    if (caller != NULL && argc > 0)
        vm_copy_call_args(caller, &vm->frames[vm->frame_count - 1], arg_base, argc);

    return !vm->error.has_error;
}

/* this function calls when a M function finishes and push the returns values on last CallFrame register */
bool vm_finish_return(VM* vm, int return_reg_base, int returned_count)
{
    if (vm->frame_count == 0)
        return false;

    CallFrame* callee = &vm->frames[vm->frame_count - 1];
    int max_returns = callee->chunk->return_count;
    if (max_returns < 0)
        max_returns = 0;

    int copy_count = max_returns;
    if (returned_count >= 0)
        copy_count = returned_count;
    if (copy_count > max_returns)
        copy_count = max_returns;

    if (return_reg_base < 0)
        return_reg_base = 0;

    if (vm->frame_count > 1 && copy_count > 0)
    {
        CallFrame* caller = &vm->frames[vm->frame_count - 2];
        int dest = caller->result_base;

        if (dest < 0)
            dest = 0;

        int available = callee->slot_count - return_reg_base;
        if (copy_count > available)
            copy_count = available;

        for (int i = 0; i < copy_count; i++)
        {
            int dst = dest + i;
            if (dst < 0 || dst >= caller->slot_count)
                break;
            caller->registers[dst] = callee->registers[return_reg_base + i];
        }

        for (int i = copy_count; i < max_returns; i++)
        {
            int dst = dest + i;
            if (dst < 0 || dst >= caller->slot_count)
                break;
            setnan(caller->registers[dst]);
        }
    }

    vm_pop_frame(vm);
    return true;
}

typedef enum
{
    M_TYPE_INT,
    M_TYPE_FLOAT
} MNumType;

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

static bool can_coerce_to_number(Value v, MNumType* type, int64_t* iout, double* fout)
{
    if (isint(v))
    {
        *type = M_TYPE_INT;
        *iout = ivalue(v);
        return true;
    }
    else if (isfloat(v))
    {
        *type = M_TYPE_FLOAT;
        *fout = fvalue(v);
        return true;
    }
    else if (isstring(v))
    {
        return try_parse_string_number(svalue(v), slenvalue(v), type, iout, fout);
    }

    return false;
}

static bool value_to_string(VM* vm, Value v, char** out_chars, size_t* out_len)
{
    char buffer[128];

    if (isint(v))
    {
        size_t len = snprintf(buffer, sizeof(buffer), "%" PRId64, ivalue(v));
        if (len < 0) return false;

        char* result = malloc(len + 1);
        if (result == NULL)
        {
            memoryCrash("Converting int to string");
            exit(1);
        }

        memcpy(result, buffer, len);
        result[len] = '\0';

        *out_chars = result;
        *out_len = len;
        return true;
    }

    if (isfloat(v))
    {
        double n = fvalue(v);

        // Si quieres que 5.0 se vea como "5.0" y no como "5"
        size_t len;
        if (floor(n) == n)
            len = snprintf(buffer, sizeof(buffer), "%.1f", n);
        else
            len = snprintf(buffer, sizeof(buffer), "%.17g", n);

        if (len < 0) return false;

        char* result = malloc(len + 1);
        if (result == NULL)
        {
            memoryCrash("Converting float to string");
            exit(1);
        }

        memcpy(result, buffer, len);
        result[len] = '\0';

        *out_chars = result;
        *out_len = len;
        return true;
    }

    if (isboolean(v))
    {
        const char* text = bvalue(v) ? "true" : "false";
        size_t len = strlen(text);

        char* result = malloc(len + 1);
        if (result == NULL)
        {
            memoryCrash("Converting boolean to string");
            exit(1);
        }

        memcpy(result, text, len + 1);
        *out_chars = result;
        *out_len = len;
        return true;
    }

    if (isstring(v))
    {
        ObjString* s = (ObjString*)v.obj;
        char* result = malloc(s->length + 1);
        if (result == NULL)
        {
            memoryCrash("Converting string to string");
            exit(1);
        }

        memcpy(result, s->chars, s->length);
        result[s->length] = '\0';

        *out_chars = result;
        *out_len = s->length;
        return true;
    }

    // Aquí luego irá list/dict/otros objetos
    if (isobject(v))
    {
        GCObject* obj = v.obj;
        size_t len = snprintf(buffer, sizeof(buffer), "<object:%p>", (void*)obj);
        if (len < 0) return false;

        char* result = malloc(len + 1);
        if (result == NULL)
        {
            memoryCrash("Converting object to string");
            exit(1);
        }

        memcpy(result, buffer, len);
        result[len] = '\0';

        *out_chars = result;
        *out_len = len;
        return true;
    }

    return false;
}

/* 
* this function compare 2 strings caracter by caracter on ASCII format 
* return < 0 if left string is smaller than right string
* returns 0 if both strings are equals
* return > 0 if right string is smaller than left string
*/
static int m_strcmp(ObjString* leftS, ObjString* rightS)
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

VMStatus vm_run(VM* vm)
{
    while (vm->frame_count > 0 && !vm->error.has_error)
    {
        CallFrame* frame = &vm->frames[vm->frame_count - 1];
        Chunk* chunk = frame->chunk;
        Instruction* pc = frame->ip;
        Value* R = frame->registers;
        Constant* K = chunk->constants;
        Instruction* end = chunk->instructions + chunk->actual_instruction;

        while (pc < end && !vm->error.has_error)
        {
            int i = (int) (pc - chunk->instructions);
            const Instruction instr = *pc++;
            frame->ip = pc;

            const uint8_t op = GET_OPCODE(instr);

            /* 
            * caution macro VM_BREAK_IF_ERROR() actually execute after mark and exit the error 
            * if you add new errors considery the flow breaks after the error and don't continue
            * like happend with div, idiv, mod that they check if R[c] is not 0, then it immediately calls 
            * VM_BREAK_IF_ERROR() because it needs to cut off the flow there, otherwise it would 
            * continue and crash.
            */
            switch (op)
            {
                case OP_LOADK:
                {
                    uint8_t a = GET_A(instr);
                    uint16_t bx = GET_Bx(instr);
                
                    Constant kbx = K[bx];
                    if (kbx.type == C_INT)
                    {
                        setint(R[a], kbx.i);
                    }
                    else if (kbx.type == C_FLOAT)
                    {
                        setfloat(R[a], kbx.f);
                    }
                    else if (kbx.type == C_BOOLEAN)
                    {
                        setboolean(R[a], kbx.b);
                    }
                    else if (kbx.type == C_NIL)
                    {
                        setnil(R[a]);
                    }
                    else if (kbx.type == C_NAN)
                    {
                        setnan(R[a]);
                    }
                    else if (kbx.type == C_STRING)
                    {
                        R[a].type = VAL_OBJ;
                        R[a].obj = allocate_string(vm, kbx.string.chars, kbx.string.length);
                    }
                
                    break;
                }

                case OP_MOVE:
                {
                    uint8_t a = GET_A(instr);
                    uint16_t bx = GET_Bx(instr);

                    R[a] = R[bx];
                    break;
                }

                case OP_ADD:
                {
                    uint8_t a = GET_A(instr);
                    uint8_t b = GET_B(instr);
                    uint8_t c = GET_C(instr);

                    // fast-path
                    if (isint(R[b]) && isint(R[c]))
                    {
                        setint(R[a], ivalue(R[b]) + ivalue(R[c]));
                    }
                    else if (isfloat(R[b]) && isfloat(R[c]))
                    {
                        setfloat(R[a], fvalue(R[b]) + fvalue(R[c]));
                    }
                    else if (islist(R[b]))
                    {
                        set_list(&R[a], copy_list(vm, R[b].obj, chunk->lines[i]));
                        set_list_element(vm, &R[a], R[c], -2, chunk->lines[i]);
                    }
                    else
                    {
                        // slow-path
                        double num_fb, num_fc;
                        int64_t num_ib, num_ic;
                        MNumType type_b, type_c;
                        if (can_coerce_to_number(R[b], &type_b, &num_ib, &num_fb) && can_coerce_to_number(R[c], &type_c, &num_ic, &num_fc))
                        {
                            if (type_b == M_TYPE_INT && type_c == M_TYPE_INT)
                            {
                                setint(R[a], num_ib + num_ic);
                            }
                            else if (type_b == M_TYPE_INT && type_c == M_TYPE_FLOAT)
                            {
                                setfloat(R[a], num_ib + num_fc);
                            }
                            else if (type_b == M_TYPE_FLOAT && type_c == M_TYPE_INT)
                            {
                                setfloat(R[a], num_fb + num_ic);
                            }
                            else if (type_b == M_TYPE_FLOAT && type_c == M_TYPE_FLOAT)
                            {
                                setfloat(R[a], num_fb + num_fc);
                            }
                        }
                        else
                        {
                            char* type1 = getValueTypeName(R[b]);
                            char* type2 = getValueTypeName(R[c]);
                            //vm->has_error = true;
                            invalidOperandsError(vm, chunk->lines[i], "+", type1, type2);
                        }
                    }
                    VM_BREAK_IF_ERROR(vm);
                    print_rvalue(R[a], true);

                    break;
                }

                case OP_SUB:
                {
                    uint8_t a = GET_A(instr);
                    uint8_t b = GET_B(instr);
                    uint8_t c = GET_C(instr);

                    // fast-path
                    if (isint(R[b]) && isint(R[c]))
                    {
                        setint(R[a], ivalue(R[b]) - ivalue(R[c]));
                    }
                    else if (isfloat(R[b]) && isfloat(R[c]))
                    {
                        setfloat(R[a], fvalue(R[b]) - fvalue(R[c]));
                    }
                    else
                    {
                        // slow-path
                        double num_fb, num_fc;
                        int64_t num_ib, num_ic;
                        MNumType type_b, type_c;
                        if (can_coerce_to_number(R[b], &type_b, &num_ib, &num_fb) && can_coerce_to_number(R[c], &type_c, &num_ic, &num_fc))
                        {
                            if (type_b == M_TYPE_INT && type_c == M_TYPE_INT)
                            {
                                setint(R[a], num_ib - num_ic);
                            }
                            else if (type_b == M_TYPE_INT && type_c == M_TYPE_FLOAT)
                            {
                                setfloat(R[a], num_ib - num_fc);
                            }
                            else if (type_b == M_TYPE_FLOAT && type_c == M_TYPE_INT)
                            {
                                setfloat(R[a], num_fb - num_ic);
                            }
                            else if (type_b == M_TYPE_FLOAT && type_c == M_TYPE_FLOAT)
                            {
                                setfloat(R[a], num_fb - num_fc);
                            }
                        }
                        else
                        {
                            char* type1 = getValueTypeName(R[b]);
                            char* type2 = getValueTypeName(R[c]);
                            //vm->has_error = true;
                            invalidOperandsError(vm, chunk->lines[i], "-", type1, type2);
                        }
                    }
                    VM_BREAK_IF_ERROR(vm);
                    print_rvalue(R[a], true);

                    break;
                }

                case OP_MUL:
                {
                    uint8_t a = GET_A(instr);
                    uint8_t b = GET_B(instr);
                    uint8_t c = GET_C(instr);

                    // fast-path
                    if (isint(R[b]) && isint(R[c]))
                    {
                        setint(R[a], ivalue(R[b]) * ivalue(R[c]));
                    }
                    else if (isfloat(R[b]) && isfloat(R[c]))
                    {
                        setfloat(R[a], fvalue(R[b]) * fvalue(R[c]));
                    }
                    else if (islist(R[b]))
                    {
                        if (!isint(R[c]))
                            typeError(vm, chunk->lines[i], "int", getValueTypeName(R[b]));

                        int amountElements = listlenvalue(R[b]);
                        int times = ivalue(R[c]);

                        set_list(&R[a], copy_list(vm, R[b].obj, chunk->lines[i]));

                        for (int i = 0; i < times; i++)
                        {
                            for (int j = 1; j <= amountElements; j++)
                            {
                                set_list_element(vm, &R[a], listvalue(R[b], j), -2, chunk->lines[i]);
                            }
                        }
                    }
                    else
                    {
                        // slow-path
                        double num_fb, num_fc;
                        int64_t num_ib, num_ic;
                        MNumType type_b, type_c;
                        if (can_coerce_to_number(R[b], &type_b, &num_ib, &num_fb) && can_coerce_to_number(R[c], &type_c, &num_ic, &num_fc))
                        {
                            if (type_b == M_TYPE_INT && type_c == M_TYPE_INT)
                            {
                                setint(R[a], num_ib * num_ic);
                            }
                            else if (type_b == M_TYPE_INT && type_c == M_TYPE_FLOAT)
                            {
                                setfloat(R[a], num_ib * num_fc);
                            }
                            else if (type_b == M_TYPE_FLOAT && type_c == M_TYPE_INT)
                            {
                                setfloat(R[a], num_fb * num_ic);
                            }
                            else if (type_b == M_TYPE_FLOAT && type_c == M_TYPE_FLOAT)
                            {
                                setfloat(R[a], num_fb * num_fc);
                            }
                        }
                        else
                        {
                            char* type1 = getValueTypeName(R[b]);
                            char* type2 = getValueTypeName(R[c]);
                            //vm->has_error = true;
                            invalidOperandsError(vm, chunk->lines[i], "*", type1, type2);
                        }
                    }
                    VM_BREAK_IF_ERROR(vm);
                    print_rvalue(R[a], true);

                    break;
                }

                case OP_DIV:
                {
                    uint8_t a = GET_A(instr);
                    uint8_t b = GET_B(instr);
                    uint8_t c = GET_C(instr);

                    // fast-path
                    if ((isint(R[c]) && ivalue(R[c]) == 0) || (isfloat(R[c]) && fvalue(R[c]) == 0.0))
                    {
                        arithmeticError(vm, chunk->lines[i]);
                        VM_BREAK_IF_ERROR(vm);
                    }
                    if (isint(R[b]) && isint(R[c]))
                    {
                        setfloat(R[a], cast_double(ivalue(R[b])) / cast_double(ivalue(R[c])));
                    }
                    else if (isfloat(R[b]) && isfloat(R[c]))
                    {
                        setfloat(R[a], fvalue(R[b]) / fvalue(R[c]));
                    }
                    else
                    {
                        // slow-path
                        double num_fb, num_fc;
                        int64_t num_ib, num_ic;
                        MNumType type_b, type_c;
                        if (can_coerce_to_number(R[b], &type_b, &num_ib, &num_fb) && can_coerce_to_number(R[c], &type_c, &num_ic, &num_fc))
                        {
                            if ((type_c == M_TYPE_INT && num_ic == 0) || (type_c == M_TYPE_FLOAT && num_fc == 0.0))
                            {
                                arithmeticError(vm, chunk->lines[i]);
                                VM_BREAK_IF_ERROR(vm);
                            }
                            if (type_b == M_TYPE_INT && type_c == M_TYPE_INT)
                            {
                                setfloat(R[a], cast_double(num_ib) / cast_double(num_ic));
                            }
                            else if (type_b == M_TYPE_INT && type_c == M_TYPE_FLOAT)
                            {
                                setfloat(R[a], cast_double(num_ib) / num_fc);
                            }
                            else if (type_b == M_TYPE_FLOAT && type_c == M_TYPE_INT)
                            {
                                setfloat(R[a], num_fb / cast_double(num_ic));
                            }
                            else if (type_b == M_TYPE_FLOAT && type_c == M_TYPE_FLOAT)
                            {
                                setfloat(R[a], num_fb / num_fc);
                            }
                        }
                        else
                        {
                            char* type1 = getValueTypeName(R[b]);
                            char* type2 = getValueTypeName(R[c]);
                            //vm->has_error = true;
                            invalidOperandsError(vm, chunk->lines[i], "/", type1, type2);
                        }
                    }
                    VM_BREAK_IF_ERROR(vm);
                    print_rvalue(R[a], true);

                    break;
                }

                case OP_IDIV:
                {
                    uint8_t a = GET_A(instr);
                    uint8_t b = GET_B(instr);
                    uint8_t c = GET_C(instr);

                    // fast-path
                    if ((isint(R[c]) && ivalue(R[c]) == 0) || (isfloat(R[c]) && fvalue(R[c]) == 0.0))
                    {
                        arithmeticError(vm, chunk->lines[i]);
                        VM_BREAK_IF_ERROR(vm);
                    }
                    if (isint(R[b]) && isint(R[c]))
                    {
                        setint(R[a], cast_int(floor(cast_double(ivalue(R[b])) / cast_double(ivalue(R[c])))));
                    }
                    else if (isfloat(R[b]) && isfloat(R[c]))
                    {
                        setfloat(R[a], floor(fvalue(R[b]) / fvalue(R[c])));
                    }
                    else
                    {
                        // slow-path
                        double num_fb, num_fc;
                        int64_t num_ib, num_ic;
                        MNumType type_b, type_c;
                        if (can_coerce_to_number(R[b], &type_b, &num_ib, &num_fb) && can_coerce_to_number(R[c], &type_c, &num_ic, &num_fc))
                        {
                            if ((type_c == M_TYPE_INT && num_ic == 0) || (type_c == M_TYPE_FLOAT && num_fc == 0.0))
                            {
                                arithmeticError(vm, chunk->lines[i]);
                                VM_BREAK_IF_ERROR(vm);
                            }
                            if (type_b == M_TYPE_INT && type_c == M_TYPE_INT)
                            {
                                setint(R[a], cast_int(floor(num_ib / num_ic)));
                            }
                            else if (type_b == M_TYPE_INT && type_c == M_TYPE_FLOAT)
                            {
                                setfloat(R[a], floor(cast_double(num_ib) / num_fc));
                            }
                            else if (type_b == M_TYPE_FLOAT && type_c == M_TYPE_INT)
                            {
                                setfloat(R[a], floor(num_fb / cast_double(num_ic)));
                            }
                            else if (type_b == M_TYPE_FLOAT && type_c == M_TYPE_FLOAT)
                            {
                                setfloat(R[a], floor(num_fb / num_fc));
                            }
                        }
                        else
                        {
                            char* type1 = getValueTypeName(R[b]);
                            char* type2 = getValueTypeName(R[c]);
                            //vm->has_error = true;
                            invalidOperandsError(vm, chunk->lines[i], "//", type1, type2);
                        }
                    }
                    VM_BREAK_IF_ERROR(vm);
                    print_rvalue(R[a], true);

                    break;
                }

                case OP_MOD:
                {
                    uint8_t a = GET_A(instr);
                    uint8_t b = GET_B(instr);
                    uint8_t c = GET_C(instr);

                    // fast-path
                    if ((isint(R[c]) && ivalue(R[c]) == 0) || (isfloat(R[c]) && fvalue(R[c]) == 0.0))
                    {
                        arithmeticError(vm, chunk->lines[i]);
                        VM_BREAK_IF_ERROR(vm);
                    }
                    if (isint(R[b]) && isint(R[c]))
                    {
                        setint(R[a], cast_int(fmod(cast_double(ivalue(R[b])), cast_double(ivalue(R[c])))));
                    }
                    else if (isfloat(R[b]) && isfloat(R[c]))
                    {
                        setfloat(R[a], fmod(fvalue(R[b]), fvalue(R[c])));
                    }
                    else
                    {
                        // slow-path
                        double num_fb, num_fc;
                        int64_t num_ib, num_ic;
                        MNumType type_b, type_c;
                        if (can_coerce_to_number(R[b], &type_b, &num_ib, &num_fb) && can_coerce_to_number(R[c], &type_c, &num_ic, &num_fc))
                        {
                            if ((type_c == M_TYPE_INT && num_ic == 0) || (type_c == M_TYPE_FLOAT && num_fc == 0.0))
                            {
                                arithmeticError(vm, chunk->lines[i]);
                                VM_BREAK_IF_ERROR(vm);
                            }
                            if (type_b == M_TYPE_INT && type_c == M_TYPE_INT)
                            {
                                setint(R[a], cast_int(fmod(cast_double(num_ib) , cast_double(num_ic))));
                            }
                            else if (type_b == M_TYPE_INT && type_c == M_TYPE_FLOAT)
                            {
                                setfloat(R[a], fmod(cast_double(num_ib), num_fc));
                            }
                            else if (type_b == M_TYPE_FLOAT && type_c == M_TYPE_INT)
                            {
                                setfloat(R[a], fmod(num_fb, cast_double(num_ic)));
                            }
                            else if (type_b == M_TYPE_FLOAT && type_c == M_TYPE_FLOAT)
                            {
                                setfloat(R[a], fmod(num_fb, num_fc));
                            }
                        }

                        else
                        {
                            char* type1 = getValueTypeName(R[b]);
                            char* type2 = getValueTypeName(R[c]);
                            //vm->has_error = true;
                            invalidOperandsError(vm, chunk->lines[i], "%", type1, type2);
                        }
                    }
                    VM_BREAK_IF_ERROR(vm);
                    print_rvalue(R[a], true);

                    break;
                }

                case OP_POW:
                {
                    uint8_t a = GET_A(instr);
                    uint8_t b = GET_B(instr);
                    uint8_t c = GET_C(instr);

                    // fast-path
                    if (isint(R[b]) && isint(R[c]))
                    {
                        int valb = ivalue(R[b]);
                        int valc = ivalue(R[c]);
                        // fast-paths because pow() is too slowly
                        if (valc == 2)
                        {
                            setint(R[a], valb * valb);
                        }
                        else if (valc == 3)
                        {
                            setint(R[a], valb * valb * valb);
                        }
                        else if (valc == 0)
                        {
                            setint(R[a], 1);
                        }
                        else if (valc == 1)
                        {
                            setint(R[a], valb);
                        }
                        else
                        {
                            // slow-path
                            setint(R[a], cast_int(pow(valb, valc)));
                        }
                    }
                    else if (isfloat(R[b]) && isfloat(R[c]))
                    {
                        double valb = fvalue(R[b]);
                        double valc = fvalue(R[c]);
                        // fast-paths because pow() is too slowly
                        if (valc == 2.0)
                        {
                            setfloat(R[a], valb * valb);
                        }
                        else if (valc == 0.5)
                        {
                            setfloat(R[a], sqrt(valb));
                        }
                        else if (valc == 3.0)
                        {
                            setfloat(R[a], valb * valb * valb);
                        }
                        else if (valc == 0.0)
                        {
                            setfloat(R[a], 1.0);
                        }
                        else if (valc == 1.0)
                        {
                            setfloat(R[a], valb);
                        }
                        else
                        {
                            // slow-path
                            setfloat(R[a], pow(valb, valc));
                        }
                    }
                    else
                    {
                        // slow-path
                        double num_fb, num_fc;
                        int64_t num_ib, num_ic;
                        MNumType type_b, type_c;
                        if (can_coerce_to_number(R[b], &type_b, &num_ib, &num_fb) && can_coerce_to_number(R[c], &type_c, &num_ic, &num_fc))
                        {
                            if (type_b == M_TYPE_INT && type_c == M_TYPE_INT)
                            {
                                setint(R[a], cast_int(pow(num_ib, num_ic)));
                            }
                            else if (type_b == M_TYPE_INT && type_c == M_TYPE_FLOAT)
                            {
                                setfloat(R[a], pow(num_ib, num_fc));
                            }
                            else if (type_b == M_TYPE_FLOAT && type_c == M_TYPE_INT)
                            {
                                setfloat(R[a], pow(num_fb, num_ic));
                            }
                            else if (type_b == M_TYPE_FLOAT && type_c == M_TYPE_FLOAT)
                            {
                                setfloat(R[a], pow(num_fb, num_fc));
                            }
                        }
                        else
                        {
                            char* type1 = getValueTypeName(R[b]);
                            char* type2 = getValueTypeName(R[c]);
                            //vm->has_error = true;
                            invalidOperandsError(vm, chunk->lines[i], "^", type1, type2);
                        }
                    }
                    VM_BREAK_IF_ERROR(vm);
                    print_rvalue(R[a], true);

                    break;
                }

                case OP_CONCAT:
                {
                    uint8_t a = GET_A(instr);
                    uint8_t b = GET_B(instr);
                    uint8_t c = GET_C(instr);

                    char* sb = NULL;
                    char* sc = NULL;
                    size_t lb = 0, lc = 0;

                    // fast-path
                    //if (isstring(R[b]) && isstring(R[c]))
                    //{
                    //    int newLength = slenvalue(R[b]) + slenvalue(R[c]);

                    //    char* result = malloc(newLength + 1);
                    //    if (result == NULL)
                    //    {
                    //        memoryCrash("VM");
                    //        exit(1);
                    //    }
                    //    
                    //    memcpy(result, svalue(R[b]), slenvalue(R[b]));
                    //    memcpy(result + slenvalue(R[b]), svalue(R[c]), slenvalue(R[c]));
                    //    result[newLength] = '\0';

                    //    //setstring(&R[a], result, newLength);
                    //    R[a].type = VAL_OBJ;
                    //    R[a].obj = allocate_string(vm, result, newLength);

                    //    free(result);

                    //    //printf("R[a] = '%.*s'\n", ((ObjString*)R[a].obj)->length, ((ObjString*)R[a].obj)->chars);

                    //}
                    if (islist(R[b]) && islist(R[c]))
                    {
                        int len = listlenvalue(R[b]) + listlenvalue(R[c]);
                        set_list(&R[a], allocate_list(vm, 0, len, false));

                        for (int j = 1; j <= listlenvalue(R[b]); j++)
                        {
                            set_list_element(vm, &R[a], listvalue(R[b], j), -2, chunk->lines[i]);
                        }

                        for (int j = 1; j <= listlenvalue(R[c]); j++)
                        {
                            set_list_element(vm, &R[a], listvalue(R[c], j), -2, chunk->lines[i]);
                        }

                    }
                    else if (value_to_string(vm, R[b], &sb, &lb) && value_to_string(vm, R[c], &sc, &lc))
                    {
                        if (islist(R[b]) || islist(R[c]))
                        {
                            if (!isstring(R[b]) && !isstring(R[c]))
                            {
                                if (sb != NULL) free(sb);
                                if (sc != NULL) free(sc);
                                char* type1 = getValueTypeName(R[b]);
                                char* type2 = getValueTypeName(R[c]);
                                invalidOperandsError(vm, chunk->lines[i], "<>", type1, type2);
                                VM_BREAK_IF_ERROR(vm);
                            }
                        }

                        size_t newLength = lb + lc;
                        char* result = malloc(newLength + 1);
                        if (result == NULL)
                        {
                            memoryCrash("Concat strings");
                            exit(1);
                        }

                        memcpy(result, sb, lb);
                        memcpy(result + lb, sc, lc);
                        result[newLength] = '\0';

                        R[a].type = VAL_OBJ;
                        R[a].obj = allocate_string(vm, result, newLength);

                        free(result);
                        free(sb);
                        free(sc);
                    }
                    else
                    {
                        if (sb != NULL) free(sb);
                        if (sc != NULL) free(sc);
                        char* type1 = getValueTypeName(R[b]);
                        char* type2 = getValueTypeName(R[c]);
                        //vm->has_error = true;
                        invalidOperandsError(vm, chunk->lines[i], "<>", type1, type2);
                    }
                    VM_BREAK_IF_ERROR(vm);
                    print_rvalue(R[a], true);

                    break;
                }

                case OP_UNM:
                {
                    uint8_t a = GET_A(instr);
                    uint8_t bx = GET_Bx(instr);

                    // fast-path
                    if (isint(R[bx]))
                    {
                        setint(R[a], -ivalue(R[bx]));
                    }
                    else if (isfloat(R[bx]))
                    {
                        setfloat(R[a], -fvalue(R[bx]));
                    }
                    else
                    {
                        // slow-path
                        double num_fbx;
                        int64_t num_ibx;
                        MNumType type_bx;
                        if (can_coerce_to_number(R[bx], &type_bx, &num_ibx, &num_fbx))
                        {
                            if (type_bx == M_TYPE_INT)
                            {
                                setint(R[a], -num_ibx);
                            }
                            else
                            {
                                setfloat(R[a], -num_fbx);
                            }
                        }
                        else
                        {
                            char* type1 = getValueTypeName(R[bx]);
                            //vm->has_error = true;
                            invalidOperandsError(vm, chunk->lines[i], "-", type1, NULL);
                        }
                    }
                    VM_BREAK_IF_ERROR(vm);
                    print_rvalue(R[a], true);

                    break;
                }

                case OP_NOT:
                {
                    uint8_t a = GET_A(instr);
                    uint16_t bx = GET_Bx(instr);

                    Value val = R[bx];
                    setboolean(R[a], isfalse(val));

                    break;
                }

                case OP_OR:
                {
                    uint8_t a = GET_A(instr);
                    uint8_t b = GET_B(instr);
                    uint8_t c = GET_C(instr);
                
                    if (isfalse(R[b]))
                    {
                        R[a] = R[c];
                    }
                    else
                    {
                        R[a] = R[b];
                    }

                    break;
                }

                case OP_AND:
                {
                    uint8_t a = GET_A(instr);
                    uint8_t b = GET_B(instr);
                    uint8_t c = GET_C(instr);

                    if (isfalse(R[b]))
                    {
                        R[a] = R[b];
                    }
                    else
                    {
                        R[a] = R[c];
                    }

                    break;
                }

                case OP_CREATE_LIST:
                {
                    uint8_t a = GET_A(instr);
                    int8_t b = GET_B(instr);
                    uint8_t c = GET_C(instr);

                    if (b > -1 && !isint(R[b]))
                    {
                        typeError(vm, chunk->lines[i], "int", getValueTypeName(R[b]));
                        VM_BREAK_IF_ERROR(vm);
                    }

                    ObjList* list = allocate_list(vm, 0, (b > -1) ? ivalue(R[b]) : 0, c);
                    set_list(&R[a], list);
                    break;
                }

                case OP_SET_INDEX:
                {
                    uint8_t a = GET_A(instr);
                    uint8_t b = GET_B(instr);
                    uint8_t c = GET_C(instr);

                    if (islist(R[a]))
                    {
                        if (!isint(R[b]))
                        {
                            indexError(vm, chunk->lines[i], "int", getValueTypeName(R[b]));
                            VM_BREAK_IF_ERROR(vm);
                        }

                        if (ivalue(R[b]) < -1 || ivalue(R[b]) == 0)
                        {
                            indexoutofbound(vm, chunk->lines[i], ivalue(R[b]), ((ObjList*)(R[a].obj))->length);
                            VM_BREAK_IF_ERROR(vm);
                        }

                        set_list_element(vm, &R[a], R[c], R[b].i, chunk->lines[i]);
                    }
                    else if (isdict(R[a]))
                    {
                        set_dict_key_value(vm,(ObjDict*) R[a].obj, R[b], R[c], chunk->lines[i]);
                    }
                    else
                    {
                        cannotAddElementNotList(vm, chunk->lines[i], getValueTypeName(R[a]));
                    }
                    VM_BREAK_IF_ERROR(vm);
                    print_rvalue(R[a], true);

                    break;
                }

                case OP_PUSH_LIST:
                {
                    uint8_t a = GET_A(instr);
                    uint8_t bx = GET_Bx(instr);

                    if (islist(R[a]))
                    {
                        set_list_element(vm, &R[a], R[bx], -2, chunk->lines[i]);
                    }
                    else
                    {
                        cannotAddElementNotList(vm, chunk->lines[i], getValueTypeName(R[a]));
                    }
                    VM_BREAK_IF_ERROR(vm);
                    print_rvalue(R[a], true);

                    break;
                }

                case OP_GET_INDEX:
                {
                    uint8_t a = GET_A(instr);
                    uint8_t b = GET_B(instr);
                    uint8_t c = GET_C(instr);

                    if (islist(R[b]))
                    {
                        if (!isint(R[c]))
                        {
                            indexError(vm, chunk->lines[i], "int", getValueTypeName(R[c]));
                            VM_BREAK_IF_ERROR(vm);
                        }

                        R[a] = listvalue(R[b], R[c].i);
                    }
                    else if (isdict(R[b]))
                    {
                        get_dict_value(vm, R[b].obj, R[c], chunk->lines[i]);
                    }
                    else
                    {
                        attempedToIndexNoCollection(vm, chunk->lines[i], getValueTypeName(R[b]));
                    }
                    VM_BREAK_IF_ERROR(vm);
                    print_rvalue(R[a], true);

                    break;
                }

                case OP_CREATE_DICT:
                {
                    uint8_t a = GET_A(instr);
                    uint8_t b = GET_B(instr);
                    uint8_t c = GET_C(instr);

                    R[a].type = VAL_OBJ;
                    R[a].obj = allocate_dict(vm, b, c);

                    break;
                }

                /* 
                * don't use VM_BREAK_IF_ERROR() here because, when happend the error there's no
                * any to break, so we only use the normal break
                */
                case OP_EQ:
                {
                    uint8_t a = GET_A(instr);
                    uint8_t b = GET_B(instr);
                    uint8_t c = GET_C(instr);

                    if (ttype(R[b]) == ttype(R[c]))
                    {
                        if (isboolean(R[b]))
                        {
                            setboolean(R[a], bvalue(R[b]) == bvalue(R[c]));
                        }
                        else if (isfloat(R[b]))
                        {
                            setboolean(R[a], fvalue(R[b]) == fvalue(R[c]));
                        }
                        else if (isint(R[b]))
                        {
                            setboolean(R[a], ivalue(R[b]) == ivalue(R[c]));
                        }
                        else if (isstring(R[b]))
                        {
                            setboolean(R[a], R[b].obj == R[c].obj);

                        }
                        else if (isnil(R[b]))
                        {
                            setboolean(R[a], true);
                        }
                        else if (ismnan(R[b]))
                        {
                            setboolean(R[a], true);
                        }
                        else
                        {
                            unknownType(vm, chunk->lines[i], ttype(R[b]));
                        }
                    }
                    else
                    {
                        setboolean(R[a], false);
                    }
                    break;
                }

                case OP_NEQ:
                {
                    uint8_t a = GET_A(instr);
                    uint8_t b = GET_B(instr);
                    uint8_t c = GET_C(instr);

                    if (ttype(R[b]) == ttype(R[c]))
                    {
                        if (isboolean(R[b]))
                        {
                            setboolean(R[a], bvalue(R[b]) != bvalue(R[c]));
                        }
                        else if (isfloat(R[b]))
                        {
                            setboolean(R[a], fvalue(R[b]) != fvalue(R[c]));
                        }
                        else if (isint(R[b]))
                        {
                            setboolean(R[a], ivalue(R[b]) != ivalue(R[c]));
                        }
                        else if (isstring(R[b]))
                        {
                            setboolean(R[a], R[b].obj != R[c].obj);
                        }
                        else if (isnil(R[b]))
                        {
                            setboolean(R[a], false);
                        }
                        else if (ismnan(R[b]))
                        {
                            setboolean(R[a], false);
                        }
                        else
                        {
                            unknownType(vm, chunk->lines[i], ttype(R[b]));
                        }
                    }
                    else
                    {
                        setboolean(R[a], true);
                    }
                    break;
                }

                case OP_LT:
                {
                    uint8_t a = GET_A(instr);
                    uint8_t b = GET_B(instr);
                    uint8_t c = GET_C(instr);

                    if (ttype(R[b]) == ttype(R[c]))
                    {
                        if (isboolean(R[b]))
                        {
                            invalidOperandsError(vm, chunk->lines[i], "<", "boolean", "boolean");
                        }
                        else if (isfloat(R[b]))
                        {
                            setboolean(R[a], fvalue(R[b]) < fvalue(R[c]));
                        }
                        else if (isint(R[b]))
                        {
                            setboolean(R[a], ivalue(R[b]) < ivalue(R[c]));
                        }
                        else if (isstring(R[b]))
                        {
                            setboolean(R[a], (m_strcmp(R[b].obj, R[c].obj) < 0));
                            print_rvalue(R[a], true);
                            //setboolean(R[a], slenvalue(R[b]) < slenvalue(R[c]));

                        }
                        else if (isnil(R[b]))
                            invalidOperandsError(vm, chunk->lines[i], "<", "nil", "nil");
                        else if (ismnan(R[b]))
                            invalidOperandsError(vm, chunk->lines[i], "<", "NaN", "NaN");
                        else
                            unknownType(vm, chunk->lines[i], ttype(R[b]));
                    }
                    else
                    {
                        const char* type1 = getValueTypeName(R[b]);
                        const char* type2 = getValueTypeName(R[c]);
                        invalidOperandsError(vm, chunk->lines[i], "<", type1, type2);
                    }
                    break;
                }

                case OP_LTE:
                {
                    uint8_t a = GET_A(instr);
                    uint8_t b = GET_B(instr);
                    uint8_t c = GET_C(instr);

                    if (ttype(R[b]) == ttype(R[c]))
                    {
                        if (isboolean(R[b]))
                            invalidOperandsError(vm, chunk->lines[i], "<=", "boolean", "boolean");
                        else if (isfloat(R[b]))
                        {
                            setboolean(R[a], fvalue(R[b]) <= fvalue(R[c]));
                        }
                        else if (isint(R[b]))
                        {
                            setboolean(R[a], ivalue(R[b]) <= ivalue(R[c]));
                        }
                        else if (isstring(R[b]))
                        {
                            setboolean(R[a], (m_strcmp(R[b].obj, R[c].obj) <= 0));
                            print_rvalue(R[a], true);
                            //setboolean(R[a], slenvalue(R[b]) <= slenvalue(R[c]));
                        }
                        else if (isnil(R[b]))
                            invalidOperandsError(vm, chunk->lines[i], "<=", "nil", "nil");
                        else if (ismnan(R[b]))
                            invalidOperandsError(vm, chunk->lines[i], "<=", "NaN", "NaN");
                        else
                            unknownType(vm, chunk->lines[i], ttype(R[b]));
                    }
                    else
                    {
                        const char* type1 = getValueTypeName(R[b]);
                        const char* type2 = getValueTypeName(R[c]);
                        invalidOperandsError(vm, chunk->lines[i], "<=", type1, type2);
                    }

                    break;
                }

                case OP_GT:
                {
                    uint8_t a = GET_A(instr);
                    uint8_t b = GET_B(instr);
                    uint8_t c = GET_C(instr);

                    if (ttype(R[b]) == ttype(R[c]))
                    {
                        if (isboolean(R[b]))
                            invalidOperandsError(vm, chunk->lines[i], ">", "boolean", "boolean");
                        else if (isfloat(R[b]))
                        {
                            setboolean(R[a], fvalue(R[b]) > fvalue(R[c]));
                        }
                        else if (isint(R[b]))
                        {
                            setboolean(R[a], ivalue(R[b]) > ivalue(R[c]));
                        }
                        else if (isstring(R[b]))
                        {
                            setboolean(R[a], (m_strcmp(R[b].obj, R[c].obj) > 0));
                            print_rvalue(R[a], true);
                            //setboolean(R[a], slenvalue(R[b]) > slenvalue(R[c]));
                        }
                        else if (isnil(R[b]))
                            invalidOperandsError(vm, chunk->lines[i], ">", "nil", "nil");
                        else if (ismnan(R[b]))
                            invalidOperandsError(vm, chunk->lines[i], ">", "NaN", "NaN");
                        else
                            unknownType(vm, chunk->lines[i], ttype(R[b]));
                    }
                    else
                    {
                        const char* type1 = getValueTypeName(R[b]);
                        const char* type2 = getValueTypeName(R[c]);
                        invalidOperandsError(vm, chunk->lines[i], ">", type1, type2);
                    }

                    break;
                }

                case OP_GTE:
                {
                    uint8_t a = GET_A(instr);
                    uint8_t b = GET_B(instr);
                    uint8_t c = GET_C(instr);

                    if (ttype(R[b]) == ttype(R[c]))
                    {
                        if (isboolean(R[b]))
                            invalidOperandsError(vm, chunk->lines[i], ">=", "boolean", "boolean");
                        else if (isfloat(R[b]))
                        {
                            setboolean(R[a], fvalue(R[b]) >= fvalue(R[c]));
                        }
                        else if (isint(R[b]))
                        {
                            setboolean(R[a], ivalue(R[b]) >= ivalue(R[c]));
                        }
                        else if (isstring(R[b]))
                        {
                            setboolean(R[a], (m_strcmp(R[b].obj, R[c].obj) >= 0));
                            print_rvalue(R[a], true);
                            //setboolean(R[a], slenvalue(R[b]) >= slenvalue(R[c]));
                        }
                        else if (isnil(R[b]))
                            invalidOperandsError(vm, chunk->lines[i], ">=", "nil", "nil");
                        else if (ismnan(R[b]))
                            invalidOperandsError(vm, chunk->lines[i], ">=", "NaN", "NaN");
                        else
                            unknownType(vm, chunk->lines[i], ttype(R[b]));
                    }
                    else
                    {
                        const char* type1 = getValueTypeName(R[b]);
                        const char* type2 = getValueTypeName(R[c]);
                        invalidOperandsError(vm, chunk->lines[i], ">=", type1, type2);
                    }

                    break;
                }

                case OP_JUMP:
                {
                    int16_t bx = GET_sBx(instr);
                    pc += bx;
                    i += bx;

                    break;
                }

                case OP_JUMP_IF_FALSE:
                {
                    uint8_t a = GET_A(instr);
                    uint16_t bx = GET_Bx(instr);
                    if (isfalse(R[a]))
                    {
                        pc += bx;
                        i += bx;
                    }

                    break;
                }

                case OP_HALT:
                {
                    return;
                }

                default:
                {
                    unknownType(vm, chunk->lines[i], op);
                    VM_BREAK_IF_ERROR(vm);
                }
            }
        }
        
        if (!vm->error.has_error && pc >= end)
            vm_pop_frame(vm);
    }

    return vm->error.has_error ? VM_RUNTIME_ERROR : VM_OK;
}

int vm_execute(Chunk* main_chunk, const char* name)
{
    setlocale(LC_NUMERIC, "C");
    // VM
    VM* vm = calloc(1, sizeof(VM));
    if (vm == NULL)
    {
        memoryCrash("VM");
        exit(1);
    }

    //memset(vm, 0, sizeof(VM));

    vm->stack_top = vm->stack;
    vm->name = name;
    vm_runtime_clear(vm);

    main_chunk->name = NULL;
    main_chunk->source = name;
    if (!vm_push_frame(vm, main_chunk, 0, name, 0))
    {
        vm_runtime_report(vm);
        free(vm);
        return 1;
    }

    // GC init
    vm->objects = NULL;
    vm->bytes_allocated = 0;
    vm->next_gc = 1024 * 15;
    vm->globals = NULL;
    vm->gcEnable = false;
    vm->strings.capacity = 0;
    vm->strings.count = 0;
    vm->strings.entries = NULL;

    VMStatus status = vm_run(vm);

    if (status == VM_RUNTIME_ERROR || vm->error.has_error)
    {
        vm_runtime_report(vm);
        free(vm);
        return 1;
    }

    free(vm);
    return 0;
}

Chunk* chunk_new()
{
    Chunk* chunk = malloc(sizeof(Chunk));

    if (chunk == NULL)
    {
        memoryCrash("Virtual Machine");
        exit(1);
    }

    chunk->constants_capacity = -1;
    chunk->actual_instruction = -1;
    chunk->constants = NULL;
    chunk->instruction_capacity = -1;
    chunk->instructions = NULL;
    chunk->lines = NULL;
    chunk->parameter_count = -1;
    chunk->register_capacity = 256;
    chunk->return_count = -1;
    chunk->name = NULL;
    chunk->source = NULL;

    return chunk;
}

void chunk_init(Chunk* chunk, ConstTable* c, IRList* ir)
{
    chunk->constants_capacity = c->count;
    chunk->constants = c->data;
    chunk->actual_instruction = 0;
    chunk->instruction_capacity = ir->count;
    chunk->parameter_count = 0;
    chunk->return_count = 0;
    chunk->source = NULL;

    Instruction* ins = malloc(sizeof(Instruction) * ir->count);

    if (ins == NULL)
    {
        memoryCrash("VM");
        exit(1);
    }

    chunk->instructions = ins;

    int* lines = malloc(sizeof(int) * ir->count);
    if (lines == NULL)
    {
        memoryCrash("VM");
        exit(1);
    }

    chunk->lines = lines;
}

void chunk_write(Chunk* chunk, Instruction instr, int line)
{
    chunk->instructions[chunk->actual_instruction] = instr;
    chunk->lines[chunk->actual_instruction] = line;
    chunk->actual_instruction++;
}

/*int main(void)
{
    
    int op = 1;
    int a = 10;
    int b = 56;
    int c = 150;
    int bx = 500;
    int sBx = -150;

    Instruction i = CREATE_ABC(op, a, b, c);
    Instruction i2 = CREATE_ABx(op, a, bx);
    Instruction i3 = CREATE_AsBx(op, a, sBx);
    /*printf("====================\n");
    printf("i = %" PRIu32 "\n", i);
    printf("op = %d\n", GET_OPCODE(i));
    printf("a = %d\n", GET_A(i));
    printf("b = %d\n", GET_B(i));
    printf("c = %d\n", GET_C(i));

    printf("====================\n");
    printf("i2 = %" PRIu32 "\n", i2);
    printf("op = %d\n", GET_OPCODE(i2));
    printf("a = %d\n", GET_A(i2));
    printf("bx = %d\n", GET_Bx(i2));

    printf("====================\n");
    printf("i3 = %" PRIu32 "\n", i3);
    printf("op = %d\n", GET_OPCODE(i3));
    printf("a = %d\n", GET_A(i3));
    printf("sBx = %d\n", GET_sBx(i3));*-/

    Chunk chunk = {.cc = 0, .ic = 0,.rc = 5};
    /*if (chunk == NULL)
        return 1; *-/
    //chunk.cc = 0;
    //chunk.ic = 0;
    chunk.instructions[chunk.ic] = i;
    //chunk.rc = 5;

    //vm_execute(&chunk);

    return 0;
}*/