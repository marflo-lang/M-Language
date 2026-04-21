#include <malloc.h>
#include "allocator.h"
#include "err.h"

void arena_init(Arena* A, size_t size)
{
    A->memory = malloc(size);
    A->capacity = size;
    A->offset = 0;
}

void* arena_allocator(Arena* A, size_t size)
{
    size = (size + 7) & ~7; // alineación a 8 bytes
    if (A->offset + size > A->capacity)
    {
        size_t newCapacity = A->capacity * 2;

        while (newCapacity < A->offset + size) {
            newCapacity *= 2;
        }

        char* newMemory = realloc(A->memory, newCapacity);
        if (newMemory == NULL)
        {
            printErr("Error de memoria", "Allocator", 3);
            exit(1);
        }
        A->memory = newMemory;
        A->capacity = newCapacity;
    }

    void* ptr = A->memory + A->offset;
    A->offset += size;
    return ptr;
}

void arena_free(Arena* A)
{
    free(A->memory);
}


