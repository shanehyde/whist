#include "module_loader.h"

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "parser.h"
#include "util.h"
#include "vec.h"

void module_loader_init(ModuleLoader* loader, const char* lib_path) {
    loader->lib_path = lib_path;

    loader->imported_sources          = NULL;
    loader->imported_sources_count    = 0;
    loader->imported_sources_capacity = 0;

    loader->imported_modules          = NULL;
    loader->imported_modules_count    = 0;
    loader->imported_modules_capacity = 0;

    loader->direct_imports          = NULL;
    loader->direct_imports_count    = 0;
    loader->direct_imports_capacity = 0;

    loader->file_imports          = NULL;
    loader->file_imports_count    = 0;
    loader->file_imports_capacity = 0;

    loader->sibling_modules          = NULL;
    loader->sibling_modules_count    = 0;
    loader->sibling_modules_capacity = 0;

    loader->import_depth = 0;
}

void module_loader_free(ModuleLoader* loader) {
    for (int i = 0; i < loader->imported_sources_count; i++) {
        free(loader->imported_sources[i]);
    }
    free(loader->imported_sources);

    for (int i = 0; i < loader->imported_modules_count; i++) {
        free(loader->imported_modules[i]);
    }
    free(loader->imported_modules);

    for (int i = 0; i < loader->direct_imports_count; i++) {
        free(loader->direct_imports[i]);
    }
    free(loader->direct_imports);

    for (int i = 0; i < loader->file_imports_count; i++) {
        free(loader->file_imports[i]);
    }
    free(loader->file_imports);

    for (int i = 0; i < loader->sibling_modules_count; i++) {
        free(loader->sibling_modules[i]);
    }
    free(loader->sibling_modules);
}

int module_loader_is_imported(ModuleLoader* loader, const char* name, size_t len) {
    for (int i = 0; i < loader->imported_modules_count; i++) {
        if (strlen(loader->imported_modules[i]) == len &&
            strncmp(loader->imported_modules[i], name, len) == 0) {
            return 1;
        }
    }
    return 0;
}

void module_loader_add_module(ModuleLoader* loader, const char* name, size_t len) {
    VEC_GROW(loader->imported_modules, loader->imported_modules_count,
             loader->imported_modules_capacity);

    char* name_copy = xmalloc(len + 1);
    memcpy(name_copy, name, len);
    name_copy[len]                                             = '\0';
    loader->imported_modules[loader->imported_modules_count++] = name_copy;
}

void module_loader_add_source(ModuleLoader* loader, char* source) {
    VEC_GROW(loader->imported_sources, loader->imported_sources_count,
             loader->imported_sources_capacity);
    loader->imported_sources[loader->imported_sources_count++] = source;
}

void module_loader_add_direct_import(ModuleLoader* loader, const char* name, size_t len) {
    VEC_GROW(loader->direct_imports, loader->direct_imports_count, loader->direct_imports_capacity);

    char* name_copy = xmalloc(len + 1);
    memcpy(name_copy, name, len);
    name_copy[len]                                         = '\0';
    loader->direct_imports[loader->direct_imports_count++] = name_copy;
}

static void add_file_import(ModuleLoader* loader, const char* name, size_t len) {
    VEC_GROW(loader->file_imports, loader->file_imports_count, loader->file_imports_capacity);

    char* name_copy = xmalloc(len + 1);
    memcpy(name_copy, name, len);
    name_copy[len]                                     = '\0';
    loader->file_imports[loader->file_imports_count++] = name_copy;
}

static void add_sibling_module(ModuleLoader* loader, const char* name, size_t len) {
    VEC_GROW(loader->sibling_modules, loader->sibling_modules_count,
             loader->sibling_modules_capacity);

    char* name_copy = xmalloc(len + 1);
    memcpy(name_copy, name, len);
    name_copy[len]                                           = '\0';
    loader->sibling_modules[loader->sibling_modules_count++] = name_copy;
}

int module_loader_is_sibling(ModuleLoader* loader, const char* name, size_t len) {
    for (int i = 0; i < loader->sibling_modules_count; i++) {
        if (strlen(loader->sibling_modules[i]) == len &&
            strncmp(loader->sibling_modules[i], name, len) == 0) {
            return 1;
        }
    }
    return 0;
}

int module_loader_file_exists(const char* path) {
    FILE* f = fopen(path, "r");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

int module_loader_is_relative_path(const char* path, size_t len) {
    if (len >= 2 && path[0] == '.' && path[1] == '/') {
        return 1;
    }
    if (len >= 3 && path[0] == '.' && path[1] == '.' && path[2] == '/') {
        return 1;
    }
    return 0;
}

void module_loader_build_path(ModuleLoader* loader, const char* source_path, char* path,
                              size_t path_size, const char* module_name, size_t module_length,
                              int is_relative) {
    if (is_relative) {
        if (source_path) {
            char* path_copy = xstrdup(source_path);
            char* dir       = dirname(path_copy);
            snprintf(path, path_size, "%s/%.*s", dir, (int)module_length, module_name);
            free(path_copy);
            return;
        }
        snprintf(path, path_size, "%.*s", (int)module_length, module_name);
        return;
    }

    // Standard library import: try lib/ directories
    if (loader->lib_path) {
        snprintf(path, path_size, "%s/%.*s.w", loader->lib_path, (int)module_length, module_name);
        if (module_loader_file_exists(path)) {
            return;
        }
    }

    // Sibling import: try source directory
    if (source_path) {
        char* path_copy = xstrdup(source_path);
        char* dir       = dirname(path_copy);
        snprintf(path, path_size, "%s/%.*s.w", dir, (int)module_length, module_name);
        free(path_copy);
        if (module_loader_file_exists(path)) {
            return;
        }
    }

    if (source_path) {
        char* path_copy = xstrdup(source_path);
        char* dir       = dirname(path_copy);
        snprintf(path, path_size, "%s/lib/%.*s.w", dir, (int)module_length, module_name);
        free(path_copy);
        if (module_loader_file_exists(path)) {
            return;
        }
    }
    // Fallback: use lib/ relative to current working directory
    snprintf(path, path_size, "lib/%.*s.w", (int)module_length, module_name);
}

int module_loader_import(ModuleLoader* loader, Parser* parser, Node* program, Node* current_module,
                         const char* module_name, size_t module_length, int is_relative) {
    // Build path to import file
    char path[1024];
    module_loader_build_path(loader, parser->source_path, path, sizeof(path), module_name,
                             module_length, is_relative);

    // For relative imports, use resolved path as module key for duplicate detection
    const char* module_key        = is_relative ? path : module_name;
    size_t      module_key_length = is_relative ? strlen(path) : module_length;

    // Check if already imported
    if (module_loader_is_imported(loader, module_key, module_key_length)) {
        // Still track for visibility
        if (!is_relative) {
            add_file_import(loader, module_name, module_length);
            if (loader->import_depth == 0) {
                module_loader_add_direct_import(loader, module_name, module_length);
            }
        }
        return 1;
    }

    // Mark as imported before parsing (prevents cycles)
    module_loader_add_module(loader, module_key, module_key_length);

    // Read the imported file
    char* source = read_file(path);
    if (!source) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Could not read import file: %s", path);
        parse_error(parser, error_msg);
        return 0;
    }

    // Store source buffer to keep it alive
    module_loader_add_source(loader, source);

    // Track file-level and root-level imports
    if (!is_relative) {
        if (loader->import_depth == 0) {
            module_loader_add_direct_import(loader, module_name, module_length);
        }
    }

    // Parse the imported file with the shared loader.
    // Save/restore the entire file_imports array so each file sees only its own
    // imports. Saving just the count is insufficient because sub-parses overwrite
    // array entries at the same indices.
    char** saved_file_imports          = loader->file_imports;
    int    saved_file_imports_count    = loader->file_imports_count;
    int    saved_file_imports_capacity = loader->file_imports_capacity;

    loader->file_imports          = NULL;
    loader->file_imports_count    = 0;
    loader->file_imports_capacity = 0;

    Parser import_parser;
    parser_init_with_loader(&import_parser, source, path, loader);

    loader->import_depth++;
    Node* import_ast = parser_parse(&import_parser);
    loader->import_depth--;

    // Free sub-file's file_imports and restore parent's
    for (int i = 0; i < loader->file_imports_count; i++) {
        free(loader->file_imports[i]);
    }
    free(loader->file_imports);

    loader->file_imports          = saved_file_imports;
    loader->file_imports_count    = saved_file_imports_count;
    loader->file_imports_capacity = saved_file_imports_capacity;

    // Add this module to the current file's imports
    if (!is_relative) {
        add_file_import(loader, module_name, module_length);
    }

    if (import_parser.had_error) {
        fprintf(stderr, "Failed to parse imported file: %s\n", path);
        node_free(import_ast);
        return 0;
    }

    // Handle the imported AST based on import type
    if (import_ast && import_ast->type == NODE_PROGRAM) {
        if (is_relative) {
            // Relative import: merge main module's declarations into current_module
            // Keep library modules as separate modules in the program
            for (int m = 0; m < import_ast->as.program.modules.count; m++) {
                Node* imported_module = import_ast->as.program.modules.nodes[m];
                if (imported_module && imported_module->type == NODE_MODULE) {
                    if (strcmp(imported_module->as.module.name, "main") == 0) {
                        for (int i = 0; i < imported_module->as.module.decls.count; i++) {
                            Node* decl = imported_module->as.module.decls.nodes[i];
                            nodelist_push(&current_module->as.module.decls, decl);
                        }
                        imported_module->as.module.decls.count = 0;
                    } else {
                        nodelist_push(&program->as.program.modules, imported_module);
                    }
                }
            }
            import_ast->as.program.modules.count = 0;
        } else {
            // Named import: rename main module to library name.
            // Detect sibling imports (not found in lib-path).
            int is_sibling_import = 0;
            if (loader->lib_path) {
                char lib_check[1024];
                snprintf(lib_check, sizeof(lib_check), "%s/%.*s.w", loader->lib_path,
                         (int)module_length, module_name);
                is_sibling_import = !module_loader_file_exists(lib_check);
            } else {
                is_sibling_import = 1;
            }

            if (is_sibling_import) {
                add_sibling_module(loader, module_name, module_length);
            }

            for (int m = 0; m < import_ast->as.program.modules.count; m++) {
                Node* imported_module = import_ast->as.program.modules.nodes[m];
                if (imported_module && imported_module->type == NODE_MODULE) {
                    if (strcmp(imported_module->as.module.name, "main") == 0) {
                        free(imported_module->as.module.name);
                        imported_module->as.module.name = xmalloc(module_length + 1);
                        memcpy(imported_module->as.module.name, module_name, module_length);
                        imported_module->as.module.name[module_length] = '\0';
                        imported_module->as.module.name_length         = (int)module_length;
                        imported_module->as.module.is_sibling          = is_sibling_import;
                    }
                    nodelist_push(&program->as.program.modules, imported_module);
                }
            }
            import_ast->as.program.modules.count = 0;
        }
    }

    node_free(import_ast);
    return 1;
}

void module_loader_import_prelude(ModuleLoader* loader, Parser* parser, Node* program,
                                  Node* main_module) {
    if (!loader || !loader->lib_path) {
        return;
    }

    char path[1024];
    snprintf(path, sizeof(path), "%s/prelude.w", loader->lib_path);
    if (!module_loader_file_exists(path)) {
        return;
    }

    module_loader_import(loader, parser, program, main_module, "prelude", 7, 0);
}
