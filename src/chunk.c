#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

void initChuck(Chunk *chuck)
{
    chuck->count = 0;
    chuck->capacity = 0;
    chuck->code = NULL;
}

void freeChunk(Chunk *chunk)
{
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    initChuck(chunk);
}

void writeChunk(Chunk *chunk, uint8_t byte)
{
    if (chunk->count + 1 > chunk->capacity) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;
}
