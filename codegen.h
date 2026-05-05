#pragma once

#include "compiler.h"
#include "mconfig.h"
#include "mobjects.h"
#include "vm.h"

typedef enum
{
/*
    name                args            description
*/
// Carga/Movimiento
    OP_LOADK,       //  A, B            R(A) = K(B)
    OP_LOAD_VAR,    //  A, B            R(A) = S(B)
    OP_STORE_VAR,   //  A, B            S(A) = R(B)
    OP_MOVE,        //  A, B            R(A) = R(B) 

    // Operaciones Aritmeticas
    OP_ADD,         // A, B, C        R(A) = R(B) + R(C)
    OP_SUB,         // A, B, C        R(A) = R(B) - R(C)
    OP_MUL,         // A, B, C        R(A) = R(B) * R(C)
    OP_DIV,         // A, B, C        R(A) = R(B) / R(C)
    OP_IDIV,        // A, B, C        R(A) = R(B) // R(C)
    OP_MOD,         // A, B, C        R(A) = R(B) % R(C)
    OP_POW,         // A, B, C        R(A) = R(B) ^ R(C)
    OP_CONCAT,      // A, B, C        R(A) = R(B) <> R(C)
    OP_UNM,         // A, B              R(A) = -R(B)
    OP_NOT,         // A, B            R(A) = not R(B)

    // Comparaciones
    OP_EQ,          // A, B, C        R(A) = R(B) == R(C)
    OP_NEQ,         // A, B, C        R(A) = R(B) != R(C)
    OP_LT,          // A, B, C        R(A) = R(B) < R(C)
    OP_LTE,         // A, B, C        R(A) = R(B) <= R(C)
    OP_GT,          // A, B, C        R(A) = R(B) > R(C)
    OP_GTE,         // A, B, C        R(A) = R(B) >= R(C)

    // Control de flujo
    OP_JUMP,        // Ax                 pc += Ax
    OP_JUMP_IF_FALSE,   // A, Bx        if !A -> pc += Bx
} BytecodeOp;




typedef struct
{
    IRList* ir;

    //Instruction* code;
    //int code_count;
    //int code_capacity;

    //Value* constants;

    int* line_info;
    int* label_to_pc;

    Chunk* chunk;
} CodeGen;


void generator_init(const char* src, const char* name, IRList* ir, ConstTable* T);
Chunk* generate_bydecode(CodeGen* G);

