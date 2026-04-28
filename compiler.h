#pragma once
#include "ast.h"
#include <stdint.h>

typedef enum
{
    // Carga/Movimiento
    IR_LOAD_CONST,
    IR_LOAD_VAR,
    IR_STORE_VAR,
    IR_MOVE_VAR,

    // Operaciones Aritmeticas
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_IDIV,
    IR_MOD,
    IR_POW,

    // Comparaciones
    IR_EQ,
    IR_NEQ,
    IR_LT,
    IR_LTE,
    IR_GT,
    IR_GTE,

    // Control de flujo
    IR_JUMP,
    IR_JUMP_IF_FALSE,
} IROpCode;

typedef struct
{
    IROpCode op;

    uint8_t a;
    uint8_t b;
    uint8_t c;
} IRInstruction;

typedef struct
{
    IRInstruction* data;

    uint32_t count;
    uint32_t capacity;
} IRList;

typedef struct
{
    const char** names;
    uint32_t count;
    uint32_t capacity;
} SymbolTable;

typedef struct
{
    IRList ir;

    SymbolTable symbol;
    uint32_t next_reg;
} Compiler;


void ir_init(IRList* list);
void symbols_init(SymbolTable* T);
void compiler_init(Compiler* C);
int compiler_expr(Compiler* C, Expr* expr);
void compiler_stmt(Compiler* C, Stmt* stmt);

