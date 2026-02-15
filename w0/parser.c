#include "alloc.h"
#include "parser_internal.h"

Node* parser_parse(Parser* parser) {
    Node* program = node_new(NODE_PROGRAM, 1, 1);
    nodelist_init(&program->as.program.modules);

    // Create main module for the entry file
    Node* main_module                  = node_new(NODE_MODULE, 1, 1);
    main_module->as.module.name        = xstrdup("main");
    main_module->as.module.name_length = 4;
    nodelist_init(&main_module->as.module.decls);

    // Auto-import prelude before adding main module, so prelude types
    // are defined first in the checker (main module can then shadow them)
    if (parser->loader) {
        module_loader_import_prelude(parser->loader, parser, program, main_module);
    }

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

        // Handle use statements
        if (match_token(parser, TOK_USE)) {
            Node* use_node = parse_use_stmt(parser);
            if (use_node) {
                nodelist_push(&main_module->as.module.decls, use_node);
            }
            if (parser->panic_mode)
                synchronize(parser);
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
