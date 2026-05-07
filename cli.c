#include <stdio.h>
//#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include <locale.h>
#include "err.h"
#include "cli.h"
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "print.h"
#include "codegen.h"

static char* trim_left(char* str)
{
    while (isspace(*str)) str++;
    return str;
}

static bool has_valid_extension(const char* path)
{
    const char* dot = strrchr(path, '.');
    if (!dot) return false;
    return (strcmp(dot, ".m") == false || strcmp(dot, ".mar") == false);
}

static bool parse_input(char* input, Config* config)
{
    config->typechecker_level = -1;
    config->optimizer_level = -1;
    config->script_path = NULL;

    // Validar inicio con "--"
    if (strncmp(input, "--", 2) != false)
    {
        printErr("Error: debe iniciar con '--'", "CLI", 3);
        return false;
    }

    // saltar el --
    char* args = input + 2;

    // CONTEXTO para strtok_s
    char* context = "";

    //char* token = strtok(args, ",");
    char* token = strtok_custom(args, ",", &context);

    int index = 0;

    while (token != NULL)
    {
        token = trim_left(token);

        // typecheker
        if (strncmp(token, "typechecker", 11) == false)
        {
            if (config->typechecker_level != -1)
            {
                printErr("Typechecker duplicado", "CLI", 3);
                if (DEBUG)
                    printf("Typecheker level -> %d\n", config->typechecker_level);
                return false;
            }

            int level;
            //if (sscanf_s(token, "typechecker %d", &level))
            if (sscanf_custom(token, "typechecker %d", &level))

            {
                printErr("Formato de typechecker invalido", "CLI", 3);
                if (DEBUG)
                    printf("token -> |%s|\n", token);

                return false;
            }

            if (level < 0 || level > 2)
            {
                printErr("Nivel de typechecker invalido, se esperaba un numero entre 0 y 2", "CLI", 3);
                if (DEBUG)
                    printf("level -> %d\n", level);
                return false;
            }

            config->typechecker_level = level;
        }
        // optimizer
        else if (strncmp(token, "optimizer", 9) == 0)
        {
            if (config->optimizer_level != -1) {
                printErr("Optimizer duplicado", "CLI", 3);
                if (DEBUG)
                    printf("Optimizer level -> %d\n", config->optimizer_level);
                return false;
            }

            int level;
            //if (sscanf_s(token, "optimizer %d", &level) != 1) {
            if (sscanf_custom(token, "optimizer %d", &level) != 1) {
                printErr("Formato invalido en optimizer", "CLI", 3);
                if (DEBUG)
                    printf("token -> %s\n", token);
                return false;
            }

            if (level < 0 || level > 5) {
                printErr("Nivel de optimizer invalido", "CLI", 3);
                if (DEBUG)
                    printf("level -> %d\n", level);
                return false;
            }

            config->optimizer_level = level;
        }
        // ruta
        else if (strncmp(token, "sources/", 8) == 0)
        {
            // Debe ser el último
            //if (strtok_s(NULL, ",", &context) != NULL) {
            if (strtok_custom(NULL, ",", &context) != NULL) {
                printErr("La ruta debe ser el ultimo argumento", "CLI", 3);
                if (DEBUG)
                    //printf("context -> %s", strtok_s(NULL, ",", &context));
                    printf("context -> %s", strtok_custom(NULL, ",", &context));
                return false;
            }

            if (!has_valid_extension(token)) {
                printErr("Extension invalida", "CLI", 3);
                if (DEBUG)
                    printf("extension -> %s\n", token);
                return false;
            }

            config->script_path = token;
            return 1; // éxito
        }
        else
        {
            printErr("Token invalido %s", "CLI", 3);
            if (DEBUG)
                printf("token -> %s\n", token);
            return false;
        }

        //token = strtok_s(NULL, ",", &context);
        token = strtok_custom(NULL, ",", &context);
        index++;
    }

    printErr("Falta la ruta del script", "CLI", 3);
    if (DEBUG)
        printf("Text -> %s", input);
    return false;

}

static Source* read_file(const char* path)
{
    FILE* file = fopen(path, "rb");

    if (!file)
    {
        printErr("No se pudo abrir el archivo", "CLI", 3);
        exit(1);
    }

    fseek(file, 0, SEEK_END);

    Source* s = malloc(sizeof(Source));
    if (s == NULL)
    {
        printErr("Error de memoria", "CLI", 3);
        exit(1);
    }

    s->length = ftell(file);

    rewind(file);

    char* buffer = malloc(sizeof(s->length + 1));

    if (buffer == NULL)
    {
        printErr("No se pudo abrir el archivo", "CLI", 3);
        fclose(file);
        free(buffer);
        exit(1);
    }

    s->src = buffer;

    size_t read_size = fread(buffer, 1, s->length, file);

    if (s->length != read_size)
    {
        printErr("Lectura incompleta del archivo", "CLI", 3);
        free(buffer);
        fclose(file);
        exit(1);
    }

    s->src[s->length] = '\0';

    fclose(file);

    return s;
}

int main(void)
{
    setlocale(LC_ALL, "spanish");
    //printErr("prueba de error", "CLI", 3);
    //printWarn("prueba de warning", "CLI", 3);
    //printTrace("prueba de stack trace", "CLI", 3);

    //char input[] = "-- typechecker 2, optimizer 5, sources/test.m";
    //char input[] = "-- typechecker 2, sources/test.m";
    //char input[] = "-- optimizer 5, sources/test.m";
    //char input[] = "-- optimizer 5, typechecker 2, sources/test.m";
    char input[] = "-- sources/test.m";


    Config config;
    clock_t startAllProgram = clock();
    if (parse_input(input, &config))
    {
        /*
        ========== CLI ==========
        */
        //printf("Ok.\n");
        //printf("Typechecker level: %d\n", config.typechecker_level);
        //printf("Optimizer lever: %d\n", config.optimizer_level);
        Source* s = read_file(config.script_path);

#if (defined(DEBUG) && DEBUG == 1) && (defined(CLI_DEBUG) && CLI_DEBUG == 1)
        // Debug CLI
        printf("Script: %s\n", config.script_path);
        printf("src %s\n", s->src);
        printf("length %zd\n", s->length);
#endif
        /*for (int i = 0; i <= s->length; i++)
        {
            printf("c = %c, d = %d\n", s->src[i], s->src[i]);
        }*/

        /*
        ========== Lexer ==========
        */
        clock_t startCompile = clock();

        Lexer* L = Lexer_init(s->src, config.script_path);
        TokenArray* Tokens = Lexer_execute(L);
#if (defined(DEBUG) && DEBUG == 1) && (defined(LEXER_DEBUG) && LEXER_DEBUG == 1)
        // Debug Lexer
            Lexer_print(L, Tokens);
#endif

        /*
        ========== Arena Allocator ==========
        */

        Arena A;
        arena_init(&A, 1024 * 1024); // 1 MB

        /*
        ========== Parser ==========
        */
        //-*
        Parser* P = parser_init(Tokens, &A, config.script_path, s->src);
        Stmt* stmt = parser_execute(P);

#if (defined(DEBUG) && DEBUG == 1) && (defined(PARSER_DEBUG) && PARSER_DEBUG == 1)
        parser_print(P, stmt);
#endif

        /*
        ========== Compiler ==========
        */
        
        Compiler* C = compiler_init(s->src, config.script_path);
        compiler_program(C, stmt);
#if (defined(DEBUG) && DEBUG == 1) && (defined(COMPILER_DEBUG) && COMPILER_DEBUG == 1)
        compiler_print(C);
#endif 

        /*
        ========== Code Generator ==========
        */
        //printf("enter codegen\n");
        CodeGen* G = generator_init(s->src, config.script_path, &C->ir, &C->constants);
        //printf("enter bytecode\n");
        Chunk* mainChunk = generate_bydecode(G);
        clock_t endCompile = clock();
        printf("The time it took to compile the program is %f seconds\n", (double)(endCompile - startCompile) / CLOCKS_PER_SEC);
        
        /*
        ========== Virtual Machine ==========
        */

        clock_t startRun = clock();
        //printf("enter vm\n");
        vm_execute(mainChunk, config.script_path);

        clock_t endRun = clock();
        printf("The time it took to run the program is %f seconds\n", (double)(endRun - startRun) / CLOCKS_PER_SEC);



        //compilerError("Esto es una prueba %s, probando %d, bye %c", "test", locationCNum(1, 2, 3, 4, 5, 6), "prueba123456", 58, 'M');
        //compilerError("Variable '%.*s' has not yet been declared. Consider declaring it before using it", "test2", locationCNum(1, 2, 3, 4, 5, 6), 5, &s->src[2]);
        //*/
        
        //free(Tokens);
        //free(L);
        //free(stmt);
        //free(P);
        //free(s);
        //free(C);
        printf("M Languaje\n");

        //print("");
        //print("esto es una prueba del print");
        //print(5);
        //print(true);
        //print(6.397);
        //print('a');
        //print("prueba de", "cadena multiple", 5, "concatenacion");
    }

    clock_t endAllProgram = clock();

    printf("The time it took to run the entire program is %f seconds\n", (double)(endAllProgram - startAllProgram) / CLOCKS_PER_SEC);

    return 0;
}

