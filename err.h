#pragma once
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#endif

void printErr(const char* text, const char* src, int level);

void printWarn(const char* text, const char* src, int level);

void printTrace(const char* text, const char* src, int level);

