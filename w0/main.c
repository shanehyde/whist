#include "checker.h"
#include "codegen.h"
#include "lexer.h"
#include "parser.h"
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

static void print_indent(int depth) {
    for (int i = 0; i < depth; i++)
        printf("  ");
}

static void print_ast(Node* node, int depth) {
    if (!node) {
        print_indent(depth);
        printf("(null)\n");
        return;
    }

    print_indent(depth);

    switch (node->type) {
    case NODE_PROGRAM:
        printf("Program\n");
        for (int i = 0; i < node->as.program.decls.count; i++) {
            print_ast(node->as.program.decls.nodes[i], depth + 1);
        }
        break;

    case NODE_FUNC_DECL:
        printf("FuncDecl: %.*s\n", node->as.func_decl.name_length, node->as.func_decl.name);
        if (node->as.func_decl.params.count > 0) {
            print_indent(depth + 1);
            printf("Params:\n");
            for (int i = 0; i < node->as.func_decl.params.count; i++) {
                print_ast(node->as.func_decl.params.nodes[i], depth + 2);
            }
        }
        if (node->as.func_decl.return_type) {
            print_indent(depth + 1);
            printf("ReturnType:\n");
            print_ast(node->as.func_decl.return_type, depth + 2);
        }
        print_indent(depth + 1);
        printf("Body:\n");
        print_ast(node->as.func_decl.body, depth + 2);
        break;

    case NODE_PARAM:
        printf("Param: %.*s", node->as.param.name_length, node->as.param.name);
        if (node->as.param.type) {
            printf("\n");
            print_indent(depth + 1);
            printf("Type:\n");
            print_ast(node->as.param.type, depth + 2);
        } else {
            printf("\n");
        }
        break;

    case NODE_STRUCT_DECL:
        printf("StructDecl: %.*s\n", node->as.struct_decl.name_length, node->as.struct_decl.name);
        for (int i = 0; i < node->as.struct_decl.fields.count; i++) {
            print_ast(node->as.struct_decl.fields.nodes[i], depth + 1);
        }
        break;

    case NODE_FIELD:
        printf("Field: %.*s\n", node->as.field.name_length, node->as.field.name);
        print_ast(node->as.field.type, depth + 1);
        break;

    case NODE_ENUM_DECL:
        printf("EnumDecl: %.*s\n", node->as.enum_decl.name_length, node->as.enum_decl.name);
        for (int i = 0; i < node->as.enum_decl.values.count; i++) {
            print_ast(node->as.enum_decl.values.nodes[i], depth + 1);
        }
        break;

    case NODE_VAR_DECL:
        printf("VarDecl: %.*s%s\n", node->as.var_decl.name_length, node->as.var_decl.name,
               node->as.var_decl.is_const ? " (const)" : "");
        if (node->as.var_decl.type) {
            print_indent(depth + 1);
            printf("Type:\n");
            print_ast(node->as.var_decl.type, depth + 2);
        }
        if (node->as.var_decl.init) {
            print_indent(depth + 1);
            printf("Init:\n");
            print_ast(node->as.var_decl.init, depth + 2);
        }
        break;

    case NODE_BLOCK:
        printf("Block\n");
        for (int i = 0; i < node->as.block.stmts.count; i++) {
            print_ast(node->as.block.stmts.nodes[i], depth + 1);
        }
        break;

    case NODE_IF:
        printf("If\n");
        print_indent(depth + 1);
        printf("Cond:\n");
        print_ast(node->as.if_stmt.cond, depth + 2);
        print_indent(depth + 1);
        printf("Then:\n");
        print_ast(node->as.if_stmt.then_block, depth + 2);
        if (node->as.if_stmt.else_block) {
            print_indent(depth + 1);
            printf("Else:\n");
            print_ast(node->as.if_stmt.else_block, depth + 2);
        }
        break;

    case NODE_WHILE:
        printf("While\n");
        print_indent(depth + 1);
        printf("Cond:\n");
        print_ast(node->as.while_stmt.cond, depth + 2);
        print_indent(depth + 1);
        printf("Body:\n");
        print_ast(node->as.while_stmt.body, depth + 2);
        break;

    case NODE_FOR:
        printf("For\n");
        if (node->as.for_stmt.init) {
            print_indent(depth + 1);
            printf("Init:\n");
            print_ast(node->as.for_stmt.init, depth + 2);
        }
        if (node->as.for_stmt.cond) {
            print_indent(depth + 1);
            printf("Cond:\n");
            print_ast(node->as.for_stmt.cond, depth + 2);
        }
        if (node->as.for_stmt.post) {
            print_indent(depth + 1);
            printf("Post:\n");
            print_ast(node->as.for_stmt.post, depth + 2);
        }
        print_indent(depth + 1);
        printf("Body:\n");
        print_ast(node->as.for_stmt.body, depth + 2);
        break;

    case NODE_RETURN:
        printf("Return\n");
        if (node->as.return_stmt.value) {
            print_ast(node->as.return_stmt.value, depth + 1);
        }
        break;

    case NODE_BREAK:
        printf("Break\n");
        break;

    case NODE_CONTINUE:
        printf("Continue\n");
        break;

    case NODE_EXPR_STMT:
        printf("ExprStmt\n");
        print_ast(node->as.expr_stmt.expr, depth + 1);
        break;

    case NODE_BINARY:
        printf("Binary: %s\n", token_type_name(node->as.binary.op));
        print_ast(node->as.binary.left, depth + 1);
        print_ast(node->as.binary.right, depth + 1);
        break;

    case NODE_UNARY:
        printf("Unary: %s%s\n", token_type_name(node->as.unary.op),
               node->as.unary.postfix ? " (postfix)" : "");
        print_ast(node->as.unary.operand, depth + 1);
        break;

    case NODE_CALL:
        printf("Call\n");
        print_indent(depth + 1);
        printf("Func:\n");
        print_ast(node->as.call.func, depth + 2);
        if (node->as.call.args.count > 0) {
            print_indent(depth + 1);
            printf("Args:\n");
            for (int i = 0; i < node->as.call.args.count; i++) {
                print_ast(node->as.call.args.nodes[i], depth + 2);
            }
        }
        break;

    case NODE_INDEX:
        printf("Index\n");
        print_ast(node->as.index.object, depth + 1);
        if (node->as.index.index) {
            print_ast(node->as.index.index, depth + 1);
        }
        break;

    case NODE_MEMBER:
        printf("Member: %.*s%s\n", node->as.member.length, node->as.member.name,
               node->as.member.arrow ? " (->)" : "");
        print_ast(node->as.member.object, depth + 1);
        break;

    case NODE_ASSIGN:
        printf("Assign: %s\n", token_type_name(node->as.assign.op));
        print_ast(node->as.assign.target, depth + 1);
        print_ast(node->as.assign.value, depth + 1);
        break;

    case NODE_INT_LIT:
        printf("Int: %ld\n", node->as.int_lit.value);
        break;

    case NODE_FLOAT_LIT:
        printf("Float: %g\n", node->as.float_lit.value);
        break;

    case NODE_STRING_LIT:
        printf("String: \"%.*s\"\n", node->as.string_lit.length, node->as.string_lit.value);
        break;

    case NODE_CHAR_LIT:
        printf("Char: '%c'\n", node->as.char_lit.value);
        break;

    case NODE_BOOL_LIT:
        printf("Bool: %s\n", node->as.bool_lit.value ? "true" : "false");
        break;

    case NODE_NULL_LIT:
        printf("Null\n");
        break;

    case NODE_IDENT:
        printf("Ident: %.*s\n", node->as.ident.length, node->as.ident.name);
        break;

    default:
        printf("Unknown node type: %d\n", node->type);
        break;
    }
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
        // Default test input
        source = "struct Point {\n"
                 "    x: i64,\n"
                 "    y: i64,\n"
                 "}\n"
                 "\n"
                 "enum Color {\n"
                 "    Red,\n"
                 "    Green,\n"
                 "    Blue,\n"
                 "}\n"
                 "\n"
                 "func add(a: i64, b: i64): i64 {\n"
                 "    return a + b;\n"
                 "}\n"
                 "\n"
                 "func main(): i64 {\n"
                 "    var x = 42;\n"
                 "    var y: f32 = 3.14;\n"
                 "    const PI = 3.14159;\n"
                 "\n"
                 "    if (x > 0 && y != 0.0) {\n"
                 "        return add(x, 1);\n"
                 "    } else {\n"
                 "        return 0;\n"
                 "    }\n"
                 "\n"
                 "    for (var i = 0; i < 10; i++) {\n"
                 "        x = x + i;\n"
                 "    }\n"
                 "\n"
                 "    while (x > 0) {\n"
                 "        x--;\n"
                 "    }\n"
                 "\n"
                 "    return x;\n"
                 "}\n";
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
