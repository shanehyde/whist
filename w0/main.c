#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "alloc.h"
#include "checker.h"
#include "codegen.h"
#include "lexer.h"
#include "parser.h"
#include "print_ast.h"

static int compile_and_run(const char* source_path, int argc, char** argv);

static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Could not open file: %s\n", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char*  buffer = xmalloc(size + 1);
    size_t read   = fread(buffer, 1, size, file);
    buffer[read]  = '\0';
    fclose(file);
    return buffer;
}

static int compile_and_run(const char* source_path, int argc, char** argv) {
    // Read source file
    char* source = read_file(source_path);
    if (!source)
        return 1;

    // Parse
    Parser parser;
    parser_init_with_path(&parser, source, source_path);
    Node* ast = parser_parse(&parser);

    if (parser.had_error) {
        fprintf(stderr, "Parse failed\n");
        node_free(ast);
        parser_free(&parser);
        free(source);
        return 1;
    }

    // Type check
    Checker checker;
    checker_init(&checker);
    checker_set_direct_imports(&checker, parser.direct_imports, parser.direct_imports_count);
    int ok = checker_check(&checker, ast);
    checker_free(&checker);

    if (!ok) {
        fprintf(stderr, "Type check failed\n");
        node_free(ast);
        parser_free(&parser);
        free(source);
        return 1;
    }

    // Create temp files
    char c_base[]   = "/tmp/w0_XXXXXX";
    char exe_path[] = "/tmp/w0_XXXXXX";

    int c_fd = mkstemp(c_base);
    if (c_fd < 0) {
        fprintf(stderr, "Could not create temp file\n");
        node_free(ast);
        free(source);
        return 1;
    }

    // Rename to .c extension for cc
    char c_path[256];
    snprintf(c_path, sizeof(c_path), "%s.c", c_base);
    close(c_fd);
    if (rename(c_base, c_path) != 0) {
        fprintf(stderr, "Could not rename temp file\n");
        unlink(c_base);
        node_free(ast);
        free(source);
        return 1;
    }

    int exe_fd = mkstemp(exe_path);
    if (exe_fd < 0) {
        fprintf(stderr, "Could not create temp file\n");
        unlink(c_path);
        node_free(ast);
        free(source);
        return 1;
    }
    close(exe_fd);

    // Generate C code
    FILE* c_file = fopen(c_path, "w");
    if (!c_file) {
        fprintf(stderr, "Could not open temp file for writing\n");
        unlink(c_path);
        unlink(exe_path);
        node_free(ast);
        free(source);
        return 1;
    }

    CodeGen gen;
    codegen_init(&gen, c_file);
    codegen_emit(&gen, ast);
    fclose(c_file);

    node_free(ast);
    parser_free(&parser);
    free(source);

    // Compile with cc
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cc -o %s %s", exe_path, c_path);

    int cc_result = system(cmd);
    unlink(c_path);

    if (cc_result != 0) {
        fprintf(stderr, "C compilation failed\n");
        unlink(exe_path);
        return 1;
    }

    // Build command with args
    size_t cmd_len = strlen(exe_path) + 1;
    for (int i = 1; i < argc; i++) {
        cmd_len += strlen(argv[i]) + 3; // space + quotes + arg
    }

    char* run_cmd = xmalloc(cmd_len + 1);
    strcpy(run_cmd, exe_path);
    for (int i = 1; i < argc; i++) {
        strcat(run_cmd, " ");
        strcat(run_cmd, argv[i]);
    }

    // Run the executable
    int run_result = system(run_cmd);
    free(run_cmd);

    // Cleanup
    unlink(exe_path);

    // Extract exit code from system() return value
    if (WIFEXITED(run_result)) {
        return WEXITSTATUS(run_result);
    }
    return run_result != 0 ? 1 : 0;
}

int main(int argc, char** argv) {
    char*       source;
    int         free_source    = 0;
    int         lex_only       = 0;
    int         parse_only     = 0;
    int         check_only     = 0;
    int         print_ast_flag = 0;
    const char* output_file    = NULL;

    // Check for 'run' subcommand
    if (argc >= 3 && strcmp(argv[1], "run") == 0) {
        return compile_and_run(argv[2], argc - 2, argv + 2);
    }

    // Parse args
    int arg_idx = 1;
    while (arg_idx < argc && argv[arg_idx][0] == '-') {
        if (strcmp(argv[arg_idx], "--lex") == 0) {
            lex_only = 1;
        } else if (strcmp(argv[arg_idx], "--parse") == 0) {
            parse_only = 1;
        } else if (strcmp(argv[arg_idx], "--check") == 0) {
            check_only = 1;
        } else if (strcmp(argv[arg_idx], "--ast") == 0) {
            print_ast_flag = 1;
        } else if (strcmp(argv[arg_idx], "-o") == 0 && arg_idx + 1 < argc) {
            arg_idx++;
            output_file = argv[arg_idx];
        }
        arg_idx++;
    }

    const char* source_file = NULL;
    if (arg_idx < argc) {
        source_file = argv[arg_idx];
        source      = read_file(source_file);
        if (!source)
            return 1;
        free_source = 1;
    } else {
        fprintf(stderr, "Usage: %s [options] <source-file>\n", argv[0]);
        fprintf(stderr, "       %s run <source-file> [args...]\n", argv[0]);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  --lex     Lex only (print tokens)\n");
        fprintf(stderr, "  --parse   Parse only (no type checking)\n");
        fprintf(stderr, "  --check   Type check only (no code generation)\n");
        fprintf(stderr, "  --ast     Print AST\n");
        fprintf(stderr, "  -o <file> Output file\n");
        fprintf(stderr, "Commands:\n");
        fprintf(stderr, "  run       Compile and run the program\n");
        return 1;
    }

    if (lex_only) {
        printf("Source:\n%s\n", source);
        printf("Tokens:\n");
        printf("%-4s  %-12s  %s\n", "LINE", "TYPE", "VALUE");
        printf("----  ------------  -----\n");

        Lexer lexer;
        lexer_init(&lexer, source);

        Token token;
        do {
            token = lexer_next(&lexer);
            printf("%3d:%-2d  %-12s  ", token.line, token.column, token_type_name(token.type));

            if (token.type == TOK_ERROR) {
                printf("%s", token.start);
            } else if (token.type != TOK_EOF) {
                printf("%.*s", (int)token.length, token.start);
            }
            printf("\n");
        } while (token.type != TOK_EOF);
    } else {
        Parser parser;
        parser_init_with_path(&parser, source, source_file);
        Node* ast = parser_parse(&parser);

        if (parser.had_error) {
            fprintf(stderr, "Parse failed\n");
            node_free(ast);
            parser_free(&parser);
            if (free_source)
                free(source);
            return 1;
        }

        if (print_ast_flag) {
            printf("AST:\n");
            printf("----\n");
            print_ast(ast, 0);
            printf("\n");
        }

        if (!parse_only) {
            Checker checker;
            checker_init(&checker);
            checker_set_direct_imports(&checker, parser.direct_imports,
                                       parser.direct_imports_count);
            int ok = checker_check(&checker, ast);

            if (!ok) {
                checker_free(&checker);
                fprintf(stderr, "Type check failed\n");
                node_free(ast);
                parser_free(&parser);
                if (free_source)
                    free(source);
                return 1;
            }

            if (!check_only) {
                // Code generation
                FILE* out = stdout;
                if (output_file) {
                    out = fopen(output_file, "w");
                    if (!out) {
                        checker_free(&checker);
                        fprintf(stderr, "Could not open output file: %s\n", output_file);
                        node_free(ast);
                        parser_free(&parser);
                        if (free_source)
                            free(source);
                        return 1;
                    }
                }

                CodeGen gen;
                codegen_init(&gen, out);
                codegen_emit(&gen, ast);
                checker_free(&checker); // Free types after codegen

                if (output_file) {
                    fclose(out);
                    fprintf(stderr, "Generated: %s\n", output_file);
                }
            } else {
                checker_free(&checker);
                fprintf(stderr, "Type check passed!\n");
            }
        }

        node_free(ast);
        parser_free(&parser);
    }

    if (free_source)
        free(source);
    return 0;
}
