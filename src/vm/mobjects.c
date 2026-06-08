
#include "mobjects.h"
#include "vm.h"

#include <inttypes.h>

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

static uint32_t hash_bytes(const uint8_t* data, size_t length)
{
    uint32_t hash = 2166136261u;

    for (size_t i = 0; i < length; i++)
    {
        hash ^= data[i];
        hash *= 16777619;
    }

    return hash;
}

static uint32_t hash_uint64(uint64_t value)
{
    return hash_bytes((uint8_t*)&value, sizeof(uint64_t));
}

static uint32_t hash_double(double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(double));
    return hash_uint64(bits);
}

static uint32_t hash_bool(bool value)
{
    return value ? 1231 : 1237;
}

static uint32_t hash_combine(uint32_t a, uint32_t b)
{
    return a ^ (b + 0x9e3779b9 + (a << 6) + (a >> 2));
}

static uint32_t hash_rvalue(VM* vm, int line, Value v)
{
    switch (v.type)
    {
        case VAL_INT:
        {
            uint32_t value_hash = hash_uint64((uint64_t) ivalue(v));
            return hash_combine(VAL_INT, value_hash);
        }
        case VAL_FLOAT:
        {
            uint32_t value_hash = hash_double(fvalue(v));
            return hash_combine(VAL_FLOAT, value_hash);
        }
        case VAL_BOOLEAN:
        {
            uint32_t value_hash = hash_bool(bvalue(v));
            return hash_combine(VAL_BOOLEAN, value_hash);
        }
        case VAL_NIL:
        {
            return 1;
        }
        case VAL_NAN:
        {
            return 0;
        }
        case VAL_OBJ:
        {
            GCObject* obj = (GCObject*) v.obj;
            if (obj->objType == OBJ_STRING)
            {
                ObjString* string = (ObjString*) obj;
                return hash_combine(OBJ_STRING, string->hash);
            }
            else
            {
                invalidKeyType(vm, 0, "int, float, boolean, string", getValueTypeName(v));
                return 0;
            }
        }
        default:
        {
            invalidKeyType(vm, 0, "int, float, boolean, string", getValueTypeName(v));
            return 0;
        }

    }
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

static uint32_t hash_string(const char* text, size_t length)
{
    uint32_t hash = 2166136261u;

    for (size_t i = 0; i < length; i++)
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

static ObjString* find_interned_string(VM* vm, const char* text, size_t length, uint32_t hash)
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

ObjString* allocate_string(VM* vm, const char* text, size_t length)
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
    {
        cannotResizeList(vm, line);
        return;
    }

    if (newCapacity < list->length)
    {
        resizeFractured(vm, line, "list because the new capacity is lower than old length");
        return;
    }

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

ObjList* copy_list(VM* vm, ObjList* listToCopy, int line)
{
    ObjList* newList = allocate_list(vm, listToCopy->length, listToCopy->capacity, listToCopy->fixed);
    for (int i = 0; i < listToCopy->length; i++)
    {
        newList->values[i] = listToCopy->values[i];
    }
    return newList;
}

void set_list_element(VM* vm, Value* o, Value element, int pos, int line)
{
    ObjList* list = (ObjList*) o->obj;

    if (pos != 0)
    {
        if (abs(pos) > list->length)
        {
            indexoutofbound(vm, line, pos, list->length);
            return;
        }

        if (pos < 0)
            list->values[list->length + pos] = element;
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
    GCObject* obj = (GCObject*)(o.obj);
    ObjList* list = (ObjList*)obj;
    return list->length;
}

extern inline Value listvalue(Value o, int pos)
{
    GCObject* obj = (GCObject*)(o.obj);
    ObjList* list = (ObjList*)obj;
    if (pos != 0)
    {
        if (abs(pos) > list->length)
            return (Value) { .type = VAL_NAN, .i = 0 };

        if (pos < 0)
            return list->values[list->length + pos];
        else
            return list->values[pos - 1];
    }
    else
        return (Value) { .type = VAL_NAN, .i = 0 };
}

ObjDict* copy_dict(VM* vm,  ObjDict* dictToCopy, int line)
{
    ObjDict* newDict = allocate_dict(vm, dictToCopy->capacity, dictToCopy->fixed);
    newDict->count = dictToCopy->count;
    for (int i = 0; i < dictToCopy->capacity; i++)
    {
        if (ismnan(dictToCopy->entries[i].key))
            continue;
        
        newDict->entries[i] = dictToCopy->entries[i];
    }
    return newDict;
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
    if (isnil(key) || ismnan(key) || islist(key) || isdict(key))
    {
        invalidKeyType(vm, line, "int, float, string, boolean", getValueTypeName(key));
        return;
    }

    DictEntry* entry = find_entry(vm, dict->entries, dict->capacity, key, line);

    if (ismnan(entry->key))
    {
        //printf("Is nan ->");
        //print_rvalue(key, true);
        if (dict->fixed && dict->count >= dict->capacity)
        {
            cannotResizeDict(vm, line);
            return;
        }

        if ((dict->count + 1) > dict->capacity * 0.75)
        {
            resize_dict(vm, dict, (dict->capacity < 8) ? 8 : (dict->capacity * 2), line);
            entry = find_entry(vm, dict->entries, dict->capacity, key, line);
        }

        dict->count++;
    }

    entry->hash = hash_rvalue(vm, line, key);
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
        //cannotResizeDict(vm, line);
        return;

    if (newCapacity < dict->count)
    {
        resizeFractured(vm, line, "dictionary because the new capacity is lower than old count");
        return;
    }

    DictEntry* oldEntries = dict->entries;
    int oldCapacity = dict->capacity;

    size_t oldSize = sizeof(DictEntry) * dict->capacity;
    size_t newSize = sizeof(DictEntry) * newCapacity;

    DictEntry* entries = calloc(newCapacity, sizeof(DictEntry));
    if (entries == NULL)
    {
        memoryCrash("Dictionary Re-Allocation");
        exit(1);
    }
    // pendiente una zona aquí de inicializar con nil los elementos nuevos
    for (int i = 0; i < oldCapacity; i++)
    {
        DictEntry* oldEntry = &oldEntries[i];

        if (oldEntry->hash == 0)
            continue;

        uint32_t hash = oldEntry->hash;

        int index = hash % newCapacity;

        while (true)
        {
            DictEntry* newEntry = &entries[index];

            if (ismnan(newEntry->key))
            {
                newEntry->hash = hash;
                newEntry->key = oldEntry->key;
                newEntry->value = oldEntry->value;

                break;
            }

            index = (index + 1) % newCapacity;
        }
    }
    free(oldEntries);
    dict->entries = entries;
    dict->capacity = newCapacity;
    vm->bytes_allocated += newSize - oldSize;
}

ObjDict* allocate_dict(VM* vm, int capacity, bool fixed)
{
    //capacity = (capacity < 8) ? 8 : capacity;
    ObjDict* obj = (ObjDict*) allocate_object(vm, sizeof(ObjDict), OBJ_DICTIONARY);
    obj->capacity = capacity;
    obj->fixed = fixed;
    obj->count = 0;

    if (capacity> 0)
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
        printf("int -> %"PRId64, v.i);
    }
    else if (isfloat(v))
    {
        char buffer[50];
        //sprintf(buffer, "%.17g", v.f);
        sprintf_s(buffer, 50, "%.17g", v.f);
        if (strchr(buffer, '.') == NULL && strchr(buffer, 'e') == NULL)
        {
            //strcat(buffer, '.0');
            strcat_s(buffer, 50, ".0");
        }
        //printf("float -> %.17g", v.f);
        printf("float -> %s", buffer);
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
        printf("string: length %zu, hash %"PRIu32", text '%s'", string->length, string->hash, string->chars);
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
            if (islist(list->values[i]) && list->values[i].obj == list)
            {
                printf("<recursion>");
            }
            else
            {
                print_rvalue(list->values[i], false);
            }
        }
        printf("]");
    }
    else if (isdict(v))
    {
        ObjDict* dict = (ObjDict*) v.obj;
        printf("dictionary: capacity %d, count %d, fixed %s, values {", dict->capacity, dict->count, (dict->fixed == true) ? "true" : "false");
        bool comma = false;
        for (int i = 0; i < dict->capacity; i++)
        {
            DictEntry* entry = &dict->entries[i];
            if (ismnan(entry->key) || ismnan(entry->value))
                continue;
            if (comma)
                printf(", ");
            else
                comma = true;
            printf("[");
            if (isdict(entry->key) && entry->key.obj == dict)
            {
                printf("<recursion>");
            }
            else
            {
                print_rvalue(entry->key, false);
            }
            printf("] = ");
            if (isdict(entry->value) && entry->value.obj == dict)
            {
                printf("<recursion>");
            }
            else
            {
                print_rvalue(entry->value, false);
            }
        }
        printf("}");
    }
    else
    {
        printf("ERROR: Type %d unrecognize\n", ttype(v));
    }

    if (newLine == true)
        printf("\n");
}

