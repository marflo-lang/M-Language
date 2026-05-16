
#include "mobjects.h"
#include "vm.h"

static void intern_string(VM* vm, ObjString* string);

static uint32_t hash_string(const char* text, int length)
{
    uint32_t hash = 2166136261u;

    for (int i = 0; i < length; i++)
    {
        hash ^= (uint8_t) text[i];
        hash *= 16777619;
    }

    return hash;
}

static void resize_interning(VM* vm)
{
    StringTable* table = &vm->strings;
    int oldCapacity = table->capacity;
    int newCapacity = (table->capacity < 8) ? 8 : table->capacity * 2;

    StringEntry* newEntries = calloc(newCapacity, sizeof(StringEntry));

    if (newEntries == NULL)
    {
        memoryCrash("Resize Interning String");
        exit(1);
    }

    StringEntry* oldEntries = table->entries;

    table->capacity = newCapacity;
    table->count = 0;
    table->entries = newEntries;
    

    for (int i = 0; i < oldCapacity; i++)
    {
        ObjString* string = oldEntries[i].key;
        if (string == NULL)
            continue;
        
        intern_string(vm, string);
    }

    free(oldEntries);
}

static void intern_string(VM* vm, ObjString* string)
{
    StringTable* table = &vm->strings;

    if ((table->count + 1) * 4 > table->capacity * 3)
        resize_interning(vm);

    uint32_t index = string->hash % table->capacity;

    while (true)
    {
        StringEntry* entry = &table->entries[index];

        if (entry->key == NULL)
        {
            entry->key = string;
            table->count++;
            return;
        }

        index = (index + 1) % table->capacity;
    }
}

static GCObject* allocate_object(VM* vm, size_t size, ObjType type)
{

    if (vm->bytes_allocated + size >= vm->next_gc)
    {
        // aquí irá la recoleccion en un futuro cuando haya gc
    }

    GCObject* obj = malloc(size);

    if (obj == NULL)
    {
        memoryCrash("Object Allocator RunTime");
        exit(1);
    }

    obj->marked = false;
    obj->next = vm->objects;
    obj->objType = type;

    vm->objects = obj;
    vm->bytes_allocated += size;

    return obj;
}

static ObjString* find_interned_string(VM* vm, const char* text, int length, uint32_t hash)
{
    StringTable* table = &vm->strings;

    if (table->capacity == 0)
    {
        return NULL;
    }

    uint32_t index = hash % table->capacity;

    while (true)
    {
        StringEntry* entry = &table->entries[index];

        if (entry->key == NULL)
        {
            return NULL;
        }

        ObjString* string = entry->key;

        if (string->hash == hash && string->length == length && memcmp(string->chars, text, length) == 0)
            return string;

        index = (index + 1) % table->capacity;
    }
}

ObjString* allocate_string(VM* vm, const char* text, int length)
{
    printf("Allocate\n");
    uint32_t hash = hash_string(text, length);

    ObjString* string = find_interned_string(vm, text, length, hash);
    if (string != NULL)
        return string;
    printf("Create\n");
    ObjString* obj = (ObjString*) allocate_object(vm, sizeof(ObjString), OBJ_STRING);
    char* chars = malloc(length + 1);
    if (chars == NULL)
    {
        memoryCrash("Copy String Text");
        exit(1);
    }
    memcpy(chars, text, length);
    chars[length] = '\0';
    obj->chars = chars;
    obj->length = length;
    obj->hash = hash;

    intern_string(vm, obj);
    return obj;
}

ObjList* allocate_list(VM* vm)
{

}

void free_object(VM* vm)
{

}

void print_object(GCObject* object)
{
#if  defined(DEBUG) && DEBUG == 1
    printf("DEBUG\n");
#else
    printf("NO DEBUG\n");
#endif //  defined(DEBUG) && DEBUG == 1

}

