
#include "../utils/err.h"
#include "../backend/codegen.h"
#include "vm.h"
#include "mdebug.h"
#include "mstack.h"

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

#define VM_REG(i)   (&R[i])
#define VM_CONSTANT(i)  (&K[i])
#define VM_UPVAL(i) (&closure->M.uprefs[i])
#define VM_SAVE_PC()    vm->frame->pc = pc
#define VM_PROTECT(func) \
    { \
        vm->frame->pc = pc; \
        { \
        func; \
        } \
        R = vm->base; \
    }

static void vm_run(VM* vm)
{
    M_ASSERT(vm != NULL);
    M_ASSERT(vm->frame_count > 0);
    Closure* closure;
    Chunk* chunk;
    const Instruction* pc;
    Value* R;
    Constant* K;
        
    //reentry:
        closure = closurevalue(vm->frame->closure);
        chunk = closure->M.chunk;
        pc = vm->frame->pc;
        R = vm->frame->registers;
        K = chunk->constants;
        dispatch:
        int i = (int)(pc - chunk->instructions);
        switch (GET_OPCODE(*pc))
        {
            case OP_LOADK:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                uint16_t bx = GET_Bx(instr);

                M_ASSERT(bx < chunk->constants_capacity);

                Constant* kbx = VM_CONSTANT(bx);
                if (kbx->type == C_INT)
                {
                    setint(ra, kbx->i);
                }
                else if (kbx->type == C_FLOAT)
                {
                    setfloat(ra, kbx->f);
                }
                else if (kbx->type == C_BOOLEAN)
                {
                    setboolean(ra, kbx->b);
                }
                else if (kbx->type == C_NIL)
                {
                    setnil(ra);
                }
                else if (kbx->type == C_NAN)
                {
                    setvoid(ra);
                }
                else if (kbx->type == C_STRING)
                {
                    ra->type = VAL_OBJ;
                    ra->obj = allocate_string(vm, kbx->string.chars, kbx->string.length);
                }
                else
                {
                    M_UNREACHABLE("LOADK");
                }

                goto dispatch;
            }

            case OP_MOVE:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_Bx(instr));

                //M_ASSERT(bx < 256);

                setobjr(ra, rb);
                goto dispatch;
            }

            case OP_ADD:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_B(instr));
                Value* rc = VM_REG(GET_C(instr));

                // fast-path
                if (isint(rb) && isint(rc))
                {
                    setint(ra, ivalue(rb) + ivalue(rc));
                }
                else if (isfloat(rb) && isfloat(rc))
                {
                    setfloat(ra, fvalue(rb) + fvalue(rc));
                }
                else if (islist(rb))
                {
                    M_ASSERT("Pendiente revisar si el depth del copy_list() es false o true");
                    set_list(&ra, copy_list(vm, rb->obj, false));
                    set_list_element(vm, ra, *rc, olistvalue(ra)->length);
                }
                else
                {
                    // slow-path
                    VM_SAVE_PC();
                    Value nb = { 0 }, nc = { 0 };
                    if (vmutils_tryToNumber(rb, &nb) && vmutils_tryToNumber(rc, &nc))
                    {
                        if (isint(&nb) && isint(&nc))
                        {
                            setint(ra, ivalue(&nb) + ivalue(&nc));
                        }
                        else if (isint(&nb) && isfloat(&nc))
                        {
                            setfloat(ra, ivalue(&nb) + fvalue(&nc));
                        }
                        else if (isfloat(&nb) && isint(&nc))
                        {
                            setfloat(ra, fvalue(&nb) + ivalue(&nc));
                        }
                        else if (isfloat(&nb) && isfloat(&nc))
                        {
                            setfloat(ra, fvalue(&nb) + fvalue(&nc));
                        }
                        else
                            M_UNREACHABLE();
                    }
                    else
                    {
                        const char* type1 = getValueTypeName(rb);
                        const char* type2 = getValueTypeName(rc);
                        m_error(vm, INVALID_OPERANDS_ERROR, "Invalid operands '%s' and '%s' for operator '+'", type1, type2);
                    }
                }
                //print_rvalue(ra, true);

                goto dispatch;
            }

            case OP_SUB:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_B(instr));
                Value* rc = VM_REG(GET_C(instr));

                // fast-path
                if (isint(rb) && isint(rc))
                {
                    setint(ra, ivalue(rb) - ivalue(rc));
                }
                else if (isfloat(rb) && isfloat(rc))
                {
                    setfloat(ra, fvalue(rb) - fvalue(rc));
                }
                else
                {
                    // slow-path
                    VM_SAVE_PC();
                    Value nb = { 0 }, nc = { 0 };
                    if (vmutils_tryToNumber(rb, &nb) && vmutils_tryToNumber(rc, &nc))
                    {
                        if (isint(&nb) && isint(&nc))
                        {
                            setint(ra, ivalue(&nb) - ivalue(&nc));
                        }
                        else if (isint(&nb) && isfloat(&nc))
                        {
                            setfloat(ra, ivalue(&nb) - fvalue(&nc));
                        }
                        else if (isfloat(&nb) && isint(&nc))
                        {
                            setfloat(ra, fvalue(&nb) - ivalue(&nc));
                        }
                        else if (isfloat(&nb) && isfloat(&nc))
                        {
                            setfloat(ra, fvalue(&nb) - fvalue(&nc));
                        }
                        else
                            M_UNREACHABLE();
                    }
                    else
                    {
                        const char* type1 = getValueTypeName(rb);
                        const char* type2 = getValueTypeName(rc);
                        //vm->has_error = true;
                        //invalidOperandsError(vm , "-", type1, type2);
                        m_error(vm, INVALID_OPERANDS_ERROR, "Invalid operands '%s' and '%s' for operator '-'", type1, type2);
                    }
                }
                //print_rvalue(ra, true);

                goto dispatch;
            }

            case OP_MUL:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_B(instr));
                Value* rc = VM_REG(GET_C(instr));

                // fast-path
                if (isint(rb) && isint(rc))
                {
                    setint(ra, ivalue(rb) * ivalue(rc));
                }
                else if (isfloat(rb) && isfloat(rc))
                {
                    setfloat(ra, fvalue(rb) * fvalue(rc));
                }
                else if (islist(rb))
                {
                    if (!isint(rc))
                        //typeError(vm , "int", getValueTypeName(rb));
                        m_error(vm, TYPES_ERROR , "Expected type 'int', but got '%s'", getValueTypeName(rb));

                    int amountElements = listlenvalue(rb);
                    int times = ivalue(rc);

                    set_list(ra, copy_list(vm, rb->obj, false));

                    M_ASSERT(!"Revisar el set_list_element() del for por la desreferenciación que se esta haciendo");
                    for (int i = 0; i < times; i++)
                    {
                        for (int j = 1; j <= amountElements; j++)
                        {
                            set_list_element(vm, ra, *(listvalue(rb, j)), olistvalue(ra)->length);
                        }
                    }
                }
                else
                {
                    // slow-path
                    VM_SAVE_PC();
                    Value nb = { 0 }, nc = { 0 };
                    if (vmutils_tryToNumber(rb, &nb) && vmutils_tryToNumber(rc, &nc))
                    {
                        if (isint(&nb) && isint(&nc))
                        {
                            setint(ra, ivalue(&nb) * ivalue(&nc));
                        }
                        else if (isint(&nb) && isfloat(&nc))
                        {
                            setfloat(ra, ivalue(&nb) * fvalue(&nc));
                        }
                        else if (isfloat(&nb) && isint(&nc))
                        {
                            setfloat(ra, fvalue(&nb) * ivalue(&nc));
                        }
                        else if (isfloat(&nb) && isfloat(&nc))
                        {
                            setfloat(ra, fvalue(&nb) * fvalue(&nc));
                        }
                        else
                            M_UNREACHABLE();
                    }
                    else
                    {
                        const char* type1 = getValueTypeName(rb);
                        const char* type2 = getValueTypeName(rc);
                        m_error(vm, INVALID_OPERANDS_ERROR, "Invalid operands '%s' and '%s' for operator '*'", type1, type2);
                    }
                }
                print_rvalue(ra, true);

                goto dispatch;
            }

            case OP_DIV:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_B(instr));
                Value* rc = VM_REG(GET_C(instr));

                // fast-path
                if ((isint(rc) && ivalue(rc) == 0) || (isfloat(rc) && fvalue(rc) == 0.0))
                {
                    VM_SAVE_PC();
                    m_error(vm, ARITHMETIC_ERROR , "Division by Zero");
                }

                if (isint(rb) && isint(rc))
                {
                    setfloat(ra, cast_double(ivalue(rb)) / cast_double(ivalue(rc)));
                }
                else if (isfloat(rb) && isfloat(rc))
                {
                    setfloat(ra, fvalue(rb) / fvalue(rc));
                }
                else
                {
                    // slow-path
                    VM_SAVE_PC();
                    Value nb = { 0 }, nc = { 0 };
                    if (vmutils_tryToNumber(rb, &nb) && vmutils_tryToNumber(rc, &nc))
                    {
                        if ((isint(&nc) && ivalue(&nc) == 0) || (isfloat(&nc) && fvalue(&nc) == 0.0))
                            m_error(vm, ARITHMETIC_ERROR , "Division by Zero");

                        if (isint(&nb) && isint(&nc))
                        {
                            setint(ra, ivalue(&nb) / ivalue(&nc));
                        }
                        else if (isint(&nb) && isfloat(&nc))
                        {
                            setfloat(ra, ivalue(&nb) / fvalue(&nc));
                        }
                        else if (isfloat(&nb) && isint(&nc))
                        {
                            setfloat(ra, fvalue(&nb) / ivalue(&nc));
                        }
                        else if (isfloat(&nb) && isfloat(&nc))
                        {
                            setfloat(ra, fvalue(&nb) / fvalue(&nc));
                        }
                        else
                            M_UNREACHABLE();
                    }
                    else
                    {
                        const char* type1 = getValueTypeName(rb);
                        const char* type2 = getValueTypeName(rc);
                        m_error(vm, INVALID_OPERANDS_ERROR, "Invalid operands '%s' and '%s' for operator '/'", type1, type2);
                    }
                }
                //print_rvalue(ra, true);

                goto dispatch;
            }

            case OP_IDIV:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_B(instr));
                Value* rc = VM_REG(GET_C(instr));

                // fast-path
                if ((isint(rc) && ivalue(rc) == 0) || (isfloat(rc) && fvalue(rc) == 0.0))
                {
                    VM_SAVE_PC();
                    m_error(vm, ARITHMETIC_ERROR , "Division by Zero");
                }

                if (isint(rb) && isint(rc))
                {
                    setint(ra, cast_int(floor(cast_double(ivalue(rb)) / cast_double(ivalue(rc)))));
                }
                else if (isfloat(rb) && isfloat(rc))
                {
                    setfloat(ra, floor(fvalue(rb) / fvalue(rc)));
                }
                else
                {
                    // slow-path
                    VM_SAVE_PC();
                    Value nb = { 0 }, nc = { 0 };
                    if (vmutils_tryToNumber(rb, &nb) && vmutils_tryToNumber(rc, &nc))
                    {
                        if ((isint(&nc) && ivalue(&nc) == 0) || (isfloat(&nc) && fvalue(&nc) == 0.0))
                            m_error(vm, ARITHMETIC_ERROR, "Division by Zero");

                        if (isint(&nb) && isint(&nc))
                        {
                            setint(ra, cast(int64_t, floor(ivalue(&nb) / ivalue(&nc))));
                        }
                        else if (isint(&nb) && isfloat(&nc))
                        {
                            setfloat(ra, floor(ivalue(&nb) / fvalue(&nc)));
                        }
                        else if (isfloat(&nb) && isint(&nc))
                        {
                            setfloat(ra, floor(fvalue(&nb) / ivalue(&nc)));
                        }
                        else if (isfloat(&nb) && isfloat(&nc))
                        {
                            setfloat(ra, floor(fvalue(&nb) / fvalue(&nc)));
                        }
                        else
                            M_UNREACHABLE();
                    }
                    else
                    {
                        const char* type1 = getValueTypeName(rb);
                        const char* type2 = getValueTypeName(rc);
                        m_error(vm, INVALID_OPERANDS_ERROR, "Invalid operands '%s' and '%s' for operator '//'", type1, type2);
                    }
                }
                //print_rvalue(ra, true);

                goto dispatch;
            }

            case OP_MOD:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_B(instr));
                Value* rc = VM_REG(GET_C(instr));

                // fast-path
                if ((isint(rc) && ivalue(rc) == 0) || (isfloat(rc) && fvalue(rc) == 0.0))
                {
                    VM_SAVE_PC();
                    m_error(vm, ARITHMETIC_ERROR , "Division by Zero");
                }

                if (isint(rb) && isint(rc))
                {
                    setint(ra, cast_int(fmod(cast_double(ivalue(rb)), cast_double(ivalue(rc)))));
                }
                else if (isfloat(rb) && isfloat(rc))
                {
                    setfloat(ra, fmod(fvalue(rb), fvalue(rc)));
                }
                else
                {
                    // slow-path
                    VM_SAVE_PC();
                    Value nb = { 0 }, nc = { 0 };
                    if (vmutils_tryToNumber(rb, &nb) && vmutils_tryToNumber(rc, &nc))
                    {
                        if ((isint(&nc) && ivalue(&nc) == 0) || (isfloat(&nc) && fvalue(&nc) == 0.0))
                            m_error(vm, ARITHMETIC_ERROR, "Division by Zero");

                        if (isint(&nb) && isint(&nc))
                        {
                            setint(ra, cast_int(fmod(cast_double(ivalue(&nb)), cast_double(ivalue(&nc)))));
                        }
                        else if (isint(&nb) && isfloat(&nc))
                        {
                            setfloat(ra, fmod(cast_double(ivalue(&nb)), fvalue(&nc)));
                        }
                        else if (isfloat(&nb) && isint(&nc))
                        {
                            setfloat(ra, fmod(fvalue(&nb), cast_double(ivalue(&nc))));
                        }
                        else if (isfloat(&nb) && isfloat(&nc))
                        {
                            setfloat(ra, fmod(fvalue(&nb), fvalue(&nc)));
                        }
                        else
                            M_UNREACHABLE();
                    }
                    else
                    {
                        const char* type1 = getValueTypeName(rb);
                        const char* type2 = getValueTypeName(rc);
                        m_error(vm, INVALID_OPERANDS_ERROR, "Invalid operands '%s' and '%s' for operator '%'", type1, type2);
                    }
                }
                //print_rvalue(ra, true);

                goto dispatch;
            }

            case OP_POW:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_B(instr));
                Value* rc = VM_REG(GET_C(instr));

                // fast-path
                if (isint(rb) && isint(rc))
                {
                    int valb = ivalue(rb);
                    int valc = ivalue(rc);
                    // fast-paths because pow() is too slowly
                    if (valc == 2)
                    {
                        setint(ra, valb * valb);
                    }
                    else if (valc == 3)
                    {
                        setint(ra, valb * valb * valb);
                    }
                    else if (valc == 0)
                    {
                        setint(ra, 1);
                    }
                    else if (valc == 1)
                    {
                        setint(ra, valb);
                    }
                    else
                    {
                        // slow-path
                        setint(ra, cast_int(pow(valb, valc)));
                    }
                }
                else if (isfloat(rb) && isfloat(rc))
                {
                    double valb = fvalue(rb);
                    double valc = fvalue(rc);
                    // fast-paths because pow() is too slowly
                    if (valc == 2.0)
                    {
                        setfloat(ra, valb * valb);
                    }
                    else if (valc == 0.5)
                    {
                        setfloat(ra, sqrt(valb));
                    }
                    else if (valc == 3.0)
                    {
                        setfloat(ra, valb * valb * valb);
                    }
                    else if (valc == 0.0)
                    {
                        setfloat(ra, 1.0);
                    }
                    else if (valc == 1.0)
                    {
                        setfloat(ra, valb);
                    }
                    else
                    {
                        // slow-path
                        setfloat(ra, pow(valb, valc));
                    }
                }
                else
                {
                    // slow-path
                    VM_SAVE_PC();
                    Value nb = { 0 }, nc = { 0 };
                    if (vmutils_tryToNumber(rb, &nb) && vmutils_tryToNumber(rc, &nc))
                    {
                        if (isint(&nb) && isint(&nc))
                        {
                            setint(ra, cast(int64_t, pow(cast_double(ivalue(&nb)), cast_double(ivalue(&nc)))));
                        }
                        else if (isint(&nb) && isfloat(&nc))
                        {
                            setfloat(ra, pow(cast_double(ivalue(&nb)), fvalue(&nc)));
                        }
                        else if (isfloat(&nb) && isint(&nc))
                        {
                            setfloat(ra, pow(fvalue(&nb), cast_double(fvalue(&nc))));
                        }
                        else if (isfloat(&nb) && isfloat(&nc))
                        {
                            setfloat(ra, pow(fvalue(&nb), fvalue(&nc)));
                        }
                        else
                            M_UNREACHABLE();
                    }
                    else
                    {
                        char* type1 = getValueTypeName(rb);
                        char* type2 = getValueTypeName(rc);
                        m_error(vm, INVALID_OPERANDS_ERROR , "Invalid operands '%s' and '%s' for operator '^'", type1, type2);
                    }
                }
                //print_rvalue(ra, true);

                goto dispatch;
            }

            case OP_CONCAT:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_B(instr));
                Value* rc = VM_REG(GET_C(instr));

                char* sb = NULL;
                char* sc = NULL;
                size_t lb = 0, lc = 0;
                
                VM_SAVE_PC();
                if (islist(rb) && islist(rc))
                {
                    int len = listlenvalue(rb) + listlenvalue(rc);
                    set_list(ra, allocate_list(vm, 0, len, false));

                    M_ASSERT(!"Revisar los set_list_element() del for por la desreferenciación que se esta haciendo");
                    for (int j = 1; j <= listlenvalue(rb); j++)
                    {
                        VM_PROTECT(set_list_element(vm, ra, *(listvalue(rb, j)), 0 ));
                    }

                    for (int j = 1; j <= listlenvalue(rc); j++)
                    {
                        VM_PROTECT(set_list_element(vm, ra, *(listvalue(rc, j)), 0 ));
                    }

                }
                else if (vmutils_tryToString(vm, rb) && vmutils_tryToString(vm, rc))
                //else if (vmutils_tryToString(vm, rb, &sb, &lb) && vmutils_tryToString(vm, rc, &sc, &lc))
                {
                    M_ASSERT(!"rb y rc no se estan copiando, se estan modificando directamente");
                    if (islist(rb) || islist(rc))
                    {
                        if (!isstring(rb) && !isstring(rc))
                        {
                            if (sb != NULL) free(sb);
                            if (sc != NULL) free(sc);
                            char* type1 = getValueTypeName(rb);
                            char* type2 = getValueTypeName(rc);
                            m_error(vm, INVALID_OPERANDS_ERROR , "Invalid operands '%s' and '%s' for operator '<>'", type1, type2);
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

                    ra->type = VAL_OBJ;
                    ra->obj = allocate_string(vm, result, newLength);

                    free(result);
                    free(sb);
                    free(sc);
                }
                else
                {
                    if (sb != NULL) free(sb);
                    if (sc != NULL) free(sc);
                    char* type1 = getValueTypeName(rb);
                    char* type2 = getValueTypeName(rc);
                    m_error(vm, INVALID_OPERANDS_ERROR , "Invalid operands '%s' and '%s' for operator '<>'", type1, type2);
                }
                //print_rvalue(ra, true);

                goto dispatch;
            }

            case OP_UNM:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_Bx(instr));

                // fast-path
                if (isint(rb))
                {
                    setint(ra, -ivalue(rb));
                }
                else if (isfloat(rb))
                {
                    setfloat(ra, -fvalue(rb));
                }
                else
                {
                    // slow-path
                    Value nb = { 0 };
                    if (vmutils_tryToNumber(rb, &nb))
                    {
                        if (isint(&nb))
                        {
                            setint(ra, -ivalue(&nb));
                        }
                        else
                        {
                            setfloat(ra, -fvalue(&nb));
                        }
                    }
                    else
                    {
                        char* type1 = getValueTypeName(rb);
                        m_error(vm, INVALID_OPERANDS_ERROR , "Invalid operand '%s' for operator unary '-'", type1);
                    }
                }
                //print_rvalue(ra, true);

                goto dispatch;
            }

            case OP_NOT:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_Bx(instr));

                setboolean(ra, isfalse(rb));

                goto dispatch;
            }

            case OP_OR:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_B(instr));
                Value* rc = VM_REG(GET_C(instr));

                if (isfalse(rb))
                {
                    setobjr(ra, rc);
                }
                else
                {
                    setobjr(ra, rb);
                }

                goto dispatch;
            }

            case OP_AND:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_B(instr));
                Value* rc = VM_REG(GET_C(instr));

                if (isfalse(rb))
                {
                    setobjr(ra, rb);
                }
                else
                {
                    setobjr(ra, rc);
                }

                goto dispatch;
            }

            case OP_CREATE_LIST:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_B(instr));
                uint8_t c = GET_C(instr);

                int8_t b = GET_B(instr);
                if (b > -1 && !isint(rb))
                {
                    VM_SAVE_PC();
                    m_error(vm, TYPES_ERROR , "Expected type 'int', but got type '%s'", getValueTypeName(rb));
                }

                ObjList* list = allocate_list(vm, 0, (b > -1) ? ivalue(rb) : 0, c);
                set_list(ra, list);
                goto dispatch;
            }

            case OP_SET_INDEX:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_B(instr));
                Value* rc = VM_REG(GET_C(instr));

                if (islist(ra))
                {
                    if (!isint(rb))
                    {
                        VM_SAVE_PC();
                        m_error(vm, INDEX_ERROR, "int");

                    }

                    VM_PROTECT(set_list_element(vm, ra, *rc, ivalue(rb)));
                }
                else if (isdict(ra))
                {
                    VM_PROTECT(set_dict_key_value(vm, (ObjDict*)ra->obj, rb, rc));
                }
                else
                {
                    VM_SAVE_PC();
                    m_error(vm, ASSIGNMENT_ERROR, getValueTypeName(ra));
                }
                //print_rvalue(ra, true);

                goto dispatch;
            }

            case OP_PUSH_LIST:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_B(instr));
                uint8_t c = GET_C(instr);

                if (islist(ra))
                {
                    for (int i = 0; i < c; i++)
                    {
                        VM_PROTECT(set_list_element(vm, ra, *(rb + i), 0 ));
                    }
                }
                else
                {
                    VM_SAVE_PC();
                    m_error(vm, ASSIGNMENT_ERROR , "Cannot assign an item to a non-list; Got type %s", getValueTypeName(ra));
                }
                //print_rvalue(ra, true);

                goto dispatch;
            }

            case OP_GET_INDEX:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_B(instr));
                Value* rc = VM_REG(GET_C(instr));

                if (islist(rb))
                {
                    if (!isint(rc))
                    {
                        VM_SAVE_PC();
                        m_error(vm, INDEX_ERROR , "Expected index type 'int', but got '%s'", getValueTypeName(rb));
                    }

                    M_ASSERT(!"Revisar '*ra = *(listvalue(rb, ivalue(rc)));'");
                    *ra = *(listvalue(rb, ivalue(rc)));
                }
                else if (isdict(rb))
                {
                    M_ASSERT(!"Prev NULL");
                    get_dict_value(vm, rb->obj, rc);
                }
                else
                {
                    VM_SAVE_PC();
                    m_error(vm, ATTEMPED_TO_INDEX_NO_COLLECTION_ERROR , "Attemped to index '%s' instead of collection", getValueTypeName(rb));
                }
                //print_rvalue(ra, true);

                goto dispatch;
            }

            case OP_CREATE_DICT:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                uint8_t b = GET_B(instr);
                uint8_t c = GET_C(instr);

                ra->type = VAL_OBJ;
                ra->obj = allocate_dict(vm, b, c);

                goto dispatch;
            }

            case OP_EQ:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_B(instr));
                Value* rc = VM_REG(GET_C(instr));

                VM_PROTECT(setboolean(ra, vmutils_valueEquals(vm, rb, rc)));
                
                goto dispatch;
            }

            case OP_NEQ:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_B(instr));
                Value* rc = VM_REG(GET_C(instr));

                VM_PROTECT(setboolean(ra, !vmutils_valueEquals(vm, rb, rc)));
                
                goto dispatch;
            }

            case OP_LT:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_B(instr));
                Value* rc = VM_REG(GET_C(instr));

                VM_PROTECT(setboolean(ra, vmutils_lessthan(vm, rb, rc)));
                
                goto dispatch;
            }

            case OP_LTE:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_B(instr));
                Value* rc = VM_REG(GET_C(instr));

                VM_PROTECT(setboolean(ra, vmutils_lessOrEquals(vm, rb, rc)));
                
                goto dispatch;
            }

            case OP_GT:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_B(instr));
                Value* rc = VM_REG(GET_C(instr));

                VM_PROTECT(setboolean(ra, !vmutils_lessthan(vm, rc, rb)));

                goto dispatch;
            }

            case OP_GTE:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                Value* rb = VM_REG(GET_B(instr));
                Value* rc = VM_REG(GET_C(instr));

                VM_PROTECT(setboolean(ra, !vmutils_lessOrEquals(vm, rc, rb)));

                goto dispatch;
            }

            case OP_JUMP:
            {
                Instruction instr = *pc++;
                int16_t bx = GET_sBx(instr);
                pc += bx;
                i += bx;

                goto dispatch;
            }

            case OP_JUMP_IF_FALSE:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                uint16_t bx = GET_Bx(instr);
                if (isfalse(ra))
                {
                    pc += bx;
                    i += bx;
                }

                goto dispatch;
            }

            case OP_RETURN:
            {
                Instruction instr = *pc++;
                Value* ra = VM_REG(GET_A(instr));
                int b = GET_B(instr) - 1;

                CallFrame* frame = vm->frame;
                CallFrame* framep = frame - 1;

                Value* res = frame->closure;

                Value* val_init = ra;
                Value* val_end = (b == -1) ? vm->top : ra + b;

                int nresults = frame->expected_returns;

                int i;
                for (i = nresults; i != 0 && val_init < val_end; i--)
                    setobjr(res++, val_init++);
                while (i-- > 0)
                    setvoid(res++);

                vm->frame = framep;
                vm->base = framep->registers;
                vm->top = (nresults == -1) ? res : framep->top;
                vm->frame_count--;

                if (frame->flags & M_CALLFRAME_RETURN)
                    goto exit;

                Closure* next_closure = closurevalue(framep->closure);
                Chunk* next_chunk = next_closure->M.chunk;

                // reentry
                pc = framep->pc;
                closure = next_closure;
                chunk = next_chunk;
                R = vm->base;
                K = next_chunk->constants;
                goto dispatch;
            }

            default:
            {
                VM_SAVE_PC();
                m_error(vm, INTERNAL_ERROR , "Unknown OPCODE type; Internal Enum '%d'", GET_OPCODE(*pc));
            }
        }
    exit: ;
}

void vm_free(VM* vm)
{
    if (vm == NULL) return;
    free(vm->stack);
    //free(vm->builtins);
    free(vm);
}

static void vm_preinit(VM* vm)
{
    vm->base = NULL;
    vm->bytes_allocated = 0;
    vm->current_handler = NULL;
    vm->frame = NULL;
    vm->frames = NULL;
    vm->frame_count = 0;
    vm->frame_last = NULL;
    vm->gcEnable = 0;
    vm->gd = NULL;
    vm->globals = NULL;
    vm->name = NULL;
    vm->nestedCcalls = 0;
    vm->next_gc = 0;
    vm->objects = NULL;
    vm->rngseed = 0;
    vm->stack = NULL;
    vm->stack_last = NULL;
    vm->stack_size = 0;
    vm->top = NULL;
}

static VM* vm_new()
{
    VM* vm = malloc(sizeof(VM));
    if (vm == NULL)
    {
        memoryCrash("VM");
        exit(1);
    }

    vm_preinit(vm);
    stack_init(vm);
    vm->strings = (StringTable){ 0 };
    return vm;
}

VM* vm_init(Chunk* main_chunk, const char* name)
{
    VM* vm = vm_new();
    main_chunk->name = NULL;
    main_chunk->source = name;
    setclosure(vm->frame->closure, allocate_Mclosure(vm, 0, vm->gd, main_chunk));
    vm->frame->pc = main_chunk->instructions;
    return vm;
}

void vm_execute(VM* vm)
{
    setlocale(LC_ALL, "C");

    vm_run(vm);
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