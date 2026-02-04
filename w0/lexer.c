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
    {"break", 5, TOK_BREAK},     {"by", 2, TOK_BY},
    {"const", 5, TOK_CONST},     {"continue", 8, TOK_CONTINUE},
    {"defer", 5, TOK_DEFER},     {"else", 4, TOK_ELSE},
    {"enum", 4, TOK_ENUM},       {"extern", 6, TOK_EXTERN},
    {"false", 5, TOK_FALSE},     {"for", 3, TOK_FOR},
    {"foreach", 7, TOK_FOREACH}, {"func", 4, TOK_FUNC},
    {"if", 2, TOK_IF},           {"import", 6, TOK_IMPORT},
    {"in", 2, TOK_IN},           {"null", 4, TOK_NULL},
    {"public", 6, TOK_PUBLIC},   {"private", 7, TOK_PRIVATE},
    {"return", 6, TOK_RETURN},   {"self", 4, TOK_SELF},
    {"struct", 6, TOK_STRUCT},   {"true", 4, TOK_TRUE},
    {"var", 3, TOK_VAR},         {"while", 5, TOK_WHILE},
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
        if (match(lexer, '='))
            return make_token(lexer, TOK_PLUS_EQ);
        return make_token(lexer, TOK_PLUS);
    case '-':
        if (match(lexer, '>'))
            return make_token(lexer, TOK_ARROW);
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

// Token type names indexed by TokenType enum value (must match lexer.h order)
static const char* token_names[] = {
    [TOK_EOF]         = "EOF",
    [TOK_IDENT]       = "IDENT",
    [TOK_INT]         = "INT",
    [TOK_FLOAT]       = "FLOAT",
    [TOK_STRING]      = "STRING",
    [TOK_CHAR]        = "CHAR",
    [TOK_IF]          = "IF",
    [TOK_ELSE]        = "ELSE",
    [TOK_WHILE]       = "WHILE",
    [TOK_FOR]         = "FOR",
    [TOK_FOREACH]     = "FOREACH",
    [TOK_RETURN]      = "RETURN",
    [TOK_BREAK]       = "BREAK",
    [TOK_CONTINUE]    = "CONTINUE",
    [TOK_STRUCT]      = "STRUCT",
    [TOK_ENUM]        = "ENUM",
    [TOK_FUNC]        = "FUNC",
    [TOK_VAR]         = "VAR",
    [TOK_CONST]       = "CONST",
    [TOK_BY]          = "BY",
    [TOK_TRUE]        = "TRUE",
    [TOK_FALSE]       = "FALSE",
    [TOK_IN]          = "IN",
    [TOK_NULL]        = "NULL",
    [TOK_SELF]        = "SELF",
    [TOK_DEFER]       = "DEFER",
    [TOK_PUBLIC]      = "PUBLIC",
    [TOK_PRIVATE]     = "PRIVATE",
    [TOK_EXTERN]      = "EXTERN",
    [TOK_IMPORT]      = "IMPORT",
    [TOK_PLUS]        = "PLUS",
    [TOK_MINUS]       = "MINUS",
    [TOK_STAR]        = "STAR",
    [TOK_SLASH]       = "SLASH",
    [TOK_PERCENT]     = "PERCENT",
    [TOK_AMP]         = "AMP",
    [TOK_PIPE]        = "PIPE",
    [TOK_CARET]       = "CARET",
    [TOK_TILDE]       = "TILDE",
    [TOK_BANG]        = "BANG",
    [TOK_EQ]          = "EQ",
    [TOK_LT]          = "LT",
    [TOK_GT]          = "GT",
    [TOK_PLUS_EQ]     = "PLUS_EQ",
    [TOK_MINUS_EQ]    = "MINUS_EQ",
    [TOK_STAR_EQ]     = "STAR_EQ",
    [TOK_SLASH_EQ]    = "SLASH_EQ",
    [TOK_PERCENT_EQ]  = "PERCENT_EQ",
    [TOK_AMP_EQ]      = "AMP_EQ",
    [TOK_PIPE_EQ]     = "PIPE_EQ",
    [TOK_CARET_EQ]    = "CARET_EQ",
    [TOK_EQ_EQ]       = "EQ_EQ",
    [TOK_BANG_EQ]     = "BANG_EQ",
    [TOK_LT_EQ]       = "LT_EQ",
    [TOK_GT_EQ]       = "GT_EQ",
    [TOK_AMP_AMP]     = "AMP_AMP",
    [TOK_PIPE_PIPE]   = "PIPE_PIPE",
    [TOK_LT_LT]       = "LT_LT",
    [TOK_GT_GT]       = "GT_GT",
    [TOK_LT_LT_EQ]    = "LT_LT_EQ",
    [TOK_GT_GT_EQ]    = "GT_GT_EQ",
    [TOK_ARROW]       = "ARROW",
    [TOK_LPAREN]      = "LPAREN",
    [TOK_RPAREN]      = "RPAREN",
    [TOK_LBRACE]      = "LBRACE",
    [TOK_RBRACE]      = "RBRACE",
    [TOK_LBRACKET]    = "LBRACKET",
    [TOK_RBRACKET]    = "RBRACKET",
    [TOK_SEMICOLON]   = "SEMICOLON",
    [TOK_COLON]       = "COLON",
    [TOK_COLON_COLON] = "COLON_COLON",
    [TOK_COMMA]       = "COMMA",
    [TOK_DOT]         = "DOT",
    [TOK_DOT_DOT]     = "DOT_DOT",
    [TOK_ERROR]       = "ERROR",
};

const char* token_type_name(TokenType type) {
    if (type >= 0 && type <= TOK_ERROR) {
        return token_names[type];
    }
    return "UNKNOWN";
}
