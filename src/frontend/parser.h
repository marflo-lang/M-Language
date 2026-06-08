#pragma once

#include "m.h"
#include "lexer.h"
#include "ast.h"
#include "../utils/allocator.h"

#include <stdio.h>

typedef struct
{
    TokenArray* Tokens;
    const char* name; // script name
    const char* src; // code for errors
    int count;
    int pos;

    Token current;
    Token previous;

    Arena* arena;

    bool hadError;
} Parser;

typedef enum
{
    PREC_NONE,
    PREC_OR, // or
    PREC_AND, // and
    PREC_EQUALITY, // ==, !=
    PREC_COMPARISON, // <=, <, >=, >
    PREC_CONCAT, // <>
    PREC_TERM, // +, -
    PREC_FACTOR, // *, /, // %
    PREC_POWER, // ^
    PREC_UNARY, // not, -
    PREC_POSTFIX, // ++, --
    PREC_ACCESS, // ., [, {, (
    PREC_PRIMARY
} Precedence;

typedef Expr* (*PrefixFn)(Parser*);
typedef Expr* (*InfixFn)(Parser*, Expr*);

typedef struct
{
    PrefixFn prefix;
    InfixFn infix;
    Precedence precedence;
} ParserRule;

// Funciones Globales del Parser

Parser* parser_init(TokenArray* Tokens, Arena* A, const char* name, const char* src);

Stmt* parser_execute(Parser* P);

#ifdef DEBUG
void parser_print(Parser* P, Stmt* block);
#endif // DEBUG

