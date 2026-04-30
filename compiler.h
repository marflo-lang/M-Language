#pragma once
#include "ast.h"
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
    IR_MOVE,        //  A, B            R(B) = R(A) 

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

    // Comparaciones
    IR_EQ,          // A, B, C        R(A) = R(B) == R(C)
    IR_NEQ,         // A, B, C        R(A) = R(B) != R(C)
    IR_LT,          // A, B, C        R(A) = R(B) < R(C)
    IR_LTE,         // A, B, C        R(A) = R(B) <= R(C)
    IR_GT,          // A, B, C        R(A) = R(B) > R(C)
    IR_GTE,         // A, B, C        R(A) = R(B) >= R(C)

    // Control de flujo
    IR_JUMP,        // Ax                 pc += Ax
    IR_JUMP_IF_FALSE,   // A, Bx        if !A -> pc += Bx
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

    int count;
    int capacity;
} IRList;

typedef enum
{
    VAL_NAN,
    VAL_NIL,
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_BOOLEAN
} ValueType;

typedef struct
{
    ValueType type;

    union
    {
        int i;
        double f;
        bool b;

        struct
        {
            const char* chars;
            int length;
        } string;
    };
} Value;

typedef struct
{
    Token* names;
    int count;
    int capacity;
} SymbolTable;

typedef struct
{
    Value* data;
    int count;
    int capacity;
} ConstTable;

typedef struct
{
    IRList ir;
    const char* src;
    const char* name;

    SymbolTable symbol;
    ConstTable constants;
    int next_reg;
    int next_const;
} Compiler;


void ir_init(IRList* list);
void symbols_init(SymbolTable* T);
void constants_init(ConstTable* c);
void compiler_init(Compiler* C, const char* src, const char* name);
int compiler_expr(Compiler* C, Expr* expr);
void compiler_stmt(Compiler* C, Stmt* stmt);

