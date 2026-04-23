#pragma once
#include "lexer.h"
#include "print.h"
#include <stdio.h>
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

