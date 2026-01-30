#ifndef WHIST_PARSER_UTIL_H
#define WHIST_PARSER_UTIL_H

#include "lexer.h"
#include "parser.h"

void  advance(Parser* parser);
int   check(Parser* parser, TokenType type);
int   match(Parser* parser, TokenType type);
void  error_at(Parser* parser, Token* token, const char* message);
void  error(Parser* parser, const char* message);
void  consume(Parser* parser, TokenType type, const char* message);
void  synchronize(Parser* parser);
char* copy_token_string(Token* token);

#endif
