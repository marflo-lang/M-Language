#pragma once

#include "mconfig.h"
#include "mobjects.h"
#include "merror.h"
#include "../backend/compiler.h"

#include <setjmp.h>

struct Chunk
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
};

/**
* --- INFORMATION ABOUT CALLS ---
* 
*/

typedef struct CallFrame
{
    Value* closure; // the closure pointer on the stack
    Value* registers;// frame base on vm->stack
    Value* top; // top for this closure
    Instruction* pc; // saved pc where has to continue
    int call_line; // caller line when it was invokated (0 = main script)
    int expected_returns; // return values expected by caller (OP_CALL C operand)
    uint8_t flags; 
}CallFrame;

// Should the interpreter return after returning from this CallFrame?, First frame must have this set
#define M_CALLFRAME_RETURN (1 << 0)

typedef enum
{
    VM_OK = 0,
    VM_RUNTIME_ERROR
} VMStatus;

struct VM
{
    // frames
    CallFrame* frames; /* frames array */
    CallFrame* frame;
    CallFrame* frame_last; /* last frame; it means the frames limit */
    size_t frame_count; /* amount frames active */
    short nestedCcalls; /* nested C calls (for stack overflow) */

    // stack
    Value* stack; /* vm stack base */
    Value* base; // current closure base
    Value* top; /* first free slot in the stack */
    Value* stack_last; /* last free stack slot; it means the stack limit */
    size_t stack_size;

    // errors
    ErrorHandler* current_handler;
    const char* name; /* main script */

    // functions
    //Chunk** function_chunk;
    //size_t function_chunk_count;

    uint64_t rngseed; /* random number generator seed */

    // builtin
    //MBuiltinDef* builtins;
    //size_t builtin_count;
    //size_t builtin_capacity;

    /*
    * In the future, when we add threads, each thread will have
    * its own env and the closure will point to the env of the thread
    * it belongs to.
    */
    ObjDict* gd; // global dictionary

    // GC
    GCObject* objects;
    size_t bytes_allocated;
    size_t next_gc;
    Value* globals;
    bool gcEnable;
    StringTable strings;
};

typedef enum
{
    M_TYPE_INT,
    M_TYPE_FLOAT
} MNumType;

#define toint(o, n) (isint(o) || ((o) = vmutils_tryToInt(o, n)) != NULL)
#define tofloat(o, n)   (isfloat(o) || ((o) = vmutils_tryToFloat((o), (n))) != NULL)
#define tonumber(o, n)  (toint(o, n) || tofloat(o, n))
#define tostring(vm, o) (isstring(o) || vmutils_tryToString(vm, o))

void mopen_builtin(VM* vm);

bool vmutils_valueEquals(VM* vm, const Value* v1, const Value* v2);
bool vmutils_lessthan(VM* vm, const Value* v1, const Value* v2);
bool vmutils_lessOrEquals(VM* vm, const Value* v1, const Value* v2);
int vmutils_strcmp(ObjString* leftS, ObjString* rightS);
bool vmutils_tryToString(VM* vm, Value* obj);
Value* vmutils_tryToInt(const Value* obj, Value* n);
Value* vmutils_tryToFloat(const Value* obj, Value* n);
Value* vmutils_tryToNumber(const Value* obj, Value* n);
void vmutils_concat(VM* vm, int total, int last);
int vmutils_precall(VM* vm, Value* func, int nresults);

void vm_free(VM* vm);
VM* vm_init(Chunk* main_chunk, const char* name);
void vm_execute(VM* vm);

/* chunk helpers */

Chunk* chunk_new();
void chunk_init(Chunk* chunk, ConstTable* c, IRList* ir);
void chunk_write(Chunk* chunk, Instruction instr, int line);

