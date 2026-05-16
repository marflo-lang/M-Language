#pragma once
#include <stdio.h>
#include <stdint.h>

typedef struct ArenaChunk
{
    size_t capacity;
    size_t offset;
    struct ArenaChunk* next;
    uint8_t data[0];
} ArenaChunk;

typedef struct
{
    ArenaChunk* head;
    ArenaChunk* current;
    size_t defaultChunkSize;
} Arena;

void arena_init(Arena* A, size_t chunkSize);

void* arena_allocator(Arena* A, size_t size);

void arena_free(Arena* A);

