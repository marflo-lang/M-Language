#include "compiler.h"
#include "err.h"
#include "m.h"

#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

static char* getLexeme(Compiler* C, Token t)
{
    //return strndup();
}

static void begin_scope(SymbolTable* T)
{
    T->scope_depth++;
}

static void end_scope(SymbolTable* T)
{
    T->scope_depth--;

    while (T->count > 0 && T->data[T->count - 1].scope_depth > T->scope_depth)
        T->count--;
}

static bool value_equals(Value a, Value b)
{
    if (a.type != b.type) return false;

    switch (a.type)
    {
        case VAL_INT:
        {
            return a.i == b.i;
        }

        case VAL_FLOAT:
        {
            return a.f == b.f;
        }

        case VAL_BOOLEAN:
        {
            return a.b == b.b;
        }

        case VAL_STRING:
        {
            return a.string.length == b.string.length && (strncmp(a.string.chars, b.string.chars, a.string.length) == 0);
        }

        case VAL_NIL:
        {
            return true;
        }

        case VAL_NAN:
        {
            return true;
        }

    }
}

static int const_find(ConstTable* T, Value v)
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
            setlocale(LC_NUMERIC, "C");
            char buffer[64];
            int length = t.length;

            if (length >= 64) length = 63;

            memcpy(buffer, C->src + t.location.begin.offset, length);
            buffer[length] = '\0';
            
            //printf("===========buffer = '%f'=============\n", atof(buffer));
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

static int const_add(ConstTable* T, Value v)
{
    int existing = const_find(T, v);
    if (existing != -1) return existing;

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

static int symbol_define(SymbolTable* T, Token name, bool isConst, const char* src)
{
    int slot = T->count;

    //for (int i = 0; i < T->count; i++)
    //{
    //    if (compare_tokens(T->names[i], name, src))
    //        return i;
    //}

    if (T->count >= T->capacity)
    {
        T->capacity = T->capacity < 8 ? 8 : T->capacity * 2;
        const char** newData = realloc(T->data, sizeof(char*) * T->capacity);

        if (newData == NULL)
        {
            memoryCrash("Time to Compile");
            exit(1);
        }

        T->data = newData;
    }

    T->data[T->count++] = (Symbol) {.name = name, .slot = slot, .scope_depth = T->scope_depth, .isConst = isConst};
    return slot;
}

static Symbol* symbols_resolve(SymbolTable* T, Token name, const char* src)
{
    for (int i = 0; i < T->count; i++)
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
    //locations_init(&C->locations);
    C->src = src;
    C->name = name;
    C->next_reg = 0;
    C->next_const = 0;

    return C;
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

            ir_emit(&C->ir, IR_LOAD_CONST, r, k, 0, literal->expr.base.location);
            return r;
        }
        case EXPR_NAME:
        {
            NameExpr* nameE = (NameExpr*) expr;
            // Pendiente por revisar el name
            Symbol* symbol  = symbols_resolve(&C->symbol, nameE->name, C->src);

            if (symbol == NULL)
                compilerError("Variable '%.*s' has not yet been declared. Consider declaring it before using it", C->name, nameE->expr.base.location, nameE->name.length, &C->src[nameE->name.location.begin.offset]);

            int slot = symbol->slot;

            int r = alloc_reg(C);
            ir_emit(&C->ir, IR_LOAD_VAR, r, slot, 0, nameE->expr.base.location);
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
                case M_CONCAT: op = IR_CONCAT; break;
                case M_MOD: op = IR_MOD; break;
                case M_POW: op = IR_POW; break;
                case M_LT: op = IR_LT; break;
                case M_LTE: op = IR_LTE; break;
                case M_GT: op = IR_GT; break;
                case M_GTE: op = IR_GTE; break;
                case M_EQ: op = IR_EQ; break;
                case M_NEQ: op = IR_NEQ; break;
                default: op = IR_ADD; break; // temporal
            }

            ir_emit(&C->ir, op, r, left, right, binary->expr.base.location);
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
                case M_MINUS: op = IR_UNM; break;
                case M_NOT: op = IR_NOT; break;
                default: op = IR_UNM; break; // temporal
            }

            ir_emit(&C->ir, op, r, right, 0, unary->expr.base.location);
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

            int slot = symbol->slot;

            int old = alloc_reg(C);
            ir_emit(&C->ir, IR_LOAD_VAR, old, slot, 0, fix->target->base.location);

            int one = alloc_reg(C);
            Value v = make_int(atoi("1"));
            int k = const_add(&C->constants, v);
            ir_emit(&C->ir, IR_LOAD_CONST, one, k, 0, fix->op.location); // Pendiente revisar comportamiento

            int result = alloc_reg(C);

            if (fix->op.type == M_INC)
            {
                ir_emit(&C->ir, IR_ADD, result, old, one, fix->expr.base.location);
            }
            else
            {
                ir_emit(&C->ir, IR_SUB, result, old, one, fix->expr.base.location);
            }

            ir_emit(&C->ir, IR_STORE_VAR, slot, result, 0, fix->expr.base.location);

            if (fix->isPre)
                return result;
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
                //printf("line: %d, column: %d, offset: %d\n", var->names[i].location.begin.line, var->names[i].location.begin.column, var->names[i].location.begin.offset);
                int slot = symbol_define(&C->symbol, var->names[i], var->isConst, C->src);

                int value_reg;

                if (i < var->valuesCount)
                {
                    value_reg = compiler_expr(C, var->values[i]);
                }
                else
                {
                    value_reg = alloc_reg(C);
                    ir_emit(&C->ir, IR_LOAD_CONST, value_reg, 0, 0, var->values[var->valuesCount - 1]->base.location); // NaN, pendiente mejorar
                }

                ir_emit(&C->ir, IR_STORE_VAR, slot, value_reg, 0, var->stmt.base.location);
            }

            return;
        }

        case STMT_BLOCK:
        {
            StmtBlock* block = (StmtBlock*) stmt;
            begin_scope(&C->symbol);

            for (int i = 0; i < block->count; i++)
                compiler_stmt(C, block->statements[i]);

            end_scope(&C->symbol);
            return;
        }

        case STMT_IF:
        {
            StmtIf* ifstmt = (StmtIf*) stmt;
            int cond = compiler_expr(C, ifstmt->condition);

            int jump_if_false = ir_emit_jump(&C->ir, IR_JUMP_IF_FALSE, cond, ifstmt->condition->base.location);

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

        case STMT_EXPR:
        {
            compiler_expr(C, stmt);
            return;
        }

        case STMT_ASSING:
        {
            StmtAssign* assign = (StmtAssign*) stmt;
            for (int i = 0; i < assign->nameCount; i++)
            {
                Symbol* symbol = symbols_resolve(&C->symbol, assign->names[i], C->src);

                if (symbol == NULL)
                    compilerError("Variable '%.*s' has not yet been declared. Consider declaring it before using it", C->name, assign->names[i].location, assign->names[i].length, &C->src[assign->names[i].location.begin.offset]);

                if (symbol->isConst)
                    compilerError("Const '%.*s' cannot be modified", C->name, assign->stmt.base.location, assign->names[i].length, &C->src[assign->names[i].location.begin.offset]);

                int slot = symbol->slot;

                int value_reg;

                if (i < assign->valueCount)
                {
                    value_reg = compiler_expr(C, assign->values[i]);
                }
                else
                {
                    value_reg = alloc_reg(C);
                    ir_emit(&C->ir, IR_LOAD_CONST, value_reg, 0, 0, assign->values[assign->valueCount - 1]->base.location); // NaN, pendiente mejorar
                }

                ir_emit(&C->ir, IR_STORE_VAR, slot, value_reg, 0, assign->stmt.base.location);
            }
            return;
        }

        case STMT_COMPOUND_ASSING:
        {
            StmtCompoundAssing* compound = (StmtCompoundAssing*) stmt;
            
            int value = compiler_expr(C, compound->value);
            int target = compiler_expr(C, compound->target);

            Symbol* symbol = symbols_resolve(&C->symbol, ((NameExpr*)compound->target)->name, C->src);

            if (symbol == NULL)
                compilerError("Variable '%.*s' has not yet been declared. Consider declaring it before using it", C->name, compound->target->base.location, ((NameExpr*)compound->target)->name.length, &C->src[compound->target->base.location.begin.offset]);

            if (symbol->isConst)
                compilerError("Const '%.*s' cannot be modified", C->name, compound->stmt.base.location, ((NameExpr*)compound->target)->name.length, &C->src[compound->target->base.location.begin.offset]);

            int slot = symbol->slot;

            int r = alloc_reg(C);

            IROpCode op;

            switch (compound->op.type)
            {
            case M_PLUS_ASSING: op = IR_ADD; break;
            case M_MINUS_ASSING: op = IR_SUB; break;
            case M_STAR_ASSING: op = IR_MUL; break;
            case M_SLASH_ASSING: op = IR_DIV; break;
            case M_FLOOR_DIV_ASSING: op = IR_IDIV; break;
            case M_CONCAT_ASSING: op = IR_CONCAT; break;
            case M_POW_ASSING: op = IR_MOD; break;
            case M_MOD_ASSING: op = IR_POW; break;
            default: op = IR_ADD; break; // temporal
            }
            //ir_emit(&C->ir, IR_LOAD_VAR, r1, target, 0);
            ir_emit(&C->ir, op, r, target, value, compound->value->base.location);
            ir_emit(&C->ir, IR_STORE_VAR, slot, r, 0, compound->stmt.base.location);

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
    compiler_stmt(C, stmt);
}

#if (defined(DEBUG) && DEBUG == 1) && (defined(COMPILER_DEBUG) && COMPILER_DEBUG == 1)

static void print_ir(Compiler* C, int i)
{
    IRInstruction ir = C->ir.data[i];
    printf("L%d - ", i);
    printf("%d: ", C->ir.locations[i].begin.line);
    if (ir.op == IR_LOAD_CONST)
    {
        Value v = C->constants.data[ir.b];
        printf("IR_LOADK R%d K%d", ir.a, ir.b);
        if (v.type == VAL_INT)
            printf(" [%d]", v.i);
        else if (v.type == VAL_FLOAT)
            printf(" [%f]", v.f);
        else if (v.type == VAL_BOOLEAN)
            printf(" [%s]", v.b == true ? "true" : "false");
        else if (v.type == VAL_NAN)
            printf(" [NaN]");
        else if (v.type == VAL_NIL)
            printf(" [nil]");
        else if (v.type == VAL_STRING)
            printf(" ['%.*s']", v.string.length, v.string.chars);
            
    }
    else if (ir.op == IR_LOAD_VAR)
        printf("IR_LOADV R%d S%d", ir.a, ir.b);
    else if (ir.op == IR_STORE_VAR)
        printf("IR_STORE S%d R%d", ir.a, ir.b);
    else if (ir.op == IR_MOVE)
        printf("IR_MOVE R%d R%d", ir.b, ir.a);
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
    else if (ir.op == IR_JUMP)
        printf("IR_JUMP L%d", ir.b);
    else if (ir.op == IR_JUMP_IF_FALSE)
        printf("IR_JUMP_IF_FALSE R%d L%d", ir.a, ir.b);
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



