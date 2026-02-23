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

func (Lexer) advance() -> void {
    if (self.is_at_end()) {
        return;
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
    while (self.peek() is Char(ch) && ch != '\n') {
        self.advance();
    }
}

func (Lexer) skip_blockcomment() -> void {
    self.advance(); // consume first /
    self.advance(); // consume *
    while (self.peek() is Char(ch)) {
        if (ch == '*' && self.peek_next() is Char(n) && n == '/') {
            self.advance(); // consume *
            self.advance(); // consume /
            return;
        }
        self.advance();
    }

    // If we reach here, the block comment was unterminated. Defer the error until next token.
    self.error_message = "Unterminated block comment";
}

func (Lexer) skip_whitespace() -> void {
    while (true) {
        if (self.peek() is EOF) {
            return;
        }

        if(self.peek() is Char(ch)) {
            if (ch.is_whitespace()) {
                self.advance();
                continue;
            }

            if (ch == '/') {
                if (self.peek_next() is Char(n)) {
                    match (n) {
                        '/' => { self.skip_linecomment(); }
                        '*' => { self.skip_blockcomment(); }
                        _ => return;
                    }
                } else {
                    return;
                }
            } else {
                return;
            }
        }
    }
}

// --- Keyword matching ---

func identifier_type(text: string) -> TokenType {
    match (text[0]) {
        'a' => match (text) {
            "as" => return TokenType::As;
         }
        'b' => match (text) {
            "break" => return TokenType::Break;
            "by" => return TokenType::By;
        }
        'c' => match (text) {
            "const" => return TokenType::Const;
            "continue" => return TokenType::Continue;
        }
        'd' => match (text) {
            "defer" => return TokenType::Defer;
        }
        'e' => match (text) {
            "else" => return TokenType::Else;
            "enum" => return TokenType::Enum;
            "extern" => return TokenType::Extern;
        }
        'f' => match (text) {
            "false" => return TokenType::False;
            "for" => return TokenType::For;
            "foreach" => return TokenType::Foreach;
            "func" => return TokenType::Func;
        }
        'i' => match (text) {
            "if" => return TokenType::If;
            "impl" => return TokenType::Impl;
            "import" => return TokenType::Import;
            "in" => return TokenType::In;
            "include" => return TokenType::Include;
            "is" => return TokenType::Is;
        }
        'm' => match (text) {
            "match" => return TokenType::Match;
        }
        'n' => match (text) {
            "new" => return TokenType::New;
            "null" => return TokenType::Null;
        }
        'p' => match (text) {
            "public" => return TokenType::Public;
            "private" => return TokenType::Private;
        }
        'r' => match (text) {
            "return" => return TokenType::Return;
        }
        's' => match (text) {
            "self" => return TokenType::SelfKw;
            "struct" => return TokenType::Struct;
        }
        't' => match (text) {
            "test" => return TokenType::Test;
            "trait" => return TokenType::Trait;
            "true" => return TokenType::True;
            "type" => return TokenType::Type;
        }
        'u' => match (text) {
            "use" => return TokenType::Use;
        }
        'v' => match (text) {
            "var" => return TokenType::Var;
        }
        'w' => match (text) {
            "while" => return TokenType::While;
        }
    }
    return TokenType::Ident;
}

// --- Identifier scanning ---

func (Lexer) identifier() -> Token {
    while (self.peek() is Char(ch) && ch.is_alnum()) {
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
        if (self.peek() is Char(ch) && (ch == 'x' || ch == 'X')) {
            self.advance();
            match(self.peek()) {
                EOF => return self.error_token("Invalid hexadecimal literal");
                Char(ch) => {
                    if (!ch.is_hex_digit()) {
                        return self.error_token("Invalid hexadecimal literal");
                    }
                }
            }
            while (self.peek() is Char(ch) && ch.is_hex_digit()) {
                self.advance();
            }
            return self.make_token(TokenType::Int);
        }
        if (self.peek() is Char(ch) && (ch == 'b' || ch == 'B')) {
            self.advance();
            match(self.peek()) {
                EOF => return self.error_token("Invalid binary literal");
                Char(ch) => {
                    if (ch != '0' && ch != '1') {
                        return self.error_token("Invalid binary literal");
                    }
                }
            }
            while (self.peek() is Char(ch) && (ch == '0' || ch == '1')) {
                self.advance();
            }
            return self.make_token(TokenType::Int);
        }
        if (self.peek() is Char(ch) && (ch == 'o' || ch == 'O')) {
            self.advance();
            match(self.peek()) {
                EOF => return self.error_token("Invalid octal literal");
                Char(ch) => {
                    if (!ch.is_octal_digit()) {
                        return self.error_token("Invalid octal literal");
                    }
                }
            }
            while (self.peek() is Char(ch) && ch.is_octal_digit()) {
                self.advance();
            }
            return self.make_token(TokenType::Int);
        }
    }

    // Decimal digits
    while (self.peek() is Char(ch) && ch.is_digit()) {
        self.advance();
    }

    // Decimal point
    if (self.peek() is Char(ch) && ch == '.' &&
        self.peek_next() is Char(next) && next.is_digit()) {
        kind = TokenType::Float;
        self.advance(); // consume '.'
        while (self.peek() is Char(ch) && ch.is_digit()) {
            self.advance();
        }
    }

    // Exponent
    if (self.peek() is Char(ch) && (ch == 'e' || ch == 'E')) {
        kind = TokenType::Float;
        self.advance();
        if (self.peek() is Char(ch) && (ch == '+' || ch == '-')) {
            self.advance();
        }
        while (self.peek() is Char(ch) && ch.is_digit()) {
            self.advance();
        }
    }

    return self.make_token(kind);
}

// --- Escape sequence handling ---

func (Lexer) advance_octal_value() {
    // Consume 1-3 octal digits greedily (like C's \NNN)
    for (var i: i64 = 0; i < 3; i += 1) {
        if (self.peek() is Char(ch) && ch.is_octal_digit()) {
            self.advance();
        } else {
            return;
        }
    }
}

func (Lexer) advance_hex_value(error_msg: string) -> Result<string, string> {
    // Exactly 2 hex digits required for \xNN
    foreach(const i in 0..2) {
        match(self.peek()) {
            EOF => return Result::Err(error_msg);
            Char(ch) => {
                if (!ch.is_hex_digit()) {
                    return Result::Err(error_msg);
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
    if (escaped is Char(c) && c == 'x') {
        // Hex escape: \xNN
        self.advance();
        return self.advance_hex_value("Invalid hex escape in string literal");
    }
    if (escaped is Char(ch) && ch.is_octal_digit()) {
        // Octal escape: 1-3 digits
        self.advance_octal_value();
        return Result::Ok("");
    }
    // Simple escape: \n, \t, \r, \\, \', \", \e, \0, etc.
    self.advance();
    return Result::Ok("");
}

// --- String scanning ---

func (Lexer) scan_string() -> Token {
    while (self.peek() is Char(ch) && ch != '"') {
        if (ch == '\\' && !(self.peek_next() is EOF)) {
            self.advance(); // skip backslash
            match (self.skip_escape()) {
                Ok(x) => {continue;}
                Err(msg) => return self.error_token(msg);
            }
        }
        self.advance();
    }

    if (self.peek() is EOF) {
        return self.error_token("Unterminated string");
    }

    self.advance(); // closing quote
    return self.make_token(TokenType::String);
}

func (Lexer) scan_triple_string() -> Token {
    // Opening """ already consumed. Scan until closing """.
    while (self.peek() is Char(ch)) {
        if (ch == '"' &&
            self.peek_next() is Char(n) && n == '"' &&
            self.peek_ahead(2) is Char(a) && a == '"') {
            self.advance(); // first "
            self.advance(); // second "
            self.advance(); // third "
            return self.make_token(TokenType::String);
        }
        if (ch == '\\' && !(self.peek_next() is EOF)) {
            self.advance(); // skip backslash
            var err = self.skip_escape();
            if (err is Err(msg)) {
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
    while (self.peek() is Char(ch)) {
        if (brace_depth == 0) {
            if (ch == '"') {
                self.advance(); // closing quote
                return self.make_token(TokenType::InterpString);
            }
            if (ch == '{') {
                if (self.peek_next() is Char(c) && c == '{') {
                    self.advance(); // skip first {
                    self.advance(); // skip second {
                    continue;
                }
                brace_depth += 1;
                self.advance();
                continue;
            }
            if (ch == '}' && self.peek_next() is Char(c) && c == '}') {
                self.advance(); // skip first }
                self.advance(); // skip second }
                continue;
            }
            if (ch == '\\' && !(self.peek_next() is EOF)) {
                self.advance(); // skip backslash
                var err = self.skip_escape();
                if (err is Err(msg)) {
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
                while (self.peek() is Char(ch) && ch != '"') {
                    if (ch == '\\' && !(self.peek_next() is EOF)) {
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

    if (self.peek() is Char(ch) && ch == '\\') {
        self.advance(); // skip backslash
        if (self.is_at_end()) {
            return self.error_token("Unterminated character literal");
        }
        var escaped = self.peek();
        if (escaped is Char(c) && c == 'x') {
            // Hex escape: \xNN
            self.advance();
            match(self.advance_hex_value("Invalid hex escape in character literal")) {
                Ok(x) => { /* continue */ }
                Err(msg) => return self.error_token(msg);
            }
        } else if (escaped is Char(ch) && ch.is_octal_digit()) {
            // Octal escape: 1-3 digits
            self.advance_octal_value();
        } else {
            // Simple escape
            self.advance();
        }
    } else {
        self.advance(); // regular character
    }

    if (self.peek() is Char(ch) && ch != '\'') {
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

    var ch : char;
    match (self.peek()) {
        EOF => return self.make_token(TokenType::EOF);
        Char(c) => ch = c;
    }
    self.advance();

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
        if (self.peek() is Char(c) && c == '"' && self.peek_next() is Char(c2) && c2 == '"') {
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
