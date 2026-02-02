#include "parse_import.h"

#include <libgen.h>
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

// Check if a module has already been imported
static int is_module_imported(Parser* parser, const char* module_name, size_t length) {
    for (int i = 0; i < parser->imported_modules_count; i++) {
        if (strlen(parser->imported_modules[i]) == length &&
            strncmp(parser->imported_modules[i], module_name, length) == 0) {
            return 1;
        }
    }
    return 0;
}

// Add a module name to the imported list
static void add_imported_module(Parser* parser, const char* module_name, size_t length) {
    // Grow array if needed
    if (parser->imported_modules_count >= parser->imported_modules_capacity) {
        int new_capacity =
            parser->imported_modules_capacity == 0 ? 8 : parser->imported_modules_capacity * 2;
        char** new_array = realloc(parser->imported_modules, new_capacity * sizeof(char*));
        if (!new_array)
            return;
        parser->imported_modules          = new_array;
        parser->imported_modules_capacity = new_capacity;
    }

    // Copy the module name
    char* name_copy = malloc(length + 1);
    if (!name_copy)
        return;
    memcpy(name_copy, module_name, length);
    name_copy[length]                                          = '\0';
    parser->imported_modules[parser->imported_modules_count++] = name_copy;
}

// Add source buffer to parser's list (keeps it alive for AST references)
static void add_imported_source(Parser* parser, char* source) {
    // Grow array if needed
    if (parser->imported_sources_count >= parser->imported_sources_capacity) {
        int new_capacity =
            parser->imported_sources_capacity == 0 ? 8 : parser->imported_sources_capacity * 2;
        char** new_array = realloc(parser->imported_sources, new_capacity * sizeof(char*));
        if (!new_array)
            return;
        parser->imported_sources          = new_array;
        parser->imported_sources_capacity = new_capacity;
    }
    parser->imported_sources[parser->imported_sources_count++] = source;
}

// Check if file exists
static int file_exists(const char* path) {
    FILE* f = fopen(path, "r");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

// Build path to imported module
// Tries source-relative path first, then falls back to cwd-relative path
static void build_import_path(Parser* parser, char* path, size_t path_size, const char* module_name,
                              size_t module_length) {
    if (parser->source_path) {
        // Make a copy because dirname may modify its argument
        char* path_copy = strdup(parser->source_path);
        if (path_copy) {
            char* dir = dirname(path_copy);
            snprintf(path, path_size, "%s/lib/%.*s.w", dir, (int)module_length, module_name);
            free(path_copy);
            if (file_exists(path)) {
                return;
            }
            // Fall through to try cwd-relative path
        }
    }
    // Fallback: use lib/ relative to current working directory
    snprintf(path, path_size, "lib/%.*s.w", (int)module_length, module_name);
}

int parse_import_stmt(Parser* parser, NodeList* decls) {
    // Expect identifier after 'import'
    Token module_name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected module name after 'import'");

    // Expect semicolon
    consume_token(parser, TOK_SEMICOLON, "Expected ';' after import statement");

    // Check if already imported (skip silently)
    if (is_module_imported(parser, module_name.start, module_name.length)) {
        return 1; // Already imported, nothing to do
    }

    // Mark as imported before parsing (prevents cycles)
    add_imported_module(parser, module_name.start, module_name.length);

    // Build path to import file
    char path[1024];
    build_import_path(parser, path, sizeof(path), module_name.start, module_name.length);

    // Read the imported file
    char* source = read_file(path);
    if (!source) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Could not read import file: %s", path);
        parse_error(parser, error_msg);
        return 0;
    }

    // Store source buffer to keep it alive (AST nodes reference strings in it)
    add_imported_source(parser, source);

    // Parse the imported file (inherits source_path for nested imports)
    Parser import_parser;
    parser_init_with_path(&import_parser, source, path);

    // Share the imported modules list with the sub-parser to handle transitive imports
    import_parser.imported_modules          = parser->imported_modules;
    import_parser.imported_modules_count    = parser->imported_modules_count;
    import_parser.imported_modules_capacity = parser->imported_modules_capacity;

    Node* import_ast = parser_parse(&import_parser);

    // Update parent parser's imported modules list (sub-parser may have added more)
    parser->imported_modules          = import_parser.imported_modules;
    parser->imported_modules_count    = import_parser.imported_modules_count;
    parser->imported_modules_capacity = import_parser.imported_modules_capacity;

    // Move any imported sources from sub-parser to parent
    for (int i = 0; i < import_parser.imported_sources_count; i++) {
        add_imported_source(parser, import_parser.imported_sources[i]);
    }
    // Clear sub-parser's list (don't free, ownership transferred)
    free(import_parser.imported_sources);
    import_parser.imported_sources       = NULL;
    import_parser.imported_sources_count = 0;

    // Clear the imported_modules in sub-parser to prevent double-free
    import_parser.imported_modules       = NULL;
    import_parser.imported_modules_count = 0;

    if (import_parser.had_error) {
        fprintf(stderr, "Failed to parse imported file: %s\n", path);
        node_free(import_ast);
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
    return 1;
}
