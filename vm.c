
#include "err.h"
#include "codegen.h"
#include "vm.h"

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <malloc.h>
#include <math.h>
#include <string.h>

static bool value_equals(Value a, Value b)
{
    if (a.type != b.type) return false;

    switch (a.type)
    {
    case VAL_INT:
    {
        return a.i == b.i;
    }

    case VAL_FLOAT:
    {
        return a.f == b.f;
    }

    case VAL_BOOLEAN:
    {
        return a.b == b.b;
    }

    case VAL_STRING:
    {
        return a.string.length == b.string.length && (strncmp(a.string.chars, b.string.chars, a.string.length) == 0);
    }

    case VAL_NIL:
    {
        return true;
    }

    case VAL_NAN:
    {
        return true;
    }

    }
}

static const char* getValueTypeName(Value v)
{
    if (v.type == VAL_INT)
        return "int";
    else if (v.type == VAL_FLOAT)
        return "float";
    else if (v.type == VAL_STRING)
        return "string";
    else if (v.type == VAL_BOOLEAN)
        return "boolean";
    else if (v.type == VAL_NIL)
        return "nil";
    else if (v.type == VAL_NAN)
        return "NaN";
    else
        return "Unrecognized type";
}

static bool can_coerce_to_float(Value v, double* out_val)
{
    if (v.type == VAL_INT)
    {
        *out_val = (double) v.i;
        return true;
    }
    else if (v.type == VAL_FLOAT)
    {
        *out_val = v.f;
        return true;
    }
    else if (v.type == VAL_STRING)
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
    Value* K = chunk->constants;
    //while (vm->frame_count > 0 && !vm->has_error)
    int i = 0;
    while(true)
    {
        i++;
        Instruction instr = *pc++;

        uint8_t op = GET_OPCODE(instr);

        switch (op)
        {
            case OP_LOADK:
            {
                uint8_t a = GET_A(instr);
                uint16_t bx = GET_Bx(instr);

                //printf("a = %d y bx = %d\n", a, bx);
                
                R[a].type = K[bx].type;
                R[a] = K[bx];
                break;
            }

            case OP_MOVE:
            {
                uint8_t a = GET_A(instr);
                uint16_t bx = GET_Bx(instr);

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
                if (R[b].type == VAL_INT && R[c].type == VAL_INT)
                {
                    R[a].type = VAL_INT;
                    R[a].i = R[b].i + R[c].i;
                    printf("ADD %i\n", R[a].i);
                }
                else if (R[b].type == VAL_FLOAT && R[c].type == VAL_FLOAT)
                {
                    R[a].type = VAL_FLOAT;
                    R[a].f = R[b].f + R[c].f;
                    printf("ADD %f\n", R[a].f);

                }
                else
                {
                    // slow-path
                    double num_b, num_c;
                    if (can_coerce_to_float(R[b], &num_b) && can_coerce_to_float(R[c], &num_c))
                    {
                        R[a].type = VAL_FLOAT;
                        R[a].f = num_b + num_c;
                        printf("ADD %f\n", R[a].f);
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
                if (R[b].type == VAL_INT && R[c].type == VAL_INT)
                {
                    R[a].type = VAL_INT;
                    R[a].i = R[b].i - R[c].i;
                }
                else if (R[b].type == VAL_FLOAT && R[c].type == VAL_FLOAT)
                {
                    R[a].type = VAL_FLOAT;
                    R[a].f = R[b].f - R[c].f;
                }
                else
                {
                    // slow-path
                    double num_b, num_c;
                    if (can_coerce_to_float(R[b], &num_b) && can_coerce_to_float(R[c], &num_c))
                    {
                        R[a].type = VAL_FLOAT;
                        R[a].f = num_b - num_c;
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
                if (R[b].type == VAL_INT && R[c].type == VAL_INT)
                {
                    R[a].type = VAL_INT;
                    R[a].i = R[b].i * R[c].i;
                }
                else if (R[b].type == VAL_FLOAT && R[c].type == VAL_FLOAT)
                {
                    R[a].type = VAL_FLOAT;
                    R[a].f = R[b].f * R[c].f;
                }
                else
                {
                    // slow-path
                    double num_b, num_c;
                    if (can_coerce_to_float(R[b], &num_b) && can_coerce_to_float(R[c], &num_c))
                    {
                        R[a].type = VAL_FLOAT;
                        R[a].f = num_b * num_c;
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
                if ((R[c].type == VAL_INT && R[c].i == 0) || (R[c].type == VAL_FLOAT && R[c].f == 0.0))
                {
                    arithmeticError(vm->name, chunk->lines[i]);
                }
                if (R[b].type == VAL_INT && R[c].type == VAL_INT)
                {
                    R[a].type = VAL_INT;
                    R[a].i = R[b].i / R[c].i;
                }
                else if (R[b].type == VAL_FLOAT && R[c].type == VAL_FLOAT)
                {
                    R[a].type = VAL_FLOAT;
                    R[a].f = R[b].f / R[c].f;
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
                        R[a].type = VAL_FLOAT;
                        R[a].f = num_b / num_c;
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
                if ((R[c].type == VAL_INT && R[c].i == 0) || (R[c].type == VAL_FLOAT && R[c].f == 0.0))
                {
                    arithmeticError(vm->name, chunk->lines[i]);
                }
                if (R[b].type == VAL_INT && R[c].type == VAL_INT)
                {
                    R[a].type = VAL_INT;
                    R[a].i = (int) floor(R[b].i / R[c].i);
                }
                else if (R[b].type == VAL_FLOAT && R[c].type == VAL_FLOAT)
                {
                    R[a].type = VAL_FLOAT;
                    R[a].f = floor(R[b].f / R[c].f);
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
                        R[a].type = VAL_FLOAT;
                        R[a].f = floor(num_b / num_c);
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
                if ((R[c].type == VAL_INT && R[c].i == 0) || (R[c].type == VAL_FLOAT && R[c].f == 0.0))
                {
                    arithmeticError(vm->name, chunk->lines[i]);
                }
                if (R[b].type == VAL_INT && R[c].type == VAL_INT)
                {
                    R[a].type = VAL_INT;
                    R[a].i = (int) (R[b].i % R[c].i);
                }
                else if (R[b].type == VAL_FLOAT && R[c].type == VAL_FLOAT)
                {
                    R[a].type = VAL_FLOAT;
                    R[a].f = fmod(R[b].f, R[c].f);
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
                        R[a].type = VAL_FLOAT;
                        R[a].f = fmod(num_b, num_c);
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
                if (R[b].type == VAL_INT && R[c].type == VAL_INT)
                {
                    R[a].type = VAL_INT;
                    int valb = R[b].i;
                    int valc = R[c].i;
                    // fast-paths because pow() is too slowly
                    if (valc == 2)
                    {
                        R[a].i = valb * valb;
                    }
                    else if (valc == 3)
                    {
                        R[a].i = valb * valb * valb;
                    }
                    else if (valc == 0)
                    {
                        R[a].i = 1;
                    }
                    else
                    {
                        // slow-path
                        R[a].i = (int) pow(valb, valc);
                    }
                }
                else if (R[b].type == VAL_FLOAT && R[c].type == VAL_FLOAT)
                {
                    R[a].type = VAL_FLOAT;
                    double valb = R[b].f;
                    double valc = R[c].f;
                    // fast-paths because pow() is too slowly
                    if (valc == 2.0)
                    {
                        R[a].f = valb * valb;
                    }
                    else if (valc == 0.5)
                    {
                        R[a].f = sqrt(valb);
                    }
                    else if (valc == 3.0)
                    {
                        R[a].f = valb * valb * valb;
                    }
                    else if (valc == 0.0)
                    {
                        R[a].f = 1.0;
                    }
                    else
                    {
                        // slow-path
                        R[a].f = pow(valb, valc);
                    }
                }
                else
                {
                    // slow-path
                    double num_b, num_c;
                    if (can_coerce_to_float(R[b], &num_b) && can_coerce_to_float(R[c], &num_c))
                    {
                        R[a].type = VAL_FLOAT;
                        R[a].f = pow(num_b, num_c);
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
                if (R[b].type == VAL_STRING && R[c].type == VAL_STRING)
                {
                    R[a].type = VAL_STRING;
                    R[a].string.length = R[b].string.length + R[c].string.length;
                    //R[a].i = R[b].i * R[c].i;
                    const char* text1 = R[b].string.chars;
                    const char* text2 = R[c].string.chars;

                    char* result = "";
                    memcpy(result, text1, R[b].string.length);
                    strcat_s(result, R[a].string.length, text2);

                    R[a].string.chars = result;
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
                if (R[bx].type == VAL_INT)
                {
                    R[a].type = VAL_INT;
                    R[a].i = -R[bx].i;
                }
                else if (R[bx].type == VAL_FLOAT)
                {
                    R[a].type = VAL_FLOAT;
                    R[a].f = -R[bx].f;
                }
                else
                {
                    // slow-path
                    double num_b;
                    if (can_coerce_to_float(R[bx], &num_b))
                    {
                        R[a].type = VAL_FLOAT;
                        R[a].f = -num_b;
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
                uint8_t bx = GET_Bx(instr);

                R[a].type = VAL_BOOLEAN;
                Value val = R[bx];
                R[a].b = (val.type == VAL_NAN || val.type == VAL_NIL || (val.type == VAL_BOOLEAN && val.b == false));

                break;
            }

            case OP_EQ:
            {
                uint8_t a = GET_A(instr);
                uint8_t b = GET_B(instr);
                uint8_t c = GET_C(instr);

                if (R[b].type == R[c].type)
                {
                    if (R[b].type == VAL_BOOLEAN)
                        R[a].b = R[b].b == R[c].b;
                    else if (R[b].type == VAL_FLOAT)
                        R[a].b = R[b].f == R[c].f;
                    else if (R[b].type = VAL_INT)
                        R[a].b = R[b].i == R[c].i;
                    else if (R[b].type == VAL_STRING)
                        R[a].b = (R[b].string.length == R[c].string.length && (strncmp(R[b].string.chars, R[c].string.chars, R[b].string.length) == 0));
                    else if (R[b].type == VAL_NIL)
                        R[a].b = true;
                    else if (R[b].type == VAL_NAN)
                        R[a].b = true;
                    else
                        printf("Error desconocido tipo %d\n", R[b].type);
                }
                else
                {
                    R[a].b = false;
                }

                break;
            }

            case OP_NEQ:
            {
                uint8_t a = GET_A(instr);
                uint8_t b = GET_B(instr);
                uint8_t c = GET_C(instr);

                if (R[b].type == R[c].type)
                {
                    if (R[b].type == VAL_BOOLEAN)
                        R[a].b = R[b].b != R[c].b;
                    else if (R[b].type == VAL_FLOAT)
                        R[a].b = R[b].f != R[c].f;
                    else if (R[b].type = VAL_INT)
                        R[a].b = R[b].i != R[c].i;
                    else if (R[b].type == VAL_STRING)
                        R[a].b = (R[b].string.length != R[c].string.length || !(strncmp(R[b].string.chars, R[c].string.chars, R[b].string.length) == 0));
                    else if (R[b].type == VAL_NIL)
                        R[a].b = false;
                    else if (R[b].type == VAL_NAN)
                        R[a].b = false;
                    else
                        printf("Error desconocido tipo %d\n", R[b].type);
                }
                else
                {
                    R[a].b = true;
                }

                break;
            }

            case OP_LT:
            {
                uint8_t a = GET_A(instr);
                uint8_t b = GET_B(instr);
                uint8_t c = GET_C(instr);

                if (R[b].type == R[c].type)
                {
                    if (R[b].type == VAL_BOOLEAN)
                        //R[a].b = R[b].b == R[c].b;
                        printf("Mejor error para LT BOOLEAN\n");
                    else if (R[b].type == VAL_FLOAT)
                        R[a].b = R[b].f < R[c].f;
                    else if (R[b].type = VAL_INT)
                        R[a].b = R[b].i < R[c].i;
                    else if (R[b].type == VAL_STRING)
                        R[a].b = (R[b].string.length < R[c].string.length);
                    else if (R[b].type == VAL_NIL)
                        //R[a].b = true;
                        printf("Mejor error para LT nil\n");
                    else if (R[b].type == VAL_NAN)
                        //R[a].b = true;
                        printf("Mejor error para LT NaN\n");
                    else
                        printf("Error desconocido tipo %d\n", R[b].type);
                }
                else
                {
                    R[a].b = false;
                }

                break;
            }

            case OP_LTE:
            {
                uint8_t a = GET_A(instr);
                uint8_t b = GET_B(instr);
                uint8_t c = GET_C(instr);

                if (R[b].type == R[c].type)
                {
                    if (R[b].type == VAL_BOOLEAN)
                        //R[a].b = R[b].b == R[c].b;
                        printf("Mejor error para LTE BOOLEAN\n");
                    else if (R[b].type == VAL_FLOAT)
                        R[a].b = R[b].f <= R[c].f;
                    else if (R[b].type = VAL_INT)
                        R[a].b = R[b].i <= R[c].i;
                    else if (R[b].type == VAL_STRING)
                        R[a].b = (R[b].string.length <= R[c].string.length);
                    else if (R[b].type == VAL_NIL)
                        //R[a].b = true;
                        printf("Mejor error para LTE nil\n");
                    else if (R[b].type == VAL_NAN)
                        //R[a].b = true;
                        printf("Mejor error para LTE NaN\n");
                    else
                        printf("Error desconocido tipo %d\n", R[b].type);
                }
                else
                {
                    R[a].b = false;
                }

                break;
            }

            case OP_GT:
            {
                uint8_t a = GET_A(instr);
                uint8_t b = GET_B(instr);
                uint8_t c = GET_C(instr);

                if (R[b].type == R[c].type)
                {
                    if (R[b].type == VAL_BOOLEAN)
                        //R[a].b = R[b].b == R[c].b;
                        printf("Mejor error para GT BOOLEAN\n");
                    else if (R[b].type == VAL_FLOAT)
                        R[a].b = R[b].f > R[c].f;
                    else if (R[b].type = VAL_INT)
                        R[a].b = R[b].i > R[c].i;
                    else if (R[b].type == VAL_STRING)
                        R[a].b = (R[b].string.length > R[c].string.length);
                    else if (R[b].type == VAL_NIL)
                        //R[a].b = true;
                        printf("Mejor error para GT nil\n");
                    else if (R[b].type == VAL_NAN)
                        //R[a].b = true;
                        printf("Mejor error para GT NaN\n");
                    else
                        printf("Error desconocido tipo %d\n", R[b].type);
                }
                else
                {
                    R[a].b = false;
                }

                break;
            }

            case OP_GTE:
            {
                uint8_t a = GET_A(instr);
                uint8_t b = GET_B(instr);
                uint8_t c = GET_C(instr);

                if (R[b].type == R[c].type)
                {
                    if (R[b].type == VAL_BOOLEAN)
                        //R[a].b = R[b].b == R[c].b;
                        printf("Mejor error para GTE BOOLEAN\n");
                    else if (R[b].type == VAL_FLOAT)
                        R[a].b = R[b].f >= R[c].f;
                    else if (R[b].type = VAL_INT)
                        R[a].b = R[b].i >= R[c].i;
                    else if (R[b].type == VAL_STRING)
                        R[a].b = (R[b].string.length >= R[c].string.length);
                    else if (R[b].type == VAL_NIL)
                        //R[a].b = true;
                        printf("Mejor error para GTE nil\n");
                    else if (R[b].type == VAL_NAN)
                        //R[a].b = true;
                        printf("Mejor error para GTE NaN\n");
                    else
                        printf("Error desconocido tipo %d\n", R[b].type);
                }
                else
                {
                    R[a].b = false;
                }

                break;
            }

            case OP_JUMP:
            {
                uint16_t bx = GET_Bx(instr);

                pc += bx;

                break;
            }

            case OP_JUMP_IF_FALSE:
            {
                uint8_t a = GET_A(instr);
                uint16_t bx = GET_Bx(instr);

                if (R[a].type == VAL_NAN || R[a].type == VAL_NIL || (R[a].type == VAL_BOOLEAN && R[a].b == false))
                    pc += bx;

                break;
            }

            case OP_HALT:
            {
                return;
            }

            default:
            {
                printf("Mejor error para op desconocido, op = %d\n", op);
                exit(1);
            }
        }

        //printf("====================\n");
        //printf("i = %" PRIu32 "\n", *i);
        //printf("op = %d\n", GET_OPCODE(*i));
        //printf("a = %d\n", GET_A(*i));
        //printf("b = %d\n", GET_B(*i));
        //printf("c = %d\n", GET_C(*i));
        
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