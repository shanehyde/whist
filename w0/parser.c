#include "parser.h"

#include <stdlib.h>
#include <string.h>

#include "parse_enum_decl.h"
#include "parse_extern_decls.h"
#include "parse_func_decl.h"
#include "parse_import.h"
#include "parse_struct_decl.h"
#include "parse_var_decl.h"
#include "parser_util.h"

// Current recursion depth for expression parsing (declared in parser_util.h)
int parse_depth = 0;

static Node* parse_declaration(Parser* parser) {
    int is_public      = match_token(parser, TOK_PUBLIC);
    int has_visibility = is_public;

    if (!is_public) {
        has_visibility = match_token(parser, TOK_PRIVATE);
        is_public      = !has_visibility; // default to public if no modifier
    }
    if (match_token(parser, TOK_EXTERN)) {
        // Extern defaults to private when no visibility modifier
        return parse_extern_decls(parser, has_visibility ? is_public : 0);
    }
    if (match_token(parser, TOK_FUNC)) {
        return parse_func_decl(parser, is_public);
    }
    if (match_token(parser, TOK_STRUCT)) {
        return parse_struct_decl(parser, is_public);
    }
    if (match_token(parser, TOK_ENUM)) {
        return parse_enum_decl(parser, is_public);
    }
    if (match_token(parser, TOK_VAR)) {
        return parse_var_decl(parser, 0, is_public);
    }
    if (match_token(parser, TOK_CONST)) {
        return parse_var_decl(parser, 1, is_public);
    }

    parse_error(parser, "Expected declaration");
    return NULL;
}

void parser_init(Parser* parser, const char* source) {
    parser_init_with_path(parser, source, NULL);
}

void parser_init_with_path(Parser* parser, const char* source, const char* source_path) {
    lexer_init(&parser->lexer, source);
    parser->had_error    = 0;
    parser->panic_mode   = 0;
    parser->error_msg[0] = '\0';
    parse_depth          = 0; // Reset recursion depth

    parser->source_path = source_path;

    // Initialize imported sources tracking
    parser->imported_sources          = NULL;
    parser->imported_sources_count    = 0;
    parser->imported_sources_capacity = 0;

    // Initialize imported modules tracking
    parser->imported_modules          = NULL;
    parser->imported_modules_count    = 0;
    parser->imported_modules_capacity = 0;

    // Initialize direct imports tracking (library modules directly imported by this file)
    parser->direct_imports          = NULL;
    parser->direct_imports_count    = 0;
    parser->direct_imports_capacity = 0;

    advance_token(parser); // Prime the parser
}

void parser_free(Parser* parser) {
    // Free all imported source buffers
    for (int i = 0; i < parser->imported_sources_count; i++) {
        free(parser->imported_sources[i]);
    }
    free(parser->imported_sources);

    // Free all imported module names
    for (int i = 0; i < parser->imported_modules_count; i++) {
        free(parser->imported_modules[i]);
    }
    free(parser->imported_modules);

    // Free all direct import names
    for (int i = 0; i < parser->direct_imports_count; i++) {
        free(parser->direct_imports[i]);
    }
    free(parser->direct_imports);
}

Node* parser_parse(Parser* parser) {
    Node* program = node_new(NODE_PROGRAM, 1, 1);
    if (!program) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    nodelist_init(&program->as.program.modules);

    // Create main module for the entry file
    Node* main_module = node_new(NODE_MODULE, 1, 1);
    if (!main_module) {
        parse_error(parser, "Out of memory");
        node_free(program);
        return NULL;
    }
    main_module->as.module.name        = strdup("main");
    main_module->as.module.name_length = 4;
    nodelist_init(&main_module->as.module.decls);
    nodelist_push(&program->as.program.modules, main_module);

    while (!check_token(parser, TOK_EOF)) {
        // Handle import statements
        if (match_token(parser, TOK_IMPORT)) {
            if (!parse_import_stmt(parser, program, main_module)) {
                // Import failed, but continue parsing
                if (parser->panic_mode)
                    synchronize(parser);
            }
            continue;
        }

        Node* decl = parse_declaration(parser);
        if (decl) {
            nodelist_push(&main_module->as.module.decls, decl);
        }
        if (parser->panic_mode)
            synchronize(parser);
    }

    return program;
}
