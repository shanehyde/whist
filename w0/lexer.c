#include "lexer.h"

#include <ctype.h>
#include <string.h>

void lexer_init(Lexer* lexer, const char* source) {
    lexer->source        = source;
    lexer->current       = source;
    lexer->start         = source;
    lexer->line          = 1;
    lexer->column        = 1;
    lexer->start_column  = 1;
    lexer->error_message = NULL;
}

static int is_at_end(Lexer* lexer) {
    return *lexer->current == '\0';
}

static char advance(Lexer* lexer) {
    char c = *lexer->current++;
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    return c;
}

static char peek(Lexer* lexer) {
    return *lexer->current;
}

static char peek_next(Lexer* lexer) {
    if (is_at_end(lexer))
        return '\0';
    return lexer->current[1];
}

static int match(Lexer* lexer, char expected) {
    if (is_at_end(lexer))
        return 0;
    if (*lexer->current != expected)
        return 0;
    advance(lexer);
    return 1;
}

static Token make_token(Lexer* lexer, TokenType type) {
    Token token;
    token.type   = type;
    token.start  = lexer->start;
    token.length = lexer->current - lexer->start;
    token.line   = lexer->line;
    token.column = lexer->start_column;
    return token;
}

static Token error_token(Lexer* lexer, const char* message) {
    Token token;
    token.type   = TOK_ERROR;
    token.start  = message;
    token.length = strlen(message);
    token.line   = lexer->line;
    token.column = lexer->start_column;
    return token;
}

static void skip_whitespace(Lexer* lexer) {
    for (;;) {
        char c = peek(lexer);
        switch (c) {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            advance(lexer);
            break;
        case '/':
            if (peek_next(lexer) == '/') {
                // Line comment
                while (peek(lexer) != '\n' && !is_at_end(lexer)) {
                    advance(lexer);
                }
            } else if (peek_next(lexer) == '*') {
                // Block comment
                advance(lexer); // consume /
                advance(lexer); // consume *
                while (!is_at_end(lexer)) {
                    if (peek(lexer) == '*' && peek_next(lexer) == '/') {
                        advance(lexer); // consume *
                        advance(lexer); // consume /
                        break;
                    }
                    advance(lexer);
                }
                if (is_at_end(lexer)) {
                    lexer->error_message = "Unterminated block comment";
                    return;
                }
            } else {
                return;
            }
            break;
        default:
            return;
        }
    }
}

typedef struct {
    const char* keyword;
    size_t      length;
    TokenType   type;
} Keyword;

static const Keyword keywords[] = {
    {"break", 5, TOK_BREAK},     {"const", 5, TOK_CONST},   {"continue", 8, TOK_CONTINUE},
    {"defer", 5, TOK_DEFER},     {"else", 4, TOK_ELSE},     {"enum", 4, TOK_ENUM},
    {"extern", 6, TOK_EXTERN},   {"false", 5, TOK_FALSE},   {"for", 3, TOK_FOR},
    {"foreach", 7, TOK_FOREACH}, {"func", 4, TOK_FUNC},     {"if", 2, TOK_IF},
    {"in", 2, TOK_IN},           {"null", 4, TOK_NULL},     {"public", 6, TOK_PUBLIC},
    {"private", 7, TOK_PRIVATE}, {"return", 6, TOK_RETURN}, {"self", 4, TOK_SELF},
    {"struct", 6, TOK_STRUCT},   {"true", 4, TOK_TRUE},     {"var", 3, TOK_VAR},
    {"by", 2, TOK_BY},           {"while", 5, TOK_WHILE},
};

static const size_t keyword_count = sizeof(keywords) / sizeof(keywords[0]);

static TokenType identifier_type(Lexer* lexer) {
    size_t      length = lexer->current - lexer->start;
    const char* start  = lexer->start;

    for (size_t i = 0; i < keyword_count; i++) {
        if (keywords[i].length == length && memcmp(start, keywords[i].keyword, length) == 0) {
            return keywords[i].type;
        }
    }
    return TOK_IDENT;
}

static Token identifier(Lexer* lexer) {
    while (isalnum(peek(lexer)) || peek(lexer) == '_') {
        advance(lexer);
    }
    return make_token(lexer, identifier_type(lexer));
}

static Token number(Lexer* lexer) {
    TokenType type = TOK_INT;

    // Handle hex, octal, binary
    if (lexer->start[0] == '0' && !is_at_end(lexer)) {
        char next = peek(lexer);
        if (next == 'x' || next == 'X') {
            advance(lexer);
            if (!isxdigit(peek(lexer))) {
                return error_token(lexer, "Invalid hexadecimal literal");
            }
            while (isxdigit(peek(lexer)))
                advance(lexer);
            return make_token(lexer, TOK_INT);
        } else if (next == 'b' || next == 'B') {
            advance(lexer);
            if (peek(lexer) != '0' && peek(lexer) != '1') {
                return error_token(lexer, "Invalid binary literal");
            }
            while (peek(lexer) == '0' || peek(lexer) == '1')
                advance(lexer);
            return make_token(lexer, TOK_INT);
        } else if (next == 'o' || next == 'O') {
            advance(lexer);
            if (peek(lexer) < '0' || peek(lexer) > '7') {
                return error_token(lexer, "Invalid octal literal");
            }
            while (peek(lexer) >= '0' && peek(lexer) <= '7')
                advance(lexer);
            return make_token(lexer, TOK_INT);
        }
    }

    while (isdigit(peek(lexer)))
        advance(lexer);

    // Look for decimal part
    if (peek(lexer) == '.' && isdigit(peek_next(lexer))) {
        type = TOK_FLOAT;
        advance(lexer); // consume '.'
        while (isdigit(peek(lexer)))
            advance(lexer);
    }

    // Look for exponent
    if (peek(lexer) == 'e' || peek(lexer) == 'E') {
        type = TOK_FLOAT;
        advance(lexer);
        if (peek(lexer) == '+' || peek(lexer) == '-')
            advance(lexer);
        while (isdigit(peek(lexer)))
            advance(lexer);
    }

    return make_token(lexer, type);
}

static Token string(Lexer* lexer) {
    while (peek(lexer) != '"' && !is_at_end(lexer)) {
        if (peek(lexer) == '\\' && peek_next(lexer) != '\0') {
            advance(lexer); // skip backslash
        }
        advance(lexer);
    }

    if (is_at_end(lexer)) {
        return error_token(lexer, "Unterminated string");
    }

    advance(lexer); // closing quote
    return make_token(lexer, TOK_STRING);
}

static Token character(Lexer* lexer) {
    if (is_at_end(lexer)) {
        return error_token(lexer, "Unterminated character literal");
    }

    if (peek(lexer) == '\\') {
        advance(lexer); // skip backslash
        if (is_at_end(lexer)) {
            return error_token(lexer, "Unterminated character literal");
        }
        char escaped = peek(lexer);
        if (escaped == 'x') {
            // Hex escape: \xNN
            advance(lexer);
            for (int i = 0; i < 2; i++) {
                if (!isxdigit(peek(lexer))) {
                    return error_token(lexer, "Invalid hex escape in character literal");
                }
                advance(lexer);
            }
        } else if (escaped >= '0' && escaped <= '7') {
            // Octal escape: \NNN (up to 3 digits)
            for (int i = 0; i < 3 && peek(lexer) >= '0' && peek(lexer) <= '7'; i++) {
                advance(lexer);
            }
        } else {
            // Simple escape: \n, \t, \r, \\, \', \", \0, etc.
            advance(lexer);
        }
    } else {
        advance(lexer); // regular character
    }

    if (peek(lexer) != '\'') {
        return error_token(lexer, "Unterminated character literal");
    }
    advance(lexer); // closing quote
    return make_token(lexer, TOK_CHAR);
}

Token lexer_next(Lexer* lexer) {
    skip_whitespace(lexer);

    lexer->start        = lexer->current;
    lexer->start_column = lexer->column;

    if (lexer->error_message) {
        const char* msg      = lexer->error_message;
        lexer->error_message = NULL;
        return error_token(lexer, msg);
    }

    if (is_at_end(lexer)) {
        return make_token(lexer, TOK_EOF);
    }

    char c = advance(lexer);

    if (isalpha(c) || c == '_')
        return identifier(lexer);
    if (isdigit(c))
        return number(lexer);

    switch (c) {
    case '(':
        return make_token(lexer, TOK_LPAREN);
    case ')':
        return make_token(lexer, TOK_RPAREN);
    case '{':
        return make_token(lexer, TOK_LBRACE);
    case '}':
        return make_token(lexer, TOK_RBRACE);
    case '[':
        return make_token(lexer, TOK_LBRACKET);
    case ']':
        return make_token(lexer, TOK_RBRACKET);
    case ';':
        return make_token(lexer, TOK_SEMICOLON);
    case ':':
        return make_token(lexer, match(lexer, ':') ? TOK_COLON_COLON : TOK_COLON);
    case ',':
        return make_token(lexer, TOK_COMMA);
    case '.':
        return make_token(lexer, match(lexer, '.') ? TOK_DOT_DOT : TOK_DOT);
    case '~':
        return make_token(lexer, TOK_TILDE);
    case '^':
        return make_token(lexer, match(lexer, '=') ? TOK_CARET_EQ : TOK_CARET);
    case '+':
        if (match(lexer, '+'))
            return make_token(lexer, TOK_PLUS_PLUS);
        if (match(lexer, '='))
            return make_token(lexer, TOK_PLUS_EQ);
        return make_token(lexer, TOK_PLUS);
    case '-':
        if (match(lexer, '-'))
            return make_token(lexer, TOK_MINUS_MINUS);
        if (match(lexer, '='))
            return make_token(lexer, TOK_MINUS_EQ);
        return make_token(lexer, TOK_MINUS);
    case '*':
        return make_token(lexer, match(lexer, '=') ? TOK_STAR_EQ : TOK_STAR);
    case '/':
        return make_token(lexer, match(lexer, '=') ? TOK_SLASH_EQ : TOK_SLASH);
    case '%':
        return make_token(lexer, match(lexer, '=') ? TOK_PERCENT_EQ : TOK_PERCENT);
    case '&':
        if (match(lexer, '&'))
            return make_token(lexer, TOK_AMP_AMP);
        if (match(lexer, '='))
            return make_token(lexer, TOK_AMP_EQ);
        return make_token(lexer, TOK_AMP);
    case '|':
        if (match(lexer, '|'))
            return make_token(lexer, TOK_PIPE_PIPE);
        if (match(lexer, '='))
            return make_token(lexer, TOK_PIPE_EQ);
        return make_token(lexer, TOK_PIPE);
    case '!':
        return make_token(lexer, match(lexer, '=') ? TOK_BANG_EQ : TOK_BANG);
    case '=':
        return make_token(lexer, match(lexer, '=') ? TOK_EQ_EQ : TOK_EQ);
    case '<':
        if (match(lexer, '<'))
            return make_token(lexer, match(lexer, '=') ? TOK_LT_LT_EQ : TOK_LT_LT);
        if (match(lexer, '='))
            return make_token(lexer, TOK_LT_EQ);
        return make_token(lexer, TOK_LT);
    case '>':
        if (match(lexer, '>'))
            return make_token(lexer, match(lexer, '=') ? TOK_GT_GT_EQ : TOK_GT_GT);
        if (match(lexer, '='))
            return make_token(lexer, TOK_GT_EQ);
        return make_token(lexer, TOK_GT);
    case '"':
        return string(lexer);
    case '\'':
        return character(lexer);
    }

    return error_token(lexer, "Unexpected character");
}

const char* token_type_name(TokenType type) {
    switch (type) {
    case TOK_EOF:
        return "EOF";
    case TOK_IDENT:
        return "IDENT";
    case TOK_INT:
        return "INT";
    case TOK_FLOAT:
        return "FLOAT";
    case TOK_STRING:
        return "STRING";
    case TOK_CHAR:
        return "CHAR";
    case TOK_IF:
        return "IF";
    case TOK_ELSE:
        return "ELSE";
    case TOK_WHILE:
        return "WHILE";
    case TOK_FOR:
        return "FOR";
    case TOK_FOREACH:
        return "FOREACH";
    case TOK_RETURN:
        return "RETURN";
    case TOK_BREAK:
        return "BREAK";
    case TOK_CONTINUE:
        return "CONTINUE";
    case TOK_STRUCT:
        return "STRUCT";
    case TOK_ENUM:
        return "ENUM";
    case TOK_FUNC:
        return "FUNC";
    case TOK_IN:
        return "IN";
    case TOK_VAR:
        return "VAR";
    case TOK_CONST:
        return "CONST";
    case TOK_TRUE:
        return "TRUE";
    case TOK_FALSE:
        return "FALSE";
    case TOK_NULL:
        return "NULL";
    case TOK_SELF:
        return "SELF";
    case TOK_DEFER:
        return "DEFER";
    case TOK_PUBLIC:
        return "PUBLIC";
    case TOK_PRIVATE:
        return "PRIVATE";
    case TOK_PLUS:
        return "PLUS";
    case TOK_MINUS:
        return "MINUS";
    case TOK_STAR:
        return "STAR";
    case TOK_SLASH:
        return "SLASH";
    case TOK_PERCENT:
        return "PERCENT";
    case TOK_AMP:
        return "AMP";
    case TOK_PIPE:
        return "PIPE";
    case TOK_CARET:
        return "CARET";
    case TOK_TILDE:
        return "TILDE";
    case TOK_BANG:
        return "BANG";
    case TOK_EQ:
        return "EQ";
    case TOK_LT:
        return "LT";
    case TOK_GT:
        return "GT";
    case TOK_PLUS_EQ:
        return "PLUS_EQ";
    case TOK_MINUS_EQ:
        return "MINUS_EQ";
    case TOK_STAR_EQ:
        return "STAR_EQ";
    case TOK_SLASH_EQ:
        return "SLASH_EQ";
    case TOK_PERCENT_EQ:
        return "PERCENT_EQ";
    case TOK_AMP_EQ:
        return "AMP_EQ";
    case TOK_PIPE_EQ:
        return "PIPE_EQ";
    case TOK_CARET_EQ:
        return "CARET_EQ";
    case TOK_EQ_EQ:
        return "EQ_EQ";
    case TOK_BANG_EQ:
        return "BANG_EQ";
    case TOK_LT_EQ:
        return "LT_EQ";
    case TOK_GT_EQ:
        return "GT_EQ";
    case TOK_AMP_AMP:
        return "AMP_AMP";
    case TOK_PIPE_PIPE:
        return "PIPE_PIPE";
    case TOK_LT_LT:
        return "LT_LT";
    case TOK_GT_GT:
        return "GT_GT";
    case TOK_LT_LT_EQ:
        return "LT_LT_EQ";
    case TOK_GT_GT_EQ:
        return "GT_GT_EQ";
    case TOK_PLUS_PLUS:
        return "PLUS_PLUS";
    case TOK_MINUS_MINUS:
        return "MINUS_MINUS";
    case TOK_ARROW:
        return "ARROW";
    case TOK_LPAREN:
        return "LPAREN";
    case TOK_RPAREN:
        return "RPAREN";
    case TOK_LBRACE:
        return "LBRACE";
    case TOK_RBRACE:
        return "RBRACE";
    case TOK_LBRACKET:
        return "LBRACKET";
    case TOK_RBRACKET:
        return "RBRACKET";
    case TOK_SEMICOLON:
        return "SEMICOLON";
    case TOK_COLON:
        return "COLON";
    case TOK_COLON_COLON:
        return "COLON_COLON";
    case TOK_COMMA:
        return "COMMA";
    case TOK_DOT:
        return "DOT";
    case TOK_DOT_DOT:
        return "DOT_DOT";
    case TOK_ERROR:
        return "ERROR";
    case TOK_EXTERN:
        return "EXTERN";
    }
    return "UNKNOWN";
}
