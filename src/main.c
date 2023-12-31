#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"



static void repl() 
{
    char line[1024];
    for (;;) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        interpret(line);
    }
}

static char *read_file(const char *path) 
{
    char *buffer = NULL;
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        perror("Could not open file");
        goto error;
    }

    if (fseek(file, 0L, SEEK_END)) {
        goto error;
    }

    int fileSize = ftell(file);
    if (fileSize < 0) {
        goto error;
    }

    buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        goto error;
    }

    if (fseek(file, 0, SEEK_SET) < 0) {
        goto error;
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < (size_t) fileSize) {
        fprintf(stderr, "Could not read file \"%s\". %s\n", path, strerror(errno));
        goto error;
    }

    buffer[bytesRead] = '\0';

    return buffer;

error:
    if (file) {
        fclose(file);
    }
    if (buffer) {
        free(buffer);
    }
    exit(1);
}



static void run_file(const char *path) 
{
    char *source = read_file(path);
    InterpretResult result = interpret(source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}


int main(int argc, const char* argv[])
{
    init_VM();

#ifdef DEBUG_LOCAL_DEBUGGER
    const char *path = "/home/masaftic/dev/clox/clox.txt";
    run_file(path);
    return 0;
#endif
    
    if (argc == 1) {
        repl();
    } 
    else if (argc == 2) {
        run_file(argv[1]);
    } 
    else {
        fprintf(stderr, "Usage: clox [path]\n");
        exit(64);
    }

    free_VM();
    return 0;
}