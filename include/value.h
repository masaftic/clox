#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef double Value;

typedef struct {
    int capacity;
    int count;
    Value *values;
} ValueArray;

void initValueArray(ValueArray *array);
void freeValueArray(ValueArray *array);
void writeValueArray(ValueArray *array, uint8_t byte);
void printValue(Value value);

#endif