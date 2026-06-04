#pragma once
#include "../frontend/ast.h"
#include "../vm/mobjects.h"

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
/*
    name                args            description
*/
    // Carga/Movimiento
    IR_LOAD_CONST,  //  A, B            R(A) = K(B)
    IR_LOAD_VAR,    //  A, B            R(A) = S(B)
    IR_STORE_VAR,   //  A, B            S(A) = R(B)
    IR_MOVE,        //  A, B            R(A) = R(B) 

    // Operaciones Aritmeticas
    IR_ADD,         // A, B, C        R(A) = R(B) + R(C)
    IR_SUB,         // A, B, C        R(A) = R(B) - R(C)
    IR_MUL,         // A, B, C        R(A) = R(B) * R(C)
    IR_DIV,         // A, B, C        R(A) = R(B) / R(C)
    IR_IDIV,        // A, B, C        R(A) = R(B) // R(C)
    IR_MOD,         // A, B, C        R(A) = R(B) % R(C)
    IR_POW,         // A, B, C        R(A) = R(B) ^ R(C)
    IR_CONCAT,      // A, B, C        R(A) = R(B) <> R(C)
    IR_UNM,         // A, B              R(A) = -R(B)
    IR_NOT,         // A, B            R(A) = not R(B)
    IR_OR,          // A, B, C        R(A) = R(B) or R(C)
    IR_AND,         // A, B, C        R(A) = R(B) and R(c)

    // Comparaciones
    IR_EQ,          // A, B, C        R(A) = R(B) == R(C)
    IR_NEQ,         // A, B, C        R(A) = R(B) != R(C)
    IR_LT,          // A, B, C        R(A) = R(B) < R(C)
    IR_LTE,         // A, B, C        R(A) = R(B) <= R(C)
    IR_GT,          // A, B, C        R(A) = R(B) > R(C)
    IR_GTE,         // A, B, C        R(A) = R(B) >= R(C)

    // Listas
    IR_CREATE_LIST, // A, B, C        R(A) = [] capacity = R(B), fixed = C
    //IR_SET_LIST,    // A, B, C        R(A)[B] = R[C]
    IR_PUSH_LIST,   // A, B           R(A)[length + 1] = R(B)
    //IR_GET_LIST,    // A, B, C        R(A) = R(B)[R(C)]

    // Diccionarios
    IR_CREATE_DICT, // A, B, C        R(A) = {} count = B, fixed = C
    //IR_SET_DICT,    // A, B, C        R(A)[R(B)] = R(C)
    //IR_GET_DICT,    // A, B, C        R(A) = R(B)[R(C)]

    IR_SET_INDEX,   // A, B, C       R(A)[R(B)] = R(C)
    IR_GET_INDEX,   // A, B, C       R(A) = R(B)[R(C)]

    // Control de flujo
    IR_JUMP,        // Ax                 pc += Ax
    IR_JUMP_IF_FALSE,   // A, Bx        if !A -> pc += Bx
    IR_HALT,
} IROpCode;

typedef struct
{
    IROpCode op;

    int a;
    int b;
    int c;
} IRInstruction;

typedef struct
{
    IRInstruction* data;
    Location* locations;
    int count;
    int capacity;
} IRList;

typedef struct
{
    Token name;
    int reg;
    int scope_depth;
    int isConst;
} Symbol;

typedef struct
{
    Symbol* data;
    int count;
    int capacity;
    int scope_depth;
} SymbolTable;

typedef struct
{
    Constant* data;
    int count;
    int capacity;
} ConstTable;

typedef struct
{
    Location* data;
    int count;
    int capacity;
} LocationInstructions;

#define REG_INVALID     (-1)
#define REG_POOL_MAX    256

typedef struct
{
    IRList ir;
    const char* src;
    const char* name;

    SymbolTable symbol;
    ConstTable constants;
    //LocationInstructions locations;
    int next_reg;
    int next_const;

    /* Pool de registros temporales (los de variables viven en SymbolTable). */
    int free_regs[REG_POOL_MAX];
    int free_count;
} Compiler;


void ir_init(IRList* list);
void symbols_init(SymbolTable* T);
void constants_init(ConstTable* c);
void locations_init(LocationInstructions* l);
Compiler* compiler_init(const char* src, const char* name);
int compiler_expr(Compiler* C, Expr* expr, int target);
void compiler_stmt(Compiler* C, Stmt* stmt);
void compiler_program(Compiler* C, Stmt* stmt);

/* Cantidad de registros usados (high water mark); para dimensionar el frame de la VM. */
int compiler_regs_used(const Compiler* C);

#if (defined(DEBUG) && DEBUG == 1) && (defined(COMPILER_DEBUG) && COMPILER_DEBUG == 1)
void compiler_print(Compiler* C);
#endif 

