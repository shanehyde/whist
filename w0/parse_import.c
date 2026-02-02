#include "parse_import.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_util.h"

static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = malloc(size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    size_t read  = fread(buffer, 1, size, file);
    buffer[read] = '\0';
    fclose(file);
    return buffer;
}

int parse_import_stmt(Parser* parser, NodeList* decls) {
    // Expect identifier after 'import'
    Token module_name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected module name after 'import'");

    // Expect semicolon
    consume_token(parser, TOK_SEMICOLON, "Expected ';' after import statement");

    // Build path: lib/<module_name>.w
    char path[1024];
    snprintf(path, sizeof(path), "lib/%.*s.w", (int)module_name.length, module_name.start);

    // Read the imported file
    char* source = read_file(path);
    if (!source) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Could not read import file: %s", path);
        parse_error(parser, error_msg);
        return 0;
    }

    // Parse the imported file
    Parser import_parser;
    parser_init(&import_parser, source);
    Node* import_ast = parser_parse(&import_parser);

    if (import_parser.had_error) {
        fprintf(stderr, "Failed to parse imported file: %s\n", path);
        node_free(import_ast);
        free(source);
        return 0;
    }

    // Merge declarations from imported file into current program
    if (import_ast && import_ast->type == NODE_PROGRAM) {
        for (int i = 0; i < import_ast->as.program.decls.count; i++) {
            nodelist_push(decls, import_ast->as.program.decls.nodes[i]);
        }
        // Don't free the individual declaration nodes, just the program wrapper
        import_ast->as.program.decls.count = 0; // Prevent double-free
    }

    node_free(import_ast);
    free(source);
    return 1;
}
