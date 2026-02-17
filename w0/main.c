#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "alloc.h"
#include "checker.h"
#include "codegen.h"
#include "lexer.h"
#include "module_loader.h"
#include "parser.h"
#include "print_ast.h"
#include "util.h"

typedef struct {
    int         lex_only;
    int         parse_only;
    int         check_only;
    int         print_ast_flag;
    int         rc_debug;
    int         emit_c;
    const char* output_file;
    const char* lib_path;
} MainOptions;

// Run a program directly via fork/exec, bypassing the shell.
// Parent ignores SIGINT/SIGQUIT while waiting so it can clean up afterward.
// Returns the raw wait status, or -1 on fork failure.
static int fork_exec_wait(char* const argv[]) {
    struct sigaction sa_ign, old_int, old_quit;
    memset(&sa_ign, 0, sizeof(sa_ign));
    sa_ign.sa_handler = SIG_IGN;
    sigemptyset(&sa_ign.sa_mask);
    sigaction(SIGINT, &sa_ign, &old_int);
    sigaction(SIGQUIT, &sa_ign, &old_quit);

    pid_t pid = fork();
    if (pid < 0) {
        sigaction(SIGINT, &old_int, NULL);
        sigaction(SIGQUIT, &old_quit, NULL);
        return -1;
    }
    if (pid == 0) {
        // Child: restore signal handling from before we ignored
        sigaction(SIGINT, &old_int, NULL);
        sigaction(SIGQUIT, &old_quit, NULL);
        execvp(argv[0], argv);
        _exit(127);
    }

    // Parent: wait for child
    int status;
    waitpid(pid, &status, 0);

    // Restore original signal handlers
    sigaction(SIGINT, &old_int, NULL);
    sigaction(SIGQUIT, &old_quit, NULL);

    return status;
}

// Compile source to C code, writing to the given output file.
// Returns 0 on success, 1 on error.
static CodeGenChecker make_codegen_checker(Checker* checker) {
    return (CodeGenChecker){
        .instances           = checker->generics.instances,
        .instance_count      = checker->generics.instance_count,
        .spans               = checker->containers.spans,
        .span_count          = checker->containers.span_count,
        .vecs                = checker->containers.vecs,
        .vec_count           = checker->containers.vec_count,
        .traits              = checker->traits.impls,
        .trait_count         = checker->traits.impl_count,
        .func_defs           = checker->generics.func_defs,
        .func_def_count      = checker->generics.func_def_count,
        .func_instances      = checker->generics.func_instances,
        .func_instance_count = checker->generics.func_instance_count,
        .sem                 = checker->sem,
    };
}

static int compile_to_c(const char* source, const char* source_path, const char* lib_path,
                        int rc_debug, int test_mode, FILE* out) {
    ModuleLoader loader;
    module_loader_init(&loader, lib_path);

    Parser parser;
    parser_init_with_loader(&parser, source, source_path, &loader);
    Node* ast = parser_parse(&parser);

    if (parser.had_error) {
        fprintf(stderr, "Parse failed\n");
        node_free(ast);
        parser_free(&parser);
        module_loader_free(&loader);
        return 1;
    }

    Checker checker;
    checker_init(&checker);
    checker_set_direct_imports(&checker, loader.direct_imports, loader.direct_imports_count);
    int ok = checker_check(&checker, ast);

    if (!ok) {
        checker_free(&checker);
        fprintf(stderr, "Type check failed\n");
        node_free(ast);
        parser_free(&parser);
        module_loader_free(&loader);
        return 1;
    }

    CodeGen gen;
    codegen_init(&gen, out, make_codegen_checker(&checker), rc_debug, test_mode, source_path);
    codegen_emit(&gen, ast);
    codegen_free(&gen);

    checker_free(&checker);
    node_free(ast);
    parser_free(&parser);
    module_loader_free(&loader);
    return 0;
}

static int compile_and_run(const char* source_path, int argc, char** argv, const char* lib_path,
                           int rc_debug, int emit_c) {
    char* source = read_file(source_path);
    if (!source) {
        fprintf(stderr, "Could not open file: %s\n", source_path);
        return 1;
    }

    // --emit-c: just dump generated C to stdout and exit
    if (emit_c) {
        int result = compile_to_c(source, source_path, lib_path, rc_debug, 0, stdout);
        free(source);
        return result;
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

    int result = compile_to_c(source, source_path, lib_path, rc_debug, 0, c_file);
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
        snprintf(cmd, sizeof(cmd), "cc -o %s %s -I%s/include %s/whist_runtime.c", exe_path, c_path,
                 lib_path, lib_path);
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

    // Build argv for child process
    char** child_argv = xmalloc((argc + 1) * sizeof(char*));
    child_argv[0]     = exe_path;
    for (int i = 1; i < argc; i++) {
        child_argv[i] = argv[i];
    }
    child_argv[argc] = NULL;

    // Run the executable directly via fork/exec
    int run_result = fork_exec_wait(child_argv);
    free(child_argv);

    // Cleanup
    unlink(exe_path);

    if (WIFEXITED(run_result)) {
        return WEXITSTATUS(run_result);
    }
    if (WIFSIGNALED(run_result)) {
        raise(WTERMSIG(run_result));
        return 128 + WTERMSIG(run_result);
    }
    return 1;
}

static int compile_and_test(const char* source_path, const char* lib_path, int rc_debug,
                            int emit_c) {
    char* source = read_file(source_path);
    if (!source) {
        fprintf(stderr, "Could not open file: %s\n", source_path);
        return 1;
    }

    // --emit-c: just dump generated C to stdout and exit
    if (emit_c) {
        int result = compile_to_c(source, source_path, lib_path, rc_debug, 1, stdout);
        free(source);
        return result;
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

    int result = compile_to_c(source, source_path, lib_path, rc_debug, 1, c_file);
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
        snprintf(cmd, sizeof(cmd), "cc -o %s %s -I%s/include %s/whist_runtime.c", exe_path, c_path,
                 lib_path, lib_path);
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

    // Run the test executable directly via fork/exec
    char* test_argv[] = {exe_path, NULL};
    int   run_result  = fork_exec_wait(test_argv);

    // Cleanup
    unlink(exe_path);

    if (WIFEXITED(run_result)) {
        return WEXITSTATUS(run_result);
    }
    if (WIFSIGNALED(run_result)) {
        raise(WTERMSIG(run_result));
        return 128 + WTERMSIG(run_result);
    }
    return 1;
}

static void print_usage(const char* program) {
    fprintf(stderr, "Usage: %s [options] <source-file>\n", program);
    fprintf(stderr, "       %s run [options] <source-file> [args...]\n", program);
    fprintf(stderr, "       %s test [options] <source-file>\n", program);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --lex     Lex only (print tokens)\n");
    fprintf(stderr, "  --parse   Parse only (no type checking)\n");
    fprintf(stderr, "  --check   Type check only (no code generation)\n");
    fprintf(stderr, "  --ast     Print AST\n");
    fprintf(stderr, "  --lib-path <dir>\n");
    fprintf(stderr, "            Library search path for module imports\n");
    fprintf(stderr, "  --rc-debug\n");
    fprintf(stderr, "            Emit RC tracking debug output to stderr\n");
    fprintf(stderr, "  --emit-c  Dump generated C to stdout (works with run/test)\n");
    fprintf(stderr, "  -o <file> Output file\n");
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  run       Compile and run the program\n");
    fprintf(stderr, "  test      Compile and run tests\n");
}

static int is_option_with_value(const char* arg) {
    return strcmp(arg, "--lib-path") == 0 || strcmp(arg, "-o") == 0;
}

static void prescan_global_options(int argc, char** argv, MainOptions* opts) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--lib-path") == 0 && i + 1 < argc) {
            opts->lib_path = argv[i + 1];
        } else if (strcmp(argv[i], "--rc-debug") == 0) {
            opts->rc_debug = 1;
        } else if (strcmp(argv[i], "--emit-c") == 0) {
            opts->emit_c = 1;
        }
    }
}

static int find_subcommand_index(int argc, char** argv, const char* subcommand) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], subcommand) == 0) {
            return i;
        }
        if (is_option_with_value(argv[i]) && i + 1 < argc) {
            i++;
        }
    }
    return -1;
}

static const char* find_subcommand_source(int argc, char** argv, int subcommand_idx,
                                          int* arg_start) {
    for (int i = subcommand_idx + 1; i < argc; i++) {
        if (strcmp(argv[i], "--lib-path") == 0 && i + 1 < argc) {
            i++;
        } else if (strcmp(argv[i], "--rc-debug") == 0 || strcmp(argv[i], "--emit-c") == 0) {
            // Skip flag.
        } else {
            if (arg_start) {
                *arg_start = i;
            }
            return argv[i];
        }
    }
    return NULL;
}

static int try_handle_subcommand(int argc, char** argv, const MainOptions* opts) {
    int run_idx = find_subcommand_index(argc, argv, "run");
    if (run_idx >= 0) {
        int         run_args_start = 0;
        const char* run_source     = find_subcommand_source(argc, argv, run_idx, &run_args_start);
        if (!run_source) {
            fprintf(stderr, "Usage: %s run [options] <source-file> [args...]\n", argv[0]);
            return 1;
        }
        return compile_and_run(run_source, argc - run_args_start, argv + run_args_start,
                               opts->lib_path, opts->rc_debug, opts->emit_c);
    }

    int test_idx = find_subcommand_index(argc, argv, "test");
    if (test_idx >= 0) {
        const char* test_source = find_subcommand_source(argc, argv, test_idx, NULL);
        if (!test_source) {
            fprintf(stderr, "Usage: %s test [options] <source-file>\n", argv[0]);
            return 1;
        }
        return compile_and_test(test_source, opts->lib_path, opts->rc_debug, opts->emit_c);
    }

    return -1;
}

static void parse_main_options(int argc, char** argv, MainOptions* opts, int* arg_idx) {
    *arg_idx = 1;
    while (*arg_idx < argc && argv[*arg_idx][0] == '-') {
        if (strcmp(argv[*arg_idx], "--lex") == 0) {
            opts->lex_only = 1;
        } else if (strcmp(argv[*arg_idx], "--parse") == 0) {
            opts->parse_only = 1;
        } else if (strcmp(argv[*arg_idx], "--check") == 0) {
            opts->check_only = 1;
        } else if (strcmp(argv[*arg_idx], "--ast") == 0) {
            opts->print_ast_flag = 1;
        } else if (strcmp(argv[*arg_idx], "--lib-path") == 0 && *arg_idx + 1 < argc) {
            (*arg_idx)++;
            opts->lib_path = argv[*arg_idx];
        } else if (strcmp(argv[*arg_idx], "--rc-debug") == 0) {
            opts->rc_debug = 1;
        } else if (strcmp(argv[*arg_idx], "--emit-c") == 0) {
            opts->emit_c = 1;
        } else if (strcmp(argv[*arg_idx], "-o") == 0 && *arg_idx + 1 < argc) {
            (*arg_idx)++;
            opts->output_file = argv[*arg_idx];
        }
        (*arg_idx)++;
    }
}

static void run_lex_mode(const char* source) {
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
}

static int run_normal_compile(const MainOptions* opts, const char* source,
                              const char* source_file) {
    FILE* out = stdout;
    if (opts->output_file) {
        out = fopen(opts->output_file, "w");
        if (!out) {
            fprintf(stderr, "Could not open output file: %s\n", opts->output_file);
            return 1;
        }
    }

    int result = compile_to_c(source, source_file, opts->lib_path, opts->rc_debug, 0, out);

    if (opts->output_file) {
        fclose(out);
        if (result == 0) {
            fprintf(stderr, "Generated: %s\n", opts->output_file);
        }
    }
    return result;
}

static int run_debug_pipeline(const MainOptions* opts, const char* source,
                              const char* source_file) {
    ModuleLoader loader;
    module_loader_init(&loader, opts->lib_path);

    Parser parser;
    parser_init_with_loader(&parser, source, source_file, &loader);
    Node* ast = parser_parse(&parser);

    if (parser.had_error) {
        fprintf(stderr, "Parse failed\n");
        node_free(ast);
        parser_free(&parser);
        module_loader_free(&loader);
        return 1;
    }

    if (opts->print_ast_flag) {
        printf("AST:\n");
        printf("----\n");
        print_ast(ast, 0);
        printf("\n");
    }

    if (!opts->parse_only) {
        Checker checker;
        checker_init(&checker);
        checker_set_direct_imports(&checker, loader.direct_imports, loader.direct_imports_count);
        int ok = checker_check(&checker, ast);

        if (!ok) {
            checker_free(&checker);
            fprintf(stderr, "Type check failed\n");
            node_free(ast);
            parser_free(&parser);
            module_loader_free(&loader);
            return 1;
        }

        if (!opts->check_only) {
            FILE* out = stdout;
            if (opts->output_file) {
                out = fopen(opts->output_file, "w");
                if (!out) {
                    checker_free(&checker);
                    fprintf(stderr, "Could not open output file: %s\n", opts->output_file);
                    node_free(ast);
                    parser_free(&parser);
                    module_loader_free(&loader);
                    return 1;
                }
            }

            CodeGen gen;
            codegen_init(&gen, out, make_codegen_checker(&checker), opts->rc_debug, 0, source_file);
            codegen_emit(&gen, ast);
            codegen_free(&gen);
            checker_free(&checker);

            if (opts->output_file) {
                fclose(out);
                fprintf(stderr, "Generated: %s\n", opts->output_file);
            }
        } else {
            checker_free(&checker);
            fprintf(stderr, "Type check passed!\n");
        }
    }

    node_free(ast);
    parser_free(&parser);
    module_loader_free(&loader);
    return 0;
}

int main(int argc, char** argv) {
    MainOptions opts = {0};
    prescan_global_options(argc, argv, &opts);

    int subcommand_result = try_handle_subcommand(argc, argv, &opts);
    if (subcommand_result >= 0) {
        return subcommand_result;
    }

    int arg_idx = 1;
    parse_main_options(argc, argv, &opts, &arg_idx);

    if (arg_idx >= argc) {
        print_usage(argv[0]);
        return 1;
    }

    const char* source_file = argv[arg_idx];
    char*       source      = read_file(source_file);
    if (!source) {
        fprintf(stderr, "Could not open file: %s\n", source_file);
        return 1;
    }

    int result = 0;
    if (opts.lex_only) {
        run_lex_mode(source);
    } else if (!opts.parse_only && !opts.check_only && !opts.print_ast_flag) {
        result = run_normal_compile(&opts, source, source_file);
    } else {
        result = run_debug_pipeline(&opts, source, source_file);
    }

    free(source);
    return result;
}
