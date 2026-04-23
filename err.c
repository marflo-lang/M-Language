#include "err.h"

// Locales

static void printStartFormatError(const char* name)
{
    printf("\033[1;31m");
    printf("%s: ", name);
    //print_without_end(name);
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
        printf("to line %d, column %d", location.end.line, location.end.column);
    printf("\033[0m");
    printf("\n");
}

// Globals

void memoryCrash(const char* src)
{
    print("Error de memoria en ", src);
}

void printErr(const char* text, const char* src, int level)
{
    level = level == NULL ? 3 : level;
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


