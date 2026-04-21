#include <stdio.h>
//#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include "err.h"
#include "cli.h"
#include "lexer.h"
#include "parser.h"
#include "print.h"

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
    //printErr("prueba de error", "CLI", 3);
    //printWarn("prueba de warning", "CLI", 3);
    //printTrace("prueba de stack trace", "CLI", 3);

    //char input[] = "-- typechecker 2, optimizer 5, sources/test.m";
    //char input[] = "-- typechecker 2, sources/test.m";
    //char input[] = "-- optimizer 5, sources/test.m";
    //char input[] = "-- optimizer 5, typechecker 2, sources/test.m";
    char input[] = "-- sources/test.m";


    Config config;

    if (parse_input(input, &config))
    {
        /*
        ========== CLI ==========
        */
        //printf("Ok.\n");
        //printf("Typechecker level: %d\n", config.typechecker_level);
        //printf("Optimizer lever: %d\n", config.optimizer_level);
        printf("Script: %s\n", config.script_path);
        Source* s = read_file(config.script_path);
        // Debug CLI
        if (DEBUG)
        {
            printf("src %s\n", s->src);
            printf("length %zd\n", s->length);
        }

        /*for (int i = 0; i <= s->length; i++)
        {
            printf("c = %c, d = %d\n", s->src[i], s->src[i]);
        }*/

        /*
        ========== Lexer ==========
        */

        Lexer* L = Lexer_init(s->src);
        TokenArray* Tokens = Lexer_execute(L);
        // Debug Lexer
        if (DEBUG) //{}
            Lexer_print(L, Tokens);

        /*
        ========== Arena Allocator ==========
        */

        //Arena A;
        //arena_init(&A, 1024 * 1024); // 1 MB

        /*
        ========== Parser ==========
        */
        /*
        Parser* P = parser_init(Tokens, &A);
        parser_execute(P);
        */
        free(Tokens);
        free(L);
        free(s);
        printf("M Languaje\n");
        print("");
        //print("esto es una prueba del print");
    }

    return 0;
}

