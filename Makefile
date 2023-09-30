CC = gcc
CFLAGS = -Wall -I./include -g

# List your source files here
SRC = src/main.c src/chunk.c src/memory.c src/value.c src/debug.c src/vm.c src/compiler.c src/scanner.c src/object.c

# Generate object file names from source file names
OBJ = $(SRC:.c=.o)

# Name of the final executable
TARGET = clox

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Rule to compile each source file into an object file
%.o: %.c
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

# doesn't work on my powershell for some reason
clean:
	$(RM) $(OBJ) $(TARGET)$(EXE)
# $(RM) $(OBJ) $(TARGET)$(EXE)
