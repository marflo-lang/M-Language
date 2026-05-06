
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
                int offset = ir->b - i;
                printf("offset = %d\n", offset);
                emit(G, CREATE_ABx(OP_JUMP, ir->a, offset), loc);
                
                break;
            }

            case IR_JUMP_IF_FALSE:
            {
                int offset = ir->b - i;
                printf("offset = %d\n", offset);
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

