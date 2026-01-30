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
Node* parser_parse(Parser* parser);

#endif
