
#include "err.h"
#include "compiler.h"
#include "codegen.h"

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <malloc.h>
#include "vm.h"

static void vm_run(VM* vm, CallFrame* frame)
{
    Chunk* chunk = frame->chunk;
    Instruction* pc = chunk->instructions;
    Value* R = frame->registers;
    Value* K = chunk->constants;
    //while (vm->frame_count > 0 && !vm->has_error)
    while(true)
    {
        Instruction instr = *pc++;

        uint8_t op = GET_OPCODE(instr);

        switch (op)
        {
            case OP_LOADK:
            {
                uint8_t a = GET_A(instr);
                uint16_t bx = GET_Bx(instr);

                //printf("a = %d y bx = %d\n", a, bx);
                
                R[a] = K[bx];
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

                // Agregar casos mixtos
                if ((R[b].type == VAL_INT /* || R[b].type == VAL_FLOAT*/) && (R[c].type == VAL_INT /* || R[c].type == VAL_FLOAT*/))
                {
                    R[a].type = VAL_INT;
                    R[a].i = R[b].i + R[c].i;
                    printf("ADD %i\n\n", R[a].i);
                }
                else if (R[b].type == VAL_INT && R[c].type == VAL_FLOAT)
                {
                    R[a].type = VAL_FLOAT;
                    R[a].f = R[b].i + R[c].f;
                    printf("ADD %f\n", R[a].f);
                }
                else if (R[b].type == VAL_FLOAT && R[c].type == VAL_INT)
                {
                    R[a].type = VAL_FLOAT;
                    R[a].f = R[b].f + R[c].i;
                    printf("ADD %f\n", R[a].f);

                }
                else if (R[b].type == VAL_FLOAT && R[c].type == VAL_FLOAT)
                {
                    R[a].type = VAL_FLOAT;
                    R[a].f = R[b].f + R[c].f;
                    printf("ADD %f\n", R[a].f);

                }


                break;
            }

            case OP_SUB:
            {

            }

            case OP_MUL:
            {

            }

            case OP_DIV:
            {

            }

            case OP_IDIV:
            {

            }

            case OP_MOD:
            {

            }

            case OP_POW:
            {

            }

            case OP_CONCAT:
            {

            }

            case OP_UNM:
            {

            }

            case OP_NOT:
            {

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

void vm_execute(Chunk* main_chunk)
{
    // VM
    VM* vm = malloc(sizeof(VM));
    if (vm == NULL)
        return;
    vm->frame_count = 0;
    vm->has_error = false;

    // frame inicial
    CallFrame frame;
    frame.chunk = main_chunk;
    frame.ip = main_chunk->instructions;
    //int rc = main_chunk->register_capacity;
    Value* re = malloc(sizeof(Value) * 256);
    if (re == NULL)
        return;
    frame.registers = re;

    // VM
    vm->frame[0] = frame;
    vm->frame_count = 1;

    vm_run(vm, &frame);
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