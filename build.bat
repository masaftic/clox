
set CFLAGS=-Wextra -Wall -O0
set SRCS=src/main.c src/chunk.c src/memory.c src/debug.c

gcc -o main.exe %CFLAGS% %SRCS%
.\main.exe