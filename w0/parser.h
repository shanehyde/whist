#ifndef WHIST_PARSER_H
#define WHIST_PARSER_H

#include "ast.h"
#include "lexer.h"
#include "module_loader.h"

typedef struct Parser {
    Lexer lexer;
    Token current;
    Token previous;
    int   had_error;
    int   panic_mode;
    char  error_msg[256];

    // Source file path for resolving imports
    const char* source_path;

    // Shared module loader (not owned by parser)
    ModuleLoader* loader;

    // Current recursion depth for expression parsing
    int parse_depth;
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

// Maximum recursion depth to prevent stack overflow
#define MAX_PARSE_DEPTH 256

// Parser lifecycle
void  parser_init(Parser* parser, const char* source);
void  parser_init_with_loader(Parser* parser, const char* source, const char* source_path,
                              ModuleLoader* loader);
void  parser_free(Parser* parser);
Node* parser_parse(Parser* parser);

// Parser utilities (used by other modules)
void  advance_token(Parser* parser);
int   check_token(Parser* parser, TokenType type);
int   match_token(Parser* parser, TokenType type);
void  parse_error_at(Parser* parser, Token* token, const char* message);
void  parse_error(Parser* parser, const char* message);
void  consume_token(Parser* parser, TokenType type, const char* message);
void  synchronize(Parser* parser);
char* copy_token_string(Token* token);

#endif
