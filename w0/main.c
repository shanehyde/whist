#include "checker.h"
#include "codegen.h"
#include "lexer.h"
#include "parser.h"
#include "print_ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Could not open file: %s\n", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = malloc(size + 1);
    if (!buffer) {
        fprintf(stderr, "Could not allocate memory for file\n");
        fclose(file);
        return NULL;
    }

    size_t read  = fread(buffer, 1, size, file);
    buffer[read] = '\0';
    fclose(file);
    return buffer;
}

int main(int argc, char** argv) {
    char*       source;
    int         free_source    = 0;
    int         lex_only       = 0;
    int         parse_only     = 0;
    int         check_only     = 0;
    int         print_ast_flag = 0;
    const char* output_file    = NULL;

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

    if (arg_idx < argc) {
        source = read_file(argv[arg_idx]);
        if (!source)
            return 1;
        free_source = 1;
    } else {
        fprintf(stderr, "Usage: %s [options] <source-file>\n", argv[0]);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  --lex     Lex only (print tokens)\n");
        fprintf(stderr, "  --parse   Parse only (no type checking)\n");
        fprintf(stderr, "  --check   Type check only (no code generation)\n");
        fprintf(stderr, "  --ast     Print AST\n");
        fprintf(stderr, "  -o <file> Output file\n");
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
        parser_init(&parser, source);
        Node* ast = parser_parse(&parser);

        if (parser.had_error) {
            fprintf(stderr, "Parse failed\n");
            node_free(ast);
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
            int ok = checker_check(&checker, ast);
            checker_free(&checker);

            if (!ok) {
                fprintf(stderr, "Type check failed\n");
                node_free(ast);
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
                        fprintf(stderr, "Could not open output file: %s\n", output_file);
                        node_free(ast);
                        if (free_source)
                            free(source);
                        return 1;
                    }
                }

                CodeGen gen;
                codegen_init(&gen, out);
                codegen_emit(&gen, ast);

                if (output_file) {
                    fclose(out);
                    fprintf(stderr, "Generated: %s\n", output_file);
                }
            } else {
                fprintf(stderr, "Type check passed!\n");
            }
        }

        node_free(ast);
    }

    if (free_source)
        free(source);
    return 0;
}
