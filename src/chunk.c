#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

void init_chuck(Chunk *chuck)
{
    chuck->count = 0;
    chuck->capacity = 0;
    chuck->code = NULL;
    chuck->lines = NULL;
    init_value_array(&chuck->constants);
}

void free_chunk(Chunk *chunk)
{
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    free_value_array(&chunk->constants);
    init_chuck(chunk);
}

void write_chunk(Chunk *chunk, uint8_t byte, int line)
{
    if (chunk->count + 1 > chunk->capacity) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity,
                                 chunk->capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines, oldCapacity,
                                  chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int add_constant(Chunk *chunk, Value value)
{
    write_value_array(&chunk->constants, value);
    return chunk->constants.count - 1;
}
