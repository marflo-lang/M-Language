
#include "codegen.h"
#include "err.h"
#include "vm.h"

#include <malloc.h>

static void emit(CodeGen* G, Instruction instr, Location loc)
{
    // Falta validacion de tamaño
    chunk_write(G->chunk, instr, loc.begin.line);
}

void generator_init(const char* src, const char* name, IRList* ir, ConstTable* T)
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
}

Chunk* generate_bydecode(CodeGen* G)
{

}

