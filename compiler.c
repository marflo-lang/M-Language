#include "compiler.h"
#include "err.h"

#include <malloc.h>
#include <string.h>

static char* getLexeme(Compiler* C, Token t)
{
    //return strndup();
}

static bool compare_tokens(Token a, Token b, const char* src)
{
    if (a.length != b.length) return false;

    return strncmp(
        src + a.location.begin.offset,
        src + b.location.begin.offset,
        a.length
    ) == 0;
}

static Value make_int(int v)
{
    Value val;
    val.type = VAL_INT;
    val.i = v;
    return val;
}

static Value make_float(double v)
{
    Value val;
    val.type = VAL_FLOAT;
    val.f = v;
    return val;
}

static Value make_boolean(bool v)
{
    Value val;
    val.type = VAL_BOOLEAN;
    val.b = v;
    return val;
}

static Value make_string(const char* src, int offset, int length)
{
    Value val;
    val.type = VAL_STRING;
    val.string.chars = src + offset;
    val.string.length = length;
    return val;
}

static Value make_nil()
{
    Value val;
    val.type = VAL_NIL;
    return val;
}

static Value make_nan()
{
    Value val;
    val.type = VAL_NAN;
    return val;
}

static Value token_to_value(Compiler* C, Token t)
{
    switch (t.type)
    {
        case M_V_INT:
        {
            char buffer[64];
            int length = t.length;

            if (length >= 64) length = 63;

            memcpy(buffer, C->src+ t.location.begin.offset, length);
            buffer[length] = '\0';

            return make_int(atoi(buffer));
        }
        case M_V_FLOAT:
        {
            char buffer[64];
            int length = t.length;

            if (length >= 64) length = 63;

            memcpy(buffer, C->src + t.location.begin.offset, length);
            buffer[length] = '\0';

            return make_float(atof(buffer));
        }
        case M_V_STRING:
        {
            return make_string(C->src, t.location.begin.offset + 1, t.length - 2);
        }
        case M_V_TRUE:
        {
            return make_boolean(true);
        }
        case M_V_FALSE:
        {
            return make_boolean(false);
        }
        case M_V_NIL:
        {
            make_nil();
        }

        default:
            return make_nan();
    }
}

static void ir_emit(IRList* list, IROpCode op, int a, int b, int c)
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

static int const_add(ConstTable* T, Value v)
{
    if (T->count >= T->capacity)
    {
        T->capacity = T->capacity < 8 ? 8 : T->capacity * 2;
        Value* newData = realloc(T->data, sizeof(Value) * T->capacity);
        if (newData == NULL)
        {
            memoryCrash("Compile time");
            exit(1);
        }

        T->data = newData;
    }

    T->data[T->count] = v;
    return T->count++;
}

static int alloc_reg(Compiler* C)
{
    return C->next_reg++;
}

static int symbol_define(SymbolTable* T, Token name, const char* src)
{
    for (int i = 0; i < T->count; i++)
    {
        if (compare_tokens(T->names[i], name, src))
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

static int symbols_resolve(SymbolTable* T, Token name, const char* src)
{
    for (int i = 0; i < T->count; i++)
    {
        if (compare_tokens(T->names[i], name, src))
        {
            return i;
        }
    }

    return -1; // Error luego
}

void compiler_init(Compiler* C, const char* src, const char* name)
{
    ir_init(&C->ir);
    symbols_init(&C->symbol);
    constants_init(&C->constants);
    C->src = src;
    C->name = name;
    C->next_reg = 0;
    C->next_const = 0;
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

void constants_init(ConstTable* c)
{
    c->data = NULL;
    c->capacity = 0;
    c->count = 0;
}

int compiler_expr(Compiler* C, Expr* expr)
{
    switch (expr->expr_type)
    {
        case EXPR_LITERAL:
        {
            LiteralExpr* literal = (LiteralExpr*) expr;
            int r = alloc_reg(C);

            Value v = token_to_value(C, literal->value);

            int k = const_add(&C->constants, v);

            ir_emit(&C->ir, IR_LOAD_CONST, r, k, 0);
            return r;
        }
        case EXPR_NAME:
        {
            NameExpr* nameE = (NameExpr*) expr;
            // Pendiente por revisar el name
            int slot = symbols_resolve(&C->symbol, nameE->name, C->src);

            int r = alloc_reg(C);
            ir_emit(&C->ir, IR_LOAD_VAR, r, slot, 0);
            return r;
        }
        case EXPR_BINARY:
        {
            BinaryExpr* binary = (BinaryExpr*) expr;

            int left = compiler_expr(C, binary->left);
            int right = compiler_expr(C, binary->right);

            int r = alloc_reg(C);

            IROpCode op;

            switch (binary->op.type)
            {
                case M_PLUS: op = IR_ADD; break;
                case M_MINUS: op = IR_SUB; break;
                case M_STAR: op = IR_MUL; break;
                case M_SLASH: op = IR_DIV; break;
                case M_FLOOR_DIV: op = IR_IDIV; break;
                case M_MOD: op = IR_MOD; break;
                case M_POW: op = IR_POW; break;
                default: op = IR_ADD; break; // temporal
            }

            ir_emit(&C->ir, op, r, left, right);
            return r;
        }
        case EXPR_UNARY:
        {
            UnaryExpr* unary = (UnaryExpr*) expr;

            int right = compiler_expr(C, unary->right);
            int r = alloc_reg(C);

            IROpCode op;

            switch (unary->op.type)
            {
                case M_MINUS: op = IR_SUB; break; // temporal
                //case M_NOT: op = 
                default: op = IR_SUB; break; // temporal
            }

            ir_emit(&C->ir, op, r, 0, 0);
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

