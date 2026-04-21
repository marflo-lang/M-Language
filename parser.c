/*#include <malloc.h>
#include "parser.h"
#include "err.h"

//ParserRule rules[] = {
//    [M_V_INT] = {parser_int, NULL, PREC_NONE},
//    [M_V_STRING] = {parser_string, NULL, PREC_NONE},
//    [M_V_TRUE] = {parser_literal, NULL, PREC_NONE},
//    [M_V_FALSE] = {parser_literal, NULL, PREC_NONE},
//
//    [M_PLUS] = {NULL, parser_binary, PREC_TERM},
//    [M_MINUS] = {NULL, parser_binary, PREC_TERM},
//
//    [M_STAR] = {NULL, parser_binary, PREC_FACTOR},
//    [M_SLASH] = {NULL, parser_binary, PREC_FACTOR},
//    [M_MOD] = {NULL, parser_binary, PREC_FACTOR},
//
//    [M_POW] = {NULL, parser_binary, PREC_POWER},
//
//    [M_CONCAT] = {NULL, parser_binary, PREC_CONCAT},
//
//    //[M_INC] = {},
//
//
//};

static void advance(Parser* P)
{
    P->previous = P->current;
    P->current = P->Tokens->data[P->pos++];
}

static Token peek(Parser* P)
{
    if (P->pos + 1 >= P->Tokens->count)
        return P->Tokens->data[P->Tokens->count];
    return P->Tokens->data[P->pos + 1];
}

static bool match(Parser* P, LTokenType ttype)
{
    if (P->current.type == ttype)
    {
        advance(P);
        return true;
    }
    return false;
}

static void consume(Parser* P, LTokenType ttype, const char* message)
{
    if (P->current.type == ttype)
    {
        advance(P);
    }
    else
    {
        //advance(P);
        // error

    }
}

/*
    EXPRESSIONS
*-/

static Expr* parser_expression(Parser* P)
{
    return parser_precedence(P, PREC_NONE);
}

static Expr* parser_precedence(Parser* P, Precedence prec)
{
    advance(P);

    PrefixFn prefix = get_rule(P->previous.type)->prefix;
    if (!prefix) { /* error message *-/ }

    Expr* left = prefix(P);

    while (prec <= get_rule(P->current.type)->precedence)
    {
        advance(P);

        InfixFn infix = get_rule(P->previous.type)->infix;
        left = infix(P, left);
    }

    return left;
}

static ParserRule* get_rule(LTokenType ttype)
{
    return &rules[ttype];
}

static void parser_int(Parser* P)
{
    return new_literal(P, P->previous);
}

static void parser_float(Parser* P)
{
    return new_literal(P, P->previous);
}

static void parser_string(Parser* P)
{
    return new_literal(P, P->previous);
}

static void parser_literal(Parser* P)
{
    return new_literal(P, P->previous);
}

static Expr* new_literal(Parser* P, Token token)
{
    LiteralExpr* expr = (LiteralExpr*) arena_allocator(P->arena, sizeof(LiteralExpr));

    expr->expr.base.ttype = NODE_EXPR;
    expr->expr.base.location = token.location;
    expr->expr.expr_type = EXPR_LITERAL;
    expr->value = token;

    return (Expr*) expr;
}

static Expr* parser_binary(Parser* P, Expr* left)
{
    Token op = P->previous;

    ParserRule* rule = get_rule(op.type);
    Precedence prec = rule->precedence;

    bool rightAssoc = (op.type == M_POW);

    Expr* rigth = parser_precedence(
        P,
        rightAssoc ? prec : prec + 1
    );

    return new_binary(P, left, op, rigth);
}

static Expr* new_binary(Parser* P, Expr* left, Token op, Expr* right)
{
    BinaryExpr* binary = (BinaryExpr*) arena_allocator(P->arena, sizeof(BinaryExpr));

    binary->expr.base.location = locationCPos(left->base.location.begin, right->base.location.end);
    binary->expr.base.ttype = NODE_EXPR;
    binary->expr.expr_type = EXPR_BINARY;
    binary->left = left;
    binary->op = op;
    binary->right = right;

    return (Expr*) binary;
}

static Expr* parser_unary(Parser* P)
{
    Token op = P->previous;

    Expr* right = parser_precedence(P, PREC_UNARY);

    return new_unary(P, right, op);
}

static Expr* new_unary(Parser* P, Expr* right, Token op)
{
    UnaryExpr* unary = (UnaryExpr*) arena_allocator(P->arena, sizeof(UnaryExpr));

    unary->expr.base.location = locationCPos(op.location.begin, right->base.location.end);
    unary->expr.base.ttype = NODE_EXPR;
    unary->expr.expr_type = EXPR_UNARY;
    unary->op = op;
    unary->right = right;

    return (Expr*) unary;
}

static Expr* parser_prefix_int(Parser* P)
{
    Token op = P->previous;

    Expr* right = parser_precedence(P, PREC_UNARY);

    return new_fix(P, op, right, true);
}

static Expr* parser_posfix(Parser* P)
{
    Token op = P->previous;

    Expr* right = parser_precedence(P, PREC_UNARY);

    return new_fix(P, op, right, false);
}

static Expr* new_fix(Parser* P, Token op, Expr* right, bool isPre)
{
    FixExpr* fix = arena_allocator(P->arena, sizeof(FixExpr));

    fix->expr.base.location = locationCPos(op.location.begin, right->base.location.end);
    fix->expr.base.ttype = NODE_EXPR;
    fix->expr.expr_type = isPre ? EXPR_PREFIX : EXPR_POSTFIX;
    fix->isPre = isPre;
    fix->op = op;
    fix->target = right;

    return (Expr*) fix;
}

static Expr* parser_grouping(Parser* P)
{
    Expr* expr = parser_expression(P);
    consume(P, M_RPAREN, "Expected ')'");

    return expr;
}

static Expr* parser_name_expr(Parser* P, Token name)
{
    NameExpr* expr = arena_allocator(P->arena, sizeof(NameExpr));

    expr->expr.base.location = name.location;
    expr->expr.base.ttype = NODE_EXPR;
    expr->expr.expr_type = EXPR_NAME;
    expr->name = name;

    return (Expr*) expr;
}

static Expr* new_variable_expr(Parser* P)
{

}

/*
    STATEMENTS
*-/

static Stmt* parser_statement(Parser* P)
{
    if (match(P, M_VAR)) return parser_var(P, false);
    if (match(P, M_CONST)) return parser_var(P, true);
    if (match(P, M_IF)) return parser_if(P);
}

static Stmt* parser_var(Parser* P, bool isConst)
{
    Position begin = P->previous.location.begin;
    Token names[8];
    int capacity = 8;
    int nameCount = 0;

    names[nameCount++] = P->previous;
    consume(P, M_V_IDENTIFIER, "Expected identifier name");

    while (match(P, M_COMMA))
    {
        if (nameCount + 1 >= nameCount)
        {
            
        }
        names[nameCount++] = P->current;
        consume(P, M_V_IDENTIFIER, "Expected identifier name");
    }

    Expr** values = NULL;

}

static Stmt* parser_ChooseCompoundOrAssing(Parser* P)
{
    if (P->previous.type == M_V_IDENTIFIER)
    {
        if (P->current.type == M_ASSING)
        {
            Expr* left = parser_expression(P->previous);
            return parser_assing(P, left);
        }
        else if (P->current.type >= M_PLUS_ASSING && P->current.type <= M_MOD_ASSING)
        {

        }
        else
        {
            // error
        }
    }
    else
    {
        // error
    }
}

static Stmt* parser_assing(Parser* P, Expr* left)
{

}

static Stmt* parser_compound_assing(Parser* P, Expr* left)
{
    Position begin = P->previous.location.begin;
    Token op = P->current;
    advance(P);

    Expr* right = parser_expression(P);

    Position end = right->base.location.end;

    StmtCompoundAssing* stmt = arena_allocator(P->arena, sizeof(StmtCompoundAssing));


    stmt->stmt.base.location = locationCPos(begin, end);
    stmt->stmt.base.ttype = NODE_STMT;
    stmt->stmt.stmt_type = STMT_COMPOUND_ASSING;
    stmt->op = op;
    stmt->target = left;
    stmt->value = right;

    return (Stmt*) stmt;
}

static Stmt* parser_if(Parser* P)
{
    Position begin = P->previous.location.begin;
    Expr* condition = parser_expression(P);
    Stmt* ifBranch = parser_statement(P);

    Stmt* elseBranch = NULL;

    if (match(P, M_ELSEIF))
        elseBranch = parser_if(P);
    else if (match(P, M_ELSE))
        elseBranch = parser_statement(P);

    StmtIf* stmt = arena_allocator(P->arena, sizeof(StmtIf));
    
    Position end = elseBranch != NULL ? elseBranch->base.location.end : ifBranch->base.location.end;

    stmt->stmt.base.location = locationCPos(begin, end);
    stmt->stmt.base.ttype = NODE_STMT;
    stmt->stmt.stmt_type = STMT_IF;
    stmt->condition = condition;
    stmt->ifBranch = ifBranch;
    stmt->elseBranch = elseBranch;

    return (Stmt*) stmt;
}

Parser* parser_init(TokenArray* Tokens, Arena* A)
{
    Parser* P = malloc(sizeof(Parser));
    if (P == NULL)
    {
        printErr("Error de memoria", "Parser", 3);
        exit(1);
    }

    P->Tokens = Tokens->data;
    P->arena = A;
    P->hadError = false;
    P->pos = -1;
    P->count = Tokens->count;
    advance(P);
    return P;
}

AST* parser_execute(Parser* P)
{

}

void parser_print(Parser* p, AST* ast)
{

}


*/