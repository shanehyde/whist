#include "alloc.h"
#include "parser_internal.h"

static Node*            parse_statement(Parser* parser);
static DestructPattern* parse_destruct_pattern(Parser* parser);

Node* parse_block(Parser* parser) {
    Node* block = node_new(NODE_BLOCK, parser->previous.line, parser->previous.column);
    nodelist_init(&block->as.block.stmts);

    while (!check_token(parser, TOK_RBRACE) && !check_token(parser, TOK_EOF)) {
        Node* stmt = parse_statement(parser);
        if (stmt)
            nodelist_push(&block->as.block.stmts, stmt);
        if (parser->panic_mode)
            synchronize(parser);
    }

    consume_token(parser, TOK_RBRACE, "Expected '}' after block");
    return block;
}

// ============================================================================
// Control Flow Statement Parsing
// ============================================================================

static Node* parse_if_stmt(Parser* parser) {
    Token token = parser->previous;
    consume_token(parser, TOK_LPAREN, "Expected '(' after 'if'");
    Node* cond = parse_expression(parser);
    consume_token(parser, TOK_RPAREN, "Expected ')' after condition");

    consume_token(parser, TOK_LBRACE, "Expected '{' after if condition");
    Node* then_block = parse_block(parser);

    Node* else_block = NULL;
    if (match_token(parser, TOK_ELSE)) {
        if (match_token(parser, TOK_IF)) {
            else_block = parse_if_stmt(parser);
        } else {
            consume_token(parser, TOK_LBRACE, "Expected '{' after 'else'");
            else_block = parse_block(parser);
        }
    }

    Node* node                  = node_new(NODE_IF, token.line, token.column);
    node->as.if_stmt.cond       = cond;
    node->as.if_stmt.then_block = then_block;
    node->as.if_stmt.else_block = else_block;
    return node;
}

static Node* parse_while_stmt(Parser* parser) {
    Token token = parser->previous;
    consume_token(parser, TOK_LPAREN, "Expected '(' after 'while'");
    Node* cond = parse_expression(parser);
    consume_token(parser, TOK_RPAREN, "Expected ')' after condition");

    consume_token(parser, TOK_LBRACE, "Expected '{' after while condition");
    Node* body = parse_block(parser);

    Node* node               = node_new(NODE_WHILE, token.line, token.column);
    node->as.while_stmt.cond = cond;
    node->as.while_stmt.body = body;
    return node;
}

static Node* parse_for_stmt(Parser* parser) {
    Token token = parser->previous;
    consume_token(parser, TOK_LPAREN, "Expected '(' after 'for'");

    // Init
    Node* init = NULL;
    if (match_token(parser, TOK_VAR)) {
        init = parse_var_decl(parser, 0, 0);
    } else if (!match_token(parser, TOK_SEMICOLON)) {
        init = parse_expression(parser);
        consume_token(parser, TOK_SEMICOLON, "Expected ';' after for initializer");
    }

    // Condition
    Node* cond = NULL;
    if (!check_token(parser, TOK_SEMICOLON)) {
        cond = parse_expression(parser);
    }
    consume_token(parser, TOK_SEMICOLON, "Expected ';' after for condition");

    // Post
    Node* post = NULL;
    if (!check_token(parser, TOK_RPAREN)) {
        post = parse_expression(parser);
    }
    consume_token(parser, TOK_RPAREN, "Expected ')' after for clauses");

    consume_token(parser, TOK_LBRACE, "Expected '{' after for clauses");
    Node* body = parse_block(parser);

    Node* node             = node_new(NODE_FOR, token.line, token.column);
    node->as.for_stmt.init = init;
    node->as.for_stmt.cond = cond;
    node->as.for_stmt.post = post;
    node->as.for_stmt.body = body;
    return node;
}

static Node* parse_foreach_stmt(Parser* parser) {
    Token token = parser->previous;
    consume_token(parser, TOK_LPAREN, "Expected '(' after 'foreach'");

    // Parse: const identifier (foreach variables are immutable)
    consume_token(parser, TOK_CONST, "Expected 'const' in foreach loop");
    consume_token(parser, TOK_IDENT, "Expected identifier after 'const'");

    Token var_token = parser->previous;
    char* var_name  = copy_token_string(&var_token);

    // Parse: in
    consume_token(parser, TOK_IN, "Expected 'in' after foreach variable");

    // Parse: first expression (either range start or collection)
    Node* expr = parse_expression(parser);

    Node* start      = NULL;
    Node* end        = NULL;
    Node* step       = NULL;
    Node* collection = NULL;

    if (match_token(parser, TOK_DOT_DOT)) {
        // Range foreach: foreach (const i in start..end [by step])
        start = expr;
        end   = parse_expression(parser);

        if (match_token(parser, TOK_BY)) {
            step = parse_expression(parser);
        } else {
            // Default step is 1
            step                   = node_new(NODE_INT_LIT, token.line, token.column);
            step->as.int_lit.value = 1;
        }
    } else {
        // Collection foreach: foreach (const item in vec)
        collection = expr;
    }

    consume_token(parser, TOK_RPAREN, "Expected ')' after foreach clauses");
    consume_token(parser, TOK_LBRACE, "Expected '{' after foreach clauses");
    Node* body = parse_block(parser);

    Node* node                            = node_new(NODE_FOREACH, token.line, token.column);
    node->as.foreach_stmt.var_name        = var_name;
    node->as.foreach_stmt.var_name_length = var_token.length;
    node->as.foreach_stmt.start           = start;
    node->as.foreach_stmt.end             = end;
    node->as.foreach_stmt.step            = step;
    node->as.foreach_stmt.body            = body;
    node->as.foreach_stmt.collection      = collection;
    return node;
}

static Node* parse_return_stmt(Parser* parser) {
    Token token = parser->previous;
    Node* value = NULL;

    if (!check_token(parser, TOK_SEMICOLON)) {
        value = parse_expression(parser);
    }
    consume_token(parser, TOK_SEMICOLON, "Expected ';' after return value");

    Node* node                 = node_new(NODE_RETURN, token.line, token.column);
    node->as.return_stmt.value = value;
    return node;
}

static Node* parse_defer_stmt(Parser* parser) {
    Token token = parser->previous;

    // Parse the deferred statement (typically an expression statement like a function call)
    Node* stmt = parse_statement(parser);

    Node* node               = node_new(NODE_DEFER, token.line, token.column);
    node->as.defer_stmt.stmt = stmt;
    return node;
}

// ============================================================================
// Match Statement Parsing
// ============================================================================

Node* parse_match(Parser* parser, int is_expr) {
    Token token = parser->previous; // TOK_MATCH already consumed
    consume_token(parser, TOK_LPAREN, "Expected '(' after 'match'");
    Node* expr = parse_expression(parser);
    consume_token(parser, TOK_RPAREN, "Expected ')' after match expression");
    consume_token(parser, TOK_LBRACE, "Expected '{' after match expression");

    Node* node                              = node_new(NODE_MATCH, token.line, token.column);
    node->as.match_stmt.expr                = expr;
    node->as.match_stmt.resolved_type       = NULL;
    node->as.match_stmt.resolved_value_type = NULL;
    nodelist_init(&node->as.match_stmt.arms);

    while (!check_token(parser, TOK_RBRACE) && !check_token(parser, TOK_EOF)) {
        Token arm_token             = parser->current;
        Node* arm                   = node_new(NODE_MATCH_ARM, arm_token.line, arm_token.column);
        arm->as.match_arm.enum_name = NULL;
        arm->as.match_arm.enum_name_length    = 0;
        arm->as.match_arm.variant_name        = NULL;
        arm->as.match_arm.variant_name_length = 0;
        arm->as.match_arm.bindings            = NULL;
        arm->as.match_arm.binding_count       = 0;
        arm->as.match_arm.is_wildcard         = 0;
        arm->as.match_arm.body                = NULL;

        // Parse pattern
        if (parser->current.type == TOK_IDENT && parser->current.length == 1 &&
            parser->current.start[0] == '_') {
            // Wildcard: _
            advance_token(parser);
            arm->as.match_arm.is_wildcard = 1;
        } else if (check_token(parser, TOK_IDENT)) {
            Token first_ident = parser->current;
            advance_token(parser);

            if (check_token(parser, TOK_COLON_COLON)) {
                // Qualified: EnumName::VariantName or EnumName::VariantName(bindings...)
                advance_token(parser); // consume ::
                Token variant = parser->current;
                consume_token(parser, TOK_IDENT, "Expected variant name after '::'");
                arm->as.match_arm.enum_name           = copy_token_string(&first_ident);
                arm->as.match_arm.enum_name_length    = first_ident.length;
                arm->as.match_arm.variant_name        = copy_token_string(&variant);
                arm->as.match_arm.variant_name_length = variant.length;
            } else {
                // Unqualified: VariantName or VariantName(bindings...)
                arm->as.match_arm.variant_name        = copy_token_string(&first_ident);
                arm->as.match_arm.variant_name_length = first_ident.length;
            }

            // Optional bindings: (a, b, ...)
            if (match_token(parser, TOK_LPAREN)) {
                int    capacity = 4;
                char** bindings = xmalloc(capacity * sizeof(char*));
                int    count    = 0;

                while (!check_token(parser, TOK_RPAREN) && !check_token(parser, TOK_EOF)) {
                    Token binding = parser->current;
                    consume_token(parser, TOK_IDENT, "Expected binding name in match pattern");
                    if (count >= capacity) {
                        capacity *= 2;
                        bindings = xrealloc(bindings, capacity * sizeof(char*));
                    }
                    bindings[count++] = copy_token_string(&binding);
                    if (!check_token(parser, TOK_RPAREN)) {
                        consume_token(parser, TOK_COMMA, "Expected ',' or ')' after binding name");
                    }
                }
                consume_token(parser, TOK_RPAREN, "Expected ')' after bindings");
                arm->as.match_arm.bindings      = bindings;
                arm->as.match_arm.binding_count = count;
            }
        } else {
            parse_error(parser, "Expected match pattern");
            node_free(arm);
            node_free(node);
            return NULL;
        }

        // Expect =>
        consume_token(parser, TOK_FAT_ARROW, "Expected '=>' after match pattern");

        if (is_expr) {
            if (check_token(parser, TOK_LBRACE)) {
                parse_error(parser, "Match expression arms must be expressions (no block body)");
                node_free(arm);
                node_free(node);
                return NULL;
            }
            arm->as.match_arm.body = parse_expression(parser);
            if (!arm->as.match_arm.body) {
                node_free(arm);
                node_free(node);
                return NULL;
            }
        } else {
            // Parse arm body: block or single statement
            if (check_token(parser, TOK_LBRACE)) {
                advance_token(parser);
                arm->as.match_arm.body = parse_block(parser);
            } else {
                arm->as.match_arm.body = parse_statement(parser);
            }
        }

        nodelist_push(&node->as.match_stmt.arms, arm);

        // Optional comma between arms
        match_token(parser, TOK_COMMA);
    }

    consume_token(parser, TOK_RBRACE, "Expected '}' after match arms");
    return node;
}

static Node* parse_match_stmt(Parser* parser) {
    return parse_match(parser, 0);
}

// ============================================================================
// Statement Parsing
// ============================================================================

static Node* parse_statement(Parser* parser) {
    if (match_token(parser, TOK_VAR)) {
        return parse_var_decl(parser, 0, 0);
    }
    if (match_token(parser, TOK_CONST)) {
        return parse_var_decl(parser, 1, 0);
    }
    if (match_token(parser, TOK_IF)) {
        return parse_if_stmt(parser);
    }
    if (match_token(parser, TOK_WHILE)) {
        return parse_while_stmt(parser);
    }
    if (match_token(parser, TOK_FOR)) {
        return parse_for_stmt(parser);
    }
    if (match_token(parser, TOK_FOREACH)) {
        return parse_foreach_stmt(parser);
    }
    if (match_token(parser, TOK_RETURN)) {
        return parse_return_stmt(parser);
    }
    if (match_token(parser, TOK_DEFER)) {
        return parse_defer_stmt(parser);
    }
    if (match_token(parser, TOK_MATCH)) {
        return parse_match_stmt(parser);
    }
    if (match_token(parser, TOK_BREAK)) {
        Token token = parser->previous;
        consume_token(parser, TOK_SEMICOLON, "Expected ';' after 'break'");
        Node* node = node_new(NODE_BREAK, token.line, token.column);
        return node;
    }
    if (match_token(parser, TOK_CONTINUE)) {
        Token token = parser->previous;
        consume_token(parser, TOK_SEMICOLON, "Expected ';' after 'continue'");
        Node* node = node_new(NODE_CONTINUE, token.line, token.column);
        return node;
    }
    if (match_token(parser, TOK_LBRACE)) {
        return parse_block(parser);
    }

    // Expression statement
    Node* expr = parse_expression(parser);
    consume_token(parser, TOK_SEMICOLON, "Expected ';' after expression");

    Node* node = node_new(NODE_EXPR_STMT, expr ? expr->line : 0, expr ? expr->column : 0);
    node->as.expr_stmt.expr = expr;
    return node;
}

// ============================================================================
// Declaration Parsing
// ============================================================================

// Parse a destructuring pattern element: either an identifier or a nested tuple pattern
// Called after '(' has been consumed for the outer pattern, or recursively for nested patterns
static DestructPattern* parse_destruct_pattern_element(Parser* parser) {
    if (check_token(parser, TOK_LPAREN)) {
        // Nested tuple pattern: (a, b) or (a, (b, c))
        advance_token(parser); // consume '('

        DestructPattern* pattern = pattern_new_tuple(4);

        // Parse first element
        DestructPattern* first = parse_destruct_pattern_element(parser);
        if (!first) {
            pattern_free(pattern);
            return NULL;
        }
        pattern_tuple_push(pattern, first);

        // Parse remaining elements (comma-separated)
        while (match_token(parser, TOK_COMMA)) {
            // Grow capacity if needed
            if (pattern->as.tuple.count >= 4) {
                int new_cap = pattern->as.tuple.count * 2;
                pattern->as.tuple.elements =
                    xrealloc(pattern->as.tuple.elements, new_cap * sizeof(DestructPattern*));
            }
            DestructPattern* elem = parse_destruct_pattern_element(parser);
            if (!elem) {
                pattern_free(pattern);
                return NULL;
            }
            pattern_tuple_push(pattern, elem);
        }

        consume_token(parser, TOK_RPAREN, "Expected ')' after nested destructuring pattern");

        // Require at least 2 elements for tuple pattern
        if (pattern->as.tuple.count < 2) {
            parse_error(parser, "Destructuring pattern requires at least 2 elements");
            pattern_free(pattern);
            return NULL;
        }

        return pattern;
    } else if (check_token(parser, TOK_IDENT)) {
        // Simple identifier
        Token name_tok = parser->current;
        advance_token(parser);
        return pattern_new_ident(name_tok.start, name_tok.length);
    } else {
        parse_error(parser, "Expected variable name or nested pattern in destructuring");
        return NULL;
    }
}

// Parse the top-level destructuring pattern: var (pattern) = ...
// Called after 'var' or 'const' keyword, when '(' is the next token
static DestructPattern* parse_destruct_pattern(Parser* parser) {
    // We expect '(' to already be the current token
    if (!check_token(parser, TOK_LPAREN)) {
        parse_error(parser, "Expected '(' for destructuring pattern");
        return NULL;
    }
    return parse_destruct_pattern_element(parser);
}

Node* parse_var_decl(Parser* parser, int is_const, int is_public) {
    Token start = parser->current;

    // Check for struct destructuring: var {field1, field2} = expr;
    if (check_token(parser, TOK_LBRACE)) {
        advance_token(parser); // consume '{'

        DestructPattern* pattern = pattern_new_struct(4);

        // Parse first field name (required)
        Token field = parser->current;
        consume_token(parser, TOK_IDENT, "Expected field name");
        pattern_struct_push(pattern, field.start, field.length);

        // Parse remaining comma-separated field names
        while (match_token(parser, TOK_COMMA)) {
            field = parser->current;
            consume_token(parser, TOK_IDENT, "Expected field name");
            pattern_struct_push(pattern, field.start, field.length);
        }

        consume_token(parser, TOK_RBRACE, "Expected '}'");

        Node*          node = node_new(NODE_VAR_DECL, start.line, start.column);
        var_decl_node* vdn  = &node->as.var_decl;

        vdn->name             = NULL;
        vdn->name_length      = 0;
        vdn->is_public        = is_public;
        vdn->is_const         = is_const;
        vdn->type             = NULL;
        vdn->init             = NULL;
        vdn->destruct_pattern = pattern;

        // Initializer is required for struct destructuring
        if (!match_token(parser, TOK_EQ)) {
            parse_error(parser, "Struct destructuring requires an initializer");
            node_free(node);
            return NULL;
        }
        vdn->init = parse_expression(parser);

        consume_token(parser, TOK_SEMICOLON, "Expected ';' after variable declaration");
        return node;
    }

    // Check for destructuring: var (a, b) = ... or var (a, (b, c)) = ...
    if (check_token(parser, TOK_LPAREN)) {
        DestructPattern* pattern = parse_destruct_pattern(parser);
        if (!pattern) {
            return NULL;
        }

        Node*          node = node_new(NODE_VAR_DECL, start.line, start.column);
        var_decl_node* vdn  = &node->as.var_decl;

        vdn->name             = NULL; // NULL indicates destructuring
        vdn->name_length      = 0;
        vdn->is_public        = is_public;
        vdn->is_const         = is_const;
        vdn->type             = NULL;
        vdn->init             = NULL;
        vdn->destruct_pattern = pattern;

        // Optional type annotation
        if (match_token(parser, TOK_COLON)) {
            vdn->type = parse_type(parser);
        }

        // Initializer is required for destructuring
        if (!match_token(parser, TOK_EQ)) {
            parse_error(parser, "Destructuring declaration requires an initializer");
            node_free(node);
            return NULL;
        }
        vdn->init = parse_expression(parser);

        consume_token(parser, TOK_SEMICOLON, "Expected ';' after variable declaration");
        return node;
    }

    // Normal variable declaration
    Token name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected variable name");

    Node*          node = node_new(NODE_VAR_DECL, name.line, name.column);
    var_decl_node* vdn  = &node->as.var_decl;

    vdn->name             = copy_token_string(&name);
    vdn->is_public        = is_public;
    vdn->name_length      = name.length;
    vdn->is_const         = is_const;
    vdn->type             = NULL;
    vdn->init             = NULL;
    vdn->destruct_pattern = NULL;

    // Optional type annotation
    if (match_token(parser, TOK_COLON)) {
        vdn->type = parse_type(parser);
    }

    // Optional initializer
    if (match_token(parser, TOK_EQ)) {
        vdn->init = parse_expression(parser);
    }

    consume_token(parser, TOK_SEMICOLON, "Expected ';' after variable declaration");
    return node;
}
