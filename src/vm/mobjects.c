
#include "mobjects.h"
#include "vm.h"

static void intern_string(VM* vm, ObjString* string);

extern inline Value make_rnil()
{
    Value v = { .type = VAL_NIL, .i = 0 };
    return v;
}

extern Value make_rnan()
{
    Value v = { .type = VAL_NAN, .i = 0 };
    return v;
}

const char* getValueTypeName(Value v)
{
    if (isint(v))
        return "int";
    else if (isfloat(v))
        return "float";
    else if (isstring(v))
        return "string";
    else if (islist(v))
        return "list";
    else if (isboolean(v))
        return "boolean";
    else if (isnil(v))
        return "nil";
    else if (ismnan(v))
        return "NaN";
    else
        return "Unrecognized type";
}

static uint32_t hash_rvalue(VM* vm, int line, Value v)
{
    if (v.type == VAL_OBJ)
        if (v.obj->objType == OBJ_STRING)
        {
            ObjString* string = (ObjString*)v.obj;
            return string->hash;
        }
        else
            invalidKeyType(vm->name, line, "int, float, string, boolean", getValueTypeName(v));

    uint32_t hash = 2166136261u;
    
    

}

static bool rvalue_equals(Value a, Value b)
{
    if (ttype(a) == ttype(b))
    {
        if (isint(a))
            return ivalue(a) == ivalue(b);
        else if (isfloat(a))
            return fvalue(a) == fvalue(b);
        else if (isboolean(a))
            return bvalue(a) == bvalue(b);
        else if (isnil(a))
            return true;
        else if (ismnan(a))
            return true;
        else if (isobject(a))
            return orefvalue(a) == orefvalue(b);
        else
            return false;
    }
    else
        return false;
}

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
    int oldCapacity = list->capacity;
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
    for (int i = oldCapacity; i < newCapacity; i++)
    {
        list->values[i] = make_rnil();
    }

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

        for (int i = 0; i < capacity; i++)
        {
            obj->values[i] = make_rnil();
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

    if (pos > -2)
    {
        if (pos > list->length || (pos == -1 && list->length == 0))
            indexoutofbound(vm->name, line, pos, list->length);

        if (pos == -1)
            list->values[list->length - 1] = element;
        else
            list->values[pos - 1] = element;
    }
    else
    {
        if (list->length >= list->capacity)
            resize_list(vm, list, (list->capacity < 8) ? 8 : (list->capacity * 2), line);

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

static DictEntry* find_entry(VM* vm, DictEntry* entries, int capacity, Value key, int line)
{
    uint32_t index = hash_rvalue(vm, line, key) % capacity;

    DictEntry* entry;

    while (true)
    {
        entry = &entries[index];

        if (ismnan(entry->key))
            return entry;

        if (rvalue_equals(entry->key, key))
            return entry;

        index = (index + 1) % capacity;
    }
}

void set_dict_key_value(VM* vm, ObjDict* dict, Value key, Value value, int line)
{
    DictEntry* entry = find_entry(vm, dict->entries, dict->capacity, key, line);

    if (ismnan(entry->key))
    {
        if (dict->fixed)
            cannotResizeDict(vm->name, line);

        if ((dict->count + 1) > dict->capacity * 0.75)
            resize_dict(vm, dict, dict->capacity * 2, line);

        dict->count++;
    }

    entry->key = key;
    entry->value = value;
}

Value get_dict_value(VM* vm, ObjDict* dict, Value key, int line)
{
    DictEntry* entry = find_entry(vm, dict->entries, dict->capacity, key, line);

    return entry->value;
}

void resize_dict(VM* vm, ObjDict* dict, int newCapacity, int line)
{
    if (dict->fixed)
        cannotResizeDict(vm, line);

    if (newCapacity < dict->count)
        resizeFractured(vm->name, line, "dictionary because the new capacity is lower than old count");
    size_t oldSize = sizeof(DictEntry) * dict->count;
    size_t newSize = sizeof(DictEntry) * newCapacity;
    DictEntry* entries = realloc(dict->entries, newSize);
    if (entries == NULL)
    {
        memoryCrash("Dictionary Re-Allocation");
        exit(1);
    }
    // pendiente una zona aquí de inicializar con nil los elementos nuevos
    dict->entries = entries;
    dict->count = newCapacity;
    vm->bytes_allocated += newSize - oldSize;
}

ObjDict* allocate_dict(VM* vm, int count, bool fixed)
{
    ObjDict* obj = (ObjDict*) allocate_object(vm, sizeof(ObjDict), OBJ_DICTIONARY);

    obj->capacity = count;
    obj->fixed = fixed;
    obj->count = count;

    if (count > 0)
    {
        obj->entries = calloc(count, sizeof(DictEntry));

        if (obj->entries == NULL)
        {
            memoryCrash("Dict Entries Allocation");
            exit(1);
        }

        vm->bytes_allocated += sizeof(DictEntry) * count;
    }
    else
    {
        obj->entries = NULL;
    }

    return obj;
}

extern inline void set_dict(Value* a, ObjDict* dict)
{
    a->type = VAL_OBJ;
    a->obj = (GCObject*) dict;
}

extern inline bool isdict(Value o)
{
    if (o.type == VAL_OBJ)
    {
        GCObject* obj = (GCObject*)(o.obj);

        if (obj->objType == OBJ_DICTIONARY)
        {
            return true;
        }
    }

    return false;
}

extern inline int dictlenvalue(Value o)
{
    if (o.type == VAL_OBJ)
    {
        GCObject* obj = (GCObject*)(o.obj);

        if (obj->objType == OBJ_DICTIONARY)
        {
            ObjDict* list = (ObjDict*) obj;

            return list->count;
        }
    }
}

void free_object(VM* vm)
{

}

void print_rvalue(Value v, bool newLine)
{
    if (isint(v))
    {
        printf("int -> %d", v.i);
    }
    else if (isfloat(v))
    {
        printf("float -> %f", v.f);
    }
    else if (isboolean(v))
    {
        printf("boolean -> %s", (v.b == true) ? "true" : "false");
    }
    else if (isnil(v))
    {
        printf("nil");
    }
    else if (ismnan(v))
    {
        printf("NaN");
    }
    else if (isstring(v))
    {
        ObjString* string = (ObjString*) v.obj;
        printf("string: length %d, hash %d, text '%s'", string->length, string->hash, string->chars);
    }
    else if (islist(v))
    {
        ObjList* list = (ObjList*) v.obj;
        printf("list: capacity %d, length %d, fixed %s, values ", list->capacity, list->length, (list->fixed == true) ? "true" : "false");
        printf("[");
        for (int i = 0; i < list->capacity; i++)
        {
            if (i > 0)
                printf(", ");
            print_rvalue(list->values[i], false);
        }
        printf("]");
    }

    if (newLine == true)
        printf("\n");
}

