#include <stdlib.h>

#include "memory.h"
#include "vm.h"

void *reallocate(void *pointer, size_t oldSize, size_t newSize)
{
    (void) oldSize;
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void *result = realloc(pointer, newSize);
    if (result == NULL) exit(1);
    return result;
}

static void free_object(Obj *object)
{
    switch (object->type)
    {
    case OBJ_STRING: {   
        ObjString *string = (ObjString *)object;
        FREE_ARRAY(char, string->chars, string->length + 1);
        FREE(ObjString, object);
        break;
    }
    
    default:
        break;
    }
}

void free_objects()
{
    Obj *object = vm.objects;
    while (object != NULL) {
        Obj *next = object->next;
        free_object(object);
        object = next;
    }    
}
