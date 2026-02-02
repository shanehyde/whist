#include "parser.h"

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
    int is_public = match_token(parser, TOK_PUBLIC);

    if (!is_public) {
        is_public = match_token(parser, TOK_PRIVATE) == 0;
    }
    if (match_token(parser, TOK_EXTERN)) {
        return parse_extern_decls(parser);
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
    lexer_init(&parser->lexer, source);
    parser->had_error    = 0;
    parser->panic_mode   = 0;
    parser->error_msg[0] = '\0';
    parse_depth          = 0; // Reset recursion depth
    advance_token(parser);    // Prime the parser
}

Node* parser_parse(Parser* parser) {
    Node* program = node_new(NODE_PROGRAM, 1, 1);
    if (!program) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    nodelist_init(&program->as.program.decls);

    while (!check_token(parser, TOK_EOF)) {
        // Handle import statements
        if (match_token(parser, TOK_IMPORT)) {
            if (!parse_import_stmt(parser, &program->as.program.decls)) {
                // Import failed, but continue parsing
                if (parser->panic_mode)
                    synchronize(parser);
            }
            continue;
        }

        Node* decl = parse_declaration(parser);
        if (decl) {
            nodelist_push(&program->as.program.decls, decl);
        }
        if (parser->panic_mode)
            synchronize(parser);
    }

    return program;
}
