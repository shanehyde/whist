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

func (char) is_alpha() -> bool {
    return (self >= 'a' && self <= 'z') || (self >= 'A' && self <= 'Z') || self == '_';
}

func (char) is_digit() -> bool {
    return self >= '0' && self <= '9';
}

func (char) is_alnum() -> bool {
    return self.is_alpha() || self.is_digit();
}

func (char) is_hex_digit() -> bool {
    return self.is_digit() || (self >= 'a' && self <= 'f') || (self >= 'A' && self <= 'F');
}

func (char) is_octal_digit() -> bool {
    return self >= '0' && self <= '7';
}

// --- Lexer core functions ---
impl Lexer {
    func init(source: string) {
        self.source = source;
        self.pos = 0;
        self.start = 0;
        self.line = 1;
        self.column = 1;
        self.start_column = 1;
        self.error_message = "";
    }
}

func (Lexer) is_at_end() -> bool {
    return self.pos >= self.source.length();
}

func (Lexer) advance() -> char {
    var ch = self.source[self.pos];
    self.pos += 1;
    match (ch) {
        '\n' => {
            self.line += 1;
            self.column = 1;
        }
        _ => self.column += 1;
    }
    return ch;
}

func (Lexer) peek() -> char {
    if (self.is_at_end()) {
        return '\0';
    }
    return self.source[self.pos];
}

func (Lexer) peek_next() -> char {
    if (self.pos + 1 >= self.source.length()) {
        return '\0';
    }
    return self.source[self.pos + 1];
}

func lexer_match(lex: Lexer, expected: char) -> bool {
    if (lex.is_at_end()) {
        return false;
    }
    if (lex.source[lex.pos] != expected) {
        return false;
    }
    lex.advance();
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

func (Lexer) skip_whitespace() -> Option<bool> {
    while (true) {
        if (self.is_at_end()) {
            return Option::None;
        }
        var ch = self.peek();
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            self.advance();
        } else if (ch == '/') {
            if (self.peek_next() == '/') {
                // Line comment
                while (self.peek() != '\n' && !self.is_at_end()) {
                    self.advance();
                }
            } else if (self.peek_next() == '*') {
                // Block comment
                self.advance(); // consume /
                self.advance(); // consume *
                while (!self.is_at_end()) {
                    if (self.peek() == '*' && self.peek_next() == '/') {
                        self.advance(); // consume *
                        self.advance(); // consume /
                        break;
                    }
                    self.advance();
                }
                if (self.is_at_end()) {
                    self.error_message = "Unterminated block comment";
                    return Option::None;
                }
            } else {
                return Option::Some(true);
            }
        } else {
            return Option::Some(true);
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

func  (Lexer) identifier() -> Token {
    while (!self.is_at_end() && self.peek().is_alnum()) {
        self.advance();
    }
    var text = self.source[self.start:self.pos];
    return lexer_make_token(self,identifier_type(text));
}

// --- Number scanning ---

func lex_number(lex: Lexer) -> Token {
    var kind = TokenType::Int;

    // Handle hex, octal, binary prefixes
    if (lex.source[lex.start] == '0' && !lex.is_at_end()) {
        var next = lex.peek();
        if (next == 'x' || next == 'X') {
            lex.advance();
            if (lex.is_at_end() || !lex.peek().is_hex_digit()) {
                return lexer_error_token(lex, "Invalid hexadecimal literal");
            }
            while (!lex.is_at_end() && lex.peek().is_hex_digit()) {
                lex.advance();
            }
            return lexer_make_token(lex, TokenType::Int);
        } else if (next == 'b' || next == 'B') {
            lex.advance();
            if (lex.is_at_end() || (lex.peek() != '0' && lex.peek() != '1')) {
                return lexer_error_token(lex, "Invalid binary literal");
            }
            while (!lex.is_at_end() && (lex.peek() == '0' || lex.peek() == '1')) {
                lex.advance();
            }
            return lexer_make_token(lex, TokenType::Int);
        } else if (next == 'o' || next == 'O') {
            lex.advance();
            if (lex.is_at_end() || !lex.peek().is_octal_digit()) {
                return lexer_error_token(lex, "Invalid octal literal");
            }
            while (!lex.is_at_end() && lex.peek().is_octal_digit()) {
                lex.advance();
            }
            return lexer_make_token(lex, TokenType::Int);
        }
    }

    // Decimal digits
    while (!lex.is_at_end() && lex.peek().is_digit()) {
        lex.advance();
    }

    // Decimal point
    if (!lex.is_at_end() && lex.peek() == '.' &&
        lex.pos + 1 < lex.source.length() && lex.source[lex.pos + 1].is_digit()) {
        kind = TokenType::Float;
        lex.advance(); // consume '.'
        while (!lex.is_at_end() && lex.peek().is_digit()) {
            lex.advance();
        }
    }

    // Exponent
    if (!lex.is_at_end() && (lex.peek() == 'e' || lex.peek() == 'E')) {
        kind = TokenType::Float;
        lex.advance();
        if (!lex.is_at_end() && (lex.peek() == '+' || lex.peek() == '-')) {
            lex.advance();
        }
        while (!lex.is_at_end() && lex.peek().is_digit()) {
            lex.advance();
        }
    }

    return lexer_make_token(lex, kind);
}

// --- Escape sequence handling ---

// Skip escape sequence after the backslash.
// Returns "" on success, error message on failure.
func skip_escape(lex: Lexer) -> string {
    if (lex.is_at_end()) {
        return "Unterminated escape sequence";
    }
    var escaped = lex.peek();
    if (escaped == 'x') {
        // Hex escape: \xNN
        lex.advance();
        var i: i64 = 0;
        while (i < 2) {
            if (lex.is_at_end() || !lex.peek().is_hex_digit()) {
                return "Invalid hex escape in string literal";
            }
            lex.advance();
            i += 1;
        }
        return "";
    } else if (escaped.is_octal_digit()) {
        // Octal escape: up to 3 digits
        var i: i64 = 0;
        while (i < 3 && !lex.is_at_end() && lex.peek().is_octal_digit()) {
            lex.advance();
            i += 1;
        }
        return "";
    } else {
        // Simple escape: \n, \t, \r, \\, \', \", \e, \0, etc.
        lex.advance();
        return "";
    }
}

// --- String scanning ---

func lex_string(lex: Lexer) -> Token {
    while (!lex.is_at_end() && lex.peek() != '"') {
        if (lex.peek() == '\\' && lex.peek_next() != '\0') {
            lex.advance(); // skip backslash
            var err = skip_escape(lex);
            if (err != "") {
                return lexer_error_token(lex, err);
            }
            continue;
        }
        lex.advance();
    }

    if (lex.is_at_end()) {
        return lexer_error_token(lex, "Unterminated string");
    }

    lex.advance(); // closing quote
    return lexer_make_token(lex, TokenType::String);
}

func lex_triple_string(lex: Lexer) -> Token {
    // Opening """ already consumed. Scan until closing """.
    while (!lex.is_at_end()) {
        if (lex.peek() == '"' && lex.peek_next() == '"' &&
            lex.pos + 2 < lex.source.length() && lex.source[lex.pos + 2] == '"') {
            lex.advance(); // first "
            lex.advance(); // second "
            lex.advance(); // third "
            return lexer_make_token(lex, TokenType::String);
        }
        if (lex.peek() == '\\' && lex.peek_next() != '\0') {
            lex.advance(); // skip backslash
            var err = skip_escape(lex);
            if (err != "") {
                return lexer_error_token(lex, err);
            }
            continue;
        }
        lex.advance();
    }
    return lexer_error_token(lex, "Unterminated triple-quoted string");
}

func lex_interp_string(lex: Lexer) -> Token {
    // Scan from after $" to closing ", tracking brace depth for {expr} regions
    var brace_depth: i64 = 0;
    while (!lex.is_at_end()) {
        var ch = lex.peek();
        if (brace_depth == 0) {
            if (ch == '"') {
                lex.advance(); // closing quote
                return lexer_make_token(lex, TokenType::InterpString);
            }
            if (ch == '{') {
                if (lex.peek_next() == '{') {
                    lex.advance(); // skip first {
                    lex.advance(); // skip second {
                    continue;
                }
                brace_depth += 1;
                lex.advance();
                continue;
            }
            if (ch == '}' && lex.peek_next() == '}') {
                lex.advance(); // skip first }
                lex.advance(); // skip second }
                continue;
            }
            if (ch == '\\' && lex.peek_next() != '\0') {
                lex.advance(); // skip backslash
                var err = skip_escape(lex);
                if (err != "") {
                    return lexer_error_token(lex, err);
                }
                continue;
            }
            lex.advance();
        } else {
            // Inside {expr} region
            if (ch == '{') {
                brace_depth += 1;
            } else if (ch == '}') {
                brace_depth -= 1;
            } else if (ch == '"') {
                // String literal inside expression - skip it
                lex.advance(); // opening quote
                while (!lex.is_at_end() && lex.peek() != '"') {
                    if (lex.peek() == '\\' && lex.peek_next() != '\0') {
                        lex.advance();
                    }
                    lex.advance();
                }
                if (!lex.is_at_end()) {
                    lex.advance(); // closing quote
                }
                continue;
            }
            lex.advance();
        }
    }
    return lexer_error_token(lex, "Unterminated interpolated string");
}

// --- Character literal scanning ---

func lex_character(lex: Lexer) -> Token {
    if (lex.is_at_end()) {
        return lexer_error_token(lex, "Unterminated character literal");
    }

    if (lex.peek() == '\\') {
        lex.advance(); // skip backslash
        if (lex.is_at_end()) {
            return lexer_error_token(lex, "Unterminated character literal");
        }
        var escaped = lex.peek();
        if (escaped == 'x') {
            // Hex escape: \xNN
            lex.advance();
            var i: i64 = 0;
            while (i < 2) {
                if (lex.is_at_end() || !lex.peek().is_hex_digit()) {
                    return lexer_error_token(lex, "Invalid hex escape in character literal");
                }
                lex.advance();
                i += 1;
            }
        } else if (escaped.is_octal_digit()) {
            // Octal escape: \NNN (up to 3 digits)
            var i: i64 = 0;
            while (i < 3 && !lex.is_at_end() && lex.peek().is_octal_digit()) {
                lex.advance();
                i += 1;
            }
        } else {
            // Simple escape
            lex.advance();
        }
    } else {
        lex.advance(); // regular character
    }

    if (lex.is_at_end() || lex.peek() != '\'') {
        return lexer_error_token(lex, "Unterminated character literal");
    }
    lex.advance(); // closing quote
    return lexer_make_token(lex, TokenType::Char);
}

// --- Main lexer function ---

func lexer_next(lex: Lexer) -> Token {
    lex.skip_whitespace();

    lex.start = lex.pos;
    lex.start_column = lex.column;

    // Check for deferred error from block comment
    if (lex.error_message != "") {
        var msg = lex.error_message;
        lex.error_message = "";
        return lexer_error_token(lex, msg);
    }

    if (lex.is_at_end()) {
        return lexer_make_token(lex, TokenType::EOF);
    }

    var ch = lex.advance();

    if (ch.is_alpha()) {
        return lex.identifier();
    }
    if (ch.is_digit()) {
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
        if (lex.peek() == '"' && lex.peek_next() == '"') {
            lex.advance(); // second "
            lex.advance(); // third "
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

func (TokenType) name() -> string {
    match (self) {
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
