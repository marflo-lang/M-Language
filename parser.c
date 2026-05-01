#include <malloc.h>
#include "parser.h"
#include "err.h"

// ===============================
// Forward declarations
// ===============================

// Pratt parser
static Expr* parser_precedence(Parser* P, Precedence prec);
static ParserRule* get_rule(LTokenType type);

// Prefix / Infix
static Expr* parser_int(Parser* P);
static Expr* parser_float(Parser* P);
static Expr* parser_string(Parser* P);
static Expr* parser_literal(Parser* P);
static Expr* parser_identifier(Parser* P);
static Expr* parser_grouping(Parser* P);
static Expr* parser_unary(Parser* P);
static Expr* parser_binary(Parser* P, Expr* left);
static Expr* parser_prefix(Parser* P);
static Expr* parser_posfix(Parser* P, Expr* left);
static Expr* parser_expr_error(Parser* P);

// Constructors
static Expr* new_literal(Parser* P, Token token);
static Expr* parser_name_expr(Parser* P, Token name);
//static Expr* new_variable_expr(Parser* P, Token token);
static Expr* new_binary(Parser* P, Expr* left, Token op, Expr* right);
static Expr* new_unary(Parser* P, Expr* right, Token op);
static Expr* new_fix(Parser* P, Token op, Expr* right, bool isPre);

// Statements
static Stmt* parser_var(Parser* P, bool isConst);
static Stmt* parser_if(Parser* P);
static Stmt* parser_block(Parser* P);
static Stmt* parser_expr_or_assignment_stmt(Parser* P);
static Stmt* parser_assign(Parser* P, Expr* left);
static Stmt* parser_stmt_error(Parser* P, Expr* expr);
//static Stmt* parser_multi_assign(Parser* P, Expr* first);


#ifdef DEBUG
// Debug
static void print_branch(const char* prefix, bool isLast);
static void build_prefix(char* buffer, const char* prefix, bool isLast);
static void print_expr(Parser* P, Expr* e, const char* prefix, bool isLast);
static void print_stmt(Parser* P, Stmt* stmt, const char* prefix, bool isLast);
#endif // DEBUG


ParserRule rules[] = {
    // int, float, string, boolean, identifier, (
    [M_V_INT] = {parser_int, NULL, PREC_NONE},
    [M_V_FLOAT] = {parser_float, NULL, PREC_NONE},
    [M_V_MALFORMED_NUMBER] = {parser_expr_error, NULL, PREC_NONE},
    [M_V_STRING] = {parser_string, NULL, PREC_NONE},
    [M_V_UNFINISHED_STRING] = {parser_expr_error, NULL, PREC_NONE},
    [M_V_TRUE] = {parser_literal, NULL, PREC_NONE},
    [M_V_FALSE] = {parser_literal, NULL, PREC_NONE},
    [M_V_IDENTIFIER] = {parser_identifier, NULL, PREC_NONE},
    [M_LPAREN] = {parser_grouping, parser_grouping, PREC_NONE},

    // <>
    [M_CONCAT] = {NULL, parser_binary, PREC_CONCAT},

    // or
    [M_OR] = {NULL, parser_binary, PREC_OR},

    // and
    [M_AND] = {NULL, parser_binary, PREC_AND},

    // ==, !=
    [M_EQ] = {NULL, parser_binary, PREC_EQUALITY},
    [M_NEQ] = {NULL, parser_binary, PREC_EQUALITY},

    // <, <=, >, >=
    [M_LT] = {NULL, parser_binary, PREC_COMPARISON},
    [M_LTE] = {NULL, parser_binary, PREC_COMPARISON},
    [M_GT] = {NULL, parser_binary, PREC_COMPARISON},
    [M_GTE] = {NULL, parser_binary, PREC_COMPARISON},

    // +, -
    [M_PLUS] = {NULL, parser_binary, PREC_TERM},
    [M_MINUS] = {parser_unary, parser_binary, PREC_TERM},

    // *, /, //, %
    [M_STAR] = {NULL, parser_binary, PREC_FACTOR},
    [M_SLASH] = {NULL, parser_binary, PREC_FACTOR},
    [M_FLOOR_DIV] = {NULL, parser_binary, PREC_FACTOR},
    [M_MOD] = {NULL, parser_binary, PREC_FACTOR},

    // ^
    [M_POW] = {NULL, parser_binary, PREC_POWER},

    // not
    [M_NOT] = {parser_unary, NULL, PREC_UNARY},

    // ++, --
    [M_INC] = {parser_prefix, parser_posfix, PREC_POSTFIX},
    [M_DEC] = {parser_prefix, parser_posfix, PREC_POSTFIX},

    //[M_EOF] = {NULL, NULL, PREC_NONE},

    //[M_INC] = {},


};

/*
Auxiliary functions
*/
static void advance(Parser* P)
{
    P->previous = P->current;
    if (P->pos + 1 >= P->Tokens->count)
        P->current = P->Tokens->data[P->Tokens->count];
    else
        P->current = P->Tokens->data[++P->pos];
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

static void consume(Parser* P, LTokenType ttype, const char* expected)
{
    if (P->current.type == ttype)
    {
        advance(P);
    }
    else
    {
        //printf("mal\n");
        ////advance(P);
        //// error
        //char* got = "";//(char*)arena_allocator(P->arena, sizeof(P->current.length + 1));
        //strncpy(got, P->src + P->current.location.begin.offset, P->current.length);
        ////strncpy_s(got, P->src + P->current.location.begin.offset, P->current.length);
        ////strncpy();
        //got[P->current.length] = '\0';
        //size_t len = P->current.length;

        //char* got = malloc(len + 1);  // espacio para '\0'
        //if (!got) return; // opcional pero correcto

        //strncpy_s(
        //    got,
        //    len + 1,  // tamaño del buffer destino
        //    P->src + P->current.location.begin.offset,
        //    len
        //);
        ////print_token(P->src, P->current);
        //got[len] = '\0';
        char got = getText(P->current.length, P->src, P->current.location.begin.offset);
        expectedButGot(expected, P->current.type == M_EOF ? "<eof>" : got, NULL, P->name, P->current.location);
        free(got);
    }
}

static bool is_valid_stmt_expr(Expr* e)
{
    switch (e->expr_type)
    {
        case EXPR_PREFIX:
        case EXPR_POSTFIX:
            return true;
        default:
            return false;
    }
}

static bool is_assignment_operator(LTokenType t)
{
    switch (t)
    {
        case M_ASSING: // =
        case M_PLUS_ASSING: // +=
        case M_MINUS_ASSING: // -=
        case M_STAR_ASSING: // *=
        case M_SLASH_ASSING: // /=
        case M_FLOOR_DIV_ASSING: // //=
        case M_CONCAT_ASSING: // <>=
        case M_POW_ASSING: // ^=
        case M_MOD_ASSING: // %=
            return true;
        default:
            return false;
    }
}

static bool isLValue(Expr* e)
{
    switch (e->expr_type)
    {
        case EXPR_NAME:
            return true;
        default:
            return false;
    }
}

/*
    EXPRESSIONS
*/

static Expr* parser_expression(Parser* P)
{
    Expr* expr = parser_precedence(P, PREC_NONE);
    match(P, M_SEMICOLON);
    return expr;
}

static Expr* parser_precedence(Parser* P, Precedence prec)
{
    advance(P);
    //printf("AQUÍ -> %d, %d\n", P->previous.type, P->current.type);
    //ParserRule* rule = get_rule(P->previous.type);
    //if (rule == NULL) 
    //{ 
    //    size_t len = P->previous.length;

    //    char* got = malloc(len + 1);  // espacio para '\0'
    //    if (!got) return; // opcional pero correcto

    //    strncpy_s(
    //        got,
    //        len + 1,  // tamaño del buffer destino
    //        P->src + P->previous.location.begin.offset,
    //        len
    //    );

    //    got[len] = '\0';
    //    expectedButGot("identifier, int, float, string, AAAAAAAAAAAliteral", P->previous.type == M_EOF ? "<eof>" : got, " when parsing expression", P->name, P->previous.location);
    //    free(got);
    //    exit(1);
    //    //printf("nonononoono %d\n", P->previous.type); exit(1);/* error message */ 
    //}

    PrefixFn prefix = get_rule(P->previous.type)->prefix;
    if (prefix == NULL) 
    {
        size_t len = P->previous.length;

        char* got = malloc(len + 1);  // espacio para '\0'
        if (!got) { memoryCrash(P->name); exit(1); } // opcional pero correcto

        strncpy_s(
            got,
            len + 1,  // tamaño del buffer destino
            P->src + P->previous.location.begin.offset,
            len
        );
        got[len] = '\0';
        
        expectedButGot("identifier", P->previous.type == M_EOF ? "<eof>" : got, " when parsing expression", P->name, P->previous.location);
        free(got);
        //exit(1);
        //printf("========line: %d, column: %d, offset: %d==============\n", P->previous.location.end.line, P->previous.location.begin.column, P->previous.location.begin.offset);
        return parser_expr_error(P);
        //printf("nonononoono %d\n", P->previous.type); exit(1);/* error message */ 
    }
    Expr* left = prefix(P);

    ParserRule* newRule = get_rule(P->current.type);

    if (newRule != NULL)
    {
        while (!is_expr_terminator(P->current.type) && prec <= get_rule(P->current.type)->precedence)
        {
            InfixFn infix = get_rule(P->current.type)->infix;

            if (infix == NULL)
            {
                //printf("a\n");
                return left;
            }

            advance(P);

            left = infix(P, left);
        }

    }

    //print("===============");
    return left;
}

static ParserRule* get_rule(LTokenType ttype)
{
    return &rules[ttype];
}

static Expr* parser_int(Parser* P)
{
    return new_literal(P, P->previous);
}

static Expr* parser_float(Parser* P)
{
    return new_literal(P, P->previous);
}

static Expr* parser_string(Parser* P)
{
    return new_literal(P, P->previous);
}

static Expr* parser_literal(Parser* P)
{
    return new_literal(P, P->previous);
}

static Expr* parser_identifier(Parser* P)
{
    //printf("========line: %d, column: %d, offset: %d==============\n", P->previous.location.end.line, P->previous.location.begin.column, P->previous.location.begin.offset);
    return parser_name_expr(P, P->previous);
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

static Expr* parser_prefix(Parser* P)
{
    Token op = P->previous;

    Expr* right = parser_precedence(P, PREC_UNARY);

    return new_fix(P, op, right, true);
}

static Expr* parser_posfix(Parser* P, Expr* left)
{
    Token op = P->previous;

    //Expr* right = parser_precedence(P, PREC_UNARY);

    return new_fix(P, op, left, false);
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
    Location begin = P->previous.location;
    Expr* expr = parser_expression(P);
    Location end = locationCPos(expr->base.location.end, expr->base.location.end);
    if (P->current.type == M_RPAREN)
    {
        end = P->current.location;
        advance(P);
    }
    else
    {
        // revisar que imprima bien, con ; y operandos imprime mal porque el parser_expression lo consume cuando no debería
        //print_token(P->src, P->previous);
        char* got = getText(P->previous.length, P->src, P->previous.location.begin.offset);
        expectedToClose(")", "(", got, " when parsing expression", P->name, begin, P->previous.location);
    }
    //consume(P, M_RPAREN, "')'");


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

static Expr* parser_expr_error(Parser* P)
{
    ErrorExpr* error = arena_allocator(P->arena, sizeof(ErrorExpr));

    error->expr.base.ttype = NODE_EXPR;
    error->expr.base.location = P->previous.location;
    //printf("========line: %d, column: %d, offset: %d==============\n", P->previous.location.end.line, P->previous.location.begin.column, P->previous.location.begin.offset);
    error->expr.expr_type = EXPR_ERROR;
    error->token = P->previous;
    error->message = "Expected 'identifier', but got '%s'";

    return (Expr*) error;
}

/*
    STATEMENTS
*/

static Stmt* parser_statement(Parser* P)
{
    if (match(P, M_VAR)) return parser_var(P, false);
    if (match(P, M_CONST)) return parser_var(P, true);
    if (match(P, M_IF)) return parser_if(P);
    if (match(P, M_LBRACE)) return parser_block(P);
    if (match(P, M_SHORT_COMMENT)) return parser_statement(P);
    if (match(P, M_LONG_COMMENT_START)) return parser_statement(P);

    //print("//////////");
    //print_token(P->name, P->current);
    //print("//////////");
    return parser_expr_or_assignment_stmt(P);
    syntaxError("Incomplete Statement", P->name, P->current.location);
    exit(1);
}

static Stmt* parser_var(Parser* P, bool isConst)
{
    //print("entra a var");
    Position begin = P->previous.location.begin;
    Token tempNames[8];
    int nameCount = 0;

    tempNames[nameCount++] = P->current;
    consume(P, M_V_IDENTIFIER, "identifier when parsing variable name");

    while (match(P, M_COMMA))
    {
        if (nameCount + 1 >= 8)
        {
            syntaxError("Too many variables (max 8)", P->name, locationCPos(tempNames[0].location.begin, tempNames[nameCount].location.end));
            break;
        }
        tempNames[nameCount++] = P->current;
        consume(P, M_V_IDENTIFIER, "identifier when parsing variable name");
    }


    Token* names = arena_allocator(P->arena, sizeof(Token) * nameCount);
    memcpy(names, tempNames, sizeof(Token) * nameCount);

    Expr* tempValues[8];
    int valuesAmount = 0;

    if (match(P, M_ASSING))
    {
        tempValues[valuesAmount++] = parser_expression(P);
        //printf("entra\n");

        while (match(P, M_COMMA))
        {
            //printf("algo");
            if (valuesAmount >= 8)
            {
                syntaxError("Too many values (max 8)", P->name, locationCPos(tempValues[0]->base.location.begin, tempValues[valuesAmount-1]->base.location.end));
                break;
            }
            tempValues[valuesAmount++] = parser_expression(P);
        }
    }
    else if (isConst)
    {
        syntaxError("const must have min one value", P->name, locationCPos(tempNames[0].location.begin, tempNames[nameCount-1].location.end));
    }

    Expr** values = arena_allocator(P->arena, sizeof(Expr*) * valuesAmount);
    memcpy(values, tempValues, sizeof(Expr*) * valuesAmount);

    StmtVar* stmt = arena_allocator(P->arena, sizeof(StmtVar));

    stmt->stmt.base.location = valuesAmount > 0 ? locationCPos(begin, values[valuesAmount-1]->base.location.end) : locationCPos(begin, names[nameCount-1].location.end);
    stmt->stmt.base.ttype = NODE_STMT;
    stmt->stmt.stmt_type = STMT_VAR;
    stmt->isConst = isConst;
    stmt->names = names;
    stmt->namesCount = nameCount;
    stmt->values = values;
    stmt->valuesCount = valuesAmount;
    
    return (Stmt*) stmt;
}

//static Stmt* parser_ChooseCompoundOrAssing(Parser* P)
//{
//    if (P->previous.type == M_V_IDENTIFIER)
//    {
//        if (P->current.type == M_ASSING)
//        {
//            //Expr* left = parser_expression(P->previous);
//            //return parser_assing(P, left);
//        }
//        else if (P->current.type >= M_PLUS_ASSING && P->current.type <= M_MOD_ASSING)
//        {
//
//        }
//        else
//        {
//            // error
//        }
//    }
//    else
//    {
//        // error
//    }
//}

static Stmt* parser_assign(Parser* P, Expr* left)
{
    if (!isLValue(left))
        syntaxError("Expected a variable name on assign statement", P->name, left->base.location);

    Expr* tempNames[8];
    tempNames[0] = left;
    int namesCount = 1;

    while (match(P, M_COMMA))
    {
        if (namesCount > 8)
        {
            syntaxError("Too many variables (max 8)", P->name, locationCPos(tempNames[0]->base.location.begin, tempNames[namesCount]->base.location.end));
            break;
        }

        Expr* name = parser_expression(P);
        if (!isLValue(name))
            syntaxError("Expected a variable name on assign statement", P->name, name->base.location);
        tempNames[namesCount++] = name;
    }

    Expr* tempValues[8];
    int valuesCount = 0;

    if (match(P, M_ASSING))
    {
        Expr* expr = parser_expression(P);
        tempValues[namesCount++] = expr;

        while (match(P, M_COMMA))
        {
            if (valuesCount > 8)
            {
                syntaxError("Too many values (max 8)", P->name, locationCPos(tempValues[0]->base.location.begin, tempValues[valuesCount - 1]->base.location.end));
                break;
            }
            tempValues[valuesCount++] = parser_expression(P);
        }

    }


    Expr** names = arena_allocator(P->arena, sizeof(Expr*) * namesCount);
    memcpy(names, tempNames, sizeof(Expr*) * namesCount);

    Expr** values = arena_allocator(P->arena, sizeof(Expr*) * valuesCount);
    memcpy(values, tempValues, sizeof(Expr*) * valuesCount);

    StmtAssign* stmt = arena_allocator(P->arena, sizeof(StmtAssign));

    stmt->stmt.base.location = valuesCount > 0 ? locationCPos(tempNames[0]->base.location.begin, tempValues[valuesCount - 1]->base.location.end) : locationCPos(tempNames[0]->base.location.begin, tempNames[namesCount - 1]->base.location.begin);
    stmt->stmt.base.ttype = NODE_STMT;
    stmt->stmt.stmt_type = STMT_ASSING;
    stmt->names = names;
    stmt->nameCount = namesCount;
    stmt->values = values;
    stmt->valueCount = valuesCount;

    return (Stmt*) stmt;
}

static Stmt* parser_compound_assing(Parser* P, Expr* left)
{
    if (!isLValue(left))
        syntaxError("Expected a variable name on assign statement", P->name, left->base.location);

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
    //print("condi1");
    Expr* condition = parser_expression(P);
    //print("condi2");
    Stmt* ifBranch = parser_statement(P);
    //print("pasa a");
    Stmt* elseBranch = NULL;

    if (match(P, M_ELSEIF))
        elseBranch = parser_if(P);
    else if (match(P, M_ELSE))
        elseBranch = parser_statement(P);

    StmtIf* stmt = arena_allocator(P->arena, sizeof(StmtIf));
    
    Position end = {0, 0, 0};///*elseBranch != NULL ? elseBranch->base.location.end : */ ifBranch->base.location.end;
    //print("final");

    stmt->stmt.base.location = locationCPos(begin, end);
    stmt->stmt.base.ttype = NODE_STMT;
    stmt->stmt.stmt_type = STMT_IF;
    stmt->condition = condition;
    stmt->ifBranch = ifBranch;
    stmt->elseBranch = elseBranch;

    return (Stmt*) stmt;
}

static Stmt* parser_block(Parser* P)
{
    Position begin = P->previous.location.begin;
    Stmt* temp[64]; // ajustable
    int count = 0;

    while ((P->current.type != M_RBRACE) && (P->current.type != M_EOF))
    {
        if (count >= 64)
        {
            memoryCrash(P->name);
            exit(1);
        }

        temp[count++] = parser_statement(P);
    }

    consume(P, M_RBRACE, "}");

    Stmt** stmts = arena_allocator(P->arena, sizeof(Stmt*) * count);
    memcpy(stmts, temp, sizeof(Stmt*) * count);

    StmtBlock* block = arena_allocator(P->arena, sizeof(StmtBlock));

    block->stmt.base.location = locationCPos(begin, P->previous.location.end);
    block->stmt.base.ttype = NODE_STMT;
    block->stmt.stmt_type = STMT_BLOCK;
    block->statements = stmts;
    block->count = count;

    //block->base.kind = STMT_BLOCK;
    //block->statements = stmts;
    //block->count = count;
    //print("fin del block");
    return (Stmt*) block;
}

static Stmt* parser_expr_or_assignment_stmt(Parser* P)
{
    Expr* first = parser_expression(P);

    if (is_assignment_operator(P->current.type))
        if (P->current.type == M_ASSING)
            return parser_assign(P, first);
        else
            return parser_compound_assing(P, first);

    if (match(P, M_COMMA))
        return parser_assign(P, first);

    match(P, M_SEMICOLON);

    if (!(first->expr_type == EXPR_PREFIX || first->expr_type == EXPR_POSTFIX))
    {

        //printf("algún error por aquí\n");
        return parser_stmt_error(P, first);
    }

    StmtExpr* stmt = arena_allocator(P->arena, sizeof(StmtExpr));
    stmt->stmt.base.ttype = NODE_STMT;
    stmt->stmt.base.location = first->base.location;
    stmt->stmt.stmt_type = STMT_EXPR;
    stmt->expr = first;

    return (Stmt*) stmt;
}

static Stmt* parser_stmt_error(Parser* P, Expr* expr)
{
    StmtError* error = arena_allocator(P->arena, sizeof(StmtError));

    error->stmt.base.ttype = NODE_STMT;
    error->stmt.base.location = expr->base.location;
    error->stmt.stmt_type = STMT_ERROR;
    error->nodo = (Node*) expr;
    error->message = "Expected a variable assignment, but got '%.*s'";

    return (Stmt*) error;
}

static Stmt* parser_program(Parser* P)
{
    Stmt* tempStmts[1024];
    int stmtCount = 0;
    while (P->current.type != M_EOF)
    {
        if (stmtCount >= 1024)
        {
            printErr("Demasiados Statements en el script principal", P->name, 3);
            exit(1);
        }
        Stmt* stmt = parser_statement(P);
        tempStmts[stmtCount++] = stmt;

        //print("esta en bucle");
        //print_token(P->src, P->current);
        //print("1");
        //break;
    }

    Stmt** stmts = arena_allocator(P->arena, sizeof(Stmt*) * stmtCount);
    memcpy(stmts, tempStmts, sizeof(Stmt*) * stmtCount);

    StmtBlock* block = arena_allocator(P->arena, sizeof(StmtBlock));

    if (stmtCount > 0)
    {
        block->stmt.base.location = locationCPos(stmts[0]->base.location.begin, stmts[stmtCount - 1]->base.location.end);
    }
    else
    {
        block->stmt.base.location = P->current.location;
    }

    block->stmt.base.ttype = NODE_STMT;
    block->stmt.stmt_type = STMT_BLOCK;
    block->statements = stmts;
    block->count = stmtCount;

    return (Stmt*) block;
}

Parser* parser_init(TokenArray* Tokens, Arena* A, const char* name, const char* src)
{
    Parser* P = malloc(sizeof(Parser));
    if (P == NULL)
    {
        memoryCrash(name);
        //printErr("Error de memoria", "Parser", 3);
        exit(1);
    }

    P->Tokens = Tokens;
    P->name = name;
    P->src = src;
    P->arena = A;
    P->hadError = false;
    P->pos = -1;
    P->count = Tokens->count;
    advance(P);
    return P;
}

Stmt* parser_execute(Parser* P)
{
    //printf("antes\n");
    //printf("VAR: %d, %d, %d, %d\n", M_VAR, P->previous.type, P->current.type, P->pos);
    //printf("%d, %d, %d \n", P->Tokens->data[0].type, P->Tokens->data[1].type, P->Tokens->data[2].type);
    //print_token(P->src, peek(P));
    //parser_statement(P);
    return parser_program(P);
    //print_token(P->src, P->current);
    //printf("%d, %d, %d\n", P->previous.type, P->current.type, P->pos);
    //printf("despues\n");
}

#if (defined(DEBUG) && DEBUG == 1) && (defined(PARSER_DEBUG) && PARSER_DEBUG == 1)

static void print_indent(int indent)
{
    for (int i = 0; i < indent; i++)
    {
        printf("  ");
    }
}

static void print_branch(const char* prefix, bool isLast)
{
    printf("%s", prefix);
    printf(isLast ? " `-- " : " |-- ");
}

static void build_prefix(char* buffer, const char* prefix, bool isLast)
{
    strcpy_s(buffer, 256, prefix);

    if (isLast)
        strcat_s(buffer, 256, "     ");  // espacio vacío
    else
        strcat_s(buffer, 256, " |   ");  // línea vertical
}

static void print_expr(Parser* P, Expr* e, const char* prefix, bool isLast)
{
    if (!e) return;

    print_branch(prefix, isLast);
    //print_indent(indent);

    switch (e->expr_type)
    {
        case EXPR_BINARY:
        {
            BinaryExpr* binary = (BinaryExpr*) e;
            printf("Binary (%.*s)\n", binary->op.length, &P->src[binary->op.location.begin.offset]);

            char newPrefix[256];
            build_prefix(newPrefix, prefix, isLast);

            print_expr(P, binary->left, newPrefix, false);
            print_expr(P, binary->right, newPrefix, true);
            break;
        }
        case EXPR_UNARY:
        {
            UnaryExpr* unary = (UnaryExpr*) e;
            printf("Unary (%.*s)\n", unary->op.length, &P->src[unary->op.location.begin.offset]);

            char newPrefix[256];
            build_prefix(newPrefix, prefix, isLast);

            //print_expr(P, unary->right, indent + 1);
            print_expr(P, unary->right, newPrefix, true);
            break;
        }
        case EXPR_LITERAL:
        {
            LiteralExpr* literal = (LiteralExpr*) e;
            printf("Literal (%.*s)\n", literal->value.length, &P->src[literal->value.location.begin.offset]);
            break;
        }
        case EXPR_NAME:
        {
            NameExpr* name = (NameExpr*) e;
            printf("Name Expr (%.*s)\n", name->name.length, &P->src[name->name.location.begin.offset]);
            break;
        }
        case EXPR_PREFIX:
        {
            FixExpr* prefixE = (FixExpr*) e;
            printf("Prefix (%.*s)\n", prefixE->op.length, &P->src[prefixE->op.location.begin.offset]);
            
            //print_indent(indent + 1);
            char targetPrefix[256];
            build_prefix(targetPrefix, prefix, isLast);
            print_branch(targetPrefix, true);
            printf("Target: \n");
            char newPrefix[256];
            build_prefix(newPrefix, targetPrefix, true);
            //print_expr(P, prefix->target, indent + 2);
            print_expr(P, prefixE->target, newPrefix, true);
            break;
        }
        case EXPR_POSTFIX:
        {
            FixExpr* postfix = (FixExpr*)e;
            printf("Postfix (%.*s)\n", postfix->op.length, &P->src[postfix->op.location.begin.offset]);

            //print_indent(indent + 1);
            char targetPrefix[256];
            build_prefix(targetPrefix, prefix, isLast);
            print_branch(targetPrefix, true);
            printf("Target: \n");
            //print_expr(P, postfix->target, indent + 2);
            char newPrefix[256];
            build_prefix(newPrefix, targetPrefix, true);
            print_expr(P, postfix->target, newPrefix, true);
            break;
        }

        case EXPR_ERROR:
        {
            ErrorExpr* error = (ErrorExpr*) e;
            printf("Expression Error\n");

            char targetPrefix[256];
            build_prefix(targetPrefix, prefix, isLast);
            print_branch(targetPrefix, false);
            printf("Message: '%s'\n", error->message);
            print_branch(targetPrefix, true);
            printf("line: %d, column: %d, offset: %d\n", error->expr.base.location.begin.line, error->expr.base.location.begin.column, error->expr.base.location.begin.offset);
            break;
        }
    }
}

static void print_stmt(Parser* P, Stmt* stmt, const char* prefix, bool isLast)
{
    if (!stmt) return;

    print_branch(prefix, isLast);
    //print_indent(indent);

    switch (stmt->stmt_type)
    {
        case STMT_VAR:
        {
            StmtVar* var = (StmtVar*) stmt;
            printf("%s\n", var->isConst ? "const" : "var");

            char newPrefix[256];
            build_prefix(newPrefix, prefix, isLast);

            print_branch(newPrefix, false);
            //print_indent(indent + 1);
            printf("Names:\n");
            char namesPrefix[256];
            build_prefix(namesPrefix, newPrefix, false);
            for (int i = 0; i < var->namesCount; i++)
            {
                print_branch(namesPrefix, i == var->namesCount - 1);
                //print_indent(indent + 2);
                printf("%.*s\n", var->names[i].length, &P->src[var->names[i].location.begin.offset]);
            }
            print_branch(newPrefix, true);
            //print_indent(indent + 1);
            printf("Vales:\n");
            char valuesPrefix[256];
            build_prefix(valuesPrefix, newPrefix, true);
            for (int i = 0; i < var->valuesCount; i++)
            {
                print_expr(P, var->values[i], valuesPrefix, i == var->valuesCount - 1);
                //print_expr(P, var->values[i], indent + 2);
            }
            break;
        }
        case STMT_IF:
        {

            StmtIf* ifnode = (StmtIf*) stmt;
            printf("If\n");
            //print_indent(indent + 1);
            char conditionPrefix[256];
            build_prefix(conditionPrefix, prefix, isLast);
            print_branch(conditionPrefix, false);
            printf("condition: \n");
            //print_expr(P, ifnode->condition, indent + 2);
            char exprPrefix[256];
            build_prefix(exprPrefix, conditionPrefix, false);
            print_expr(P, ifnode->condition, exprPrefix, true);
            //print_indent(indent + 1);
            char ifBranchPrefix[256];
            build_prefix(ifBranchPrefix, prefix, isLast);
            print_branch(ifBranchPrefix, ifnode->elseBranch == NULL);
            printf("If Branch:\n");
            //print_stmt(P, ifnode->ifBranch, indent + 2);
            char trueBlockPrefix[256];
            build_prefix(trueBlockPrefix, ifBranchPrefix, ifnode->elseBranch == NULL);
            print_stmt(P, ifnode->ifBranch, trueBlockPrefix, true);
            if (ifnode->elseBranch)
            {
                //print_indent(indent + 1);
                char elseBranchPrefix[256];
                build_prefix(elseBranchPrefix, prefix, isLast);
                print_branch(elseBranchPrefix, true);
                printf("Else Branch\n");
                //print_stmt(P, ifnode->elseBranch, indent + 2);
                char falseBlockPrefix[256];
                build_prefix(falseBlockPrefix, elseBranchPrefix, true);
                print_stmt(P, ifnode->elseBranch, falseBlockPrefix, true);
            }
            break;
        }
        case STMT_EXPR:
        {
            StmtExpr* expr = (StmtExpr*) stmt;
            printf("Statement Expression\n");

            //print_indent(indent + 1);
            char newPrefix[256];
            build_prefix(newPrefix, prefix, isLast);
            //print_expr(P, expr->expr, indent + 2);
            print_expr(P, expr->expr, newPrefix, true);
        }
            break;
        case STMT_ASSING:
        {
            StmtAssign* assign = (StmtAssign*) stmt;
            printf("Assign\n");

            //print_indent(indent + 1);
            printf("Targets:\n");
            for (int i = 0; i < assign->nameCount; i++)
            {
                //print_indent(indent + 2);
                printf("aquí hay un error en el AST, para ser más específicos en el assign->names[i].length\n");
                //printf("%.*s\n", assign->names[i].length, &P->src[assign->names[i].location.begin.offset]);
                //printf("%d\n", assign->names[i].length);
            }

            //print_indent(indent + 1);
            printf("Vales:\n");
            for (int i = 0; i < assign->valueCount; i++)
            {
                //print_expr(P, assign->values[i], indent + 2);
            }
            break;
        }
        case STMT_BLOCK:
        {
            printf("Block\n");
            StmtBlock* block = (StmtBlock*) stmt;

            char newPrefix[256];
            build_prefix(newPrefix, prefix, isLast);

            for (int i = 0; i < block->count; i++)
            {
                //print_stmt(P, block->statements[i], indent + 1);
                print_stmt(P, block->statements[i], newPrefix, i == block->count - 1);

            }
            break;
        }
        case STMT_COMPOUND_ASSING:
        {
            StmtCompoundAssing* compoundassing = (StmtCompoundAssing*) stmt;
            printf("Compound Assing (%.*s)\n", compoundassing->op.length, &P->src[compoundassing->op.location.begin.offset]);

            //print_indent(indent + 1);
            char targetPrefix[256];
            build_prefix(targetPrefix, prefix, isLast);
            print_branch(targetPrefix, false);
            printf("Target:\n");

            char exprPrefix[256];
            build_prefix(exprPrefix, targetPrefix, false);
            //print_expr(P, compoundassing->target, indent + 2);
            print_expr(P, compoundassing->target, exprPrefix, true);
            //print_indent(indent + 1);
            char valuePrefix[256];
            build_prefix(valuePrefix, prefix, isLast);
            print_branch(valuePrefix, true);
            printf("Value:\n");
            //print_expr(P, compoundassing->target, indent + 2);
            char exprVPrefix[256];
            build_prefix(exprVPrefix, valuePrefix, true);
            print_expr(P, compoundassing->value, exprVPrefix, true);
            break;
        }

        case STMT_ERROR:
        {
            StmtError* error = (StmtError*) stmt;
            printf("Statement Error\n");

            char targetPrefix[256];
            build_prefix(targetPrefix, prefix, isLast);
            print_branch(targetPrefix, true);
            printf("Message: '%s'\n", error->message);
            break;
        }

    }
}

void parser_print(Parser* P, StmtBlock* block)
{
    printf("===== PARSER DEBUG =====\n");
    printf("----- AST nodes -----\n");
    print_stmt(P, block, "", true);
    printf("===== END PARSER DEBUG =====\n");
}

#endif // DEBUG

