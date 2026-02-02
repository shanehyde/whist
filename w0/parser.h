#ifndef WHIST_PARSER_H
#define WHIST_PARSER_H

#include "ast.h"
#include "lexer.h"

typedef struct {
    Lexer lexer;
    Token current;
    Token previous;
    int   had_error;
    int   panic_mode;
    char  error_msg[256];

    // Source file path for resolving imports
    const char* source_path;

    // Imported source buffers (kept alive for AST string references)
    char** imported_sources;
    int    imported_sources_count;
    int    imported_sources_capacity;

    // Imported module names (to prevent duplicate imports)
    char** imported_modules;
    int    imported_modules_count;
    int    imported_modules_capacity;
} Parser;

// Precedence levels for binary operators
typedef enum {
    PREC_NONE       = 0,
    PREC_OR         = 3,  // ||
    PREC_AND        = 4,  // &&
    PREC_BIT_OR     = 5,  // |
    PREC_BIT_XOR    = 6,  // ^
    PREC_BIT_AND    = 7,  // &
    PREC_EQUALITY   = 8,  // == !=
    PREC_COMPARISON = 9,  // < > <= >=
    PREC_SHIFT      = 10, // << >>
    PREC_TERM       = 11, // + -
    PREC_FACTOR     = 12, // * / %
} Precedence;

void  parser_init(Parser* parser, const char* source);
void  parser_init_with_path(Parser* parser, const char* source, const char* source_path);
void  parser_free(Parser* parser);
Node* parser_parse(Parser* parser);

#endif
