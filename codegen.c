
#include "codegen.h"
#include "err.h"
#include "vm.h"

#include <malloc.h>

static void emit(CodeGen* G, Instruction instr, Location loc)
{
    // Falta validacion de tamaño
    //printf("=============\n");
    //printf("instr = %d\n", GET_OPCODE(instr));
    chunk_write(G->chunk, instr, loc.begin.line);
    //printf("G->chunk->instr = %d\n", GET_OPCODE(G->chunk->instructions[G->chunk->actual_instruction - 1]));
}

CodeGen* generator_init(const char* src, const char* name, IRList* ir, ConstTable* T)
{
    CodeGen* G = malloc(sizeof(CodeGen));

    if (G == NULL)
    {
        memoryCrash("Code Generator");
        exit(1);
    }
    
    //G->code = NULL;
    G->ir = ir;
    //G->constants = T->data;
    //G->code_capacity = 0;
    //G->code_count = 0;
    G->label_to_pc = 0;
    G->line_info = NULL;
    G->chunk = chunk_new(); // pendiente a implementar

    chunk_init(G->chunk, T, ir);

    return G;
}

Chunk* generate_bydecode(CodeGen* G)
{
    for (int i = 0; i < G->ir->count; i++)
    {
        IRInstruction* ir = &G->ir->data[i];

        Location loc = G->ir->locations[i];

        switch (ir->op)
        {
            case IR_LOAD_CONST:
            {
                emit(G, CREATE_ABx(OP_LOADK, ir->a, ir->b), loc);

                break;
            }

            case IR_MOVE:
            {
                emit(G, CREATE_ABx(OP_MOVE, ir->a, ir->b), loc);

                break;
            }

            case IR_ADD:
            {
                emit(G, CREATE_ABC(OP_ADD, ir->a, ir->b, ir->c), loc);

                break;
            }

            case IR_SUB:
            {
                emit(G, CREATE_ABC(OP_SUB, ir->a, ir->b, ir->c), loc);

                break;

            }

            case IR_MUL:
            {
                emit(G, CREATE_ABC(OP_MUL, ir->a, ir->b, ir->c), loc);

                break;

            }

            case IR_DIV:
            {
                emit(G, CREATE_ABC(OP_DIV, ir->a, ir->b, ir->c), loc);

                break;
            }

            case IR_IDIV:
            {
                emit(G, CREATE_ABC(OP_IDIV, ir->a, ir->b, ir->c), loc);

                break;
            }

            case IR_MOD:
            {
                emit(G, CREATE_ABC(OP_MOD, ir->a, ir->b, ir->c), loc);

                break;
            }

            case IR_POW:
            {
                emit(G, CREATE_ABC(OP_POW, ir->a, ir->b, ir->c), loc);

                break;
            }

            case IR_CONCAT:
            {
                emit(G, CREATE_ABC(OP_CONCAT, ir->a, ir->b, ir->c), loc);

                break;
            }

            case IR_UNM:
            {
                emit(G, CREATE_ABC(OP_UNM, ir->a, ir->b, ir->c), loc);

                break;
            }

            case IR_NOT:
            {
                emit(G, CREATE_ABC(OP_NOT, ir->a, ir->b, ir->c), loc);

                break;
            }

            case IR_OR:
            {
                emit(G, CREATE_ABC(OP_OR, ir->a, ir->b, ir->c), loc);

                break;
            }

            case IR_AND:
            {
                emit(G, CREATE_ABC(OP_AND, ir->a, ir->b, ir->c), loc);

                break;
            }

            case IR_EQ:
            {
                emit(G, CREATE_ABC(OP_EQ, ir->a, ir->b, ir->c), loc);

                break;
            }

            case IR_NEQ:
            {
                emit(G, CREATE_ABC(OP_NEQ, ir->a, ir->b, ir->c), loc);

                break;
            }

            case IR_LT:
            {
                emit(G, CREATE_ABC(OP_LT, ir->a, ir->b, ir->c), loc);

                break;
            }

            case IR_LTE:
            {
                emit(G, CREATE_ABC(OP_LTE, ir->a, ir->b, ir->c), loc);

                break;
            }

            case IR_GT:
            {
                emit(G, CREATE_ABC(OP_GT, ir->a, ir->b, ir->c), loc);

                break;
            }

            case IR_GTE:
            {
                emit(G, CREATE_ABC(OP_GTE, ir->a, ir->b, ir->c), loc);

                break;
            }

            case IR_JUMP:
            {
                int offset = ir->b - i - 1;
                emit(G, CREATE_AsBx(OP_JUMP, ir->a, offset), loc);
                
                break;
            }

            case IR_JUMP_IF_FALSE:
            {
                int offset = ir->b - i - 1;
                emit(G, CREATE_ABx(OP_JUMP_IF_FALSE, ir->a, offset), loc);
                
                break;
            }

            case IR_HALT:
            {
                emit(G, CREATE_ABC(OP_HALT, 0, 0, 0), loc);
                
                break;
            }

            default:
            {
                printf("Mejor error para IR desconocido\n");

                break;
            }
        }
    }

    return G->chunk;
}

#if (defined(DEBUG) && DEBUG == 1) && (defined(CODEGEN_DEBUG) && CODEGEN_DEBUG == 1)

static void print_bytecode(CodeGen* G, int i)
{
    Chunk* chunk = G->chunk;
    //Instruction* instr = chunk->instructions;
    Instruction inst = chunk->instructions[i];
    uint8_t op = GET_OPCODE(inst);
    printf("L%d - ", i);
    printf("%d: ", chunk->lines[i]);
    if (op == OP_LOADK)
    {
        Value v = chunk->constants[GET_Bx(inst)];
        printf("LOADK R%d K%d", GET_A(inst), GET_Bx(inst));
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
        else if (v.type == VAL_OBJ)
        {
            GCObject* obj = (GCObject*) v.obj;
            if (obj->objType == OBJ_STRING)
            {
                ObjString* string = (ObjString*) obj;
                printf(" ['%.*s']", string->length, string->chars);
            }

        }

    }
    else if (op == OP_LOAD_VAR)
        printf("LOADV R%d S%d", GET_A(inst), GET_Bx(inst));
    else if (op == OP_STORE_VAR)
        printf("STORE S%d R%d", GET_A(inst), GET_Bx(inst));
    else if (op == OP_MOVE)
        printf("MOVE R%d R%d", GET_A(inst), GET_Bx(inst));
    else if (op == OP_ADD)
        printf("ADD R%d R%d R%d", GET_A(inst), GET_B(inst), GET_C(inst));
    else if (op == OP_SUB)
        printf("SUB R%d R%d R%d", GET_A(inst), GET_B(inst), GET_C(inst));
    else if (op == OP_MUL)
        printf("MUL R%d R%d R%d", GET_A(inst), GET_B(inst), GET_C(inst));
    else if (op == OP_DIV)
        printf("DIV R%d R%d R%d", GET_A(inst), GET_B(inst), GET_C(inst));
    else if (op == OP_IDIV)
        printf("IDIV R%d R%d R%d", GET_A(inst), GET_B(inst), GET_C(inst));
    else if (op == OP_MOD)
        printf("MOD R%d R%d R%d", GET_A(inst), GET_B(inst), GET_C(inst));
    else if (op == OP_POW)
        printf("POW R%d R%d R%d", GET_A(inst), GET_B(inst), GET_C(inst));
    else if (op == OP_CONCAT)
        printf("CONCAT R%d R%d R%d", GET_A(inst), GET_B(inst), GET_C(inst));
    else if (op == OP_UNM)
        printf("UNM R%d R%d", GET_A(inst), GET_Bx(inst));
    else if (op == OP_NOT)
        printf("NOT R%d R%d", GET_A(inst), GET_Bx(inst));
    else if (op == OP_OR)
        printf("OR R%d R%d R%d", GET_A(inst), GET_B(inst), GET_C(inst));
    else if (op == OP_AND)
        printf("AND R%d R%d R%d", GET_A(inst), GET_B(inst), GET_C(inst));
    else if (op == OP_EQ)
        printf("EQ R%d R%d R%d", GET_A(inst), GET_B(inst), GET_C(inst));
    else if (op == OP_NEQ)
        printf("NEQ R%d R%d R%d", GET_A(inst), GET_B(inst), GET_C(inst));
    else if (op == OP_LT)
        printf("LT R%d R%d R%d", GET_A(inst), GET_B(inst), GET_C(inst));
    else if (op == OP_LTE)
        printf("LTE R%d R%d R%d", GET_A(inst), GET_B(inst), GET_C(inst));
    else if (op == OP_GT)
        printf("GT R%d R%d R%d", GET_A(inst), GET_B(inst), GET_C(inst));
    else if (op == OP_GTE)
        printf("GTE R%d R%d R%d", GET_A(inst), GET_B(inst), GET_C(inst));
    else if (op == OP_JUMP)
        printf("JUMP %d", GET_sBx(inst));
    else if (op == OP_JUMP_IF_FALSE)
        printf("JUMPIFFALSE R%d %d", GET_A(inst), GET_Bx(inst));
    else if (op == OP_HALT)
        printf("HALT");
    else
        printf("Invalid op '%d'", op);
    printf("\n");

}

void codegen_print(CodeGen* G)
{
    printf("===== CODE GENERATOR DEBUG =====\n");
    printf("----- Bytecode -----\n");
    int i = 0;
    for (i = 0; GET_OPCODE(G->chunk->instructions[i]) != OP_HALT; i++)
    {
        print_bytecode(G, i);
    }

    print_bytecode(G, i);
    printf("===== END CODE GENERATOR DEBUG =====\n");

}

#endif 