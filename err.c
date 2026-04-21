#include "err.h"

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