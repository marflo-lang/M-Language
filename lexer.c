#include <stdio.h>
#include <stdbool.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "lexer.h"
#include "err.h"

static void consume(Lexer* L)
{
    if (L->position >= L->length)
    {
        L->current == '\0';
        return;
    }
    assert(L->position < L->length);
    L->lastPosition = pos(L->line, L->column, L->position);
    L->position++;
    L->column++;
    L->current = L->src[L->position];
    if (L->current == '\n')
    {
        L->line++;
        L->column = 0;
        consume(L);
    }
    //if (L->position >= L->length)
        //L->current = '\0';
}

static void consumeNoLine(Lexer* L)
{
    if (L->position >= L->length)
    {
        L->current == '\0';
        return;
    }
    assert(L->position < L->length);
    L->lastPosition = pos(L->line, L->column, L->position);
    L->position++;
    L->column++;
    L->current = L->src[L->position];
    //if (L->position >= L->length)
        //L->current = '\0';
}

static char* peek(Lexer* L)
{
    if (L->position + 1 >= L->length) return '\0';
    assert(L->position + 1 < L->length);
    return L->src[L->position + 1];
}

static char* nextPeek(Lexer* L)
{
    if (L->position + 2 >= L->length) return '\0';
    assert(L->position + 2 < L->length);
    return L->src[L->position + 2];
}

static bool match(Lexer* L, char expected)
{
    if (L->current == expected)
    {
        consume(L);
        return true;
    }
    return false;
}

static void skipWhiteSpace(Lexer* L)
{
    while (true)
    {
        switch (L->current)
        {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                consume(L);
                break;
            default:
                return;
        }
    }
}

static void skipShortComments(Lexer* L)
{
    //printf("entró\n");
    while (L->current != '\n' && L->current != '\0')
    {
        consumeNoLine(L);
        //printf("'%c'\n", L->current);
    }
    if (L->current == '\n')
    {
        L->line++;
        L->column = 0;
    }
    consume(L); // \n o \0
}

static void skipLongComment(Lexer* L)
{
    while (L->current != '*' && peek(L) != '/')
        consume(L);
    consume(L); // *
    consume(L); // /
}


// cambiar al switch principal debido a que se queda en el stack
static Token makeString(Lexer* L, char quote, Position pos)
{
    Token t; 
    //printf("AAAAAAAAAAquote = %c\n", quote);
    LTokenType ttype = M_V_STRING;

    bool finished = false;

    //while (L->current != quote && L->current != '\0')
    while (true)
    {
        if (L->current == '\\')
        {
            consumeNoLine(L);
            if (L->current == '\n')
                break;
            consume(L);
            continue;
        }

        if (L->current == quote)
        {
            finished = true;
            consume(L);
            break;
            //consume(L);
        }
        else if (L->current == '\n' || L->current == '\0')
        {
            finished = false;
            break;
        }

        consumeNoLine(L);
    }
    
    //consume(L);

    if (!finished)
        ttype = M_V_UNFINISHED_STRING;

    Location loc = locationCPosNum(pos, L->line, L->column, L->position);
    //Location loc = locationCPosNum(pos, L->line, L->column, L->position);
    /*if (loc == NULL)
    {
        printErr("Error de memoria", "Lexer", 3);
        exit(0);
    }*/

    /*loc.begin = pos;
    loc.end->line = L->line;
    loc.end->column = L->column - 1;
    loc.end->offset = L->position - 1;*/

    t.type = ttype;
    //t.length = L->lastPosition.offset - pos.offset;
    //printf("LastPos = %d, off = %d, length = %d\n", L->position, pos.offset, L->lastPosition.offset - pos.offset);
    t.length = L->position - pos.offset;
    t.location = loc;

    return t;
}

// mismo caso que makeString
static Token makeNumber(Lexer* L, Position pos)
{
    Token t;

    /*if (t == NULL)
    {
        printErr("Error de memoria", "Lexer", 3);
        exit(0);
    }*/

    LTokenType ttype = M_V_INT;

    while (isdigit(L->current))
        consume(L);

    if (L->current == '.' && isdigit((int) peek(L)))
    {
        ttype = M_V_FLOAT;
        consume(L);

        while (isdigit(L->current) || L->current == '.')
        {
            if (L->current == '.')
                ttype = M_V_MALFORMED_NUMBER;
            consume(L);
        }
    }
    Location loc = locationCPosNum(pos, L->line, L->column, L->position);
    /*if (loc == NULL)
    {
        printErr("Error de memoria", "Lexer", 3);
        exit(0);
    }*/

    /*loc->begin = pos;
    loc->end->line = L->line;
    loc->end->column = L->column - 1;
    loc->end->offset = L->position - 1;*/

    t.type = ttype;
    //printf("LastPos = %d, off = %d, length = %d\n", L->position, pos.offset, L->lastPosition.offset - pos.offset);
    t.length = L->position - pos.offset;
    t.location = loc;

    return t;
}

static Token makeIdentifier(Lexer* L, Position pos, int length)
{
    Token t = {.type = M_V_IDENTIFIER, .location = locationCPosNum(pos, L->line, L->column, L->position), .length = length};
    return t;
}

static Token makeKeyWord(Lexer* L, LTokenType ttype, Position pos, int length)
{
    Token t = { .type = ttype, .location = locationCPosNum(pos, L->line, L->column, L->position), .length = length };
    return t;
}

static Token chooseKeywordOrIdentifier(Lexer* L, Position pos)
{
    //printf("algo\n");
    while (isalnum((unsigned char)L->current) || L->current == '_')
    {
        consume(L);
    }

    int length = L->position - pos.offset;

    LTokenType t = identifierType(L, pos.offset, length);
    if (t == M_V_IDENTIFIER)
        return makeIdentifier(L, pos, length);
    else
        return makeKeyWord(L, t, pos, length);
}

static Token makeToken(Lexer* L, LTokenType ttype, Position pos)
{
    //printf("LastPos = %d, off = %d, length = %d\n", L->position, pos.offset, L->lastPosition.offset - pos.offset);
    Token t = { .type = ttype, .location = locationCPosNum(pos, L->line, L->column, L->position), .length = L->position - pos.offset };
    return t;
}

static Token makeEOF(Lexer* L, Position pos)
{
    Token t = { .type = M_EOF, .location = locationCPos(pos, pos), .length = 0 };
    return t;
}

static LTokenType identifierType(Lexer* L, int start, int length)
{
    const char* text = L->src + start;

    switch (text[0])
    {
        case 'v':
            if (length == 3 && memcmp(text, "var", 3) == 0)
                return M_VAR;
            break;
        case 'i':
            if (length == 2 && memcmp(text, "if", 2) == 0)
                return M_IF;
            break;
        case 'e':
            if (length == 4 && memcmp(text, "else", 4) == 0)
                return M_ELSE;
            else if (length == 6 && memcmp(text, "elseif", 6) == 0)
                return M_ELSEIF;
            break;
        case 'a':
            if (length == 3 && memcmp(text, "and", 3) == 0)
                return M_AND;
            break;
        case 'o':
            if (length == 2 && memcmp(text, "or", 2) == 0)
                return M_OR;
            break;
        case 'n':
            if (length == 3)
                if (memcmp(text, "not", 3) == 0)
                    return M_NOT;
                else if (memcmp(text, "nil", 3) == 0)
                    return M_V_NIL;
            break;
        case 'c':
            if (length == 5 && memcmp(text, "const", 5) == 0)
                return M_CONST;
            break;
        case 't':
            if (length == 4 && memcmp(text, "true", 4) == 0)
                return M_V_TRUE;
            break;
        case 'f':
            if (length == 5 && memcmp(text, "false", 5) == 0)
                return M_V_FALSE;
            break;
    }

    return M_V_IDENTIFIER;
}

static Token makeError(Lexer* L, const char* context, Position pos)
{
    //Token err = { .length = L->position - pos.offset, .location = locationCPos(pos, L->lastPosition), .type = M_ERROR };
    Token err = { .length = L->position - pos.offset, .location = locationCPosNum(pos, L->line, L->column, L->position), .type = M_ERROR };
    return err;
}

static Token scanToken(Lexer* L)
{
    skipWhiteSpace(L);
    //printf("M -> %c\n", L->current);
    Position begin = pos(L->line, L->column, L->position);

    /*if (begin == NULL)
    {
        printErr("Error de memoria", "Lexer", 3);
        exit(0);
    }*/

    /*begin.line = L->line;
    begin.column = L->column;
    begin.offset = L->position;*/

    char c = L->current;
    consume(L);

    switch (c)
    {
        case '+':
            if (match(L, '='))
            {
                // +=
                return makeToken(L, M_PLUS_ASSING, begin);
            }
            else if (match(L, '+'))
            {
                // ++
                return makeToken(L, M_INC, begin);
            }
            else
            {
                // +
                //printf("WELL 2\n");
                return makeToken(L, M_PLUS, begin);
            }
        case '-':
            if (match(L, '='))
            {
                // -=
                return makeToken(L, M_MINUS_ASSING, begin);
            }
            else if (match(L, '-'))
            {
                // --
                return makeToken(L, M_DEC, begin);
            }
            else
            {
                // -
                return makeToken(L, M_MINUS, begin);
            }
        case '*':
            if (match(L, '='))
            {
                // *=
                return makeToken(L, M_STAR_ASSING, begin);
            }
            else if (match(L, '/'))
            {
                // */ Long Finish Comment
                return makeToken(L, M_LONG_COMMENT_FINISH, begin);
            }
            else
            {
                // *
                return makeToken(L, M_STAR, begin);
            }
        case '/':
            if (match(L, '/'))
            {
                if (match(L, '='))
                {
                    // //=
                    return makeToken(L, M_FLOOR_DIV_ASSING, begin);
                }
                else
                {
                    // //
                    return makeToken(L, M_FLOOR_DIV, begin);
                }

            }
            else if (match(L, '='))
            {
                // /=
                return makeToken(L, M_SLASH_ASSING, begin);
            }
            else if (match(L, '*'))
            {
                // /* Long Start Comment
                // change to makeLongComment(L, begin)
                Token t = makeToken(L, M_LONG_COMMENT_START, begin);
                skipLongComment(L);
                return t;
            }
            else
            {
                // /
                return makeToken(L, M_SLASH, begin);
            }
        case '%':
            if (match(L, '='))
            {
                // %=
                return makeToken(L, M_MOD_ASSING, begin);
            }
            else
            {
                // %
                return makeToken(L, M_MOD, begin);
            }
        case '^':
            if (match(L, '='))
            {
                // ^=
                return makeToken(L, M_POW_ASSING, begin);
            }
            else
            {
                // ^
                return makeToken(L, M_POW, begin);
            }
        case '=':
            if (match(L, '='))
            {
                // ==
                return makeToken(L, M_EQ, begin);
            }
            else
            {
                // =
                return makeToken(L, M_ASSING, begin);
            }
        case '<':
            if (match(L, '>'))
            {
                if (match(L, '='))
                {
                    // <>= concat assing
                    return makeToken(L, M_CONCAT_ASSING, begin);
                }
                else
                {
                    // <> concat
                    return makeToken(L, M_CONCAT, begin);
                }
            }
            else if (match(L, '='))
            {
                // <=
                return makeToken(L, M_LTE, begin);
            }
            else
            {
                // <
                return makeToken(L, M_LT, begin);
            }
        case '>':
            if (match(L, '='))
            {
                // >=
                return makeToken(L, M_GTE, begin);
            }
            else
            {
                // >
                return makeToken(L, M_GT, begin);
            }
        case '!':
            if (match(L, '='))
            {
                // !=
                return makeToken(L, M_NEQ, begin);
            }
            else
            {
                // Error no hay uso para ! por si solo
                illegalCharacter(c, L->name, locationCPosNum(begin, L->line, L->column, L->position));
                return makeError(L, "IllegalCharacterError", begin);
                //return makeToken(L, M_ERROR, begin);
            }
        case '"':
            // " string
        case '\'':
            // ' string
        case '`':
            // ` interpolate string
            return makeString(L, c, begin);
        case '(':
            // (
            return makeToken(L, M_LPAREN, begin);
        case ')':
            // )
            return makeToken(L, M_RPAREN, begin);
        case '{':
            // {
            return makeToken(L, M_LBRACE, begin);
        case '}':
            // }
            return makeToken(L, M_RBRACE, begin);
        case '[':
            // [
            return makeToken(L, M_LBRAKET, begin);
        case ']':
            // ]
            return makeToken(L, M_RBRAKET, begin);
        case ';':
            // ;
            return makeToken(L, M_SEMICOLON, begin);
        case ',':
            return makeToken(L, M_COMMA, begin);
        case '#':
            // # short comment
            // change to makeShortComment(L, begin)
            Token t = makeToken(L, M_SHORT_COMMENT, begin);
            skipShortComments(L);
            return t;
        case '\0':
            //printf("Mmmm\n");
            return makeEOF(L, begin);
        default:
            if (isdigit((unsigned char) c))
            {
                // number ya sea int o float
                //printf("WELL 1\n");
                //printf("AAAAAAAAAAAAAAAAAAAAAAAAA");
                return makeNumber(L, begin);
            }
            //printf(" -> %d\n", c);
            if (isalpha((unsigned char) c) || c == '_')
            {
                // identifier o keyword
                return chooseKeywordOrIdentifier(L, begin);
            }
            //if (c == '\0')
                //return makeEOF(L, begin);
            // Error simbol invalid
            //printErr("Character  invalid", "Lexer", 3);
            illegalCharacter(c, L->name, locationCPosNum(begin, L->line, L->column, L->position));
            //printf("%c\n", L->current);
            return makeError(L, "IllegalCharacterError", begin);
            //exit(1);
            //consume(L);
    }

}

/*static Position* tes(int l, int n, int off)
{
    return &pos(l, n, off);
}*/

/*static void pushToken(TokenArray* Tokens, Token t)
{
    if (Tokens->count < Tokens->capacity)
    {
        Tokens->data[Tokens->count] = t;
        Tokens->count++;
    }
    else
    {
        int newCapacity = Tokens->capacity < 8 ? 8 : Tokens->capacity * 2;
        TokenArray* newdata = realloc(Tokens->data, sizeof(Token) * newCapacity);
        if (newdata != NULL)
        {
            //Tokens = newTokens;
            Tokens->data = newdata;
            Tokens->data[Tokens->count] = t;
            Tokens->count++;
        }
        else
        {
            printErr("Error de memoria", "Lexer", 3);
            exit(1);
        }
    }
}*/

static void pushToken(TokenArray* Tokens, Token t)
{
    //printf("Empieza\n");
    //printf("count = %d, capacity = %d\n", Tokens->count, Tokens->capacity);
    if (Tokens->count + 1 >= Tokens->capacity)
    {
        //printf("entra\n");
        int newCapacity = Tokens->capacity < 8 ? 8 : Tokens->capacity * 2;

        Token* newData = realloc(Tokens->data, sizeof(Token) * newCapacity);

        if (newData == NULL)
        {
            memoryCrash("Lexer");
            exit(1);
            //printErr("Error de memoria", "Lexer", 3);
            //exit(1);
        }
        Tokens->data = newData;
        Tokens->capacity = newCapacity;
        //printf("sale\n");
    }
    //else if (Tokens->count > Tokens->capacity)
    //{
    //    memoryCrash("Lexer");
    //    exit(1);
    //    //printErr("Memory overflow", "Lexer", 3);
    //    //exit(1);
    //}

    //printf("count = %d, capacity = %d\n", Tokens->count, Tokens->capacity);
    assert(Tokens->count < Tokens->capacity);
    Tokens->data[Tokens->count] = t;
    Tokens->count++;
    //printf("Termina\n");
}

TokenArray* Lexer_execute(Lexer* L)
{
    TokenArray* Tokens = malloc(sizeof(TokenArray));
    if (Tokens == NULL)
    {
        memoryCrash("Lexer");
        //printErr("Error de memoria", "Lexer", 3);
        exit(1);
    }
    Tokens->capacity = 0;
    Tokens->count = 0;
    Token* data = malloc(sizeof(Token) * Tokens->capacity);
    if (data == NULL)
    {
        memoryCrash("Lexer");
        //printErr("Error de memoria", "Lexer", 3);
        exit(1);
    }
    Tokens->data = data;

    //printf("src = %s\n", L->src); algo
    //while (L->current != '\0')
    while (true)

    {
        //printf("%c\n", L->current);
        //consume(L);
        //printf("antes\n");
        Token t = scanToken(L);
        pushToken(Tokens, t);

        if (t.type == M_EOF) break;
    }

    return Tokens;
}

char* getText(size_t len, const char* src, int offset)
{

    char* text = malloc(len + 1);  // espacio para '\0'
    if (!text) return ""; // opcional pero correcto

    strncpy_s(
        text,
        len + 1,  // tamaño del buffer destino
        src + offset,
        len
    );
    //print_token(P->src, P->current);
    text[len] = '\0';

    return text;
}

Lexer* Lexer_init(const char* src, const char* name)
{
    Lexer* L = malloc(sizeof(Lexer));

    if (L == NULL)
    {
        memoryCrash("Lexer");
        //printErr("Error de memoria", "Lexer", 3);
        exit(1);
    }

    L->column = 0;
    L->line = 1;
    L->position = -1;
    L->src = src;
    L->name = name;
    L->length = (int) strlen(src);
    consume(L); // L->current

    return L;
}

#if defined(DEBUG) && DEBUG == 1
void print_token(const char* src, Token token)
{
    printf("Type: %d '", token.type);
    if (token.type == M_V_INT)
        printf("int");
    else if (token.type == M_V_FLOAT)
        printf("float");
    else if (token.type == M_V_MALFORMED_NUMBER)
        printf("Malformed Number");
    else if (token.type == M_V_STRING)
        printf("String");
    else if (token.type == M_V_UNFINISHED_STRING)
        printf("Unfinished String");
    else if (token.type == M_ERROR)
        printf("ERROR");
    else if (token.type == M_EOF)
        printf("<eof>");
    else if (token.type == M_SHORT_COMMENT)
        printf("Short Comment");
    else if (token.type == M_LONG_COMMENT_START)
        printf("Start Long Comment");
    else if (token.type >= M_PLUS)
        printf("Operand");
    else if (token.type == M_V_IDENTIFIER)
        printf("identifier");
    else
        printf("Keyword");

    printf("', Value: ");
    /*for (int j = 0; j < token.length; j++)
    {
        int p = token.location.begin.offset + j;
        printf("%c", L->src[p]);
    }*/

    if (token.type == M_EOF)
        printf("<eof>");
    else
        for (int j = token.location.begin.offset; j < token.location.end.offset; j++)
        {
            printf("%c", src[j]);
        }

    printf(", Length: %d", token.length);

    //printf(", Line: begin %d -> end %d, Column: begin %d -> end %d, Offset: begin %d -> end %d",
    printf(", Begin %d, %d, %d -> End %d, %d, %d",
        token.location.begin.line, 
        token.location.begin.column, 
        token.location.begin.offset, 
        token.location.end.line,
        token.location.end.column,
        token.location.end.offset);

    printf("\n");
}

#endif

#if (defined(DEBUG) && DEBUG == 1) && (defined(LEXER_DEBUG) && LEXER_DEBUG == 1)

void Lexer_print(Lexer* L, TokenArray* Tokens)
{
    printf("===== LEXER DEBUG =====\n");
    printf("----- Tokens -----\n");
    printf("Tokens count -> %d\n", Tokens->count);
    for (int i = 0; i < Tokens->count; i++)
    {
        print_token(L->src, Tokens->data[i]);
        //printf("Type: %d '", Tokens->data[i].type);
        //if (Tokens->data[i].type == M_V_INT)
        //    printf("int");
        //else if (Tokens->data[i].type == M_V_FLOAT)
        //    printf("float");
        //else if (Tokens->data[i].type == M_V_STRING)
        //    printf("String");
        //else if (Tokens->data[i].type == M_V_UNFINISHED_STRING)
        //    printf("Unfinished String");
        //else if (Tokens->data[i].type == M_ERROR)
        //    printf("ERROR");
        //else if (Tokens->data[i].type == M_EOF)
        //    printf("<eof>");
        //else if (Tokens->data[i].type >= 15)
        //    printf("Operand");
        //else if (Tokens->data[i].type == M_V_IDENTIFIER)
        //    printf("identifier");
        //else
        //    printf("Keyword");
        //
        //printf("', Value: ");
        ///*for (int j = 0; j < Tokens->data[i].length; j++)
        //{
        //    int p = Tokens->data[i].location.begin.offset + j;
        //    printf("%c", L->src[p]);
        //}*/

        //for (int j = Tokens->data[i].location.begin.offset; j <= Tokens->data[i].location.end.offset; j++)
        //{
        //    printf("%c", L->src[j]);
        //}

        //printf(", Length: %d", Tokens->data[i].length);

        //printf(", Offset: begin %d -> end %d", Tokens->data[i].location.begin.offset, Tokens->data[i].location.end.offset);

        //printf("\n");
    }
        printf("===== END LEXER DEBUG =====\n");
}

#endif

int main2(void)
{
    /*Position p1 = pos(5, 3, 6) /*tes(5, 3, 6)*-/;
    Position p2 = p1;
    p2.line = 1555;
    p1.column = 0;
    printf("print something more \n");
    printf("line = %d\n", p1.line);
    printf("column = %d\n", p1.column);
    printf("offset = %d\n", p1.offset);
    printf("print something more 2 \n");
    printf("line = %d\n", p2.line);
    printf("column = %d\n", p2.column);
    printf("offset = %d\n", p2.offset);*/

    char* src = "59 + 6.3       * \n 3009.3063 / 'hola m \n '   \n - 56.3 \t ";
    Lexer* L = Lexer_init(src, "internal_test.m");
    TokenArray* Tokens = Lexer_execute(L);
    //Lexer_print(L, Tokens);
    printf("Finish\n");
    return 0;
}


