#pragma once
#include <stdio.h>

typedef struct
{
    char* memory;
    size_t capacity;
    size_t offset;
} Arena;

void arena_init(Arena* A, size_t size);

void* arena_allocator(Arena* A, size_t size);

void arena_free(Arena* A);

