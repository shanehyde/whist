#ifndef MODULE_LOADER_H
#define MODULE_LOADER_H

#include <stddef.h>

#include "ast.h"

typedef struct Parser Parser;

typedef struct {
    const char* lib_path;

    // Imported source buffers (kept alive for AST string references)
    char** imported_sources;
    int    imported_sources_count;
    int    imported_sources_capacity;

    // Imported module names (to prevent duplicate imports)
    char** imported_modules;
    int    imported_modules_count;
    int    imported_modules_capacity;

    // Library modules directly imported by the root file (for checker visibility)
    char** direct_imports;
    int    direct_imports_count;
    int    direct_imports_capacity;

    // Library modules imported by the currently-parsing file (for func accessible_modules).
    // Saved/restored around sub-parser calls so each file sees only its own imports.
    char** file_imports;
    int    file_imports_count;
    int    file_imports_capacity;

    // Sibling modules (imported from same directory, not lib-path)
    char** sibling_modules;
    int    sibling_modules_count;
    int    sibling_modules_capacity;

    // 0 = root file, >0 = transitive import
    int import_depth;
} ModuleLoader;

// Lifecycle
void module_loader_init(ModuleLoader* loader, const char* lib_path);
void module_loader_free(ModuleLoader* loader);

// Module tracking
int  module_loader_is_imported(ModuleLoader* loader, const char* name, size_t len);
void module_loader_add_module(ModuleLoader* loader, const char* name, size_t len);
void module_loader_add_source(ModuleLoader* loader, char* source);
void module_loader_add_direct_import(ModuleLoader* loader, const char* name, size_t len);

// Path utilities
int  module_loader_file_exists(const char* path);
int  module_loader_is_relative_path(const char* path, size_t len);
int  module_loader_is_sibling(ModuleLoader* loader, const char* name, size_t len);
void module_loader_build_path(ModuleLoader* loader, const char* source_path, char* path,
                              size_t path_size, const char* module_name, size_t module_length,
                              int is_relative);

// Import orchestration
int module_loader_import(ModuleLoader* loader, Parser* parser, Node* program, Node* current_module,
                         const char* module_name, size_t module_length, int is_relative);

// Auto-import the prelude module (if lib_path is set and prelude.w exists)
void module_loader_import_prelude(ModuleLoader* loader, Parser* parser, Node* program,
                                  Node* main_module);

#endif // MODULE_LOADER_H
