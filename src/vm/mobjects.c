
#include "mobjects.h"
#include "vm.h"
#include "mdebug.h"
#include "mstack.h"

#include <inttypes.h>

const Value m_nil_obj_ = { .type = 1, .i = 0 };
const Value m_void_obj_ = { 0 };

#define candeep(x) (islist(x) || isdict(x))

static void intern_string(VM* vm, ObjString* string);

extern inline Value* make_rnil()
{
    return m_nil_obj;
}

extern Value* make_rnan()
{
    return m_void_obj;
}

const char* getValueTypeName(Value* v)
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
    else if (isvoid(v))
        return "NaN";
    else if (isclosure(v))
        return "function";
    else
    {
        M_UNREACHABLE();
        return "Unrecognized type";
    }
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

static uint32_t hash_rvalue(VM* vm, const Value* v)
{
    switch (v->type)
    {
    case VAL_INT:
    {
        uint32_t value_hash = hash_uint64((uint64_t)ivalue(v));
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
    case VAL_VOID:
    {
        return 0;
    }
    case VAL_OBJ:
    {
        GCObject* obj = (GCObject*)v->obj;
        if (obj->objType == OBJ_STRING)
        {
            ObjString* string = (ObjString*)obj;
            return hash_combine(OBJ_STRING, string->hash);
        }
        else
        {
            //invalidKeyType(vm, 0, "int, float, boolean, string", getValueTypeName(v));
            m_error(vm, INVALID_KEY_TYPE_ERROR, "Expected types 'int, float, boolean, string' for dict key, but got type '%s'", getValueTypeName(v));
            return 0;
        }
    }
    default:
    {
        //invalidKeyType(vm, 0, "int, float, boolean, string", getValueTypeName(v));
        m_error(vm, INVALID_KEY_TYPE_ERROR, "Expected types 'int, float, boolean, string' for dict key, but got type '%s'", getValueTypeName(v));
        return 0;
    }

    }
}

static uint32_t hash_string(const char* text, size_t length)
{
    uint32_t hash = 2166136261u;

    for (size_t i = 0; i < length; i++)
    {
        hash ^= (uint8_t)text[i];
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
        memoryCrash("Object Allocator RunTime");

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

ObjString* bufstart_string(VM* vm, size_t length)
{

    ObjString* obj = (ObjString*)allocate_object(vm, sizeof(ObjString) + length, OBJ_STRING);
    obj->chars[length] = '\0';
    obj->length = length;

    return obj;
}

ObjString* allocate_string(VM* vm, const char* text, size_t length)
{
    uint32_t hash = hash_string(text, length);

    ObjString* string = find_interned_string(vm, text, length, hash);
    if (string != NULL)
        return string;

    ObjString* obj = (ObjString*)allocate_object(vm, sizeof(ObjString) + length, OBJ_STRING);

    memcpy(obj->chars, text, length);
    obj->chars[length] = '\0';
    obj->length = length;
    obj->hash = hash;

    intern_string(vm, obj);
    return obj;
}

ObjString* bufend_string(VM* vm, ObjString* string)
{
    string->hash = hash_string(string->chars, string->length);
    ObjString* fs;
    if (fs = find_interned_string(vm, string->chars, string->length, string->hash))
    {
        free(string->chars);
        free(string);
        return fs;
    }
    intern_string(vm, string);
    return string;
}

extern inline bool isstring(const Value* o)
{
    if (o->type == VAL_OBJ)
    {
        GCObject* obj = (GCObject*)(o->obj);

        if (obj->objType == OBJ_STRING)
        {
            return true;
        }
    }

    return false;
}

extern inline const char* svalue(Value* o)
{
    GCObject* obj = (GCObject*)(o->obj);
    ObjString* string = (ObjString*)obj;
    return string->chars;
}

extern inline ObjString* osvalue(const Value* o)
{
    GCObject* obj = (GCObject*)(o->obj);
    return (ObjString*)obj;
}

extern inline size_t slenvalue(Value* o)
{
    GCObject* obj = (GCObject*)(o->obj);
    ObjString* string = (ObjString*)obj;
    return string->length;
}

void resize_list(VM* vm, ObjList* list, int newCapacity)
{
    if (list->fixed)
        m_error(vm, RESIZE_LIST_ERROR, "Cannot resize a fixed list");

    if (newCapacity < 0)
        m_error(vm, MEMORY_ERROR, "list overflow; list length too big");

    if (newCapacity == list->capacity) // same capacity, this is very likely in 'list.reserve()'
        return;

    int oldCapacity = list->capacity;
    size_t oldSize = sizeof(Value) * list->capacity;
    size_t newSize = sizeof(Value) * newCapacity;
    Value* values = realloc(list->values, newSize);
    if (values == NULL)
        memoryCrash("List Re-Allocation");

    // pendiente una zona aquí de inicializar con nil los elementos nuevos
    list->values = values;
    for (int i = oldCapacity; i < newCapacity; i++)
    {
        //setobjr(&list->values[i], make_rnil());
        setvoid(&list->values[i]);
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
    obj->readonly = false;

    if (capacity > 0)
    {
        obj->values = calloc(capacity, sizeof(Value));

        if (obj->values == NULL)
            memoryCrash("List Allocation");

        //for (int i = 0; i < capacity; i++)
        //{
        //    setobjr(&obj->values[i], make_rnil());
        //}

        vm->bytes_allocated += sizeof(Value) * capacity;
    }
    else
    {
        obj->values = (Value[]){ 0 };
    }

    return obj;
}

ObjList* copy_list(VM* vm, ObjList* listToCopy, bool deep)
{
    ObjList* newList = allocate_list(vm, listToCopy->length, listToCopy->capacity, listToCopy->fixed);
    for (int i = 0; i < listToCopy->length; i++)
    {
        if (candeep(&listToCopy->values[i]) && deep)
        {
            if (islist(&listToCopy->values[i]))
            {
                ObjList* nlist = copy_list(vm, &listToCopy->values[i], true);
                newList->values[i] = (Value){ .type = VAL_OBJ, .obj = (GCObject*)nlist };
            }
            else if (isdict(&listToCopy->values[i]))
            {
                ObjDict* ndict = copy_dict(vm, &listToCopy->values[i], true);
                newList->values[i] = (Value){ .type = VAL_OBJ, .obj = (GCObject*)ndict };
            }
            else
                M_UNREACHABLE("copy list");
            continue;
        }
        newList->values[i] = listToCopy->values[i];
    }
    return newList;
}

void set_list_element(VM* vm, ObjList* list, Value element, int pos)
{
    if (abs(pos) > list->length) // this means we add in the last position but no more far
        return m_error(vm, INDEX_OUT_OF_BOUNDS_ERROR, "tried to index position %d when length is %d", pos, list->length);

    if (list->length >= list->capacity)
        resize_list(vm, list, (list->capacity < 8) ? 8 : (list->capacity * 2));

    if (pos >= 0)
    {
        list->values[pos] = element;
        if (pos == list->length)
            ++list->length;
    }
    else
        list->values[list->length + pos] = element; // negative position is relative to length
}

extern inline void set_list(Value* a, ObjList* list)
{
    a->type = VAL_OBJ;
    a->obj = (GCObject*)list;
}

extern inline bool islist(const Value* o)
{
    if (o->type == VAL_OBJ)
    {
        GCObject* obj = (GCObject*)(o->obj);

        if (obj->objType == OBJ_LIST)
            return true;
    }

    return false;
}

extern inline int listlenvalue(Value* o)
{
    GCObject* obj = (GCObject*)(o->obj);
    ObjList* list = (ObjList*)obj;
    return list->length;
}

extern inline ObjList* olistvalue(Value* o)
{
    GCObject* obj = (GCObject*)(o->obj);
    return (ObjList*)obj;
}

extern inline Value* listvalue(Value* o, int pos)
{
    GCObject* obj = (GCObject*)(o->obj);
    ObjList* list = (ObjList*)obj;
    if (abs(pos) > list->length)
        return m_void_obj; // this means doesn't exist

    if (pos >= 0)
        return &list->values[pos];
    else
        return &list->values[list->length + pos]; // negative position is relative to length
}

void setfixedlist(VM* vm, ObjList* list, int lenght)
{
    if (list->fixed)
        m_error(vm, RESIZE_LIST_ERROR, "list is already fixed");

    list->fixed = true;
    resize_list(vm, list, lenght);
}

void setreadonlylist(VM* vm, ObjList* list)
{
    if (list->readonly)
        m_error(vm, RESIZE_LIST_ERROR, "list is already readonly");

    list->readonly = true;
}

ObjDict* copy_dict(VM* vm, ObjDict* dictToCopy, bool deep)
{
    ObjDict* newDict = allocate_dict(vm, dictToCopy->capacity, dictToCopy->fixed);
    newDict->count = dictToCopy->count;
    for (size_t i = 0; i < dictToCopy->capacity; i++)
    {
        // we don't copy the entries that are empty (void), but we do copy the entries that are deleted (nil)
        // because the deleted entries are still part of the dictionary and we want to keep them in the copy by linear probing
        if (isvoid(&dictToCopy->entries[i].key))
            continue;

        bool valuedeep = false;

        if (candeep(&dictToCopy->entries[i].value) && deep)
        {
            if (islist(&dictToCopy->entries[i].value))
            {
                ObjList* nlist = copy_list(vm, &dictToCopy->entries[i].value, true);
                newDict->entries[i].value = (Value){ .type = VAL_OBJ, .obj = (GCObject*)nlist };
            }
            else if (isdict(&dictToCopy->entries[i].value))
            {
                ObjDict* ndict = copy_dict(vm, &dictToCopy->entries[i].value, true);
                newDict->entries[i].value = (Value){ .type = VAL_OBJ, .obj = (GCObject*)ndict };
            }
            else
                M_UNREACHABLE("copy dict");

            valuedeep = true;
        }

        if (candeep(&dictToCopy->entries[i].key) && deep)
        {
            if (islist(&dictToCopy->entries[i].key))
            {
                ObjList* nlist = copy_list(vm, &dictToCopy->entries[i].key, true);
                newDict->entries[i].key = (Value){ .type = VAL_OBJ, .obj = (GCObject*)nlist };
            }
            else if (isdict(&dictToCopy->entries[i].key))
            {
                ObjDict* ndict = copy_dict(vm, &dictToCopy->entries[i].key, true);
                newDict->entries[i].key = (Value){ .type = VAL_OBJ, .obj = (GCObject*)ndict };
            }
            else
                M_UNREACHABLE("copy dict");

            continue;
        }
        else if (valuedeep)
        {
            newDict->entries[i].key = dictToCopy->entries[i].key;
            continue;
        }

        newDict->entries[i] = dictToCopy->entries[i];
    }
    return newDict;
}

static DictEntry* find_entry(VM* vm, DictEntry* entries, int capacity, const Value* key)
{
    uint32_t hash = hash_rvalue(vm, key);
    int index = hash % capacity;
    int start = index;
    DictEntry* entry;
    DictEntry* deletedEntry = NULL;
    do
    {
        entry = &entries[index];

        if (isnil(&entry->key) && deletedEntry == NULL)
        {
            // This is a deleted entry, keep track of it
            deletedEntry = entry;
        }

        if (isvoid(&entry->key))
            return deletedEntry != NULL ? deletedEntry : entry;

        if (hash == entry->hash && vmutils_valueEquals(vm, &entry->key, key))
            return entry;

        index = (index + 1) % capacity;
    } while (index != start);

    M_UNREACHABLE("Entry not found");
    return NULL;
}

extern inline void set_dict(Value* a, ObjDict* dict)
{
    a->type = VAL_OBJ;
    a->obj = (GCObject*)dict;
}

void set_dict_key_value(VM* vm, ObjDict* dict, Value* key, Value* value)
{
    if (isnil(key) || isvoid(key) || islist(key) || isdict(key))
    {
        //invalidKeyType(vm, line, "int, float, string, boolean", getValueTypeName(key));
        m_error(vm, INVALID_KEY_TYPE_ERROR, "Expected types 'int, float, boolean, string' for dict key, but got type '%s'", getValueTypeName(key));
        return;
    }

    DictEntry* entry = find_entry(vm, dict->entries, dict->capacity, key);

    if (entry == NULL || isvoid(&entry->key))
    {
        // this means we are adding a new key-value pair to the dictionary
        if (dict->fixed && dict->count >= dict->capacity)
        {
            m_error(vm, RESIZE_DICT_ERROR, "Cannot resize a fixed dictionary");
            return;
        }

        // Check if we need to resize the dictionary before adding a new entry
        // We use a load factor of 0.75 (3/4) to determine when to resize
        if (((dict->count + 1) * 4 > dict->capacity * 3) && !dict->fixed)
        {
            resize_dict(vm, dict, (dict->capacity < 8) ? 8 : (dict->capacity * 2));
            // After resizing, we need to find the entry again because the entries array has changed
            entry = find_entry(vm, dict->entries, dict->capacity, key);
        }

        dict->count++;
    }

    entry->hash = hash_rvalue(vm, key);
    entry->key = *key;
    entry->value = *value;
}

Value* get_dict_value(VM* vm, ObjDict* dict, Value* key)
{
    DictEntry* entry = find_entry(vm, dict->entries, dict->capacity, key);
    return !(isvoid(&entry->key) || isnil(&entry->key)) ? &entry->value : m_void_obj;
}

extern inline dictlistvalue(Value* o)
{
    return cast(ObjDict*, o->obj)->count;
}

void resize_dict(VM* vm, ObjDict* dict, int newCapacity)
{
    if (dict->fixed)
        m_error(vm, RESIZE_DICT_ERROR, "Cannot resize a fixed dict");

    if (newCapacity < 0)
        m_error(vm, MEMORY_ERROR, "dict overflow; dict length too big");

    DictEntry* oldEntries = dict->entries;
    int oldCapacity = dict->capacity;

    size_t oldSize = sizeof(DictEntry) * dict->capacity;
    size_t newSize = sizeof(DictEntry) * newCapacity;

    DictEntry* entries = calloc(newCapacity, sizeof(DictEntry));
    if (entries == NULL)
    {
        m_error(vm, MEMORY_ERROR, "Memory Crash in Dictionary Re-Allocation");
        return;
    }

    /* In the key, to know if a key is empty or deleted,
    * we will use void and nil; void means empty, nil means deleted,
    * thus saving us from creating enums with deadkey or tombstone.
    */
    for (int i = 0; i < oldCapacity; i++)
    {
        DictEntry* oldEntry = oldEntries + i;

        if (oldEntry->hash == 0 && (isvoid(&oldEntry->key))) // nil key doesn't continue loop because nil means deleted, and we need to keep it for lineal probling 
            continue;

        uint32_t hash = oldEntry->hash;

        int index = hash % newCapacity;
        int start = index;

        do
        {
            DictEntry* newEntry = &entries[index];

            if (isvoid(&newEntry->key))
            {
                newEntry->hash = hash;
                newEntry->key = oldEntry->key;
                newEntry->value = oldEntry->value;
                break;
            }

            index = (index + 1) % newCapacity;
        } while (index != start);
    }
    free(oldEntries);
    dict->entries = entries;
    dict->capacity = newCapacity;
    vm->bytes_allocated += newSize - oldSize;
}

ObjDict* allocate_dict(VM* vm, int capacity, bool fixed)
{
    //capacity = (capacity < 8 && !fixed) ? 8 : capacity;
    ObjDict* obj = (ObjDict*)allocate_object(vm, sizeof(ObjDict), OBJ_DICTIONARY);
    obj->capacity = capacity;
    obj->fixed = fixed;
    obj->count = 0;
    obj->readonly = false;
    obj->safeenv = true;

    if (capacity > 0)
    {
        obj->entries = calloc(capacity, sizeof(DictEntry));

        if (obj->entries == NULL)
        {
            m_error(vm, MEMORY_ERROR, "Memory Crash in Dictionary Allocation");
        }

        vm->bytes_allocated += sizeof(DictEntry) * capacity;
    }
    else
    {
        obj->entries = NULL;
    }

    return obj;
}

bool remove_dict_key_value(VM* vm, ObjDict* dict, const Value* key, Value* val_out)
{
    DictEntry* entry = find_entry(vm, dict->entries, dict->capacity, key);
    if (!isvoid(&entry->key))
    {
        if (val_out)
            setobjr(val_out, &entry->value);
        setnil(&entry->key); // nil key means deleted
        setvoid(&entry->value);
        entry->hash = 0;
        return true;
    }

    if (val_out)
        setvoid(val_out);
    return false;
}

extern inline bool isdict(const Value* o)
{
    if (o->type == VAL_OBJ)
    {
        GCObject* obj = (GCObject*)(o->obj);

        if (obj->objType == OBJ_DICTIONARY)
        {
            return true;
        }
    }

    return false;
}

extern inline size_t dictlenvalue(Value* o)
{
    ObjDict* dict = (ObjDict*)o->obj;
    return dict->count;
}

extern inline bool issafeenv(ObjDict* dict)
{
    return dict->safeenv;
}

extern inline Closure* closurevalue(Value* o)
{
    return (Closure*)o->obj;

}

extern inline bool isclosure(const Value* o)
{
    if (isobject(o))
    {
        GCObject* obj = (GCObject*)o->obj;
        if (obj->objType == OBJ_CLOSURE)
            return true;
    }
    return false;
}

extern inline void setclosure(Value* o, Closure* clo)
{
    o->type = VAL_OBJ;
    o->obj = clo;
}

Closure* allocate_Mclosure(VM* vm, int numupvals, ObjDict* env, Chunk* chunk)
{
    Closure* clos = allocate_object(vm, sizeof(Closure) + (sizeof(Value) * numupvals), OBJ_CLOSURE);

    clos->isC = false;
    clos->stack_size = chunk->register_capacity;
    clos->M.chunk = chunk;
    clos->upvalue_count = numupvals;
    clos->env = env;
    
    return clos;
}

Closure* allocate_Cclosure(VM* vm, int numupvals, ObjDict* env)
{
    Closure* clos = allocate_object(vm, sizeof(Closure) + (sizeof(Value) * numupvals), OBJ_CLOSURE);

    clos->isC = true;
    clos->stack_size = M_STACK_INITIAL;
    clos->C.funcC = NULL;
    clos->C.name = NULL;
    clos->upvalue_count = numupvals;
    clos->env = env;

    return clos;
}

void free_object(VM* vm)
{

}

// basic m_StrBuf manipulation
static size_t getnextbuffersize(VM* vm, size_t capacity, size_t required)
{
    if (required > (SIZE_MAX - capacity))
        m_error(vm, INTERNAL_ERROR, "buffer to large");

    size_t required_capacity = capacity + required;
    size_t doubled_capacity = capacity <= (SIZE_MAX / 2) ? capacity * 2 : SIZE_MAX;
    size_t new_capacity = max(required_capacity, doubled_capacity);
    return new_capacity;
}

static char* extendstrbuff(m_StrBuf* buf, size_t addsize, int boxloc)
{
    VM* vm = buf->vm;

    char* base = buf->storage ? buf->storage->chars : buf->buffer;

    size_t capacity = buf->end - base;
    size_t next_size = getnextbuffersize(vm, capacity, addsize);

    ObjString* storage = bufstart_string(vm, next_size);
    memcpy(storage->chars, base, buf->p - base);

    if (base == buf->buffer)
    {
        stack_pushnil(vm);
        stack_insert(vm, boxloc);
    }

    setstring(vm->top + boxloc, storage);
    buf->p = storage->chars + (buf->p - base);
    buf->end = storage->chars + next_size;
    buf->storage = storage;

    return buf->p;
}

void m_bufferinit(VM* vm, m_StrBuf* buf)
{
    // Start with an internal buffer
    buf->p = buf->buffer;
    buf->end = buf->p + 512;

    buf->vm = vm;
    buf->storage = NULL;
}

char* m_buffinitsize(VM* vm, m_StrBuf* buf, size_t size)
{
    m_bufferinit(vm, buf);
    return m_prepbuffsize(buf, size);
}

char* m_prepbuffsize(m_StrBuf* buf, size_t size)
{
    if (cast(size_t, buf->end - buf->p) < size)
        return extendstrbuff(buf, size - (buf->end - buf->p), -1);
    return buf->p;
}

void m_buffaddlenstring(m_StrBuf* buf, const char* s, size_t len)
{
    M_ASSERT(s != NULL);
    M_ASSERT(len > 0);
    if (cast(size_t, buf->end - buf->p) < len)
        extendstrbuff(buf, len - (buf->end - buf->p), -1);

    memcpy(buf->p, s, len);
    buf->p += len;
}

void m_buffaddchar(m_StrBuf* buf, char c)
{
    if (!(buf->p < buf->end))
        m_prepbuffsize(buf, 1);

    *buf->p++ = c;
}

void m_buffaddvalue(m_StrBuf* buf)
{
    VM* vm = buf->vm;

    size_t len;
    char* s;
    if (s = stack_tolenstring(vm, -1, &len))
    {
        if (cast(size_t, buf->end - buf->p) < len)
            extendstrbuff(buf, len - (buf->end - buf->p), -2);

        memcpy(buf->p, s, len);
        buf->p += len;

        stack_pop(vm, 1);
    }
}

void m_buffaddvalueany(m_StrBuf* buf, int idx)
{
    VM* vm = buf->vm;
    size_t len;
    stack_ptolenstring(vm, idx, &len);

    // note: m_buffaddlenstring assumes box is stored at top of stack, so we can't call it here
    // instead we use m_buffaddvalue which will take the string from the top of the stack and add that
    m_buffaddvalue(buf);
}

void m_buffpushresult(m_StrBuf* buf)
{
    VM* vm = buf->vm;

    ObjString* storage = buf->storage;
    if (storage)
    {
        if (buf->p == buf->end)
        {
            setstring(vm->top - 1, bufend_string(vm, storage));
        }
        else
        {
            setstring(vm->top - 1, allocate_string(vm, buf->storage->chars, buf->p - buf->storage->chars));
        }
    }
    else
    {
        stack_pushlenstring(vm, buf->buffer, buf->p - buf->buffer);
    }
}

bool m_buffwrite(m_StrBuf* buf, FILE* stream)
{
    const char* data = m_buffdata(buf);
    size_t len = m_bufflen(buf);
    size_t write = fwrite(data, 1, len, stream);
    if (write != len) return false;
    return true;
}

void print_rvalue(Value* v, bool newLine)
{
    if (isint(v))
    {
        printf("int -> %"PRId64, v->i);
    }
    else if (isfloat(v))
    {
        char buffer[50];
        //sprintf(buffer, "%.17g", v.f);
        sprintf_s(buffer, 50, "%.17g", v->f);
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
        printf("boolean -> %s", (v->b == true) ? "true" : "false");
    }
    else if (isnil(v))
    {
        printf("nil");
    }
    else if (isvoid(v))
    {
        printf("void");
    }
    else if (isstring(v))
    {
        ObjString* string = (ObjString*)v->obj;
        printf("string: length %zu, hash %"PRIu32", text '%s'", string->length, string->hash, string->chars);
    }
    else if (islist(v))
    {
        ObjList* list = (ObjList*)v->obj;
        printf("list: capacity %d, length %d, fixed %s, values ", list->capacity, list->length, (list->fixed == true) ? "true" : "false");
        printf("[");
        for (int i = 0; i < list->capacity; i++)
        {
            if (i > 0)
                printf(", ");
            if (islist(&list->values[i]) && list->values[i].obj == list)
            {
                printf("<recursion>");
            }
            else
            {
                print_rvalue(&list->values[i], false);
            }
        }
        printf("]");
    }
    else if (isdict(v))
    {
        ObjDict* dict = (ObjDict*)v->obj;
        printf("dictionary: capacity %d, count %d, fixed %s, values {", dict->capacity, dict->count, (dict->fixed == true) ? "true" : "false");
        bool comma = false;
        for (int i = 0; i < dict->capacity; i++)
        {
            DictEntry* entry = &dict->entries[i];
            if (isvoid(&entry->key) || isvoid(&entry->value))
                continue;
            if (comma)
                printf(", ");
            else
                comma = true;
            printf("[");
            if (isdict(&entry->key) && entry->key.obj == dict)
            {
                printf("<recursion>");
            }
            else
            {
                print_rvalue(&entry->key, false);
            }
            printf("] = ");
            if (isdict(&entry->value) && entry->value.obj == dict)
            {
                printf("<recursion>");
            }
            else
            {
                print_rvalue(&entry->value, false);
            }
        }
        printf("}");
    }
    else if (isclosure(v))
    {
        Closure* clo = (Closure*)v->obj;
        if (clo->isC)
            printf("<function: %s>", clo->C.name ? clo->C.name : "??");
        else
            printf("<function: %s>", clo->M.chunk->name ? clo->M.chunk->name : "??");
    }
    else
    {
        printf("ERROR: Type %d unrecognize\n", ttype(v));
    }

    if (newLine == true)
        printf("\n");
}

