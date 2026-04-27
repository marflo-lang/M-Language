#pragma once
#include "m.h"
//#include <>

typedef struct
{
    int line;
    int column;
    int offset;
} Position;

typedef struct
{
    Position begin;
    Position end;
} Location;

typedef struct
{
    const char* src; // Codigo fuente
    const char* name; // Name of script
    char current;
    int position;
    int line;
    int column;
    int length;
    Position lastPosition;
} Lexer;

typedef enum
{
    // Literales
    M_V_INT,
    M_V_FLOAT,
    M_V_STRING,
    M_V_UNFINISHED_STRING,
    M_V_IDENTIFIER,
    
    // KEYWORDS
    M_VAR,
    M_CONST,
    M_IF,
    M_ELSEIF,
    M_ELSE,
    M_AND,
    M_OR,
    M_NOT,
    M_V_NIL,
    M_V_TRUE,
    M_V_FALSE,
    
    // Operadores
    M_PLUS, // +
    M_MINUS, // -
    M_STAR, // *
    M_SLASH, // /
    M_FLOOR_DIV, // //
    M_MOD, // %
    M_POW, // ^
    M_CONCAT, // <>
    
    // Operadores
    M_LT, // <
    M_LTE, // <=
    M_GT, // >
    M_GTE, // >=
    M_EQ, // ==
    M_NEQ, // !=
    
    // Asignacion
    M_ASSING, // =
    M_PLUS_ASSING, // +=
    M_MINUS_ASSING, // -=
    M_STAR_ASSING, // *=
    M_SLASH_ASSING, // /=
    M_FLOOR_DIV_ASSING, // //=
    M_CONCAT_ASSING, // <>=
    M_POW_ASSING, // ^=
    M_MOD_ASSING, // %=
    
    // Incremento
    M_INC, // ++
    M_DEC, // --
    
    // Simbolos
    M_LPAREN, // (
    M_RPAREN, // )
    M_LBRACE, // {
    M_RBRACE, // }
    M_LBRAKET, // [
    M_RBRAKET, // ]
    M_SEMICOLON, // ;
    M_COMMA, // ,
    
    // Comment
    M_SHORT_COMMENT, // #
    M_LONG_COMMENT_START, // /*
    M_LONG_COMMENT_FINISH, // */
    
    M_EOF, // <eof>
    M_ERROR
} LTokenType;

typedef struct
{
    LTokenType type;
    //const char* lexeme;
    int length;
    Location location;
} Token;

typedef struct
{
    Token* data;
    int count;
    int capacity;
} TokenArray;


inline Position pos(int l, int c, int off)
{
    Position p = { .line = l, .column = c, .offset = off };
    return p;
}

inline Location locationCPos(Position p1, Position p2)
{
    Location l = { .begin = p1, .end = p2 };
    return l;
}

inline Location locationCNum(int l1, int c1, int off1, int l2, int c2, int off2)
{
    Location l = { .begin = pos(l1, c1, off1), .end = pos(l2, c2, off2) };
    return l;
}

inline Location locationCPosNum(Position p1, int l2, int c2, int off2)
{
    Location l = { .begin = p1, .end = pos(l2, c2, off2) };
    return l;
}

inline Location getLocationToken(Token* T)
{
    return T->location;
}

inline Position getBeginPos(Token* T)
{
    return T->location.begin;
}

inline Position getEndPosition(Token* T)
{
    return T->location.end;
}

// macros constructoras
//#define pos(l, c, off)  (Position) {.line = (l), .column = (c), .offset = (off)}

//#define locationCPos(p1, p2)    (Location*) &({.begin = (p1), .end = (p2)})
//#define locationCNum(l1, c1, off1, l2, c2, off2)    (Location*) &({.begin = {.line = (l1), .column = (c1), .offset = (off1)}, .end = {.line = (l2), .column = (c2), .offset = (off2)}})
//#define locationCPosNum(p1, l2, c2, off2)   (Location*) &({.begin = (p1), .end = {.line = (l2), .column = (c2), .offset = (off2)}})

//#define getLocationToken(T) (Location*) (T)->location
//#define getBeginPos(T)  (Position*) (T)->location->begin
//#define getEndPos(T)    (Position*) (T)->location->end


Lexer* Lexer_init(const char* src, const char* name);

TokenArray* Lexer_execute(Lexer* L);

char* getText(size_t len, const char* src, int offset);

// static
#if (defined(DEBUG) && DEBUG == 1) && (defined(LEXER_DEBUG) && LEXER_DEBUG == 1)
void Lexer_print(Lexer* L, TokenArray* Tokens);
#endif

#if defined(DEBUG) && DEBUG == 1
void print_token(const char* src, Token token);
#endif
