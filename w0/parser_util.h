#ifndef WHIST_PARSER_UTIL_H
#define WHIST_PARSER_UTIL_H

#include "lexer.h"
#include "parser.h"

// Maximum recursion depth to prevent stack overflow
#define MAX_PARSE_DEPTH 256

// Current recursion depth for expression parsing
extern int parse_depth;

void  advance(Parser* parser);
int   check(Parser* parser, TokenType type);
int   match(Parser* parser, TokenType type);
void  parse_error_at(Parser* parser, Token* token, const char* message);
void  parse_error(Parser* parser, const char* message);
void  consume(Parser* parser, TokenType type, const char* message);
void  synchronize(Parser* parser);
char* copy_token_string(Token* token);

#endif
