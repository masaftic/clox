#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "vm.h"
#include "object.h"
#include "memory.h"

VM vm;

static void resetStack()
{
    vm.stackTop = vm.stack;
}

static void runtime_error(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code;
    int line = vm.chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    resetStack();
}

void init_VM()
{
    resetStack();
    vm.objects = NULL;
    init_table(&vm.strings);
    init_table(&vm.globals);
}

void free_VM()
{
    free_table(&vm.strings);
    free_table(&vm.globals);
    free_objects();
}

void push(Value value)
{
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop()
{
    vm.stackTop--;
    return *vm.stackTop;
}

static Value peek(int distance)
{
    return vm.stackTop[-(1 + distance)];
}

static bool is_falsey(Value value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate()
{
    ObjString *b = AS_STRING(pop());
    ObjString *a = AS_STRING(pop());

    int length = a->length + b->length;
    char *chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString *result = take_string(chars, length);
    push(OBJ_VAL(result));
}

static InterpretResult run() 
{
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op) \
    do { \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtime_error("Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(pop()); \
        double a = AS_NUMBER(pop()); \
        push(valueType(a op b)); \
    } while (0)



    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            print_value(*slot);
            printf(" ]");
        }
        printf("\n");

        disassemble_instruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif


        uint8_t instruction;
        switch (instruction = READ_BYTE())
        {

        case OP_CONSTANT: {
            Value constant = READ_CONSTANT();
            push(constant);
            break;
        }

        case OP_NIL:
            push(NIL_VAL);
            break;
        case OP_TRUE:
            push(BOOL_VAL(true));
            break;
        case OP_FALSE:
            push(BOOL_VAL(false));
            break;

        case OP_EQUAL: {
            Value b = pop();
            Value a = pop();
            push(BOOL_VAL(values_equal(a, b)));
            break;
        }
        case OP_GREATER:
            BINARY_OP(BOOL_VAL, >);
            break;        
        case OP_LESS:
            BINARY_OP(BOOL_VAL, <);
            break;

        case OP_ADD: {
            if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                concatenate();
            }
            else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                double b = AS_NUMBER(pop());
                double a = AS_NUMBER(pop());
                push(NUMBER_VAL(a + b));
            }
            else {
                runtime_error("Operands must be two numbers or two strings.");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }    
        case OP_SUBTRACT: 
            BINARY_OP(NUMBER_VAL, -); 
            break;
        case OP_MULTIPLY: 
            BINARY_OP(NUMBER_VAL, *); 
            break;
        case OP_DIVIDE:   
            BINARY_OP(NUMBER_VAL, /); 
            break;

        case OP_NOT:
            push(BOOL_VAL(is_falsey(pop())));
            break;

        case OP_NEGATE: {
            if (!IS_NUMBER(peek(0))) {
                runtime_error("Operand must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }
            push(NUMBER_VAL(-AS_NUMBER(pop())));
            break;
        }

        case OP_PRINT: {
            print_value(pop());
            printf("\n");
            break;
        }

        case OP_POP: {
            pop();
            break;
        }

        case OP_DEFINE_GLOBAL: {
            ObjString *name = READ_STRING();
            table_set(&vm.globals, name, peek(0));
            pop();
            break;
        }

        case OP_GET_GLOBAL: {
            ObjString* name = READ_STRING();
            Value value;
            if (!table_get(&vm.globals, name, &value)) {
                runtime_error("Undefined variable '%s'.");
                return INTERPRET_RUNTIME_ERROR;
            }
            push(value);
            break;
        }

        case OP_RETURN: {
            // Exit interpreter.
            return INTERPRET_OK;
        }
        
        default:
            break;
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(const char *source)
{
    Chunk chunk;
    init_chuck(&chunk);

    if (!compile(source, &chunk)) {
        free_chunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = run();

    free_chunk(&chunk);
    return result;
}

