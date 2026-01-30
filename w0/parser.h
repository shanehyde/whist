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

void  parser_init(Parser* parser, const char* source);
Node* parser_parse(Parser* parser);

#endif
