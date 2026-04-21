#pragma once

#include "mconfig.h"
#include "mobjects.h"

typedef struct Chunk
{
    Instruction* instructions; // array of Instruction
    int ic; // Instruction Count, the actual instruction
    Value* constants; // array of Value
    int cc; // Constant Count, the actual actual constant
    int rc; // Register Count, the max of registers
    int parameter_count; // Parameter Count
    int return_count; // Return Count
    int* lines; // array of the code lines
}Chunk;

typedef struct
{
    Chunk* chunk;
    Instruction* ip; // The actual instruction pointer
    Value* registers; // array of registers
}CallFrame;

typedef struct
{
    CallFrame frame[MAX_FRAMES];
    int frame_count; // amount frames active
    bool has_error;
    char* error_message;
}VM;
