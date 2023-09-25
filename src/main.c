#include <stdio.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(int argc, const char* argv[])
{
    Chunk chunk;
    initChuck(&chunk);
    
    int constant = addConstant(&chunk, 420.69);
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constant, 123);

    writeChunk(&chunk, OP_RETURN, 123);

    disassembleChunk(&chunk, "test chunk");

    freeChunk(&chunk);
    

    return 0;
}