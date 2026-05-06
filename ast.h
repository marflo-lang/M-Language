#pragma once
#include <stdbool.h>
#include "lexer.h"


typedef struct
{
    int x;
} AST;

// Nodo base del que TODOS los nodos 'heredan'

typedef enum
{
    NODE_EXPR,
    NODE_STMT
} NodeType;

typedef struct
{
    Location location;
    NodeType ttype;
} Node;

// Nodo base de las expresiones del cual todas las EXPRESIONES 'heredan'

typedef enum
{
    EXPR_BINARY,
    EXPR_UNARY,
    EXPR_LITERAL,
    EXPR_NAME,
    EXPR_PREFIX,
    EXPR_POSTFIX,
    EXPR_ERROR,
} ExprType;

typedef struct
{
    Node base;
    ExprType expr_type;
} Expr;

// Diferentes nodos de expresiones

typedef struct
{
    Expr expr;

    Token token;
    const char* message;
} ErrorExpr;

typedef struct
{
    Expr expr;

    Expr* left;
    Token op;
    Expr* right;
} BinaryExpr;

typedef struct
{
    Expr expr;

    Token op;
    Expr* right;
} UnaryExpr;

typedef struct
{
    Expr expr;

    Expr* target;
    Token op;
    bool isPre;
} FixExpr;

typedef struct
{
    Expr expr;

    Token name;
} NameExpr;

typedef struct
{
    Expr expr;

    Token value;
} LiteralExpr;

// Nodo base de las sentencias del cual todas las SENTENCIAS 'heredan'

typedef enum
{
    STMT_IF,
    STMT_VAR,
    //STMT_CONST,
    STMT_EXPR,
    STMT_ASSING,
    STMT_BLOCK,
    STMT_COMPOUND_ASSING,
    STMT_ERROR,
} StmtType;

typedef struct
{
    Node base;
    StmtType stmt_type;
} Stmt;

// Diferentes nodos de sentencias

typedef struct
{
    Stmt stmt;

    Node* nodo;
    const char* message;
} StmtError;

typedef struct
{
    Stmt stmt;

    bool isConst;

    Token* names;
    int namesCount;

    Expr** values;
    int valuesCount;
} StmtVar;

typedef struct
{
    Stmt stmt;

    Token* names;
    int nameCount;

    Expr** values;
    int valueCount;
} StmtAssign;

typedef struct
{
    Stmt stmt;

    Expr* target;
    Token op;
    Expr* value;
} StmtCompoundAssing;

typedef struct
{
    Stmt stmt;

    Expr* expr;
} StmtExpr;

typedef struct
{
    Stmt stmt;

    Expr* condition;
    Stmt* ifBranch;

    Stmt* elseBranch;
} StmtIf;

typedef struct
{
    Stmt stmt;

    Stmt** statements;
    int count;
} StmtBlock;



#define is_expr_terminator(x)   ((x) == M_EOF || (x) == M_RBRACE || (x) == M_RPAREN || (x) == M_COMMA || (x) == M_SEMICOLON)
//#define is_expr_terminator(x)   ((x) >= M_PLUS && (x) <= M_CONCAT || (x) >= M_INC && (x) <= M_RPAREN  /*|| (x) == M_SEMICOLON || ((x) >= M_VAR && (x) <= M_ELSE)*/)

