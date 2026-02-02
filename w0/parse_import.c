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

// Add a direct import (library module directly imported by current file)
static void add_direct_import(Parser* parser, const char* module_name, size_t length) {
    // Grow array if needed
    if (parser->direct_imports_count >= parser->direct_imports_capacity) {
        int new_capacity =
            parser->direct_imports_capacity == 0 ? 8 : parser->direct_imports_capacity * 2;
        char** new_array = realloc(parser->direct_imports, new_capacity * sizeof(char*));
        if (!new_array)
            return;
        parser->direct_imports          = new_array;
        parser->direct_imports_capacity = new_capacity;
    }

    // Copy the module name
    char* name_copy = malloc(length + 1);
    if (!name_copy)
        return;
    memcpy(name_copy, module_name, length);
    name_copy[length]                                      = '\0';
    parser->direct_imports[parser->direct_imports_count++] = name_copy;
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

// Check if path is a relative import (starts with ./ or ../)
static int is_relative_path(const char* path, size_t length) {
    if (length >= 2 && path[0] == '.' && path[1] == '/') {
        return 1;
    }
    if (length >= 3 && path[0] == '.' && path[1] == '.' && path[2] == '/') {
        return 1;
    }
    return 0;
}

// Build path to imported module
// For relative paths (./ or ../), resolves relative to source file
// For module names, tries source-relative lib/ first, then falls back to cwd-relative lib/
static void build_import_path(Parser* parser, char* path, size_t path_size, const char* module_name,
                              size_t module_length, int is_relative) {
    if (is_relative) {
        // Relative import: resolve relative to source file's directory
        if (parser->source_path) {
            char* path_copy = strdup(parser->source_path);
            if (path_copy) {
                char* dir = dirname(path_copy);
                snprintf(path, path_size, "%s/%.*s", dir, (int)module_length, module_name);
                free(path_copy);
                return;
            }
        }
        // Fallback if no source path: use path as-is relative to cwd
        snprintf(path, path_size, "%.*s", (int)module_length, module_name);
        return;
    }

    // Standard library import: try lib/ directories
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

int parse_import_stmt(Parser* parser, Node* program, Node* current_module) {
    // Expect identifier or string after 'import'
    Token       import_token = parser->current;
    const char* module_name;
    size_t      module_length;
    int         is_relative = 0;

    if (parser->current.type == TOK_STRING) {
        // String import: "./path/to/file.w" or "../path/to/file.w"
        advance_token(parser);
        // Extract path without quotes
        module_name   = import_token.start + 1;
        module_length = import_token.length - 2;
        is_relative   = is_relative_path(module_name, module_length);
        if (!is_relative) {
            parse_error(parser, "String imports must be relative paths (start with ./ or ../)");
            return 0;
        }
    } else if (parser->current.type == TOK_IDENT) {
        // Identifier import: std
        advance_token(parser);
        module_name   = import_token.start;
        module_length = import_token.length;
    } else {
        parse_error(parser, "Expected module name or path after 'import'");
        return 0;
    }

    // Expect semicolon
    consume_token(parser, TOK_SEMICOLON, "Expected ';' after import statement");

    // Build path to import file first (needed for duplicate detection with relative paths)
    char path[1024];
    build_import_path(parser, path, sizeof(path), module_name, module_length, is_relative);

    // For relative imports, use the resolved path as the module key for duplicate detection
    const char* module_key        = is_relative ? path : module_name;
    size_t      module_key_length = is_relative ? strlen(path) : module_length;

    // Check if already imported
    if (is_module_imported(parser, module_key, module_key_length)) {
        // Module already imported - but still track as direct import if it's a library import
        // This allows this file to access the module's symbols even if a dependency imported it
        // first
        if (!is_relative) {
            add_direct_import(parser, module_name, module_length);
        }
        return 1; // Already imported, nothing more to do
    }

    // Mark as imported before parsing (prevents cycles)
    add_imported_module(parser, module_key, module_key_length);

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

    // Free sub-parser's direct_imports (not needed - each file tracks its own imports via
    // source_file)
    for (int i = 0; i < import_parser.direct_imports_count; i++) {
        free(import_parser.direct_imports[i]);
    }
    free(import_parser.direct_imports);
    import_parser.direct_imports       = NULL;
    import_parser.direct_imports_count = 0;

    if (import_parser.had_error) {
        fprintf(stderr, "Failed to parse imported file: %s\n", path);
        node_free(import_ast);
        return 0;
    }

    // For library imports, track as direct import
    if (!is_relative) {
        add_direct_import(parser, module_name, module_length);
    }

    // Handle the imported AST based on import type
    if (import_ast && import_ast->type == NODE_PROGRAM) {
        if (is_relative) {
            // Relative import: merge only the "main" module's declarations into current_module
            // Keep library modules (non-main) as separate modules in the program
            for (int m = 0; m < import_ast->as.program.modules.count; m++) {
                Node* imported_module = import_ast->as.program.modules.nodes[m];
                if (imported_module && imported_module->type == NODE_MODULE) {
                    if (strcmp(imported_module->as.module.name, "main") == 0) {
                        // Merge main module's declarations into current module
                        for (int i = 0; i < imported_module->as.module.decls.count; i++) {
                            Node* decl = imported_module->as.module.decls.nodes[i];
                            nodelist_push(&current_module->as.module.decls, decl);
                        }
                        // Clear to prevent double-free
                        imported_module->as.module.decls.count = 0;
                    } else {
                        // Keep library modules as separate modules
                        nodelist_push(&program->as.program.modules, imported_module);
                    }
                }
            }
            // Clear to prevent double-free (only main was freed, others moved)
            import_ast->as.program.modules.count = 0;
        } else {
            // Library import: move modules from imported AST to program
            // The first module (main) of the library becomes a module named after the import
            for (int m = 0; m < import_ast->as.program.modules.count; m++) {
                Node* imported_module = import_ast->as.program.modules.nodes[m];
                if (imported_module && imported_module->type == NODE_MODULE) {
                    // Rename "main" module to the library name
                    if (strcmp(imported_module->as.module.name, "main") == 0) {
                        free(imported_module->as.module.name);
                        imported_module->as.module.name = malloc(module_length + 1);
                        if (imported_module->as.module.name) {
                            memcpy(imported_module->as.module.name, module_name, module_length);
                            imported_module->as.module.name[module_length] = '\0';
                            imported_module->as.module.name_length         = (int)module_length;
                        }
                    }
                    nodelist_push(&program->as.program.modules, imported_module);
                }
            }
            // Clear to prevent double-free
            import_ast->as.program.modules.count = 0;
        }
    }

    node_free(import_ast);
    return 1;
}
