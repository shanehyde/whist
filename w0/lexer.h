#ifndef WHIST_LEXER_H
#define WHIST_LEXER_H

#include <stddef.h>

typedef enum {
    // End of file
    TOK_EOF = 0,

    // Literals
    TOK_IDENT,
    TOK_INT,
    TOK_FLOAT,
    TOK_STRING,
    TOK_CHAR,
    TOK_INTERP_STRING,

    // Keywords
    TOK_IF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_FOR,
    TOK_FOREACH,
    TOK_RETURN,
    TOK_BREAK,
    TOK_CONTINUE,
    TOK_STRUCT,
    TOK_ENUM,
    TOK_TRAIT,
    TOK_TYPE,
    TOK_IMPL,
    TOK_FUNC,
    TOK_VAR,
    TOK_CONST,
    TOK_BY,
    TOK_TRUE,
    TOK_FALSE,
    TOK_IN,
    TOK_NULL,
    TOK_NEW,
    TOK_SELF,
    TOK_DEFER,
    TOK_MATCH,
    TOK_PUBLIC,
    TOK_PRIVATE,
    TOK_EXTERN,
    TOK_IMPORT,
    TOK_AS,
    TOK_USE,

    // Operators
    TOK_PLUS,    // +
    TOK_MINUS,   // -
    TOK_STAR,    // *
    TOK_SLASH,   // /
    TOK_PERCENT, // %
    TOK_AMP,     // &
    TOK_PIPE,    // |
    TOK_CARET,   // ^
    TOK_TILDE,   // ~
    TOK_BANG,    // !
    TOK_EQ,      // =
    TOK_LT,      // <
    TOK_GT,      // >

    // Multi-char operators
    TOK_PLUS_EQ,    // +=
    TOK_MINUS_EQ,   // -=
    TOK_STAR_EQ,    // *=
    TOK_SLASH_EQ,   // /=
    TOK_PERCENT_EQ, // %=
    TOK_AMP_EQ,     // &=
    TOK_PIPE_EQ,    // |=
    TOK_CARET_EQ,   // ^=
    TOK_EQ_EQ,      // ==
    TOK_BANG_EQ,    // !=
    TOK_LT_EQ,      // <=
    TOK_GT_EQ,      // >=
    TOK_AMP_AMP,    // &&
    TOK_PIPE_PIPE,  // ||
    TOK_LT_LT,      // <<
    TOK_GT_GT,      // >>
    TOK_LT_LT_EQ,   // <<=
    TOK_GT_GT_EQ,   // >>=
    TOK_ARROW,      // ->
    TOK_FAT_ARROW,  // =>

    // Punctuation
    TOK_LPAREN,      // (
    TOK_RPAREN,      // )
    TOK_LBRACE,      // {
    TOK_RBRACE,      // }
    TOK_LBRACKET,    // [
    TOK_RBRACKET,    // ]
    TOK_SEMICOLON,   // ;
    TOK_COLON,       // :
    TOK_COLON_COLON, // ::
    TOK_COMMA,       // ,
    TOK_DOT,         // .
    TOK_DOT_DOT,     // ..
    TOK_ELLIPSIS,    // ...

    // Error
    TOK_ERROR
} TokenType;

typedef struct {
    TokenType   type;
    const char* start;
    size_t      length;
    int         line;
    int         column;
} Token;

typedef struct {
    const char* source;
    const char* current;
    const char* start;
    int         line;
    int         column;
    int         start_column;
    const char* error_message;
} Lexer;

void        lexer_init(Lexer* lexer, const char* source);
Token       lexer_next(Lexer* lexer);
const char* token_type_name(TokenType type);

#endif
