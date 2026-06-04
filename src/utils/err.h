#pragma once
#include "print.h"
#include "../frontend/lexer.h"

#include <stdio.h>
#include <stdarg.h>
#ifdef _WIN32
#include <windows.h>
#endif

// Globales
void memoryCrash(const char* src);

void printErr(const char* text, const char* src, int level);

void printWarn(const char* text, const char* src, int level);

void printTrace(const char* text, const char* src, int level);

// Errores del Lexer
void illegalCharacter(const char character, const char* name, Location location);

// Errores del Parser
void syntaxError(const char* message, const char* name, Location location);
void expectedButGot(const char* expected, const char* got, const char* context, const char* name, Location location);
void expectedToClose(const char* expected, const char* close, const char* got, const char* context, const char* name, Location begin, Location exp);

// Errores del Compiler
void compilerError(const char* message, const char* name, Location location, ...);

// Errores del runtime (marcan vm->error; no terminan el proceso)
typedef struct VM VM;
//void runtimeError(const char* message, const char* name, int line, ...);
void invalidOperandsError(VM* vm, int line, const char* op, const char* type1, const char* type2);
void typeError(VM* vm, int line, const char* type1, const char* type2);
void arithmeticError(VM* vm, int line);
void memoryError(VM* vm, int line);
void unknownType(VM* vm, int line, int type);
void cannotAddElementNotList(VM* vm, int line, const char* type);
void cannotResizeList(VM* vm, int line);
void cannotResizeDict(VM* vm, int line);
void resizeFractured(VM* vm, int line, const char* complement);
void indexoutofbound(VM* vm, int line, int pos, int capacity);
void attempedToIndexNoCollection(VM* vm, int line, const char* type);
void indexError(VM* vm, int line, const char* expected, const char* got);
void invalidKeyType(VM* vm, int line, const char* expected, const char* got);
