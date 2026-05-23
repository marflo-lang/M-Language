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
    //print("at line", location.begin.line, "column", location.begin.column, "to column", location.end.column);
    //print_without_end("at line")
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

void memoryCrash(const char* src)
{
    //print("Error de memoria en ", src);
    printf("Error de memria en %s", src);
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
void runtimeError(const char* message, const char* name, int line, ...)
{
    printStartRuntimeFormatError(name, line);
    printf("\033[0m");
    printf("\033[4;31m");
    printf("\033[0m");
    printf("\033[31m");
    printf(" ");
    va_list args;
    va_start(args, message);
    vprintf(message, args);
    va_end(args);
    printf(" ");
    printf("\033[0m");
    printf("\n");
    exit(1);
}

static void vm_raise_error(VM* vm, RErrorType type, int line, const char* format, ...)
{
    vm->error.has_error = true;
    vm->error.line = line;
    vm->error.type = type;
    
    va_list args;
    va_start(args, format);
    vsnprintf(vm->error.message, sizeof(vm->error.message), format, args);

    va_end(args);
}

void invalidOperandsError(const char* name, int line, const char* op, const char* type1, const char* type2)
{
    //vm_raise_error(vm, INVALID_OPERANDS_ERROR, line)
    runtimeError("TypeError: Invalid operands '%s' and '%s' for operator '%s'", name, line, type1 != NULL ? type1 : "", type2 != NULL ? type2 : "", op);
}

void typeError(const char* name, int line, const char* type1, const char* type2)
{
    runtimeError("TypeError: Expected '%s', but got '%s'", name, line, type1, type2);
}

void arithmeticError(const char* name, int line)
{
    runtimeError("ArithmeticError: Division By Zero", name, line);
}

void memoryError(const char* name, int line)
{
    runtimeError("MemoryError: An error occurred in memory allocation", name, line);
}

void unknownType(const char* name, int line, int type)
{
    runtimeError("InternalError: Unknown Type; Number Enum Type %d", name, line, type);
}

void cannotAddElementNotList(const char* name, int line, const char* type)
{
    runtimeError("AssignmentError: Cannot assign an item to a non-list; Got type %s", name, line, type);
}

void cannotResizeList(const char* name, int line)
{
    runtimeError("ResizeListError: Cannot resize a fixed list", name, line);
}

void cannotResizeDict(const char* name, int line)
{
    runtimeError("ResizeDictError: Cannot resize a fixed Dictionary", name, line);
}

void resizeFractured(const char* name, int line, const char* complement)
{
    runtimeError("ResizeFracturedError: Something was wrong when try to resize the %s", name, line, complement);
}

void indexoutofbound(const char* name, int line, int pos, int capacity)
{
    runtimeError("IndexOutOfBoundsError: Tried to index position %d when length is %d", name, line, pos, capacity);
}

void attempedToIndexNoCollection(const char* name, int line, const char* type)
{
    runtimeError("AttempedToIndexNoCollection: Attemped to index %s instead of collection", name, line, type);
}

void indexError(const char* name, int line, const char* expected, const char* got)
{
    runtimeError("IndexError: Expected index type '%s', but got '%s'", name, line, expected, got);
}

void invalidKeyType(const char* name, int line, const char* expected, const char* got)
{
    runtimeError("InvalidKeyTypeError: Expected types '%s' to dict key, but got type '%s'", name, line, expected, got);
}
