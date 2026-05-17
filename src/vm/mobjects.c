
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
    uint32_t hash = hash_string(text, length);

    ObjString* string = find_interned_string(vm, text, length, hash);
    if (string != NULL)
        return string;
    
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

extern inline bool isstring(Value o)
{
    if (o.type == VAL_OBJ)
    {
        GCObject* obj = (GCObject*)(o.obj);

        if (obj->objType == OBJ_STRING)
        {
            return true;
        }
    }

    return false;
}

extern inline const char* svalue(Value o)
{
    GCObject* obj = (GCObject*) (o.obj);
    ObjString* string = (ObjString*) obj;
    return string->chars;
}

extern inline int slenvalue(Value o)
{
    GCObject* obj = (GCObject*)(o.obj);
    ObjString* string = (ObjString*)obj;
    return string->length;
}

void resize_list(VM* vm, ObjList* list, int newCapacity, int line)
{
    if (list->fixed)
        cannotResizeList(vm, line);

    if (newCapacity < list->length)
        resizeFractured(vm->name, line, "list because the new capacity is lower than old length");
    size_t oldSize = sizeof(Value) * list->capacity;
    size_t newSize = sizeof(Value) * newCapacity;
    Value* values = realloc(list->values, newSize);
    if (values == NULL)
    {
        memoryCrash("List Re-Allocation");
        exit(1);
    }
    // pendiente una zona aquí de inicializar con nil los elementos nuevos
    list->values = values;
    list->capacity = newCapacity;
    vm->bytes_allocated += newSize - oldSize;
}

ObjList* allocate_list(VM* vm, int length, int capacity, bool fixed)
{
    ObjList* obj = (ObjList*)allocate_object(vm, sizeof(ObjList), OBJ_LIST);

    obj->length = length;
    obj->capacity = capacity;
    obj->fixed = fixed;
    
    if (capacity > 0)
    {
        obj->values = calloc(capacity, sizeof(Value));

        if (obj->values == NULL)
        {
            memoryCrash("List Allocation");
            exit(1);
        }

        vm->bytes_allocated += sizeof(Value) * capacity;
    }
    else
    {
        obj->values = NULL;
    }

    return obj;
}

void set_list_element(VM* vm, Value* o, Value element, int pos, int line)
{
    ObjList* list = (ObjList*) o->obj;

    if (pos > -1)
    {
        if (pos > list->capacity)
            indexoutofbound(vm->name, line, pos, list->capacity);

        list->values[pos - 1] = element;
    }
    else
    {
        if (list->length > list->capacity)
            resize_list(vm, list, list->capacity * 2, line);

        list->values[list->length++] = element;
    }
}

extern inline void set_list(Value* a, ObjList* list)
{
    a->type = VAL_OBJ;
    a->obj = (GCObject*) list;
}

extern inline bool islist(Value o)
{
    if (o.type == VAL_OBJ)
    {
        GCObject* obj = (GCObject*)(o.obj);

        if (obj->objType == OBJ_LIST)
        {
            return true;
        }
    }

    return false;
}

extern inline int listlenvalue(Value o)
{
    if (o.type == VAL_OBJ)
    {
        GCObject* obj = (GCObject*)(o.obj);

        if (obj->objType == OBJ_LIST)
        {
            ObjList* list = (ObjList*)obj;

            return list->length;
        }
    }
}

extern inline Value listvalue(Value o, int pos)
{
    GCObject* obj = (GCObject*)(o.obj);
    ObjList* list = (ObjList*)obj;
    if (pos > list->length)
        return (Value) { .type = VAL_NAN, .i = 0 };
    else
        return list->values[pos - 1];
}

void resize_dict(VM* vm, ObjDict* dict, int newCapacity, int line)
{
    if (dict->fixed)
        cannotResizeDict(vm, line);

    if (newCapacity < dict->count)
        resizeFractured(vm->name, line, "dictionary because the new capacity is lower than old count");
    size_t oldSize = sizeof(DictEntry) * dict->capacity;
    size_t newSize = sizeof(DictEntry) * newCapacity;
    DictEntry* entries = realloc(dict->entries, newSize);
    if (entries == NULL)
    {
        memoryCrash("Dictionary Re-Allocation");
        exit(1);
    }
    // pendiente una zona aquí de inicializar con nil los elementos nuevos
    dict->entries = entries;
    dict->capacity = newCapacity;
    vm->bytes_allocated += newSize - oldSize;
}

ObjDict* allocate_dict(VM* vm, int count, int capacity, bool fixed)
{
    ObjDict* obj = (ObjDict*) allocate_object(vm, sizeof(ObjDict), OBJ_DICTIONARY);

    obj->capacity = capacity;
    obj->fixed = fixed;
    obj->count = count;

    if (capacity > 0)
    {
        obj->entries = calloc(capacity, sizeof(DictEntry));

        if (obj->entries == NULL)
        {
            memoryCrash("Dict Entries Allocation");
            exit(1);
        }

        vm->bytes_allocated += sizeof(DictEntry) * capacity;
    }
    else
    {
        obj->entries = NULL;
    }

    return obj;
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

