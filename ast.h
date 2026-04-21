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
    EXPR_POSTFIX
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
    STMT_COMPOUND_ASSING
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





