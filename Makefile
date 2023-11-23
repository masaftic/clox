CC = gcc
CFLAGS = -Wall -Wextra -pedantic -I./include -ggdb

# List your source files here
SRC = src/main.c src/chunk.c src/memory.c src/value.c src/debug.c src/vm.c src/compiler.c src/scanner.c src/object.c src/table.c

# Generate object file names from source file names
OBJ = $(patsubst src/%.c, build/%.o, $(SRC))

# Name of the final executable
TARGET = clox

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Rule to compile each source file into an object file
build/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Determine the OS (Windows or Unix-like)
ifeq ($(OS),Windows_NT)
    RM = del -fR
    EXE = .exe
else
    RM = rm -f
    EXE =
endif

SEPS = $(OBJ:.o=,.o)

clean:
	$(RM) $(OBJ) $(TARGET)$(EXE)
