
#include "../utils/err.h"
#include "../backend/codegen.h"
#include "vm.h"

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <malloc.h>
#include <math.h>
#include <string.h>


static const char* getValueTypeName(Value v)
{
    if (isint(v))
        return "int";
    else if (isfloat(v))
        return "float";
    else if (isstring(v))
        return "string";
    else if (islist(v))
        return "list";
    else if (isboolean(v))
        return "boolean";
    else if (isnil(v))
        return "nil";
    else if (ismnan(v))
        return "NaN";
    else
        return "Unrecognized type";
}

static bool can_coerce_to_float(Value v, double* out_val)
{
    if (isint(v))
    {
        *out_val = cast_double(ivalue(v));
        return true;
    }
    else if (isfloat(v))
    {
        *out_val = fvalue(v);
        return true;
    }
    else if (isstring(v))
    {
        // aqui va funcion para convertir de string a float
        return false; // temporal
    }

    return false;
}

static void vm_run(VM* vm)
{
    CallFrame* frame = &vm->frames[vm->frame_count - 1];
    Chunk* chunk = frame->chunk;
    Instruction* pc = frame->ip;
    Value* R = frame->registers;
    Constant* K = chunk->constants;
    //while (vm->frame_count > 0 && !vm->has_error)
    int i = 0;
    while(true)
    {
        i++;
        const Instruction instr = *pc++;

        uint8_t op = GET_OPCODE(instr);

        switch (op)
        {
            case OP_LOADK:
            {
                uint8_t a = GET_A(instr);
                uint16_t bx = GET_Bx(instr);
                
                //R[a].type = K[bx].type;
                //R[a] = K[bx];
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
                    //setstring(&R[a], kbx.string.chars, kbx.string.length);
                    R[a].type = VAL_OBJ;
                    R[a].obj = allocate_string(vm, kbx.string.chars, kbx.string.length);
                }

                break;
            }

            case OP_MOVE:
            {
                uint8_t a = GET_A(instr);
                uint16_t bx = GET_Bx(instr);
                
                //printf("MOVE %d %d\n", a, bx);

                R[a].type = R[bx].type;
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
                    //printf("ADD %i\n", R[a].i);
                }
                else if (isfloat(R[b]) && isfloat(R[c]))
                {
                    setfloat(R[a], fvalue(R[b]) + fvalue(R[c]));
                    //printf("ADD %f\n", R[a].f);
                }
                else
                {
                    // slow-path
                    double num_b, num_c;
                    if (can_coerce_to_float(R[b], &num_b) && can_coerce_to_float(R[c], &num_c))
                    {
                        setfloat(R[a], num_b + num_c);
                        //printf("ADD %f\n", R[a].f);
                    }
                    else
                    {
                        char* type1 = getValueTypeName(R[b]);
                        char* type2 = getValueTypeName(R[c]);
                        vm->has_error = true;
                        invalidOperandsError(vm->name, chunk->lines[i], "+", type1, type2);
                    }
                }


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
                    double num_b, num_c;
                    if (can_coerce_to_float(R[b], &num_b) && can_coerce_to_float(R[c], &num_c))
                    {
                        setfloat(R[a], num_b - num_c);
                    }
                    else
                    {
                        char* type1 = getValueTypeName(R[b]);
                        char* type2 = getValueTypeName(R[c]);
                        vm->has_error = true;
                        invalidOperandsError(vm->name, chunk->lines[i], "-", type1, type2);
                    }
                }


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
                    //printf("MUL %d %d %d\n", ivalue(R[a]), ivalue(R[b]), ivalue(R[c]));
                }
                else if (isfloat(R[b]) && isfloat(R[c]))
                {
                    setfloat(R[a], fvalue(R[b]) * fvalue(R[c]));
                }
                else
                {
                    // slow-path
                    double num_b, num_c;
                    if (can_coerce_to_float(R[b], &num_b) && can_coerce_to_float(R[c], &num_c))
                    {
                        setfloat(R[a], num_b * num_c);
                    }
                    else
                    {
                        char* type1 = getValueTypeName(R[b]);
                        char* type2 = getValueTypeName(R[c]);
                        vm->has_error = true;
                        invalidOperandsError(vm->name, chunk->lines[i], "*", type1, type2);
                    }
                }


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
                    arithmeticError(vm->name, chunk->lines[i]);
                }
                if (isint(R[b]) && isint(R[c]))
                {
                    setint(R[a], ivalue(R[b]) / ivalue(R[c]));
                }
                else if (isfloat(R[b]) && isfloat(R[c]))
                {
                    setfloat(R[a], fvalue(R[b]) / fvalue(R[c]));
                }
                else
                {
                    // slow-path
                    double num_b, num_c;
                    if (can_coerce_to_float(R[b], &num_b) && can_coerce_to_float(R[c], &num_c))
                    {
                        if (num_c == 0.0)
                        {
                            arithmeticError(vm->name, chunk->lines[i]);
                        }
                        setfloat(R[a], num_b / num_c);
                    }
                    else
                    {
                        char* type1 = getValueTypeName(R[b]);
                        char* type2 = getValueTypeName(R[c]);
                        vm->has_error = true;
                        invalidOperandsError(vm->name, chunk->lines[i], "/", type1, type2);
                    }
                }


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
                    arithmeticError(vm->name, chunk->lines[i]);
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
                    double num_b, num_c;
                    if (can_coerce_to_float(R[b], &num_b) && can_coerce_to_float(R[c], &num_c))
                    {
                        if (num_c == 0.0)
                        {
                            arithmeticError(vm->name, chunk->lines[i]);
                        }
                        setfloat(R[a], floor(num_b / num_c));
                    }
                    else
                    {
                        char* type1 = getValueTypeName(R[b]);
                        char* type2 = getValueTypeName(R[c]);
                        vm->has_error = true;
                        invalidOperandsError(vm->name, chunk->lines[i], "//", type1, type2);
                    }
                }


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
                    arithmeticError(vm->name, chunk->lines[i]);
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
                    double num_b, num_c;
                    if (can_coerce_to_float(R[b], &num_b) && can_coerce_to_float(R[c], &num_c))
                    {
                        if (num_c == 0.0)
                        {
                            arithmeticError(vm->name, chunk->lines[i]);
                        }
                        setfloat(R[a], fmod(num_b, num_c));
                    }
                    else
                    {
                        char* type1 = getValueTypeName(R[b]);
                        char* type2 = getValueTypeName(R[c]);
                        vm->has_error = true;
                        invalidOperandsError(vm->name, chunk->lines[i], "%%", type1, type2);
                    }
                }


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
                    double num_b, num_c;
                    if (can_coerce_to_float(R[b], &num_b) && can_coerce_to_float(R[c], &num_c))
                    {
                        setfloat(R[a], pow(num_b, num_c));
                    }
                    else
                    {
                        char* type1 = getValueTypeName(R[b]);
                        char* type2 = getValueTypeName(R[c]);
                        vm->has_error = true;
                        invalidOperandsError(vm->name, chunk->lines[i], "^", type1, type2);
                    }
                }


                break;
            }

            case OP_CONCAT:
            {
                uint8_t a = GET_A(instr);
                uint8_t b = GET_B(instr);
                uint8_t c = GET_C(instr);

                // fast-path
                if (isstring(R[b]) && isstring(R[c]))
                {
                    int newLength = slenvalue(R[b]) + slenvalue(R[c]);

                    char* result = malloc(newLength + 1);
                    if (result == NULL)
                    {
                        memoryCrash("VM");
                        exit(1);
                    }
                    
                    memcpy(result, svalue(R[b]), slenvalue(R[b]));
                    memcpy(result + slenvalue(R[b]), svalue(R[c]), slenvalue(R[c]));
                    result[newLength] = '\0';

                    //setstring(&R[a], result, newLength);
                    R[a].type = VAL_OBJ;
                    R[a].obj = allocate_string(vm, result, newLength);

                    free(result);

                    //printf("R[a] = '%.*s'\n", ((ObjString*)R[a].obj)->length, ((ObjString*)R[a].obj)->chars);

                }
                else
                {
                    // slow-path
                    //double num_b, num_c;
                    //if (can_coerce_to_float(R[b], &num_b) && can_coerce_to_float(R[c], &num_c))
                    //{
                    //    R[a].type = VAL_FLOAT;
                    //    R[a].f = num_b * num_c;
                    //}
                    //else
                    //{
                        char* type1 = getValueTypeName(R[b]);
                        char* type2 = getValueTypeName(R[c]);
                        vm->has_error = true;
                        invalidOperandsError(vm->name, chunk->lines[i], "<>", type1, type2);
                    //}
                }


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
                    double num_b;
                    if (can_coerce_to_float(R[bx], &num_b))
                    {
                        setfloat(R[a], -num_b);
                    }
                    else
                    {
                        char* type1 = getValueTypeName(R[bx]);
                        vm->has_error = true;
                        invalidOperandsError(vm->name, chunk->lines[i], "-", type1, NULL);
                    }
                }


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
                    R[a].type = R[c].type;
                    R[a] = R[c];
                }
                else
                {
                    R[a].type = R[b].type;
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
                    R[a].type = R[b].type;
                    R[a] = R[b];
                }
                else
                {
                    R[a].type = R[c].type;
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
                    typeError(vm->name, chunk->lines[i], "int", getValueTypeName(R[b]));

                ObjList* list = allocate_list(vm, 0, (b > -1) ? ivalue(R[b]) : 0, c);
                set_list(&R[a], list);
                break;
            }

            case OP_SET_LIST:
            {
                uint8_t a = GET_A(instr);
                uint8_t b = GET_B(instr);
                uint8_t c = GET_C(instr);

                if (!islist(R[a]))
                    cannotAddElementNotList(vm->name, chunk->lines[i], getValueTypeName(R[a]));
                

                break;
            }

            case OP_PUSH_LIST:
            {


                break;
            }

            case OP_GET_LIST:
            {


                break;
            }

            case OP_CREATE_DICT:
            {


                break;
            }

            case OP_SET_DICT:
            {


                break;
            }

            case OP_GET_DICT:
            {


                break;
            }

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
                        //setboolean(R[a], (R[b].string.length == R[c].string.length && (strncmp(R[b].string.chars, R[c].string.chars, R[b].string.length) == 0)));
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
                        unknownType(vm->name, chunk->lines[i], ttype(R[b]));
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
                        //setboolean(R[a], (R[b].string.length != R[c].string.length || !(strncmp(R[b].string.chars, R[c].string.chars, R[b].string.length) == 0)));
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
                        unknownType(vm->name, chunk->lines[i], ttype(R[b]));
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

                //if (R[b].type == R[c].type)
                if (ttype(R[b]) == ttype(R[c]))
                {
                    if (isboolean(R[b]))
                    {
                        invalidOperandsError(vm->name, chunk->lines[i], "<", "boolean", "boolean");
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
                        setboolean(R[a], slenvalue(R[b]) < slenvalue(R[c]));

                    }
                    else if (isnil(R[b]))
                        invalidOperandsError(vm->name, chunk->lines[i], "<", "nil", "nil");
                    else if (ismnan(R[b]))
                        invalidOperandsError(vm->name, chunk->lines[i], "<", "NaN", "NaN");
                    else
                        unknownType(vm->name, chunk->lines[i], ttype(R[b]));
                }
                else
                {
                    const char* type1 = getValueTypeName(R[b]);
                    const char* type2 = getValueTypeName(R[c]);
                    invalidOperandsError(vm->name, chunk->lines[i], "<", type1, type2);
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
                        invalidOperandsError(vm->name, chunk->lines[i], "<=", "boolean", "boolean");
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
                        setboolean(R[a], slenvalue(R[b]) <= slenvalue(R[c]));
                    }
                    else if (isnil(R[b]))
                        invalidOperandsError(vm->name, chunk->lines[i], "<=", "nil", "nil");
                    else if (ismnan(R[b]))
                        invalidOperandsError(vm->name, chunk->lines[i], "<=", "NaN", "NaN");
                    else
                        unknownType(vm->name, chunk->lines[i], ttype(R[b]));
                }
                else
                {
                    const char* type1 = getValueTypeName(R[b]);
                    const char* type2 = getValueTypeName(R[c]);
                    invalidOperandsError(vm->name, chunk->lines[i], "<=", type1, type2);
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
                        invalidOperandsError(vm->name, chunk->lines[i], ">", "boolean", "boolean");
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
                        setboolean(R[a], slenvalue(R[b]) > slenvalue(R[c]));
                    }
                    else if (isnil(R[b]))
                        invalidOperandsError(vm->name, chunk->lines[i], ">", "nil", "nil");
                    else if (ismnan(R[b]))
                        invalidOperandsError(vm->name, chunk->lines[i], ">", "NaN", "NaN");
                    else
                        unknownType(vm->name, chunk->lines[i], ttype(R[b]));
                }
                else
                {
                    const char* type1 = getValueTypeName(R[b]);
                    const char* type2 = getValueTypeName(R[c]);
                    invalidOperandsError(vm->name, chunk->lines[i], ">", type1, type2);
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
                        invalidOperandsError(vm->name, chunk->lines[i], ">=", "boolean", "boolean");
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
                        setboolean(R[a], slenvalue(R[b]) >= slenvalue(R[c]));
                    }
                    else if (isnil(R[b]))
                        invalidOperandsError(vm->name, chunk->lines[i], ">=", "nil", "nil");
                    else if (ismnan(R[b]))
                        invalidOperandsError(vm->name, chunk->lines[i], ">=", "NaN", "NaN");
                    else
                        unknownType(vm->name, chunk->lines[i], ttype(R[b]));
                }
                else
                {
                    const char* type1 = getValueTypeName(R[b]);
                    const char* type2 = getValueTypeName(R[c]);
                    invalidOperandsError(vm->name, chunk->lines[i], ">=", type1, type2);
                }

                break;
            }

            case OP_JUMP:
            {
                int16_t bx = GET_sBx(instr);
                //printf("ENTRO\n");
                //printf("JUMP Bx = %d\n", bx);
                pc += bx;
                i += bx;

                break;
            }

            case OP_JUMP_IF_FALSE:
            {
                uint8_t a = GET_A(instr);
                uint16_t bx = GET_Bx(instr);
                //printf("ENTRO\n");
                //printf("JUMP IF FALSE Bx = %d\n", bx);
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
                unknownType(vm->name, chunk->lines[i], op);
            }
        }        
    }
}

void vm_execute(Chunk* main_chunk, const char* name)
{
    // VM
    //VM* vm = malloc(sizeof(VM));
    //if (vm == NULL)
        //return;
    VM* vm = malloc(sizeof(VM));
    if (vm == NULL)
    {
        memoryCrash("VM");
        exit(1);
    }
    vm->frame_count = 0;
    vm->has_error = false;
    vm->stack_top = vm->stack;
    vm->name = name;
    vm->objects = NULL;
    vm->bytes_allocated = 0;
    vm->next_gc = 1024 * 15;
    vm->globals = NULL;
    vm->gcEnable = false;
    vm->strings.capacity = 0;
    vm->strings.count = 0;
    vm->strings.entries = NULL;

    // frame inicial
    CallFrame* frame = &vm->frames[vm->frame_count++];
    frame->chunk = main_chunk;
    frame->ip = main_chunk->instructions;
    //int rc = main_chunk->register_capacity;
    //Value* re = malloc(sizeof(Value) * 256);
    //if (re == NULL)
        //return;
    frame->registers = vm->stack_top;
    vm->stack_top += main_chunk->register_capacity;

    // VM
    //vm->frame[0] = frame;
    //vm->frame_count = 1;

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
    //printf("====== %d====\n", chunk->actual_instruction);
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