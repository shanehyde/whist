// Whist Lexer — Self-hosted port of w0/lexer.c

enum TokenType {
    // End of file
    EOF,

    // Literals
    Ident,
    Int,
    Float,
    String,
    Char,
    InterpString,

    // Keywords
    If,
    Else,
    While,
    For,
    Foreach,
    Return,
    Break,
    Continue,
    Struct,
    Enum,
    Trait,
    Type,
    Impl,
    Func,
    Var,
    Const,
    Is,
    By,
    True,
    False,
    In,
    Null,
    New,
    SelfKw,
    Defer,
    Match,
    Public,
    Private,
    Extern,
    Import,
    Include,
    As,
    Use,
    Test,

    // Operators
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Amp,
    Pipe,
    Caret,
    Tilde,
    Bang,
    Eq,
    Lt,
    Gt,

    // Multi-char operators
    PlusEq,
    MinusEq,
    StarEq,
    SlashEq,
    PercentEq,
    AmpEq,
    PipeEq,
    CaretEq,
    EqEq,
    BangEq,
    LtEq,
    GtEq,
    AmpAmp,
    PipePipe,
    LtLt,
    GtGt,
    LtLtEq,
    GtGtEq,
    Arrow,
    FatArrow,

    // Punctuation
    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    Semicolon,
    Colon,
    ColonColon,
    Comma,
    Dot,
    DotDot,
    Ellipsis,
    Question,

    // Error
    Error,
}

struct Token {
    kind: TokenType,
    value: string,
    line: i64,
    column: i64,
}

struct Lexer {
    source: string,
    pos: i64,
    start: i64,
    line: i64,
    column: i64,
    start_column: i64,
    error_message: string,
}

// --- Character classification helpers ---

func is_alpha(ch: char) -> bool {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

func is_digit(ch: char) -> bool {
    return ch >= '0' && ch <= '9';
}

func is_alnum(ch: char) -> bool {
    return is_alpha(ch) || is_digit(ch);
}

func is_hex_digit(ch: char) -> bool {
    return is_digit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

func is_octal_digit(ch: char) -> bool {
    return ch >= '0' && ch <= '7';
}

// --- Lexer core functions ---

func lexer_init(source: string) -> Lexer {
    var lex = new Lexer{
        source: source,
        pos: 0,
        start: 0,
        line: 1,
        column: 1,
        start_column: 1,
        error_message: "",
    };
    return lex;
}

func lexer_is_at_end(lex: Lexer) -> bool {
    return lex.pos >= lex.source.length();
}

func lexer_advance(lex: Lexer) -> char {
    var ch = lex.source[lex.pos];
    lex.pos += 1;
    if (ch == '\n') {
        lex.line += 1;
        lex.column = 1;
    } else {
        lex.column += 1;
    }
    return ch;
}

func lexer_peek(lex: Lexer) -> char {
    if (lexer_is_at_end(lex)) {
        return '\0';
    }
    return lex.source[lex.pos];
}

func lexer_peek_next(lex: Lexer) -> char {
    if (lex.pos + 1 >= lex.source.length()) {
        return '\0';
    }
    return lex.source[lex.pos + 1];
}

func lexer_match(lex: Lexer, expected: char) -> bool {
    if (lexer_is_at_end(lex)) {
        return false;
    }
    if (lex.source[lex.pos] != expected) {
        return false;
    }
    lexer_advance(lex);
    return true;
}

func lexer_make_token(lex: Lexer, kind: TokenType) -> Token {
    var tok = new Token{
        kind: kind,
        value: lex.source[lex.start:lex.pos],
        line: lex.line,
        column: lex.start_column,
    };
    return tok;
}

func lexer_error_token(lex: Lexer, message: string) -> Token {
    var tok = new Token{
        kind: TokenType::Error,
        value: message,
        line: lex.line,
        column: lex.start_column,
    };
    return tok;
}

// --- Whitespace and comment skipping ---

func lexer_skip_whitespace(lex: Lexer) {
    while (true) {
        if (lexer_is_at_end(lex)) {
            return;
        }
        var ch = lexer_peek(lex);
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            lexer_advance(lex);
        } else if (ch == '/') {
            if (lexer_peek_next(lex) == '/') {
                // Line comment
                while (lexer_peek(lex) != '\n' && !lexer_is_at_end(lex)) {
                    lexer_advance(lex);
                }
            } else if (lexer_peek_next(lex) == '*') {
                // Block comment
                lexer_advance(lex); // consume /
                lexer_advance(lex); // consume *
                while (!lexer_is_at_end(lex)) {
                    if (lexer_peek(lex) == '*' && lexer_peek_next(lex) == '/') {
                        lexer_advance(lex); // consume *
                        lexer_advance(lex); // consume /
                        break;
                    }
                    lexer_advance(lex);
                }
                if (lexer_is_at_end(lex)) {
                    lex.error_message = "Unterminated block comment";
                    return;
                }
            } else {
                return;
            }
        } else {
            return;
        }
    }
}

// --- Keyword matching ---

func identifier_type(text: string) -> TokenType {
    if (text == "as") { return TokenType::As; }
    if (text == "break") { return TokenType::Break; }
    if (text == "by") { return TokenType::By; }
    if (text == "const") { return TokenType::Const; }
    if (text == "continue") { return TokenType::Continue; }
    if (text == "defer") { return TokenType::Defer; }
    if (text == "else") { return TokenType::Else; }
    if (text == "enum") { return TokenType::Enum; }
    if (text == "extern") { return TokenType::Extern; }
    if (text == "false") { return TokenType::False; }
    if (text == "for") { return TokenType::For; }
    if (text == "foreach") { return TokenType::Foreach; }
    if (text == "func") { return TokenType::Func; }
    if (text == "if") { return TokenType::If; }
    if (text == "impl") { return TokenType::Impl; }
    if (text == "import") { return TokenType::Import; }
    if (text == "in") { return TokenType::In; }
    if (text == "include") { return TokenType::Include; }
    if (text == "is") { return TokenType::Is; }
    if (text == "match") { return TokenType::Match; }
    if (text == "new") { return TokenType::New; }
    if (text == "null") { return TokenType::Null; }
    if (text == "public") { return TokenType::Public; }
    if (text == "private") { return TokenType::Private; }
    if (text == "return") { return TokenType::Return; }
    if (text == "self") { return TokenType::SelfKw; }
    if (text == "struct") { return TokenType::Struct; }
    if (text == "test") { return TokenType::Test; }
    if (text == "trait") { return TokenType::Trait; }
    if (text == "true") { return TokenType::True; }
    if (text == "type") { return TokenType::Type; }
    if (text == "use") { return TokenType::Use; }
    if (text == "var") { return TokenType::Var; }
    if (text == "while") { return TokenType::While; }
    return TokenType::Ident;
}

// --- Identifier scanning ---

func lex_identifier(lex: Lexer) -> Token {
    while (!lexer_is_at_end(lex) && is_alnum(lexer_peek(lex))) {
        lexer_advance(lex);
    }
    var text = lex.source[lex.start:lex.pos];
    return lexer_make_token(lex, identifier_type(text));
}

// --- Number scanning ---

func lex_number(lex: Lexer) -> Token {
    var kind = TokenType::Int;

    // Handle hex, octal, binary prefixes
    if (lex.source[lex.start] == '0' && !lexer_is_at_end(lex)) {
        var next = lexer_peek(lex);
        if (next == 'x' || next == 'X') {
            lexer_advance(lex);
            if (lexer_is_at_end(lex) || !is_hex_digit(lexer_peek(lex))) {
                return lexer_error_token(lex, "Invalid hexadecimal literal");
            }
            while (!lexer_is_at_end(lex) && is_hex_digit(lexer_peek(lex))) {
                lexer_advance(lex);
            }
            return lexer_make_token(lex, TokenType::Int);
        } else if (next == 'b' || next == 'B') {
            lexer_advance(lex);
            if (lexer_is_at_end(lex) || (lexer_peek(lex) != '0' && lexer_peek(lex) != '1')) {
                return lexer_error_token(lex, "Invalid binary literal");
            }
            while (!lexer_is_at_end(lex) && (lexer_peek(lex) == '0' || lexer_peek(lex) == '1')) {
                lexer_advance(lex);
            }
            return lexer_make_token(lex, TokenType::Int);
        } else if (next == 'o' || next == 'O') {
            lexer_advance(lex);
            if (lexer_is_at_end(lex) || !is_octal_digit(lexer_peek(lex))) {
                return lexer_error_token(lex, "Invalid octal literal");
            }
            while (!lexer_is_at_end(lex) && is_octal_digit(lexer_peek(lex))) {
                lexer_advance(lex);
            }
            return lexer_make_token(lex, TokenType::Int);
        }
    }

    // Decimal digits
    while (!lexer_is_at_end(lex) && is_digit(lexer_peek(lex))) {
        lexer_advance(lex);
    }

    // Decimal point
    if (!lexer_is_at_end(lex) && lexer_peek(lex) == '.' &&
        lex.pos + 1 < lex.source.length() && is_digit(lex.source[lex.pos + 1])) {
        kind = TokenType::Float;
        lexer_advance(lex); // consume '.'
        while (!lexer_is_at_end(lex) && is_digit(lexer_peek(lex))) {
            lexer_advance(lex);
        }
    }

    // Exponent
    if (!lexer_is_at_end(lex) && (lexer_peek(lex) == 'e' || lexer_peek(lex) == 'E')) {
        kind = TokenType::Float;
        lexer_advance(lex);
        if (!lexer_is_at_end(lex) && (lexer_peek(lex) == '+' || lexer_peek(lex) == '-')) {
            lexer_advance(lex);
        }
        while (!lexer_is_at_end(lex) && is_digit(lexer_peek(lex))) {
            lexer_advance(lex);
        }
    }

    return lexer_make_token(lex, kind);
}

// --- Escape sequence handling ---

// Skip escape sequence after the backslash.
// Returns "" on success, error message on failure.
func skip_escape(lex: Lexer) -> string {
    if (lexer_is_at_end(lex)) {
        return "Unterminated escape sequence";
    }
    var escaped = lexer_peek(lex);
    if (escaped == 'x') {
        // Hex escape: \xNN
        lexer_advance(lex);
        var i: i64 = 0;
        while (i < 2) {
            if (lexer_is_at_end(lex) || !is_hex_digit(lexer_peek(lex))) {
                return "Invalid hex escape in string literal";
            }
            lexer_advance(lex);
            i += 1;
        }
        return "";
    } else if (is_octal_digit(escaped)) {
        // Octal escape: up to 3 digits
        var i: i64 = 0;
        while (i < 3 && !lexer_is_at_end(lex) && is_octal_digit(lexer_peek(lex))) {
            lexer_advance(lex);
            i += 1;
        }
        return "";
    } else {
        // Simple escape: \n, \t, \r, \\, \', \", \e, \0, etc.
        lexer_advance(lex);
        return "";
    }
}

// --- String scanning ---

func lex_string(lex: Lexer) -> Token {
    while (!lexer_is_at_end(lex) && lexer_peek(lex) != '"') {
        if (lexer_peek(lex) == '\\' && lexer_peek_next(lex) != '\0') {
            lexer_advance(lex); // skip backslash
            var err = skip_escape(lex);
            if (err != "") {
                return lexer_error_token(lex, err);
            }
            continue;
        }
        lexer_advance(lex);
    }

    if (lexer_is_at_end(lex)) {
        return lexer_error_token(lex, "Unterminated string");
    }

    lexer_advance(lex); // closing quote
    return lexer_make_token(lex, TokenType::String);
}

func lex_triple_string(lex: Lexer) -> Token {
    // Opening """ already consumed. Scan until closing """.
    while (!lexer_is_at_end(lex)) {
        if (lexer_peek(lex) == '"' && lexer_peek_next(lex) == '"' &&
            lex.pos + 2 < lex.source.length() && lex.source[lex.pos + 2] == '"') {
            lexer_advance(lex); // first "
            lexer_advance(lex); // second "
            lexer_advance(lex); // third "
            return lexer_make_token(lex, TokenType::String);
        }
        if (lexer_peek(lex) == '\\' && lexer_peek_next(lex) != '\0') {
            lexer_advance(lex); // skip backslash
            var err = skip_escape(lex);
            if (err != "") {
                return lexer_error_token(lex, err);
            }
            continue;
        }
        lexer_advance(lex);
    }
    return lexer_error_token(lex, "Unterminated triple-quoted string");
}

func lex_interp_string(lex: Lexer) -> Token {
    // Scan from after $" to closing ", tracking brace depth for {expr} regions
    var brace_depth: i64 = 0;
    while (!lexer_is_at_end(lex)) {
        var ch = lexer_peek(lex);
        if (brace_depth == 0) {
            if (ch == '"') {
                lexer_advance(lex); // closing quote
                return lexer_make_token(lex, TokenType::InterpString);
            }
            if (ch == '{') {
                if (lexer_peek_next(lex) == '{') {
                    lexer_advance(lex); // skip first {
                    lexer_advance(lex); // skip second {
                    continue;
                }
                brace_depth += 1;
                lexer_advance(lex);
                continue;
            }
            if (ch == '}' && lexer_peek_next(lex) == '}') {
                lexer_advance(lex); // skip first }
                lexer_advance(lex); // skip second }
                continue;
            }
            if (ch == '\\' && lexer_peek_next(lex) != '\0') {
                lexer_advance(lex); // skip backslash
                var err = skip_escape(lex);
                if (err != "") {
                    return lexer_error_token(lex, err);
                }
                continue;
            }
            lexer_advance(lex);
        } else {
            // Inside {expr} region
            if (ch == '{') {
                brace_depth += 1;
            } else if (ch == '}') {
                brace_depth -= 1;
            } else if (ch == '"') {
                // String literal inside expression - skip it
                lexer_advance(lex); // opening quote
                while (!lexer_is_at_end(lex) && lexer_peek(lex) != '"') {
                    if (lexer_peek(lex) == '\\' && lexer_peek_next(lex) != '\0') {
                        lexer_advance(lex);
                    }
                    lexer_advance(lex);
                }
                if (!lexer_is_at_end(lex)) {
                    lexer_advance(lex); // closing quote
                }
                continue;
            }
            lexer_advance(lex);
        }
    }
    return lexer_error_token(lex, "Unterminated interpolated string");
}

// --- Character literal scanning ---

func lex_character(lex: Lexer) -> Token {
    if (lexer_is_at_end(lex)) {
        return lexer_error_token(lex, "Unterminated character literal");
    }

    if (lexer_peek(lex) == '\\') {
        lexer_advance(lex); // skip backslash
        if (lexer_is_at_end(lex)) {
            return lexer_error_token(lex, "Unterminated character literal");
        }
        var escaped = lexer_peek(lex);
        if (escaped == 'x') {
            // Hex escape: \xNN
            lexer_advance(lex);
            var i: i64 = 0;
            while (i < 2) {
                if (lexer_is_at_end(lex) || !is_hex_digit(lexer_peek(lex))) {
                    return lexer_error_token(lex, "Invalid hex escape in character literal");
                }
                lexer_advance(lex);
                i += 1;
            }
        } else if (is_octal_digit(escaped)) {
            // Octal escape: \NNN (up to 3 digits)
            var i: i64 = 0;
            while (i < 3 && !lexer_is_at_end(lex) && is_octal_digit(lexer_peek(lex))) {
                lexer_advance(lex);
                i += 1;
            }
        } else {
            // Simple escape
            lexer_advance(lex);
        }
    } else {
        lexer_advance(lex); // regular character
    }

    if (lexer_is_at_end(lex) || lexer_peek(lex) != '\'') {
        return lexer_error_token(lex, "Unterminated character literal");
    }
    lexer_advance(lex); // closing quote
    return lexer_make_token(lex, TokenType::Char);
}

// --- Main lexer function ---

func lexer_next(lex: Lexer) -> Token {
    lexer_skip_whitespace(lex);

    lex.start = lex.pos;
    lex.start_column = lex.column;

    // Check for deferred error from block comment
    if (lex.error_message != "") {
        var msg = lex.error_message;
        lex.error_message = "";
        return lexer_error_token(lex, msg);
    }

    if (lexer_is_at_end(lex)) {
        return lexer_make_token(lex, TokenType::EOF);
    }

    var ch = lexer_advance(lex);

    if (is_alpha(ch)) {
        return lex_identifier(lex);
    }
    if (is_digit(ch)) {
        return lex_number(lex);
    }

    // Operators and punctuation
    if (ch == '(') { return lexer_make_token(lex, TokenType::LParen); }
    if (ch == ')') { return lexer_make_token(lex, TokenType::RParen); }
    if (ch == '{') { return lexer_make_token(lex, TokenType::LBrace); }
    if (ch == '}') { return lexer_make_token(lex, TokenType::RBrace); }
    if (ch == '[') { return lexer_make_token(lex, TokenType::LBracket); }
    if (ch == ']') { return lexer_make_token(lex, TokenType::RBracket); }
    if (ch == ';') { return lexer_make_token(lex, TokenType::Semicolon); }
    if (ch == ',') { return lexer_make_token(lex, TokenType::Comma); }
    if (ch == '~') { return lexer_make_token(lex, TokenType::Tilde); }
    if (ch == '?') { return lexer_make_token(lex, TokenType::Question); }

    if (ch == ':') {
        if (lexer_match(lex, ':')) {
            return lexer_make_token(lex, TokenType::ColonColon);
        }
        return lexer_make_token(lex, TokenType::Colon);
    }

    if (ch == '.') {
        if (lexer_match(lex, '.')) {
            if (lexer_match(lex, '.')) {
                return lexer_make_token(lex, TokenType::Ellipsis);
            }
            return lexer_make_token(lex, TokenType::DotDot);
        }
        return lexer_make_token(lex, TokenType::Dot);
    }

    if (ch == '+') {
        if (lexer_match(lex, '=')) {
            return lexer_make_token(lex, TokenType::PlusEq);
        }
        return lexer_make_token(lex, TokenType::Plus);
    }

    if (ch == '-') {
        if (lexer_match(lex, '>')) {
            return lexer_make_token(lex, TokenType::Arrow);
        }
        if (lexer_match(lex, '=')) {
            return lexer_make_token(lex, TokenType::MinusEq);
        }
        return lexer_make_token(lex, TokenType::Minus);
    }

    if (ch == '*') {
        if (lexer_match(lex, '=')) {
            return lexer_make_token(lex, TokenType::StarEq);
        }
        return lexer_make_token(lex, TokenType::Star);
    }

    if (ch == '/') {
        if (lexer_match(lex, '=')) {
            return lexer_make_token(lex, TokenType::SlashEq);
        }
        return lexer_make_token(lex, TokenType::Slash);
    }

    if (ch == '%') {
        if (lexer_match(lex, '=')) {
            return lexer_make_token(lex, TokenType::PercentEq);
        }
        return lexer_make_token(lex, TokenType::Percent);
    }

    if (ch == '^') {
        if (lexer_match(lex, '=')) {
            return lexer_make_token(lex, TokenType::CaretEq);
        }
        return lexer_make_token(lex, TokenType::Caret);
    }

    if (ch == '&') {
        if (lexer_match(lex, '&')) {
            return lexer_make_token(lex, TokenType::AmpAmp);
        }
        if (lexer_match(lex, '=')) {
            return lexer_make_token(lex, TokenType::AmpEq);
        }
        return lexer_make_token(lex, TokenType::Amp);
    }

    if (ch == '|') {
        if (lexer_match(lex, '|')) {
            return lexer_make_token(lex, TokenType::PipePipe);
        }
        if (lexer_match(lex, '=')) {
            return lexer_make_token(lex, TokenType::PipeEq);
        }
        return lexer_make_token(lex, TokenType::Pipe);
    }

    if (ch == '!') {
        if (lexer_match(lex, '=')) {
            return lexer_make_token(lex, TokenType::BangEq);
        }
        return lexer_make_token(lex, TokenType::Bang);
    }

    if (ch == '=') {
        if (lexer_match(lex, '>')) {
            return lexer_make_token(lex, TokenType::FatArrow);
        }
        if (lexer_match(lex, '=')) {
            return lexer_make_token(lex, TokenType::EqEq);
        }
        return lexer_make_token(lex, TokenType::Eq);
    }

    if (ch == '<') {
        if (lexer_match(lex, '<')) {
            if (lexer_match(lex, '=')) {
                return lexer_make_token(lex, TokenType::LtLtEq);
            }
            return lexer_make_token(lex, TokenType::LtLt);
        }
        if (lexer_match(lex, '=')) {
            return lexer_make_token(lex, TokenType::LtEq);
        }
        return lexer_make_token(lex, TokenType::Lt);
    }

    if (ch == '>') {
        if (lexer_match(lex, '>')) {
            if (lexer_match(lex, '=')) {
                return lexer_make_token(lex, TokenType::GtGtEq);
            }
            return lexer_make_token(lex, TokenType::GtGt);
        }
        if (lexer_match(lex, '=')) {
            return lexer_make_token(lex, TokenType::GtEq);
        }
        return lexer_make_token(lex, TokenType::Gt);
    }

    if (ch == '"') {
        if (lexer_peek(lex) == '"' && lexer_peek_next(lex) == '"') {
            lexer_advance(lex); // second "
            lexer_advance(lex); // third "
            return lex_triple_string(lex);
        }
        return lex_string(lex);
    }

    if (ch == '\'') {
        return lex_character(lex);
    }

    if (ch == '$') {
        if (lexer_match(lex, '"')) {
            return lex_interp_string(lex);
        }
        return lexer_error_token(lex, "Unexpected character '$'");
    }

    return lexer_error_token(lex, "Unexpected character");
}

// --- Token type name ---

func token_type_name(tt: TokenType) -> string {
    match (tt) {
        EOF => return "EOF";
        Ident => return "IDENT";
        Int => return "INT";
        Float => return "FLOAT";
        String => return "STRING";
        Char => return "CHAR";
        InterpString => return "INTERP_STRING";
        If => return "IF";
        Else => return "ELSE";
        While => return "WHILE";
        For => return "FOR";
        Foreach => return "FOREACH";
        Return => return "RETURN";
        Break => return "BREAK";
        Continue => return "CONTINUE";
        Struct => return "STRUCT";
        Enum => return "ENUM";
        Trait => return "TRAIT";
        Type => return "TYPE";
        Impl => return "IMPL";
        Func => return "FUNC";
        Var => return "VAR";
        Const => return "CONST";
        Is => return "IS";
        By => return "BY";
        True => return "TRUE";
        False => return "FALSE";
        In => return "IN";
        Null => return "NULL";
        New => return "NEW";
        SelfKw => return "SELF";
        Defer => return "DEFER";
        Match => return "MATCH";
        Public => return "PUBLIC";
        Private => return "PRIVATE";
        Extern => return "EXTERN";
        Import => return "IMPORT";
        Include => return "INCLUDE";
        As => return "AS";
        Use => return "USE";
        Test => return "TEST";
        Plus => return "PLUS";
        Minus => return "MINUS";
        Star => return "STAR";
        Slash => return "SLASH";
        Percent => return "PERCENT";
        Amp => return "AMP";
        Pipe => return "PIPE";
        Caret => return "CARET";
        Tilde => return "TILDE";
        Bang => return "BANG";
        Eq => return "EQ";
        Lt => return "LT";
        Gt => return "GT";
        PlusEq => return "PLUS_EQ";
        MinusEq => return "MINUS_EQ";
        StarEq => return "STAR_EQ";
        SlashEq => return "SLASH_EQ";
        PercentEq => return "PERCENT_EQ";
        AmpEq => return "AMP_EQ";
        PipeEq => return "PIPE_EQ";
        CaretEq => return "CARET_EQ";
        EqEq => return "EQ_EQ";
        BangEq => return "BANG_EQ";
        LtEq => return "LT_EQ";
        GtEq => return "GT_EQ";
        AmpAmp => return "AMP_AMP";
        PipePipe => return "PIPE_PIPE";
        LtLt => return "LT_LT";
        GtGt => return "GT_GT";
        LtLtEq => return "LT_LT_EQ";
        GtGtEq => return "GT_GT_EQ";
        Arrow => return "ARROW";
        FatArrow => return "FAT_ARROW";
        LParen => return "LPAREN";
        RParen => return "RPAREN";
        LBrace => return "LBRACE";
        RBrace => return "RBRACE";
        LBracket => return "LBRACKET";
        RBracket => return "RBRACKET";
        Semicolon => return "SEMICOLON";
        Colon => return "COLON";
        ColonColon => return "COLON_COLON";
        Comma => return "COMMA";
        Dot => return "DOT";
        DotDot => return "DOT_DOT";
        Ellipsis => return "ELLIPSIS";
        Question => return "QUESTION";
        Error => return "ERROR";
    }
}
