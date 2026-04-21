#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <malloc.h>
#include "vm.h"

static void vm_run(VM* vm)
{
    while (vm->frame_count > 0 && !vm->has_error)
    {
        Instruction* i = &vm->frame[0].chunk->instructions[0];
        printf("====================\n");
        printf("i = %" PRIu32 "\n", *i);
        printf("op = %d\n", GET_OPCODE(*i));
        printf("a = %d\n", GET_A(*i));
        printf("b = %d\n", GET_B(*i));
        printf("c = %d\n", GET_C(*i));
        break;
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
    Value* re = malloc(sizeof(Value) * main_chunk->rc);
    if (re == NULL)
        return;
    frame.registers = re;

    // VM
    vm->frame[0] = frame;
    vm->frame_count = 1;

    vm_run(vm);
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