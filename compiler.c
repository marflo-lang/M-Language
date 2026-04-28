#include "compiler.h"
#include "err.h"

#include <malloc.h>


static void ir_emit(IRList* list, IROpCode op, uint8_t a, uint8_t b, uint8_t c)
{
    if (list->count >= list->capacity)
    {
        list->capacity = list->capacity < 8 ? 8 : list->capacity * 2;
        IRInstruction* newData = realloc(list->data, sizeof(IRInstruction) * list->capacity);
        if (newData == NULL)
        {
            memoryCrash("Time to Compile");
            exit(1);
        }
        list->data = newData;
    }

    IRInstruction instr = { op, a, b, c };
    list->data[list->count++] = instr;
}

static uint32_t alloc_reg(Compiler* C)
{
    return C->next_reg++;
}

static uint32_t symbol_define(SymbolTable* T, const char* name)
{
    for (uint32_t i = 0; i < T->count; i++)
    {
        if (strcmp(T->names[i], name) == 0)
            return i;
    }

    if (T->count >= T->capacity)
    {
        T->capacity = T->capacity < 8 ? 8 : T->capacity * 2;
        const char** newNames = realloc(T->names, sizeof(char*) * T->capacity);

        if (newNames == NULL)
        {
            memoryCrash("Time to Compile");
            exit(1);
        }

        T->names = newNames;
    }

    T->names[T->count] = name;
    return T->count++;
}

static uint32_t symbols_resolve(SymbolTable* T, const char* name)
{
    for (uint32_t i = 0; i < T->count; i++)
    {
        if (strcmp(T->names[i], name), name)
        {
            return i;
        }
    }

    return -1; // Error luego
}

void compiler_init(Compiler* C)
{
    ir_init(&C->ir);
    symbols_init(&C->symbol);
    C->next_reg = 0;
}

void ir_init(IRList* list)
{
    list->data = NULL;
    list->count = 0;
    list->capacity = 0;
}

void symbols_init(SymbolTable* T)
{
    T->names = NULL;
    T->count = 0;
    T->capacity = 0;
}

int compiler_expr(Compiler* C, Expr* expr)
{
    switch (expr->expr_type)
    {
        case EXPR_LITERAL:
        {
            LiteralExpr* literal = (LiteralExpr*) expr;
            uint32_t r = alloc_reg(C);

            // Pendiente por revisar b
            ir_emit(&C->ir, IR_LOAD_CONST, r, literal->value.location.begin.offset, 0);
            break;
        }
        case EXPR_NAME:
        {
            NameExpr* nameE = (NameExpr*) expr;
            // Pendiente por revisar el name
            uint32_t slot = symbols_resolve(&C->symbol, nameE->name.location.begin.offset);

            uint32_t r = alloc_reg(C);
            ir_emit(&C->ir, IR_LOAD_VAR, r, slot, 0);
            break;
        }
        case EXPR_BINARY:
        {

            break;
        }
        case EXPR_UNARY:
        {

            break;
        }
        case EXPR_POSTFIX:
        {

            break;
        }
        case EXPR_PREFIX:
        {

            break;
        }
    }
}

void compiler_stmt(Compiler* C, Stmt* stmt)
{

}

