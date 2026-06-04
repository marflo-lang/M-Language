
#include "m.h"
#include "compiler.h"
#include "../utils/err.h"

#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <float.h>
#include <locale.h>

static void free_temp_reg(Compiler* C, int reg);

static void begin_scope(SymbolTable* T)
{
    T->scope_depth++;
}

static void end_scope(Compiler* C, SymbolTable* T)
{
    T->scope_depth--;

    while (T->count > 0 && T->data[T->count - 1].scope_depth > T->scope_depth)
    {
        int reg = T->data[T->count - 1].reg;
        T->count--;
        free_temp_reg(C, reg);
    }
}

static bool value_equals(Constant a, Constant b)
{
    if (a.type != b.type) return false;

    switch (a.type)
    {
        case C_INT:
        {
            return a.i == b.i;
        }

        case C_FLOAT:
        {
            return a.f == b.f;
        }

        case C_BOOLEAN:
        {
            return a.b == b.b;
        }

        case C_STRING:
        {
            return a.string.length == b.string.length && (strncmp(a.string.chars, b.string.chars, a.string.length) == 0);
        }

        case C_NIL:
        {
            return true;
        }

        case C_NAN:
        {
            return true;
        }

        default:
        {
            return false;
        }

    }
}

static int const_find(ConstTable* T, Constant v)
{
    for (int i = 0; i < T->count; i++)
    {
        if (value_equals(T->data[i], v))
            return i;
    }

    return -1;
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

static Constant make_int(int64_t v)
{
    Constant val = {0};
    val.type = C_INT;
    val.i = v;
    return val;
}

static Constant make_float(double v)
{
    Constant val = {0};
    val.type = C_FLOAT;
    val.f = v;
    return val;
}

static Constant make_boolean(bool v)
{
    Constant val = {0};
    val.type = C_BOOLEAN;
    val.b = v;
    return val;
}

static Constant make_string(const char* src, int offset, size_t length)
{
    Constant val = {0};
    val.type = C_STRING;
    val.string.chars = src + offset;
    val.string.length = length;
    return val;
}

static Constant make_nil()
{
    Constant val = {0};
    val.type = C_NIL;
    return val;
}

static Constant make_nan()
{
    Constant val = {0};
    val.type = C_NAN;
    return val;
}

static void copyNumberWithoutSeparators(char* out, size_t outSize, const char* start, int length)
{
    int j = 0;
    for (int i = 0; i < length; i++)
    {
        if (start[i] != '_' && (j + 1 < outSize))
        {
            out[j++] = start[i];
        }
    }
    out[j] = '\0';
}

static void parseIntegerLiteral(Compiler* C, Token t, const char* buffer, int64_t* out, int base)
{
    char* endptr;
    errno = 0;
    const char* ptr = (base == 10) ? buffer : buffer + 2;
    int64_t num = strtoll(ptr, &endptr, base);
    if (errno == ERANGE)
    {
        // Desvordamiento
        if (num == LLONG_MAX)
            compilerError("Number Overflow; The number '%.*s' is too big", C->name, t.location, t.length, &C->src[t.location.begin.offset]);
        else if (num == LLONG_MIN)
            compilerError("Number Underflow; The number '%.*s' is too small", C->name, t.location, t.length, &C->src[t.location.begin.offset]);
    }
    else if (endptr == ptr || *endptr != '\0')
        compilerError("Number Invalid; The number '%.*s' is invalid", C->name, t.location, t.length, &C->src[t.location.begin.offset]);
    else
        *out = num;
}

static Constant token_to_value(Compiler* C, Token t)
{
    switch (t.type)
    {
        case M_V_INT:
        {
            char buffer[64];
            int64_t num = 0;
            copyNumberWithoutSeparators(buffer, sizeof(buffer), C->src + t.location.begin.offset, t.length);
            if (*buffer == '0' && (buffer[1] == 'x' || buffer[1] == 'X'))
            {
                // Hexadecimal
                parseIntegerLiteral(C, t, buffer, &num, 16);
                return make_int(num);
            }
            else if (*buffer == '0' && (buffer[1] == 'o' || buffer[1] == 'O'))
            {
                // Octal
                parseIntegerLiteral(C, t, buffer, &num, 8);
                return make_int(num);
            }
            else if ((*buffer == '0' && (buffer[1] == 'b' || buffer[1] == 'B')))
            {
                // Binario
                parseIntegerLiteral(C, t, buffer, &num, 2);
                return make_int(num);
            }
            else
            {
                // Decimal
                parseIntegerLiteral(C, t, buffer, &num, 10);
                return make_int(num);
            }

        }
        case M_V_FLOAT:
        {
            char buffer[64];
            copyNumberWithoutSeparators(buffer, sizeof(buffer), C->src + t.location.begin.offset, t.length);
            char* endptr;
            errno = 0;

            double num = strtod(buffer, &endptr);
            if (errno == ERANGE)
                compilerError("Number Overflow; The number '%.*s' breaks the limits", C->name, t.location, t.length, &C->src[t.location.begin.offset]);
            else if (endptr == buffer || *endptr != '\0')
                compilerError("Number Invalid; The number '%.*s' is invalid", C->name, t.location, t.length, &C->src[t.location.begin.offset]);
            else
                return make_float(num);

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
            return make_nil();
        }

        case M_V_MALFORMED_NUMBER:
        {
            compilerError("Expected a primitive type, but got 'Malformed Number'", C->name, t.location);
        }

        case M_V_UNFINISHED_STRING:
        {
            compilerError("Expected a primitive type, but got 'Unfinished String'", C->name, t.location);
        }

        default:
            return make_nan();
    }
}

static void ir_emit(IRList* list, IROpCode op, int a, int b, int c, Location loc)
{
    if (list->count >= list->capacity)
    {
        list->capacity = list->capacity < 8 ? 8 : list->capacity * 2;
        IRInstruction* newData = realloc(list->data, sizeof(IRInstruction) * list->capacity);
        Location* newLocations = realloc(list->locations, sizeof(Location) * list->capacity);
        if (newData == NULL || newLocations == NULL)
        {
            memoryCrash("Time to Compile");
            exit(1);
        }
        list->data = newData;
        list->locations = newLocations;
    }

    IRInstruction instr = { op, a, b, c };
    list->data[list->count] = instr;
    list->locations[list->count] = loc;
    list->count++;
}

static int const_add(ConstTable* T, Constant v)
{
    int existing = const_find(T, v);
    if (existing != -1) return existing;

    if (T->count >= T->capacity)
    {
        T->capacity = T->capacity < 8 ? 8 : T->capacity * 2;
        Constant* newData = realloc(T->data, sizeof(Constant) * T->capacity);
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

/* True si el registro pertenece a una variable/const activa en la tabla de símbolos. */
static bool reg_is_local(/*const*/ Compiler* C, int reg)
{
    if (reg < 0)
        return false;

    for (int i = 0; i < C->symbol.count; i++)
    {
        if (C->symbol.data[i].reg == reg)
            return true;
    }

    return false;
}

static int alloc_reg(Compiler* C)
{
    if (C->free_count > 0)
        return C->free_regs[--C->free_count];

    if (C->next_reg >= REG_POOL_MAX)
    {
        compilerError("Register limit exceeded (%d)", C->name, (Location) { 0 }, REG_POOL_MAX);
    }

    return C->next_reg++;
}

/* Libera un registro temporal; nunca libera registros de variables. */
static void free_temp_reg(Compiler* C, int reg)
{
    if (reg < 0 || reg_is_local(C, reg))
        return;

    if (C->free_count >= REG_POOL_MAX)
        return;

    C->free_regs[C->free_count++] = reg;
}

/* Libera operandos temporales conservando resultado y variables. */
static void free_expr_reg(Compiler* C, int reg, int keep_a, int keep_b)
{
    if (reg < 0)
        return;

    if (reg != keep_a && reg != keep_b)
        free_temp_reg(C, reg);
}

static int symbol_define(Compiler* C, SymbolTable* T, Token name, bool isConst, const char* src)
{
    int reg = alloc_reg(C);

    if (T->count >= T->capacity)
    {
        T->capacity = T->capacity < 8 ? 8 : T->capacity * 2;
        Symbol* newData = realloc(T->data, sizeof(Symbol) * T->capacity);

        if (newData == NULL)
        {
            memoryCrash("Time to Compile");
            exit(1);
        }

        T->data = newData;
    }

    T->data[T->count++] = (Symbol) {.name = name, .reg = reg, .scope_depth = T->scope_depth, .isConst = isConst};
    return reg;
}

static Symbol* symbols_resolve(SymbolTable* T, Token name, const char* src)
{
    for (int i = T->count - 1; i >= 0; i--)
    {
        if (compare_tokens(T->data[i].name, name, src))
        {
            return &T->data[i];
        }
    }

    return NULL; // Error luego
}

static int ir_emit_jump(IRList* ir, IROpCode op, int a, Location loc)
{
    int pos = ir->count;

    ir_emit(ir, op, a, -1, 0, loc);

    return pos;
}

static void ir_patch(IRList* ir, int pos, int target)
{
    ir->data[pos].b = target;
}

static int ir_current(IRList* ir)
{
    return ir->count;
}

Compiler* compiler_init(const char* src, const char* name)
{
    Compiler* C = malloc(sizeof(Compiler));

    if (C == NULL)
    {
        memoryCrash("Compiler Time");
        exit(1);
    }

    ir_init(&C->ir);
    symbols_init(&C->symbol);
    constants_init(&C->constants);
    C->src = src;
    C->name = name;
    C->next_reg = 0;
    C->next_const = 0;
    C->free_count = 0;

    return C;
}

int compiler_regs_used(/*const*/ Compiler* C)
{
    return C->next_reg;
}

void ir_init(IRList* list)
{
    list->data = NULL;
    list->locations = NULL;
    list->count = 0;
    list->capacity = 0;
}

void symbols_init(SymbolTable* T)
{
    T->data = NULL;
    T->count = 0;
    T->capacity = 0;
}

void constants_init(ConstTable* c)
{
    c->data = NULL;
    c->capacity = 0;
    c->count = 0;
}

void locations_init(LocationInstructions* l)
{
    l->data = NULL;
    l->count = 0;
    l->capacity = 0;
}

int compiler_expr(Compiler* C, Expr* expr, int target)
{
    switch (expr->expr_type)
    {
        case EXPR_LITERAL:
        {
            LiteralExpr* literal = (LiteralExpr*) expr;
            int r = (target > -1) ? target : alloc_reg(C);

            Constant v = token_to_value(C, literal->value);

            int k = const_add(&C->constants, v);

            ir_emit(&C->ir, IR_LOAD_CONST, r, k, 0, literal->expr.base.location);
            return r;
        }
        case EXPR_LIST:
        {
            LiteralListExpr* list = (LiteralListExpr*) expr;

            int r = (target > -1) ? target : alloc_reg(C);
            int capacity = -1;

            if (list->capacity != NULL)
                capacity = compiler_expr(C, list->capacity, -1);

            ir_emit(&C->ir, IR_CREATE_LIST, r, capacity, list->fixed, list->expr.base.location);
            free_expr_reg(C, capacity, r, REG_INVALID);

            for (int i = 0; i < list->count; i++)
            {
                int element = compiler_expr(C, list->elements[i], -1);
                ir_emit(C, IR_PUSH_LIST, r, element, 0, list->elements[i]->base.location);
                free_expr_reg(C, element, r, REG_INVALID);
            }

            return r;
        }
        case EXPR_DICT:
        {
            LiteralDictExpr* dict = (LiteralDictExpr*) expr;
            int r = (target > -1) ? target : alloc_reg(C);
            int count = dict->count;
            
            ir_emit(&C->ir, IR_CREATE_DICT, r, count, dict->fixed, dict->expr.base.location);

            for (int i = 0; i < count; i++)
            {
                int key = compiler_expr(C, dict->entries[i].key, -1);
                int value = compiler_expr(C, dict->entries[i].value, -1);
                ir_emit(&C->ir, IR_SET_INDEX, r, key, value, locationCPos(dict->entries[i].key->base.location.begin, dict->entries[i].value->base.location.end));
                free_expr_reg(C, key, r, REG_INVALID);
                free_expr_reg(C, value, r, REG_INVALID);
            }

            return r;
        }
        case EXPR_NAME:
        {
            NameExpr* nameE = (NameExpr*) expr;
            // Pendiente por revisar el name
            Symbol* symbol  = symbols_resolve(&C->symbol, nameE->name, C->src);

            if (symbol == NULL)
                compilerError("Variable '%.*s' has not yet been declared. Consider declaring it before using it", C->name, nameE->expr.base.location, nameE->name.length, &C->src[nameE->name.location.begin.offset]);

            if (target > -1)
            {
                ir_emit(&C->ir, IR_MOVE, target, symbol->reg, 0, nameE->expr.base.location);
                return target;
            }

            return symbol->reg;
        }
        case EXPR_INDEX:
        {
            IndexExpr* index = (IndexExpr*) expr;

            int reg = (target > -1) ? target : alloc_reg(C);
            int coll = compiler_expr(C, index->collection, -1);
            int ind = compiler_expr(C, index->index, -1);

            ir_emit(&C->ir, IR_GET_INDEX, reg, coll, ind, index->expr.base.location);
            free_expr_reg(C, coll, reg, REG_INVALID);
            free_expr_reg(C, ind, reg, REG_INVALID);

            return reg;
        }
        case EXPR_BINARY:
        {
            BinaryExpr* binary = (BinaryExpr*) expr;

            int r = (target > -1) ? target : alloc_reg(C);
            int left = compiler_expr(C, binary->left, -1);
            int right = compiler_expr(C, binary->right, -1);

            IROpCode op;

            switch (binary->op.type)
            {
                case M_PLUS: op = IR_ADD; break;
                case M_MINUS: op = IR_SUB; break;
                case M_STAR: op = IR_MUL; break;
                case M_SLASH: op = IR_DIV; break;
                case M_FLOOR_DIV: op = IR_IDIV; break;
                case M_CONCAT: op = IR_CONCAT; break;
                case M_MOD: op = IR_MOD; break;
                case M_POW: op = IR_POW; break;
                case M_LT: op = IR_LT; break;
                case M_LTE: op = IR_LTE; break;
                case M_GT: op = IR_GT; break;
                case M_GTE: op = IR_GTE; break;
                case M_EQ: op = IR_EQ; break;
                case M_NEQ: op = IR_NEQ; break;
                case M_OR: op = IR_OR; break;
                case M_AND: op = IR_AND; break;
                default: op = IR_ADD; break; // temporal
            }

            ir_emit(&C->ir, op, r, left, right, binary->expr.base.location);
            free_expr_reg(C, left, r, REG_INVALID);
            free_expr_reg(C, right, r, REG_INVALID);
            return r;
        }
        case EXPR_UNARY:
        {
            UnaryExpr* unary = (UnaryExpr*) expr;

            int r = (target > -1) ? target : alloc_reg(C);
            int right = compiler_expr(C, unary->right, -1);
            IROpCode op;

            switch (unary->op.type)
            {
                case M_MINUS: op = IR_UNM; break;
                case M_NOT: op = IR_NOT; break;
                default: op = IR_UNM; break; // temporal
            }

            ir_emit(&C->ir, op, r, right, 0, unary->expr.base.location);
            free_expr_reg(C, right, r, REG_INVALID);
            return r;
        }
        case EXPR_POSTFIX:
        case EXPR_PREFIX:
        {
            FixExpr* fix = (FixExpr*) expr;
            if (!(fix->target->expr_type == EXPR_NAME))
            {
                compilerError("Expected a variable name with '%s', but got '%.*s'",
                    C->name, 
                    fix->target->base.location, 
                    fix->isPre ? "Prefix expression" : "Postfix expression",
                    (fix->target->base.location.end.offset - fix->target->base.location.begin.offset),
                    &C->src[fix->target->base.location.begin.offset]
                );
            }

            NameExpr* name = (NameExpr*) fix->target;
            Symbol* symbol = symbols_resolve(&C->symbol, name->name, C->src);

            if (symbol == NULL)
                compilerError("Variable '%.*s' has not yet been declared. Consider declaring it before using it", C->name, name->expr.base.location, name->name.length, &C->src[name->name.location.begin.offset]);

            if (symbol->isConst)
                compilerError("Const '%.*s' cannot be modified", C->name, fix->expr.base.location, name->name.length, &C->src[name->name.location.begin.offset]);


            int reg = symbol->reg;
            int old = -1;

            if (!fix->isPre)
            {
                old = (target > -1) ? target : alloc_reg(C);
                ir_emit(&C->ir, IR_MOVE, old, reg, 0, fix->target->base.location);
            }

            int one = alloc_reg(C);
            Constant v = make_int(1);
            int k = const_add(&C->constants, v);
            ir_emit(&C->ir, IR_LOAD_CONST, one, k, 0, fix->op.location); // Pendiente revisar comportamiento

            if (fix->op.type == M_INC)
            {
                ir_emit(&C->ir, IR_ADD, reg, reg, one, fix->expr.base.location);
            }
            else
            {
                ir_emit(&C->ir, IR_SUB, reg, reg, one, fix->expr.base.location);
            }

            free_temp_reg(C, one);

            if (fix->isPre)
            {
                if (target > -1 && target != reg)
                {
                    ir_emit(&C->ir, IR_MOVE, target, reg, 0, fix->target->base.location);
                    return target;
                }
                return reg;
            }
            else
                return old;
        }
        case EXPR_ERROR:
        {
            ErrorExpr* error = (ErrorExpr*) expr;
            compilerError(error->message, C->name, error->token.location, error->token.type == M_V_MALFORMED_NUMBER ? "Malformed Number" : "Malformed String");
        }
    }
}

static void compiler_assign(Compiler* C, Expr* lvalue, Expr* rvalue, Location lastLoc)
{
    NameExpr* name = (NameExpr*) lvalue;
    Symbol* symbol = symbols_resolve(&C->symbol, name->name, C->src);

    if (symbol == NULL)
        compilerError("Variable '%.*s' has not yet been declared. Consider declaring it before using it", C->name, name->expr.base.location, name->name.length, &C->src[name->expr.base.location.begin.offset]);

    if (symbol->isConst)
        compilerError("Const '%.*s' cannot be modified", C->name, name->expr.base.location, name->name.length, &C->src[name->expr.base.location.begin.offset]);

    int reg = symbol->reg;

    if (rvalue != NULL)
    {
        compiler_expr(C, rvalue, reg);
    }
    else
    {
        Constant v = make_nan();
        int k = const_add(&C->constants, v);
        ir_emit(&C->ir, IR_LOAD_CONST, reg, k, 0, lastLoc);
    }
}

static void compiler_index_assign(Compiler* C, Expr* lvalue, Expr* rvalue)
{
    IndexExpr* index = (IndexExpr*) lvalue;

    if (!(index->collection->expr_type == EXPR_NAME))
        compilerError("An internal error occurred in the node type of the collection", C->name, index->collection->base.location);

    NameExpr* name = (NameExpr*) index->collection;

    Symbol* symbol = symbols_resolve(&C->symbol, name->name, C->src);

    if (symbol == NULL)
        compilerError("Variable '%.*s' has not yet been declared. Consider declaring it before using it", C->name, name->expr.base.location, name->name.length, &C->src[name->expr.base.location.begin.offset]);

    int reg = symbol->reg;
    int ind = compiler_expr(C, index->index, -1);
    int value = compiler_expr(C, rvalue, -1);

    ir_emit(&C->ir, IR_SET_INDEX, reg, ind, value, index->expr.base.location);
    free_expr_reg(C, ind, reg, REG_INVALID);
    free_expr_reg(C, value, reg, REG_INVALID);
}

void compiler_stmt(Compiler* C, Stmt* stmt)
{
    switch (stmt->stmt_type)
    {
        case STMT_VAR:
        {
            StmtVar* var = (StmtVar*) stmt;

            if (var->isConst && var->valuesCount < var->namesCount)
                compilerError("All const assign must have a value, got '%d' names and '%d' values", C->name, var->stmt.base.location, var->namesCount, var->valuesCount);

            for (int i = 0; i < var->namesCount; i++)
            {
                int reg = symbol_define(C, &C->symbol, var->names[i], var->isConst, C->src);

                if (i < var->valuesCount)
                {
                    compiler_expr(C, var->values[i], reg);
                }
                else
                {
                    Constant v = make_nan();
                    int k = const_add(&C->constants, v);
                    ir_emit(&C->ir, IR_LOAD_CONST, reg, k, 0, var->values[var->valuesCount - 1]->base.location); // NaN, pendiente mejorar
                }
            }

            return;
        }

        case STMT_BLOCK:
        {
            StmtBlock* block = (StmtBlock*) stmt;
            begin_scope(&C->symbol);

            for (int i = 0; i < block->count; i++)
                compiler_stmt(C, block->statements[i]);

            end_scope(C, &C->symbol);
            return;
        }

        case STMT_IF:
        {
            StmtIf* ifstmt = (StmtIf*) stmt;
            int cond = compiler_expr(C, ifstmt->condition, -1);
            int jump_if_false = ir_emit_jump(&C->ir, IR_JUMP_IF_FALSE, cond, ifstmt->condition->base.location);

            free_temp_reg(C, cond);
            compiler_stmt(C, ifstmt->ifBranch);

            int jump_end = 0;

            if (ifstmt->elseBranch != NULL)
                jump_end = ir_emit_jump(&C->ir, IR_JUMP, 0, locationCPos(ifstmt->ifBranch->base.location.end, ifstmt->ifBranch->base.location.end));

            int else_pos = ir_current(&C->ir);
            ir_patch(&C->ir, jump_if_false, else_pos);

            if (ifstmt->elseBranch != NULL)
            {
                compiler_stmt(C, ifstmt->elseBranch);
            }

            int end_pos = ir_current(&C->ir);
            if (ifstmt->elseBranch != NULL)
                ir_patch(&C->ir, jump_end, end_pos);
            return;
        }

        case STMT_WHILE:
        {
            StmtWhile* whileStmt = (StmtWhile*) stmt;

            int current = ir_current(&C->ir);
            int cond = compiler_expr(C, whileStmt->condition, -1);
            int jump_if_false = ir_emit_jump(&C->ir, IR_JUMP_IF_FALSE, cond, whileStmt->condition->base.location);
            free_temp_reg(C, cond);
            compiler_stmt(C, whileStmt->loopBranch);
            ir_emit(&C->ir, IR_JUMP, 0, current, 0, whileStmt->loopBranch->base.location);

            int nextR = ir_current(&C->ir);
            ir_patch(&C->ir, jump_if_false, nextR);

            return;
        }

        case STMT_FOR_NUMERIC:
        {
            StmtForNumeric* stmtForN = (StmtForNumeric*) stmt;
            begin_scope(&C->symbol);
            int init = ir_current(&C->ir);
            compiler_stmt(C, stmtForN->from);
            
            int current = ir_current(&C->ir);
            int cond = compiler_expr(C, stmtForN->to, -1);
            int jump_if_false = ir_emit_jump(&C->ir, IR_JUMP_IF_FALSE, cond, stmtForN->to->base.location);
            free_temp_reg(C, cond);
            compiler_stmt(C, stmtForN->loopBranch);

            if (stmtForN->step != NULL)
                compiler_stmt(C, stmtForN->step);
            else
            {
                int one = alloc_reg(C);
                Constant v = make_int(1);
                int k = const_add(&C->constants, v);
                ir_emit(&C->ir, IR_LOAD_CONST, one, k, 0, locationCPos(stmtForN->to->base.location.end, stmtForN->to->base.location.end));
                IRInstruction ir = C->ir.data[init];
                ir_emit(&C->ir, IR_ADD, ir.a, ir.a, one, locationCPos(stmtForN->to->base.location.end, stmtForN->to->base.location.end));
                free_temp_reg(C, one);
            }

            ir_emit(&C->ir, IR_JUMP, 0, current, 0, locationCPos(stmtForN->loopBranch->base.location.end, stmtForN->loopBranch->base.location.end));
            int nextR = ir_current(&C->ir);
            ir_patch(&C->ir, jump_if_false, nextR);
            end_scope(C, &C->symbol);

            return;
        }

        case STMT_EXPR:
        {
            StmtExpr* stmtE = (StmtExpr*) stmt;
            int r = compiler_expr(C, stmtE->expr, -1);
            free_temp_reg(C, r);
            return;
        }

        case STMT_ASSING:
        {
            StmtAssign* assign = (StmtAssign*) stmt;
            for (int i = 0; i < assign->nameCount; i++)
            {
                switch (assign->names[i]->expr_type)
                {
                    case EXPR_NAME:
                        compiler_assign(C, assign->names[i], (i < assign->valueCount) ? assign->values[i] : NULL, assign->values[assign->valueCount - 1]->base.location);
                        break;
                    case EXPR_INDEX:
                        compiler_index_assign(C, assign->names[i], (i < assign->valueCount) ? assign->values[i] : NULL);
                        break;
                    default:
                        compilerError("Unknown assignment type", C->name, assign->names[i]->base.location);
                        break;
                }
            }
            return;
        }

        case STMT_COMPOUND_ASSING:
        {
            StmtCompoundAssing* compound = (StmtCompoundAssing*) stmt;
            
            int value = compiler_expr(C, compound->value, -1);
            Symbol* symbol = symbols_resolve(&C->symbol, ((NameExpr*)compound->target)->name, C->src);

            if (symbol == NULL)
                compilerError("Variable '%.*s' has not yet been declared. Consider declaring it before using it", C->name, compound->target->base.location, ((NameExpr*)compound->target)->name.length, &C->src[compound->target->base.location.begin.offset]);

            if (symbol->isConst)
                compilerError("Const '%.*s' cannot be modified", C->name, compound->stmt.base.location, ((NameExpr*)compound->target)->name.length, &C->src[compound->target->base.location.begin.offset]);

            int target = symbol->reg;
            IROpCode op;

            switch (compound->op.type)
            {
            case M_PLUS_ASSING: op = IR_ADD; break;
            case M_MINUS_ASSING: op = IR_SUB; break;
            case M_STAR_ASSING: op = IR_MUL; break;
            case M_SLASH_ASSING: op = IR_DIV; break;
            case M_FLOOR_DIV_ASSING: op = IR_IDIV; break;
            case M_CONCAT_ASSING: op = IR_CONCAT; break;
            case M_POW_ASSING: op = IR_POW; break;
            case M_MOD_ASSING: op = IR_MOD; break;
            default: op = IR_ADD; break; // temporal
            }
            ir_emit(&C->ir, op, target, target, value, compound->value->base.location);
            free_expr_reg(C, value, target, REG_INVALID);

            return;
        }

        case STMT_ERROR:
        {
            StmtError* error = (StmtError*) stmt;
            
            compilerError(error->message, C->name, stmt->base.location, (error->nodo->location.end.offset - error->nodo->location.begin.offset), &C->src[error->nodo->location.begin.offset]);
        }
    }
}

void compiler_program(Compiler* C, Stmt* stmt)
{
    setlocale(LC_NUMERIC, "C");
    compiler_stmt(C, stmt);
    ir_emit(&C->ir, IR_HALT, 0, 0, 0, locationCPos(stmt->base.location.end, stmt->base.location.end));
}

#if (defined(DEBUG) && DEBUG == 1) && (defined(COMPILER_DEBUG) && COMPILER_DEBUG == 1)

static void print_ir(Compiler* C, int i)
{
    IRInstruction ir = C->ir.data[i];
    printf("L%d - ", i);
    printf("%d: ", C->ir.locations[i].begin.line);
    if (ir.op == IR_LOAD_CONST)
    {
        Constant v = C->constants.data[ir.b];
        printf("IR_LOADK R%d K%d", ir.a, ir.b);
        if (v.type == C_INT)
            printf(" [%d]", v.i);
        else if (v.type == C_FLOAT)
            printf(" [%f]", v.f);
        else if (v.type == C_BOOLEAN)
            printf(" [%s]", v.b == true ? "true" : "false");
        else if (v.type == C_NAN)
            printf(" [NaN]");
        else if (v.type == C_NIL)
            printf(" [nil]");
        else if (v.type == C_STRING)
            printf(" [%.*s]", v.string.length, v.string.chars);
    }
    else if (ir.op == IR_LOAD_VAR)
        printf("IR_LOADV R%d S%d", ir.a, ir.b);
    else if (ir.op == IR_STORE_VAR)
        printf("IR_STORE S%d R%d", ir.a, ir.b);
    else if (ir.op == IR_MOVE)
        printf("IR_MOVE R%d R%d", ir.a, ir.b);
    else if (ir.op == IR_ADD)
        printf("IR_ADD R%d R%d R%d", ir.a, ir.b, ir.c);
    else if (ir.op == IR_SUB)
        printf("IR_SUB R%d R%d R%d", ir.a, ir.b, ir.c);
    else if (ir.op == IR_MUL)
        printf("IR_MUL R%d R%d R%d", ir.a, ir.b, ir.c);
    else if (ir.op == IR_DIV)
        printf("IR_DIV R%d R%d R%d", ir.a, ir.b, ir.c);
    else if (ir.op == IR_IDIV)
        printf("IR_IDIV R%d R%d R%d", ir.a, ir.b, ir.c);
    else if (ir.op == IR_MOD)
        printf("IR_MOD R%d R%d R%d", ir.a, ir.b, ir.c);
    else if (ir.op == IR_POW)
        printf("IR_POW R%d R%d R%d", ir.a, ir.b, ir.c);
    else if (ir.op == IR_CONCAT)
        printf("IR_CONCAT R%d R%d R%d", ir.a, ir.b, ir.c);
    else if (ir.op == IR_UNM)
        printf("IR_UNM R%d R%d", ir.a, ir.b);
    else if (ir.op == IR_NOT)
        printf("IR_NOT R%d R%d", ir.a, ir.b);
    else if (ir.op == IR_OR)
        printf("IR_OR R%d R%d R%d", ir.a, ir.b, ir.c);
    else if (ir.op == IR_AND)
        printf("IR_AND R%d R%d R%d", ir.a, ir.b, ir.c);
    else if (ir.op == IR_EQ)
        printf("IR_EQ R%d R%d R%d", ir.a, ir.b, ir.c);
    else if (ir.op == IR_NEQ)
        printf("IR_NEQ R%d R%d R%d", ir.a, ir.b, ir.c);
    else if (ir.op == IR_LT)
        printf("IR_LT R%d R%d R%d", ir.a, ir.b, ir.c);
    else if (ir.op == IR_LTE)
        printf("IR_LTE R%d R%d R%d", ir.a, ir.b, ir.c);
    else if (ir.op == IR_GT)
        printf("IR_GT R%d R%d R%d", ir.a, ir.b, ir.c);
    else if (ir.op == IR_GTE)
        printf("IR_GTE R%d R%d R%d", ir.a, ir.b, ir.c);
    else if (ir.op == IR_CREATE_LIST)
        printf("IR_CREATE_LIST R%d R%d %s", ir.a, ir.b, (ir.c == true) ? "true" : "false");
    else if (ir.op == IR_SET_INDEX)
        printf("IR_SET_INDEX R%d %d R%d", ir.a, ir.b, ir.c);
    else if (ir.op == IR_PUSH_LIST)
    printf("IR_PUSH_LIST R%d R%d", ir.a, ir.b);
    else if (ir.op == IR_GET_INDEX)
    printf("IR_GET_INDEX R%d R%d R%d", ir.a, ir.b, ir.c);
    else if (ir.op == IR_CREATE_DICT)
    printf("IR_CREATE_DICT R%d R%d R%d", ir.a, ir.b, ir.c);
    else if (ir.op == IR_JUMP)
        printf("IR_JUMP L%d", ir.b);
    else if (ir.op == IR_JUMP_IF_FALSE)
        printf("IR_JUMP_IF_FALSE R%d L%d", ir.a, ir.b);
    else if (ir.op == IR_HALT)
        printf("IR_HALT");
    else
        printf("Invalid op '%d'", ir.op);
    printf("\n");

}

void compiler_print(Compiler* C)
{
    printf("===== COMPILER DEBUG =====\n");
    printf("----- IR Instructions -----\n");
    for (int i = 0; i < C->ir.count; i++)
    {
        print_ir(C, i);
    }
    printf("===== END COMPILER DEBUG =====\n");

}

#endif 



