#pragma once

#include "mconfig.h"
#include "mobjects.h"
#include "../backend/compiler.h"

typedef struct Chunk
{
    /* function name for stack trace */
    const char* name; 
    /* chunk source; NULL -> main script (vm->name) */
    const char* source;
    /* array of Instruction */
    Instruction* instructions; 
    /* the actual instruccion */
    int actual_instruction; 
    /* Instruction Count, the actual instruction */
    int instruction_capacity; 
    /* array of constansts Value */
    Constant* constants; 
    /* Constant Count, the actual actual constant */
    int constants_capacity; 
    /* Register Count, the max of registers */
    int register_capacity; 
    /* maximun of formal parameters */
    int parameter_count;
    /* maximun of chunk return values (0 = none) */
    int return_count; 
    /* array of the code lines */
    int* lines; 
}Chunk;

typedef struct
{
    Chunk* chunk;
    /* The actual instruction pointer */
    Instruction* ip; 
    /* frame base on vm->stack */
    Value* registers; 
    /* chunk->register_capacity */
    int slot_count; 
    /* caller line when it was invokated (0 = main script) */
    int call_line; 
    /* name for trace; NULL --> chuck->name or script name */
    const char* func_name; 
    /* caller register index when writte down return values */
    int result_base; 
}CallFrame;

typedef enum {
    INVALID_OPERANDS_ERROR,
    TYPES_ERROR,
    ARITHMETIC_ERROR,
    MEMORY_ERROR,
    INTERNAL_ERROR,
    ASSIGNMENT_ERROR,
    RESIZE_LIST_ERROR,
    RESIZE_DICT_ERROR,
    INDEX_OUT_OF_BOUNDS_ERROR,
    ATTEMPED_TO_INDEX_NO_COLLECTION_ERROR,
    INDEX_ERROR,
    INVALID_KEY_TYPE_ERROR,
} RErrorType;

typedef struct
{
    bool has_error;
    char message[512];
    int line;
    RErrorType type;
} RuntimeError;

typedef enum
{
    VM_OK = 0,
    VM_RUNTIME_ERROR,
    VM_HALTED
} VMStatus;

struct VM
{
    // frames
    CallFrame frames[MAX_FRAMES]; /* frames array */
    int frame_count; /* amount frames active */
    
    // stack
    Value stack[M_MAXSTACK]; /* all vm stack */
    Value* stack_top; /* vm stack top */

    // errors
    RuntimeError error; /* error */
    const char* name; /* main script */

    // GC
    GCObject* objects;
    size_t bytes_allocated;
    size_t next_gc;
    Value* globals;
    bool gcEnable;
    StringTable strings;
};

/* vm helpers */

/* --- runtime error helpers --- */

void vm_runtime_raise(VM* vm, RErrorType type, int line, const char* format, ...);
void vm_runtime_clear(VM* vm);
void vm_runtime_report(VM* vm);

int vm_current_line(VM* vm);
const char* vm_frame_name(VM* vm, int frame_index);
const char* vm_frame_source(VM* vm, int frame_index);

bool vm_push_frame(VM* vm, Chunk* chunk, int call_line, const char* func_name, int result_base);
void vm_pop_frame(VM* vm);

/*
 * Llamadas (convención para OP_CALL / codegen futuro):
 * - Caller coloca argumentos en R[arg_base .. arg_base+argc-1].
 * - argc puede ser < parameter_count; faltantes quedan nil (defaults los emite el compilador).
 * - Argumentos extra (argc > parameter_count) se ignoran.
 * - result_base: primer registro del caller donde vm_finish_return escribe los retornos.
 */
bool vm_begin_call(VM* vm, Chunk* callee, int call_line, const char* func_name, int result_base, int arg_base, int argc);

/*
 * returned_count: valores realmente devueltos por OP_RETURN (-1 = usar chunk->return_count).
 * return_reg_base: primer registro del callee con los valores a devolver.
 */
bool vm_finish_return(VM* vm, int return_reg_base, int returned_count);

VMStatus vm_run(VM* vm);
int vm_execute(Chunk* main_chunk, const char* name);

/* vm macros */

/* return if vm has error */
#define VM_HAS_ERROR(vm)        ((vm)->error.has_error)

/* exit from helpers void when there's an error */
#define VM_RETURN_IF_ERROR(vm) \
    { if (VM_HAS_ERROR(vm)) return; }

/* exir form vm_run / functions returns VMStatus */
#define VM_RETURN_STATUS_IF_ERROR(vm) \
    { if (VM_HAS_ERROR(vm)) return VM_RUNTIME_ERROR; }

/* exit from interprete case (switch) */
#define VM_BREAK_IF_ERROR(vm) \
    { if (VM_HAS_ERROR(vm)) break; }

/* actual line from active frame (para vm_runtime_raise) */
#define VM_LINE(vm) vm_current_line(vm)

/* chunk helpers */

Chunk* chunk_new();
void chunk_init(Chunk* chunk, ConstTable* c, IRList* ir);
void chunk_write(Chunk* chunk, Instruction instr, int line);

