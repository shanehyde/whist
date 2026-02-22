// Whist Lexer — Self-hosted port of w0/lexer.c

public enum TokenType {
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

public struct Token {
    kind: TokenType,
    value: string,
    line: i64,
    column: i64,
}

public struct Lexer {
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
    public func init(source: string) {
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

func (Lexer) match_next(expected: char) -> bool {
    if (self.is_at_end()) {
        return false;
    }
    if (self.source[self.pos] != expected) {
        return false;
    }
    self.advance();
    return true;
}

func (Lexer) make_token(kind: TokenType) -> Token {
    var tok = new Token{
        kind: kind,
        value: self.source[self.start:self.pos],
        line: self.line,
        column: self.start_column,
    };
    return tok;
}

func (Lexer) error_token(message: string) -> Token {
    var tok = new Token{
        kind: TokenType::Error,
        value: message,
        line: self.line,
        column: self.start_column,
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

func (Lexer) identifier() -> Token {
    while (!self.is_at_end() && self.peek().is_alnum()) {
        self.advance();
    }
    var text = self.source[self.start:self.pos];
    return self.make_token(identifier_type(text));
}

// --- Number scanning ---

func (Lexer) number() -> Token {
    var kind = TokenType::Int;

    // Handle hex, octal, binary prefixes
    if (self.source[self.start] == '0' && !self.is_at_end()) {
        var next = self.peek();
        if (next == 'x' || next == 'X') {
            self.advance();
            if (self.is_at_end() || !self.peek().is_hex_digit()) {
                return self.error_token("Invalid hexadecimal literal");
            }
            while (!self.is_at_end() && self.peek().is_hex_digit()) {
                self.advance();
            }
            return self.make_token(TokenType::Int);
        } else if (next == 'b' || next == 'B') {
            self.advance();
            if (self.is_at_end() || (self.peek() != '0' && self.peek() != '1')) {
                return self.error_token("Invalid binary literal");
            }
            while (!self.is_at_end() && (self.peek() == '0' || self.peek() == '1')) {
                self.advance();
            }
            return self.make_token(TokenType::Int);
        } else if (next == 'o' || next == 'O') {
            self.advance();
            if (self.is_at_end() || !self.peek().is_octal_digit()) {
                return self.error_token("Invalid octal literal");
            }
            while (!self.is_at_end() && self.peek().is_octal_digit()) {
                self.advance();
            }
            return self.make_token(TokenType::Int);
        }
    }

    // Decimal digits
    while (!self.is_at_end() && self.peek().is_digit()) {
        self.advance();
    }

    // Decimal point
    if (!self.is_at_end() && self.peek() == '.' &&
        self.pos + 1 < self.source.length() && self.source[self.pos + 1].is_digit()) {
        kind = TokenType::Float;
        self.advance(); // consume '.'
        while (!self.is_at_end() && self.peek().is_digit()) {
            self.advance();
        }
    }

    // Exponent
    if (!self.is_at_end() && (self.peek() == 'e' || self.peek() == 'E')) {
        kind = TokenType::Float;
        self.advance();
        if (!self.is_at_end() && (self.peek() == '+' || self.peek() == '-')) {
            self.advance();
        }
        while (!self.is_at_end() && self.peek().is_digit()) {
            self.advance();
        }
    }

    return self.make_token(kind);
}

// --- Escape sequence handling ---

// Skip escape sequence after the backslash.
// Returns "" on success, error message on failure.
func (Lexer) skip_escape() -> string {
    if (self.is_at_end()) {
        return "Unterminated escape sequence";
    }
    var escaped = self.peek();
    if (escaped == 'x') {
        // Hex escape: \xNN
        self.advance();
        var i: i64 = 0;
        while (i < 2) {
            if (self.is_at_end() || !self.peek().is_hex_digit()) {
                return "Invalid hex escape in string literal";
            }
            self.advance();
            i += 1;
        }
        return "";
    } else if (escaped.is_octal_digit()) {
        // Octal escape: up to 3 digits
        var i: i64 = 0;
        while (i < 3 && !self.is_at_end() && self.peek().is_octal_digit()) {
            self.advance();
            i += 1;
        }
        return "";
    } else {
        // Simple escape: \n, \t, \r, \\, \', \", \e, \0, etc.
        self.advance();
        return "";
    }
}

// --- String scanning ---

func (Lexer) scan_string() -> Token {
    while (!self.is_at_end() && self.peek() != '"') {
        if (self.peek() == '\\' && self.peek_next() != '\0') {
            self.advance(); // skip backslash
            var err = self.skip_escape();
            if (err != "") {
                return self.error_token(err);
            }
            continue;
        }
        self.advance();
    }

    if (self.is_at_end()) {
        return self.error_token("Unterminated string");
    }

    self.advance(); // closing quote
    return self.make_token(TokenType::String);
}

func (Lexer) scan_triple_string() -> Token {
    // Opening """ already consumed. Scan until closing """.
    while (!self.is_at_end()) {
        if (self.peek() == '"' && self.peek_next() == '"' &&
            self.pos + 2 < self.source.length() && self.source[self.pos + 2] == '"') {
            self.advance(); // first "
            self.advance(); // second "
            self.advance(); // third "
            return self.make_token(TokenType::String);
        }
        if (self.peek() == '\\' && self.peek_next() != '\0') {
            self.advance(); // skip backslash
            var err = self.skip_escape();
            if (err != "") {
                return self.error_token(err);
            }
            continue;
        }
        self.advance();
    }
    return self.error_token("Unterminated triple-quoted string");
}

func (Lexer) scan_interp_string() -> Token {
    // Scan from after $" to closing ", tracking brace depth for {expr} regions
    var brace_depth: i64 = 0;
    while (!self.is_at_end()) {
        var ch = self.peek();
        if (brace_depth == 0) {
            if (ch == '"') {
                self.advance(); // closing quote
                return self.make_token(TokenType::InterpString);
            }
            if (ch == '{') {
                if (self.peek_next() == '{') {
                    self.advance(); // skip first {
                    self.advance(); // skip second {
                    continue;
                }
                brace_depth += 1;
                self.advance();
                continue;
            }
            if (ch == '}' && self.peek_next() == '}') {
                self.advance(); // skip first }
                self.advance(); // skip second }
                continue;
            }
            if (ch == '\\' && self.peek_next() != '\0') {
                self.advance(); // skip backslash
                var err = self.skip_escape();
                if (err != "") {
                    return self.error_token(err);
                }
                continue;
            }
            self.advance();
        } else {
            // Inside {expr} region
            if (ch == '{') {
                brace_depth += 1;
            } else if (ch == '}') {
                brace_depth -= 1;
            } else if (ch == '"') {
                // String literal inside expression - skip it
                self.advance(); // opening quote
                while (!self.is_at_end() && self.peek() != '"') {
                    if (self.peek() == '\\' && self.peek_next() != '\0') {
                        self.advance();
                    }
                    self.advance();
                }
                if (!self.is_at_end()) {
                    self.advance(); // closing quote
                }
                continue;
            }
            self.advance();
        }
    }
    return self.error_token("Unterminated interpolated string");
}

// --- Character literal scanning ---

func (Lexer) scan_char() -> Token {
    if (self.is_at_end()) {
        return self.error_token("Unterminated character literal");
    }

    if (self.peek() == '\\') {
        self.advance(); // skip backslash
        if (self.is_at_end()) {
            return self.error_token("Unterminated character literal");
        }
        var escaped = self.peek();
        if (escaped == 'x') {
            // Hex escape: \xNN
            self.advance();
            var i: i64 = 0;
            while (i < 2) {
                if (self.is_at_end() || !self.peek().is_hex_digit()) {
                    return self.error_token("Invalid hex escape in character literal");
                }
                self.advance();
                i += 1;
            }
        } else if (escaped.is_octal_digit()) {
            // Octal escape: \NNN (up to 3 digits)
            var i: i64 = 0;
            while (i < 3 && !self.is_at_end() && self.peek().is_octal_digit()) {
                self.advance();
                i += 1;
            }
        } else {
            // Simple escape
            self.advance();
        }
    } else {
        self.advance(); // regular character
    }

    if (self.is_at_end() || self.peek() != '\'') {
        return self.error_token("Unterminated character literal");
    }
    self.advance(); // closing quote
    return self.make_token(TokenType::Char);
}

// --- Main lexer function ---

public func (Lexer) next() -> Token {
    self.skip_whitespace();

    self.start = self.pos;
    self.start_column = self.column;

    // Check for deferred error from block comment
    if (self.error_message != "") {
        var msg = self.error_message;
        self.error_message = "";
        return self.error_token(msg);
    }

    if (self.is_at_end()) {
        return self.make_token(TokenType::EOF);
    }

    var ch = self.advance();

    if (ch.is_alpha()) {
        return self.identifier();
    }
    if (ch.is_digit()) {
        return self.number();
    }

    // Operators and punctuation
    if (ch == '(') { return self.make_token(TokenType::LParen); }
    if (ch == ')') { return self.make_token(TokenType::RParen); }
    if (ch == '{') { return self.make_token(TokenType::LBrace); }
    if (ch == '}') { return self.make_token(TokenType::RBrace); }
    if (ch == '[') { return self.make_token(TokenType::LBracket); }
    if (ch == ']') { return self.make_token(TokenType::RBracket); }
    if (ch == ';') { return self.make_token(TokenType::Semicolon); }
    if (ch == ',') { return self.make_token(TokenType::Comma); }
    if (ch == '~') { return self.make_token(TokenType::Tilde); }
    if (ch == '?') { return self.make_token(TokenType::Question); }

    if (ch == ':') {
        if (self.match_next(':')) {
            return self.make_token(TokenType::ColonColon);
        }
        return self.make_token(TokenType::Colon);
    }

    if (ch == '.') {
        if (self.match_next('.')) {
            if (self.match_next('.')) {
                return self.make_token(TokenType::Ellipsis);
            }
            return self.make_token(TokenType::DotDot);
        }
        return self.make_token(TokenType::Dot);
    }

    if (ch == '+') {
        if (self.match_next('=')) {
            return self.make_token(TokenType::PlusEq);
        }
        return self.make_token(TokenType::Plus);
    }

    if (ch == '-') {
        if (self.match_next('>')) {
            return self.make_token(TokenType::Arrow);
        }
        if (self.match_next('=')) {
            return self.make_token(TokenType::MinusEq);
        }
        return self.make_token(TokenType::Minus);
    }

    if (ch == '*') {
        if (self.match_next('=')) {
            return self.make_token(TokenType::StarEq);
        }
        return self.make_token(TokenType::Star);
    }

    if (ch == '/') {
        if (self.match_next('=')) {
            return self.make_token(TokenType::SlashEq);
        }
        return self.make_token(TokenType::Slash);
    }

    if (ch == '%') {
        if (self.match_next('=')) {
            return self.make_token(TokenType::PercentEq);
        }
        return self.make_token(TokenType::Percent);
    }

    if (ch == '^') {
        if (self.match_next('=')) {
            return self.make_token(TokenType::CaretEq);
        }
        return self.make_token(TokenType::Caret);
    }

    if (ch == '&') {
        if (self.match_next('&')) {
            return self.make_token(TokenType::AmpAmp);
        }
        if (self.match_next('=')) {
            return self.make_token(TokenType::AmpEq);
        }
        return self.make_token(TokenType::Amp);
    }

    if (ch == '|') {
        if (self.match_next('|')) {
            return self.make_token(TokenType::PipePipe);
        }
        if (self.match_next('=')) {
            return self.make_token(TokenType::PipeEq);
        }
        return self.make_token(TokenType::Pipe);
    }

    if (ch == '!') {
        if (self.match_next('=')) {
            return self.make_token(TokenType::BangEq);
        }
        return self.make_token(TokenType::Bang);
    }

    if (ch == '=') {
        if (self.match_next('>')) {
            return self.make_token(TokenType::FatArrow);
        }
        if (self.match_next('=')) {
            return self.make_token(TokenType::EqEq);
        }
        return self.make_token(TokenType::Eq);
    }

    if (ch == '<') {
        if (self.match_next('<')) {
            if (self.match_next('=')) {
                return self.make_token(TokenType::LtLtEq);
            }
            return self.make_token(TokenType::LtLt);
        }
        if (self.match_next('=')) {
            return self.make_token(TokenType::LtEq);
        }
        return self.make_token(TokenType::Lt);
    }

    if (ch == '>') {
        if (self.match_next('>')) {
            if (self.match_next('=')) {
                return self.make_token(TokenType::GtGtEq);
            }
            return self.make_token(TokenType::GtGt);
        }
        if (self.match_next('=')) {
            return self.make_token(TokenType::GtEq);
        }
        return self.make_token(TokenType::Gt);
    }

    if (ch == '"') {
        if (self.peek() == '"' && self.peek_next() == '"') {
            self.advance(); // second "
            self.advance(); // third "
            return self.scan_triple_string();
        }
        return self.scan_string();
    }

    if (ch == '\'') {
        return self.scan_char();
    }

    if (ch == '$') {
        if (self.match_next('"')) {
            return self.scan_interp_string();
        }
        return self.error_token("Unexpected character '$'");
    }

    return self.error_token("Unexpected character");
}

// --- Token type name ---

public func (TokenType) name() -> string {
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
