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

enum LexerCharacter {
    Char(char),
    EOF,
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

func (Lexer) advance() -> LexerCharacter {
    if (self.is_at_end()) {
        return LexerCharacter::EOF;
    }
    var ch = self.source[self.pos];
    self.pos += 1;
    match (ch) {
        '\n' => {
            self.line += 1;
            self.column = 1;
        }
        _ => self.column += 1;
    }
    return LexerCharacter::Char(ch);
}

func (Lexer) peek() -> LexerCharacter {
    if (self.is_at_end()) {
        return LexerCharacter::EOF;
    }
    return LexerCharacter::Char(self.source[self.pos]);
}

func (Lexer) peek_ahead(n: i64) -> LexerCharacter {
    if (self.pos + n >= self.source.length()) {
        return LexerCharacter::EOF;
    }
    return LexerCharacter::Char(self.source[self.pos + n]);
}

func (Lexer) peek_next() -> LexerCharacter {
    return self.peek_ahead(1);
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

func (char) is_whitespace() -> bool {
    return self == ' ' || self == '\t' || self == '\r' || self == '\n';
}

func (Lexer) skip_linecomment() -> void {
    self.advance(); // consume first /
    self.advance(); // consume second /
    while (self.peek() is LexerCharacter::Char(ch) && ch != '\n') {
        self.advance();
    }
}

func (Lexer) skip_blockcomment() -> void {
    self.advance(); // consume first /
    self.advance(); // consume *
    while (self.peek() is LexerCharacter::Char(ch)) {
        if(ch == '*') {
            if (self.peek_next() is LexerCharacter::Char(n) && n == '/') {
                self.advance(); // consume *
                self.advance(); // consume /
                return;
            }
        }
        self.advance();
    }

    // If we reach here, the block comment was unterminated. Defer the error until next token.
    self.error_message = "Unterminated block comment";
}

func (Lexer) skip_whitespace() -> void {
    while (true) {
        match (self.peek()) {
            LexerCharacter::EOF => return;
            LexerCharacter::Char(ch) => {
                if (ch.is_whitespace()) {
                    self.advance();
                    continue;
                }
                if (ch == '/') {
                    match (self.peek_next()) {
                         LexerCharacter::EOF => return;
                         LexerCharacter::Char(next) => {
                            match (next) {
                                '/' => { self.skip_linecomment();}
                                '*' => { self.skip_blockcomment();}
                                _ => return;
                            }
                         }
                    }
                } else {
                    return;
                }
            }
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
    while (self.peek() is LexerCharacter::Char(ch) && ch.is_alnum()) {
        self.advance();
    }
    var text = self.source[self.start:self.pos];
    return self.make_token(identifier_type(text));
}

// --- Number scanning ---

func (Lexer) number(start_char: char) -> Token {
    var kind = TokenType::Int;

    // Handle hex, octal, binary prefixes
    if (start_char == '0') {
        var next = self.peek();
        if(self.peek() is LexerCharacter::Char(ch) && (ch == 'x' || ch == 'X')) {
            self.advance();
            match(self.peek()) {
                LexerCharacter::EOF => return self.error_token("Invalid hexadecimal literal");
                LexerCharacter::Char(ch) => {
                    if (!ch.is_hex_digit()) {
                        return self.error_token("Invalid hexadecimal literal");
                    }
                }
            }
            while (self.peek() is LexerCharacter::Char(ch) && ch.is_hex_digit()) {
                self.advance();
            }
            return self.make_token(TokenType::Int);
        }
        if(self.peek() is LexerCharacter::Char(ch) && (ch == 'b' || ch == 'B')) {
            self.advance();
            match(self.peek()) {
                LexerCharacter::EOF => return self.error_token("Invalid binary literal");
                LexerCharacter::Char(ch) => {
                    if (ch != '0' && ch != '1') {
                        return self.error_token("Invalid binary literal");
                    }
                }
            }
            while (self.peek() is LexerCharacter::Char(ch) && (ch == '0' || ch == '1')) {
                self.advance();
            }
            return self.make_token(TokenType::Int);
        }
        if(self.peek() is LexerCharacter::Char(ch) && (ch == 'o' || ch == 'O')) {
            self.advance();
            match(self.peek()) {
                LexerCharacter::EOF => return self.error_token("Invalid octal literal");
                LexerCharacter::Char(ch) => {
                    if (!ch.is_octal_digit()) {
                        return self.error_token("Invalid octal literal");
                    }
                }
            }
            while (self.peek() is LexerCharacter::Char(ch) && ch.is_octal_digit()) {
                self.advance();
            }
            return self.make_token(TokenType::Int);
        }
    }

    // Decimal digits
    while (self.peek() is LexerCharacter::Char(ch) && ch.is_digit()) {
        self.advance();
    }

    // Decimal point
    if (self.peek() is LexerCharacter::Char(ch) && ch == '.' &&
        self.pos + 1 < self.source.length() && self.source[self.pos + 1].is_digit()) {
        kind = TokenType::Float;
        self.advance(); // consume '.'
        while (self.peek() is LexerCharacter::Char(ch) && ch.is_digit()) {
            self.advance();
        }
    }

    // Exponent
    if (self.peek() is LexerCharacter::Char(ch) && (ch == 'e' || ch == 'E')) {
        kind = TokenType::Float;
        self.advance();
        if (self.peek() is LexerCharacter::Char(ch) && (ch == '+' || ch == '-')) {
            self.advance();
        }
        while (self.peek() is LexerCharacter::Char(ch) && ch.is_digit()) {
            self.advance();
        }
    }

    return self.make_token(kind);
}

// --- Escape sequence handling ---

func (Lexer) advance_octal_value() -> Result<string, string> {
    foreach(const i in 0..3) {
        match(self.peek()) {
            LexerCharacter::EOF => return Result::Err("Unterminated octal escape sequence");
            LexerCharacter::Char(ch) => {
                if (!ch.is_octal_digit()) {
                    return Result::Err("Invalid octal escape sequence");
                }
                self.advance();
            }
        }
    }
    return Result::Ok("");
}

func (Lexer) advance_hex_value() -> Result<string, string> {
    foreach(const i in 0..3) {
        match(self.peek()) {
            LexerCharacter::EOF => return Result::Err("Unterminated hex escape sequence");
            LexerCharacter::Char(ch) => {
                if (!ch.is_hex_digit()) {
                    return Result::Err("Invalid hex escape sequence");
                }
                self.advance();
            }
        }
    }
    return Result::Ok("");
}

// Skip escape sequence after the backslash.
// Returns "" on success, error message on failure.
func (Lexer) skip_escape() -> Result<string, string> {
    if (self.is_at_end()) {
        return Result::Err("Unterminated escape sequence");
    }
    var escaped = self.peek();
    if (escaped == LexerCharacter::Char('x')) {
        // Hex escape: \xNN
        self.advance();
        return self.advance_hex_value();
    }
    if (escaped is LexerCharacter::Char(ch) && ch.is_octal_digit()) {
        return self.advance_octal_value();
    }
    // Simple escape: \n, \t, \r, \\, \', \", \e, \0, etc.
    self.advance();
    return Result::Ok("");
}

// --- String scanning ---

func (Lexer) scan_string() -> Token {
    while (self.peek() is LexerCharacter::Char(ch) && ch != '"') {
        if (ch == '\\' && self.peek_next() != LexerCharacter::EOF) {
            self.advance(); // skip backslash
            match (self.skip_escape()) {
                Result::Ok(x) => {continue;}
                Result::Err(msg) => return self.error_token(msg);
            }
            continue;
        }
        self.advance();
    }

    if (self.peek() is LexerCharacter::EOF) {
        return self.error_token("Unterminated string");
    }

    self.advance(); // closing quote
    return self.make_token(TokenType::String);
}

func (Lexer) scan_triple_string() -> Token {
    // Opening """ already consumed. Scan until closing """.
    while (self.peek() is LexerCharacter::Char(ch)) {
        if (ch == '"' &&
            self.peek_next() == LexerCharacter::Char('"') &&
            self.peek_ahead(2) == LexerCharacter::Char('"')) {
            self.advance(); // first "
            self.advance(); // second "
            self.advance(); // third "
            return self.make_token(TokenType::String);
        }
        if (ch == '\\' && self.peek_next() != LexerCharacter::EOF) {
            self.advance(); // skip backslash
            var err = self.skip_escape();
            if (err is Result::Err(msg)) {
                return self.error_token(msg);
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
    while (self.peek() is LexerCharacter::Char(ch)) {
        if (brace_depth == 0) {
            if (ch == '"') {
                self.advance(); // closing quote
                return self.make_token(TokenType::InterpString);
            }
            if (ch == '{') {
                if (self.peek_next() == LexerCharacter::Char('{')) {
                    self.advance(); // skip first {
                    self.advance(); // skip second {
                    continue;
                }
                brace_depth += 1;
                self.advance();
                continue;
            }
            if (ch == '}' && self.peek_next() == LexerCharacter::Char('}')) {
                self.advance(); // skip first }
                self.advance(); // skip second }
                continue;
            }
            if (ch == '\\' && self.peek_next() != LexerCharacter::EOF) {
                self.advance(); // skip backslash
                var err = self.skip_escape();
                if (err is Result::Err(msg)) {
                    return self.error_token(msg);
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
                while (self.peek() is LexerCharacter::Char(ch) && ch != '"') {
                    if (ch == '\\' && self.peek_next() != LexerCharacter::EOF) {
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

    if (self.peek() == LexerCharacter::Char('\\')) {
        self.advance(); // skip backslash
        if (self.is_at_end()) {
            return self.error_token("Unterminated character literal");
        }
        var escaped = self.peek();
        if (escaped == LexerCharacter::Char('x')) {
            // Hex escape: \xNN
            self.advance();
            match(self.advance_hex_value()) {
                Result::Ok(x) => { /* continue */ }
                Result::Err(msg) => return self.error_token(msg);
            }

            // return self.advance_hex_value().unwrap_or_else(|msg: string| self.error_token(msg));
        } else if (escaped is LexerCharacter::Char(ch) && ch.is_octal_digit()) {
            match(self.advance_octal_value()) {
                Result::Ok(x) => { /* continue */ }
                Result::Err(msg) => return self.error_token(msg);
            }
            // return self.advance_octal_value().unwrap_or_else(|msg: string| self.error_token(msg));
        } else {
            // Simple escape
            self.advance();
        }
    } else {
        self.advance(); // regular character
    }

    if (self.peek() is LexerCharacter::Char(ch) && ch != '\'') {
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

    // if (self.is_at_end()) {
    //     return self.make_token(TokenType::EOF);
    // }

    // var c = self.advance();
    // if (c is LexerCharacter::EOF) {
    //     return self.make_token(TokenType::EOF);
    // }
    var ch : char;
    match (self.advance()) {
        LexerCharacter::EOF => return self.make_token(TokenType::EOF);
        LexerCharacter::Char(c) => ch = c;
    }

    if (ch.is_alpha()) {
        return self.identifier();
    }
    if (ch.is_digit()) {
        return self.number(ch);
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
        if (self.peek() == LexerCharacter::Char('"') && self.peek_next() == LexerCharacter::Char('"')) {
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
