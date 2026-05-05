#include "allocator.h"
#include "err.h"

#include <malloc.h>
#include <assert.h>
#include <string.h>


static ArenaChunk* createChunk(size_t capacity)
{
    ArenaChunk* chunk = malloc(sizeof(ArenaChunk) + capacity);

    if (!chunk)
    {
        memoryCrash("Arena Allocator");
        exit(1);
    }

    chunk->capacity = capacity;
    chunk->offset = 0;
    chunk->next = NULL;

    return chunk;
}

void arena_init(Arena* A, size_t chunkSize)
{
    A->defaultChunkSize = chunkSize;
    A->head = createChunk(chunkSize);
    A->current = A->head;
}

void* arena_allocator(Arena* A, size_t size)
{
    size = (size + 7) & ~7; // alineación a 8 bytes

    // Si no cabe en el bloque actual, creamos uno nuevo
    if (A->current->offset + size > A->current->capacity)
    {
        // Si piden un tamaño mayor al bloque por defecto, adaptamos el nuevo bloque
        size_t newCapacity = (size > A->defaultChunkSize) ? size : A->defaultChunkSize;

        ArenaChunk* newChunk = createChunk(newCapacity);
        A->current->next = newChunk;
        A->current = newChunk;
    }

    assert(A->current->offset + size <= A->current->capacity);

    void* ptr = A->current->data + A->current->offset;
    A->current->offset += size;
    memset(ptr, 0, size);
    return ptr;
}

void arena_free(Arena* A)
{
    ArenaChunk* chunk = A->head;

    while (chunk != NULL)
    {
        ArenaChunk* next = chunk->next;
        free(chunk);
        chunk = next;
    }

    A->head = NULL;
    A->current = NULL;
    A->defaultChunkSize = 0;
}


