#include "err.h"
#include "../vm/vm.h"

// Locales

static void printStartFormatError(const char* name)
{
    printf("\033[1;31m");
    printf("%s: ", name);
    //print_without_end(name);
    printf("\033[0m");
    printf("\033[31m");
}

static void printStartRuntimeFormatError(const char* name, int line)
{
    printf("\033[1;31m");
    printf("Runtime Error at %s:%d:", name, line);
    printf("\033[0m");
    printf("\033[31m");
}

static void printEndFormatError(Location location)
{
    printf("at line %d column %d ", location.begin.line, location.begin.column);
    if (location.begin.line == location.end.line)
        printf("to column %d", location.end.column);
    else
        printf("to line %d column %d", location.end.line, location.end.column);
    printf("\033[0m");
    printf("\n");
}

static void printEndToCloseFormatError(Location l1, Location l2)
{
    if (l1.begin.line != l2.begin.line)
        printf(" at line %d, column %d", l1.begin.line, l1.begin.column);
    else
        printf(" at column %d", l1.begin.column);
}

// Globals
/* Note that this function does not terminate the program flow */
void memoryCrash(const char* src)
{
    printStartFormatError(src);
    printf("\033[0m");
    printf("\033[4;31m");
    printf("Memory Crash:");
    printf("\033[0m");
    printf("\033[31m");
    printf(" ");
    printf("Error de memoria en %s", src);
    printf("\033[0m");
    printf("\n");
    //printf("Error de memoria en %s\n", src);
}

void printErr(const char* text, const char* src, int level)
{
    level = level == -1 ? 3 : level;
    printf("\033[1;31m");
    printf("%s:", src);
    printf("\033[0m");
    printf("\033[31m");
    printf(" Error: %s", text);
    printf("\033[0m");
    printf("\n");
}

void printWarn(const char* text, const char* src, int level)
{
    printf("\033[4;33m");
    printf("%s:", src);
    printf("\033[0m");
    printf("\033[33m");
    printf(" Warning: %s", text);
    printf("\033[0m");
    printf("\n");
}

void printTrace(const char* text, const char* src, int level)
{
    printf("\033[3;36m");
    printf("%s:", src);
    printf(" Stack trace: %s", text);
    printf("\033[0m");
    printf("\n");
}

// Lexer Print Errors
void illegalCharacter(const char character, const char* name, Location location)
{
    printStartFormatError(name);
    printf("\033[0m");
    printf("\033[4;31m");
    printf("IllegalCharacterError:");
    printf("\033[0m");
    printf("\033[31m");
    printf(" Illegal character '%c' ", character);
    printEndFormatError(location);
}

// Parser Errors
void syntaxError(const char* message, const char* name, Location location)
{
    printStartFormatError(name);
    printf("\033[0m");
    printf("\033[4;31m");
    printf("SyntaxError:");
    printf("\033[0m");
    printf("\033[31m");
    printf(" %s", message);
    printEndFormatError(location);
}

void expectedButGot(const char* expected, const char* got, const char* context, const char* name, Location location)
{
    printStartFormatError(name);
    printf("\033[0m");
    printf("\033[4;31m");
    printf("SyntaxError:");
    printf("\033[0m");
    printf("\033[31m");
    printf(" Expected '%s', but got '%s'%s ", expected, got, context != NULL ? context : "");
    printEndFormatError(location);
}

void expectedToClose(const char* expected, const char* close, const char* got, const char* context, const char* name, Location begin, Location exp)
{
    printStartFormatError(name);
    printf("\033[0m");
    printf("\033[4;31m");
    printf("SyntaxError:");
    printf("\033[0m");
    printf("\033[31m");
    printf(" Expected '%s' to close '%s'", expected, close);
    printEndToCloseFormatError(begin, exp);
    printf(", but got '%s'%s ", got, context != NULL ? context : "");
    //printEndFormatError(exp);
    printEndToCloseFormatError(exp, begin);
    printf("\033[0m");
    printf("\n");
}

// Compiler Error
void compilerError(const char* message, const char* name, Location location, ...)
{
    printStartFormatError(name);
    printf("\033[0m");
    printf("\033[4;31m");
    printf("Compiler Error:");
    printf("\033[0m");
    printf("\033[31m");
    printf(" ");
    va_list args;
    va_start(args, message);
    vprintf(message, args);
    va_end(args);
    printf(" ");
    printEndFormatError(location);
    exit(1);
}

// Runtime errors
void vm_runtime_raise(VM* vm, RErrorType type, int line, const char* format, ...)
{
    if (line <= 0)
        line = vm_current_line(vm);

    vm->error.line = line;
    vm->error.type = type;
    vm->error.has_error = true;

    va_list args;
    va_start(args, format);
    vsnprintf(vm->error.message, sizeof(vm->error.message), format, args);
    va_end(args);
}

//void runtimeError(const char* message, const char* name, int line, ...)
//{
//    printStartRuntimeFormatError(name, line);
//    printf("\033[0m");
//    printf("\033[4;31m");
//    printf("\033[0m");
//    printf("\033[31m");
//    printf(" ");
//    va_list args;
//    va_start(args, message);
//    vprintf(message, args);
//    va_end(args);
//    printf(" ");
//    printf("\033[0m");
//    printf("\n");
//    exit(1);
//}

void invalidOperandsError(VM* vm, int line, const char* op, const char* type1, const char* type2)
{
    vm_runtime_raise(vm, INVALID_OPERANDS_ERROR, line,
    "TypeError: Invalid operands '%s' and '%s' for operator '%s'", 
     type1 != NULL ? type1 : "", 
     type2 != NULL ? type2 : "",
     op);
}

void typeError(VM* vm, int line, const char* type1, const char* type2)
{
    vm_runtime_raise(vm, TYPES_ERROR, line,
    "TypeError: Expected '%s', but got '%s'", 
    type1, type2);
}

void arithmeticError(VM* vm, int line)
{
    vm_runtime_raise(vm, ARITHMETIC_ERROR, line,
    "ArithmeticError: Division By Zero");
}

void memoryError(VM* vm, int line)
{
    vm_runtime_raise(vm, MEMORY_ERROR, line,
    "MemoryError: An error occurred in memory allocation");
}

void unknownType(VM* vm, int line, int type)
{
    vm_runtime_raise(vm, INTERNAL_ERROR, line,
    "InternalError: Unknown Type; Number Enum Type %d", type);
}

void cannotAddElementNotList(VM* vm, int line, const char* type)
{
    vm_runtime_raise(vm, ASSIGNMENT_ERROR, line,
    "AssignmentError: Cannot assign an item to a non-list; Got type %s", type);
}

void cannotResizeList(VM* vm, int line)
{
    vm_runtime_raise(vm, RESIZE_LIST_ERROR, line,
    "ResizeListError: Cannot resize a fixed list");
}

void cannotResizeDict(VM* vm, int line)
{
    vm_runtime_raise(vm, RESIZE_DICT_ERROR, line,
    "ResizeDictError: Cannot resize a fixed Dictionary");
}

void resizeFractured(VM* vm, int line, const char* complement)
{
    vm_runtime_raise(vm, INTERNAL_ERROR, line,
    "InternalError: Something was wrong when try to resize the %s", complement);
}

void indexoutofbound(VM* vm, int line, int pos, int capacity)
{
    vm_runtime_raise(vm, INDEX_OUT_OF_BOUNDS_ERROR, line,
    "IndexOutOfBoundsError: Tried to index position %d when length is %d", pos, capacity);
}

void attempedToIndexNoCollection(VM* vm, int line, const char* type)
{
    vm_runtime_raise(vm, ATTEMPED_TO_INDEX_NO_COLLECTION_ERROR, line,
    "AttempedToIndexNoCollection: Attemped to index %s instead of collection", type);
}

void indexError(VM* vm, int line, const char* expected, const char* got)
{
    vm_runtime_raise(vm, INDEX_ERROR, line,
    "IndexError: Expected index type '%s', but got '%s'", expected, got);
}

void invalidKeyType(VM* vm, int line, const char* expected, const char* got)
{
    vm_runtime_raise(vm, INVALID_KEY_TYPE_ERROR, line,
    "InvalidKeyTypeError: Expected types '%s' to dict key, but got type '%s'", expected, got);
}
