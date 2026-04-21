/*#pragma once
//#include <stdbool.h>
#include <stdio.h>
#include "lexer.h"
#include "ast.h"
#include "allocator.h"

typedef struct
{
    TokenArray* Tokens;
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
    PREC_CONCAT, // <>
    PREC_OR, // or
    PREC_AND, // and
    PREC_EQUALITY, // ==, !=
    PREC_COMPARISON, // <=, <, >=, >
    PREC_TERM, // +, -
    PREC_FACTOR, // *, /, // %
    PREC_POWER, // ^
    PREC_UNARY, // not, -
    PREC_POSTFIX, // ++, --
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


ParserRule rules[] = {
    [M_V_INT] = {parser_int, NULL, PREC_NONE},
    [M_V_STRING] = {parser_string, NULL, PREC_NONE},
    [M_V_TRUE] = {parser_literal, NULL, PREC_NONE},
    [M_V_FALSE] = {parser_literal, NULL, PREC_NONE},

    [M_PLUS] = {NULL, parser_binary, PREC_TERM},
    [M_MINUS] = {NULL, parser_binary, PREC_TERM},

    [M_STAR] = {NULL, parser_binary, PREC_FACTOR},
    [M_SLASH] = {NULL, parser_binary, PREC_FACTOR},
    [M_MOD] = {NULL, parser_binary, PREC_FACTOR},

    [M_POW] = {NULL, parser_binary, PREC_POWER},

    [M_CONCAT] = {NULL, parser_binary, PREC_CONCAT},

    //[M_INC] = {},


};

// Funciones Globales del Parser

Parser* parser_init(TokenArray* Tokens, Arena* A);

AST* parser_execute(Parser* P);

void parser_print(Parser* P, AST* ast);

*/