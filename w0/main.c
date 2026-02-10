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
#include "util.h"

// Compile source to C code, writing to the given output file.
// Returns 0 on success, 1 on error.
static int compile_to_c(const char* source, const char* source_path, const char* lib_path,
                        int rc_debug, FILE* out) {
    Parser parser;
    parser_init_with_path(&parser, source, source_path, lib_path);
    Node* ast = parser_parse(&parser);

    if (parser.had_error) {
        fprintf(stderr, "Parse failed\n");
        node_free(ast);
        parser_free(&parser);
        return 1;
    }

    Checker checker;
    checker_init(&checker);
    checker_set_direct_imports(&checker, parser.direct_imports, parser.direct_imports_count);
    int ok = checker_check(&checker, ast);

    if (!ok) {
        checker_free(&checker);
        fprintf(stderr, "Type check failed\n");
        node_free(ast);
        parser_free(&parser);
        return 1;
    }

    CodeGen gen;
    codegen_init(&gen, out, checker.generic_instances, checker.generic_instance_count,
                 checker.span_instances, checker.span_instance_count, checker.vec_instances,
                 checker.vec_instance_count, checker.trait_impls, checker.trait_impl_count,
                 rc_debug);
    codegen_emit(&gen, ast);

    checker_free(&checker);
    node_free(ast);
    parser_free(&parser);
    return 0;
}

static int compile_and_run(const char* source_path, int argc, char** argv, const char* lib_path,
                           int rc_debug) {
    char* source = read_file(source_path);
    if (!source) {
        fprintf(stderr, "Could not open file: %s\n", source_path);
        return 1;
    }

    // Create temp files
    char c_base[]   = "/tmp/w0_XXXXXX";
    char exe_path[] = "/tmp/w0_XXXXXX";

    int c_fd = mkstemp(c_base);
    if (c_fd < 0) {
        fprintf(stderr, "Could not create temp file\n");
        free(source);
        return 1;
    }

    char c_path[256];
    snprintf(c_path, sizeof(c_path), "%s.c", c_base);
    close(c_fd);
    if (rename(c_base, c_path) != 0) {
        fprintf(stderr, "Could not rename temp file\n");
        unlink(c_base);
        free(source);
        return 1;
    }

    int exe_fd = mkstemp(exe_path);
    if (exe_fd < 0) {
        fprintf(stderr, "Could not create temp file\n");
        unlink(c_path);
        free(source);
        return 1;
    }
    close(exe_fd);

    FILE* c_file = fopen(c_path, "w");
    if (!c_file) {
        fprintf(stderr, "Could not open temp file for writing\n");
        unlink(c_path);
        unlink(exe_path);
        free(source);
        return 1;
    }

    int result = compile_to_c(source, source_path, lib_path, rc_debug, c_file);
    fclose(c_file);
    free(source);

    if (result != 0) {
        unlink(c_path);
        unlink(exe_path);
        return 1;
    }

    // Compile with cc
    char cmd[1024];
    if (lib_path) {
        snprintf(cmd, sizeof(cmd), "cc -o %s %s -I%s/include", exe_path, c_path, lib_path);
    } else {
        snprintf(cmd, sizeof(cmd), "cc -o %s %s", exe_path, c_path);
    }

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
    int         rc_debug       = 0;
    const char* output_file    = NULL;
    const char* lib_path       = NULL;

    // Pre-scan for --lib-path and --rc-debug (needed before 'run' subcommand)
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--lib-path") == 0 && i + 1 < argc) {
            lib_path = argv[i + 1];
        } else if (strcmp(argv[i], "--rc-debug") == 0) {
            rc_debug = 1;
        }
    }

    // Check for 'run' subcommand (may appear after flags like --lib-path)
    int run_idx = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "run") == 0) {
            run_idx = i;
            break;
        }
        // Skip the value argument of --lib-path and -o
        if ((strcmp(argv[i], "--lib-path") == 0 || strcmp(argv[i], "-o") == 0) && i + 1 < argc) {
            i++;
        }
    }
    if (run_idx >= 0) {
        // Find the source file: first non-flag argument after 'run'
        const char* run_source     = NULL;
        int         run_args_start = 0;
        for (int i = run_idx + 1; i < argc; i++) {
            if (strcmp(argv[i], "--lib-path") == 0 && i + 1 < argc) {
                i++; // skip value
            } else if (strcmp(argv[i], "--rc-debug") == 0) {
                // skip flag
            } else {
                run_source     = argv[i];
                run_args_start = i;
                break;
            }
        }
        if (!run_source) {
            fprintf(stderr, "Usage: %s run [options] <source-file> [args...]\n", argv[0]);
            return 1;
        }
        return compile_and_run(run_source, argc - run_args_start, argv + run_args_start, lib_path,
                               rc_debug);
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
        } else if (strcmp(argv[arg_idx], "--lib-path") == 0 && arg_idx + 1 < argc) {
            arg_idx++;
            lib_path = argv[arg_idx];
        } else if (strcmp(argv[arg_idx], "--rc-debug") == 0) {
            rc_debug = 1;
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
        if (!source) {
            fprintf(stderr, "Could not open file: %s\n", source_file);
            return 1;
        }
        free_source = 1;
    } else {
        fprintf(stderr, "Usage: %s [options] <source-file>\n", argv[0]);
        fprintf(stderr, "       %s run <source-file> [args...]\n", argv[0]);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  --lex     Lex only (print tokens)\n");
        fprintf(stderr, "  --parse   Parse only (no type checking)\n");
        fprintf(stderr, "  --check   Type check only (no code generation)\n");
        fprintf(stderr, "  --ast     Print AST\n");
        fprintf(stderr, "  --lib-path <dir>\n");
        fprintf(stderr, "            Library search path for module imports\n");
        fprintf(stderr, "  --rc-debug\n");
        fprintf(stderr, "            Emit RC tracking debug output to stderr\n");
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
    } else if (!parse_only && !check_only && !print_ast_flag) {
        // Normal compilation
        FILE* out = stdout;
        if (output_file) {
            out = fopen(output_file, "w");
            if (!out) {
                fprintf(stderr, "Could not open output file: %s\n", output_file);
                if (free_source)
                    free(source);
                return 1;
            }
        }

        int result = compile_to_c(source, source_file, lib_path, rc_debug, out);

        if (output_file) {
            fclose(out);
            if (result == 0)
                fprintf(stderr, "Generated: %s\n", output_file);
        }
        if (free_source)
            free(source);
        return result;
    } else {
        // Debug modes: --parse, --check, --ast
        Parser parser;
        parser_init_with_path(&parser, source, source_file, lib_path);
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
                // --ast with full codegen
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
                codegen_init(&gen, out, checker.generic_instances, checker.generic_instance_count,
                             checker.span_instances, checker.span_instance_count,
                             checker.vec_instances, checker.vec_instance_count, checker.trait_impls,
                             checker.trait_impl_count, rc_debug);
                codegen_emit(&gen, ast);
                checker_free(&checker);

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
