
#include "cli.h"
#include "../utils/err.h"
#include "../frontend/lexer.h"
#include "../frontend/parser.h"
#include "../backend/compiler.h"
#include "../backend/codegen.h"
#include "../utils/print.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <locale.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

static void findTest(TestList* list, const char* path);
static Source* read_file(const char* path);

typedef struct
{
    VM* vm;
    int status;
} M_VMResult;

static TestList createTest(size_t capacity)
{
    capacity = (capacity < 1) ? 1 : capacity;
    TestList list = {0};
    list.paths = calloc(capacity, sizeof(char*));
    if (list.paths == NULL)
    {
        memoryCrash("CLI Test1");
        exit(1);
    }
    list.count = 0;
    list.capacity = capacity;
    return list;
}

static void addTest(TestList* list, const char* path) {
    if (list->count >= list->capacity) {
        list->capacity *= 2;  // duplicar tamaño
        char** newPaths = realloc(list->paths, list->capacity * sizeof(char*));
        if (newPaths == NULL)
        {
            memoryCrash("CLI Test2");
            exit(1);
        }
        list->paths = newPaths;
    }
    list->paths[list->count] = _strdup(path);  // copiar string
    list->count++;
}

static void freeTestList(TestList* list) {
    for (size_t i = 0; i < list->count; i++)
        free(list->paths[i]);
    free(list->paths);
    list->count = 0;
    list->capacity = 0;
}

static Stmt* cli_execute_frontend(Source* s, Config* config)
{
    /*
        ========== Lexer ==========
    */

    Lexer* L = Lexer_init(s->src, config->script_path);
    TokenArray* Tokens = Lexer_execute(L);
#if (defined(DEBUG) && DEBUG == 1) && (defined(LEXER_DEBUG) && LEXER_DEBUG == 1)
    // Debug Lexer
    Lexer_print(L, Tokens);
#endif

    /*
    ========== Arena Allocator ==========
    */

    Arena* A = malloc(sizeof(Arena));
    if (A == NULL) 
    {
        memoryCrash("CLI");
        exit(1);
    }
    arena_init(A, 1024 * 1024); // 1 MB

    /*
    ========== Parser ==========
    */
    //-*
    Parser* P = parser_init(Tokens, A, config->script_path, s->src);
    Stmt* stmt = parser_execute(P);

#if (defined(DEBUG) && DEBUG == 1) && (defined(PARSER_DEBUG) && PARSER_DEBUG == 1)
    parser_print(P, stmt);
#endif

    return stmt;
}

static Chunk* cli_execute_backend(Source* s, Config* config, Stmt* stmt)
{
    /*
        ========== Compiler ==========
        */

    Compiler* C = compiler_init(s->src, config->script_path);
    compiler_program(C, stmt);
#if (defined(DEBUG) && DEBUG == 1) && (defined(COMPILER_DEBUG) && COMPILER_DEBUG == 1)
    compiler_print(C);
#endif 

    /*
    ========== Code Generator ==========
    */
    CodeGen* G = generator_init(s->src, config->script_path, &C->ir, &C->constants);
    Chunk* mainChunk = generate_bydecode(G);
    mainChunk->register_capacity = compiler_regs_used(C);

#if (defined(DEBUG) && DEBUG == 1) && (defined(CODEGEN_DEBUG) && CODEGEN_DEBUG == 1)
    codegen_print(G);
#endif

    return mainChunk;
}

static M_VMResult cli_execute_vm(Config* config, Chunk* mainChunk)
{
    /*
        ========== Virtual Machine ==========
    */

    VM* vm = vm_init(mainChunk, config->script_path);
    int result =  vm_execute(vm);
    M_VMResult m_vmresult = { .vm = vm, .status = result };
    return m_vmresult;
}

static void cli_run(Source* s, Config* config)
{
    clock_t startCompile = clock();
    Stmt* stmt = cli_execute_frontend(s, config);
    Chunk* mainChunk = cli_execute_backend(s, config, stmt);
    clock_t endCompile = clock();
    printf("The time it took to compile the program is %f seconds\n", (double)(endCompile - startCompile) / CLOCKS_PER_SEC);
    clock_t startRun = clock();
    M_VMResult m_vmresult = cli_execute_vm(config, mainChunk);
    clock_t endRun = clock();
    printf("The time it took to run the program is %f seconds\n", (double)(endRun - startRun) / CLOCKS_PER_SEC);
    if (m_vmresult.status > 0 || m_vmresult.vm->error.has_error)
        vm_runtime_report(m_vmresult.vm);
}

static M_VMResult cli_execute_test(Source* s, const char* name)
{
    Lexer* L = Lexer_init(s->src, name);
    TokenArray* Tokens = Lexer_execute(L);
    Arena A;
    arena_init(&A, 1024 * 1024); // 1 MB
    Parser* P = parser_init(Tokens, &A, name, s->src);
    Stmt* stmt = parser_execute(P);
    Compiler* C = compiler_init(s->src, name);
    compiler_program(C, stmt);
    CodeGen* G = generator_init(s->src, name, &C->ir, &C->constants);
    Chunk* mainChunk = generate_bydecode(G);
    mainChunk->register_capacity = compiler_regs_used(C);
    VM* vm = vm_init(mainChunk, name);
    int result = vm_execute(vm);
    M_VMResult m_vmresult = { .vm = vm, .status = result };
    return m_vmresult;
}

static printInfoTest(const char* name, double time, bool pass)
{
    //printf("\033[1;32m");
    //printf("\033[31m");
    if (pass)
    {
        printf("\033[1;32m");
        printf("[PASS] ");
    }
    else
    {
        printf("\033[31m");
        printf("[FAILED] ");
    }

    printf("%s; Take %.3fs", name, time);
    printf("\033[0m");
    printf("\n");
}

static void printEndTesting(size_t success, size_t failed, size_t all)
{
    printf("Run %zu tests, ", all);
    printf("\033[32m");
    printf("Successful %zu", success);
    printf("\033[0m");
    printf(", ");
    printf("\033[31m");
    printf("Failed %zu", failed);
    printf("\033[0m");
    printf(".\n");
}

static void cli_run_mode_test(Config* config)
{
    size_t success = 0;
    size_t failed = 0;
    printf("Running %zu tests...\n", config->list.count);
    for (size_t i = 0;i < config->list.count; i++)
    {
        printf("-----------------------------\n");
        Source* s = read_file(config->list.paths[i]);
        clock_t startRun = clock();
        M_VMResult m_vmressult = cli_execute_test(s, config->list.paths[i]);
        clock_t endRun = clock();
        double time = (double)(endRun - startRun) / CLOCKS_PER_SEC;
        printInfoTest(m_vmressult.vm->name, time, m_vmressult.status == 0);
        if (m_vmressult.status > 0 || m_vmressult.vm->error.has_error)
        {
            failed++;
            vm_runtime_report(m_vmressult.vm);
        }
        else
            success++;
    }
    printf("-----------------------------\n");
    printEndTesting(success, failed, config->list.count);
}

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

    // Validar inicio con "m"
    if (strncmp(input, "m ", 2) != false)
    {
        printErr("Error: debe iniciar con 'm'", "CLI", 3);
        return false;
    }

    // saltar el m
    char* args = input + 2;

    // CONTEXTO para strtok_s
    char* context = "";

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
        else if (strncmp(token, "run", 3) == 0)
        {
            token += 4;

            if (!has_valid_extension(token)) {
                printErr("Extension invalida", "CLI", 3);
#ifdef DEBUG
                    printf("extension -> %s\n", token);
#endif
                return false;
            }

            config->script_path = token;
            config->state = M_RUN;
            return 1; // éxito
        }
        else if (strncmp(token, "test", 4) == 0)
        {
            TestList list = createTest(0);
            findTest(&list, "test");
            config->state = M_TEST;
            config->list = list;
            config->script_path = NULL;
            return true;
        }
        else
        {
            printErr("Token invalido %s", "CLI", 3);
            if (DEBUG)
                printf("token -> %s\n", token);
            return false;
        }

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

    char* buffer = malloc(s->length + 1);

    if (buffer == NULL)
    {
        printErr("No se pudo abrir el archivo", "CLI", 3);
        fclose(file);
        //free(buffer);
        exit(1);
    }

    s->src = buffer;

    size_t read_size = fread(buffer, 1, s->length, file);

    if (s->length != read_size)
    {
        printErr("Lectura incompleta del archivo", "CLI", 3);
        //free(buffer);
        fclose(file);
        exit(1);
    }

    s->src[s->length] = '\0';

    fclose(file);

    return s;
}

#ifdef _WIN32
static void findTestWindows(TestList* list, const char* path)
{
    WIN32_FIND_DATA findFileData;
    HANDLE hFind;
    
    char searchPath[1024];
    snprintf(searchPath, sizeof(searchPath), "%s\\*", path);
    hFind = FindFirstFileA(searchPath, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        printErr("Carpeta test no encontrada", "CLI", 3);
        exit(1);
    }

    do
    {
        const char* name = findFileData.cFileName;

        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;

        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s\\%s", path, name);

        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            findTestWindows(list, fullpath);
        else
        {
            const char* ext = strrchr(name, '.');
            if (ext && (strcmp(ext, ".m") == 0 || strcmp(ext, ".mar") == 0))
            {
                addTest(list, fullpath);
                //printf("Test encontrado: %s\n", fullpath);
            }
        }
    } while (FindNextFileA(hFind, &findFileData));
    FindClose(hFind);
}

static void findTest(TestList* list, const char* path)
{
    findTestWindows(list, path);
}
#else
static void findTestsPosix(TestList* list, const char* path) {
    DIR* dir = opendir(path);
    if (!dir) {
        printErr("Carpeta test no encontrada", "CLI", 3);
        exit(1);
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        struct stat st;
        stat(fullpath, &st);

        if (S_ISDIR(st.st_mode)) {
            findTestsPosix(list, fullpath);
        }
        else {
            const char* ext = strrchr(entry->d_name, '.');
            if (ext && (strcmp(ext, ".m") == 0 || strcmp(ext, ".mar") == 0)) {
                printf("Test encontrado: %s\n", fullpath);
            }
        }
    }

    closedir(dir);
}
static void findTests(TestList* list, const char* path)
{
    findTestsPosix(list, path);
}
#endif


int main(void)
{
    setlocale(LC_ALL, "spanish");
    //char input[] = "-- typechecker 2, optimizer 5, sources/test.m";
    //char input[] = "-- typechecker 2, sources/test.m";
    //char input[] = "-- optimizer 5, sources/test.m";
    //char input[] = "-- optimizer 5, typechecker 2, sources/test.m";
    //char input[] = "-- sources/test.m";
    //char input[] = "m run sources/test.m";
    char input[] = "m test";
    
    Config config;
    //clock_t startAllProgram = clock();
    if (parse_input(input, &config))
    {
        if (config.state == M_RUN)
        {
            printf("PATH -> %s\n", config.script_path);
            /*
            ========== CLI ==========
            */
            Source* s = read_file(config.script_path);

    #if (defined(DEBUG) && DEBUG == 1) && (defined(CLI_DEBUG) && CLI_DEBUG == 1)
            // Debug CLI
            printf("===== CLI DEBUG =====\n");
            printf("----- Configs -----\n");
            printf("Typechecker level: %d\n", config.typechecker_level);
            printf("Optimizer lever: %d\n", config.optimizer_level);
            printf("Script: %s\n", config.script_path);
            printf("src: `%s`\n", s->src);
            printf("length: %zd\n", s->length);
            printf("===== END CLI DEBUG =====\n");
    #endif
            clock_t startAllProgram = clock();
            cli_run(s, &config);
            clock_t endAllProgram = clock();
            printf("The time it took to run the entire program is %f seconds\n", (double)(endAllProgram - startAllProgram) / CLOCKS_PER_SEC);
            return 0;
        }
        else if (config.state == M_TEST)
        {
            clock_t startAllTest = clock();
            cli_run_mode_test(&config);
            clock_t endAllTest = clock();
            printf("The time it took to run all test are %f seconds\n", (double)(endAllTest - startAllTest) / CLOCKS_PER_SEC);
            return 0;
        }
        else if (config.state == M_ANALYZE)
        {
            printf("PATH -> %s\n", config.script_path);
            /*
            ========== CLI ==========
            */
            Source* s = read_file(config.script_path);

#if (defined(DEBUG) && DEBUG == 1) && (defined(CLI_DEBUG) && CLI_DEBUG == 1)
            // Debug CLI
            printf("===== CLI DEBUG =====\n");
            printf("----- Configs -----\n");
            printf("Typechecker level: %d\n", config.typechecker_level);
            printf("Optimizer lever: %d\n", config.optimizer_level);
            printf("Script: %s\n", config.script_path);
            printf("src: `%s`\n", s->src);
            printf("length: %zd\n", s->length);
            printf("===== END CLI DEBUG =====\n");
#endif
            clock_t startAllAnalyze = clock();
            cli_execute_frontend(s, &config);
            clock_t endAllAnalyze = clock();
            printf("The time it took to analyze the entire program is %f seconds\n", (double)(endAllAnalyze - startAllAnalyze) / CLOCKS_PER_SEC);
            return 0;
        }
        else
        {
            printf("Inavlid State: %d\n", config.state);
            return 1;
        }
        return 1;
        /*
        ========== Lexer ==========
        */
        clock_t startCompile = clock();
        Source* s = NULL;
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
        CodeGen* G = generator_init(s->src, config.script_path, &C->ir, &C->constants);
        Chunk* mainChunk = generate_bydecode(G);
        mainChunk->register_capacity = compiler_regs_used(C);

#if (defined(DEBUG) && DEBUG == 1) && (defined(CODEGEN_DEBUG) && CODEGEN_DEBUG == 1)
        codegen_print(G);
#endif 

        clock_t endCompile = clock();
        printf("The time it took to compile the program is %f seconds\n", (double)(endCompile - startCompile) / CLOCKS_PER_SEC);
        
        /*
        ========== Virtual Machine ==========
        */

        clock_t startRun = clock();
        VM* vm = vm_init(mainChunk, config.script_path);
        int result = vm_execute(vm);
        if (result > 0)
            vm_runtime_report(vm);

        clock_t endRun = clock();
        printf("The time it took to run the program is %f seconds\n", (double)(endRun - startRun) / CLOCKS_PER_SEC);
    }

    clock_t endAllProgram = clock();
    //printf("The time it took to run the entire program is %f seconds\n", (double)(endAllProgram - startAllProgram) / CLOCKS_PER_SEC);
    printf("M Languaje\n");
    return 0;
}

