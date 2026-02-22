#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "parser_internal.h"

typedef struct FuncReceiverInfo {
    char*    type;
    int      type_len;
    int      is_const;
    NodeList type_args;
} FuncReceiverInfo;

// Initializes receiver parsing state used while reading an optional method receiver.
static void init_func_receiver_info(FuncReceiverInfo* receiver) {
    receiver->type     = NULL;
    receiver->type_len = 0;
    receiver->is_const = 0;
    nodelist_init(&receiver->type_args);
}

// Parses an optional method receiver like `(Type)`, `(const Type)`, or `(Box<T>)`.
static int parse_optional_func_receiver(Parser* parser, FuncReceiverInfo* receiver) {
    if (!check_token(parser, TOK_LPAREN)) {
        return 1;
    }

    advance_token(parser); // consume '('
    if (match_token(parser, TOK_CONST)) {
        receiver->is_const = 1;
    }

    Token recv_type = parser->current;
    consume_token(parser, TOK_IDENT, "Expected receiver type name");
    receiver->type     = copy_token_string(&recv_type);
    receiver->type_len = recv_type.length;

    if (match_token(parser, TOK_LT)) {
        do {
            Node* type_arg = parse_type(parser);
            if (!type_arg) {
                free(receiver->type);
                nodelist_free(&receiver->type_args);
                return 0;
            }
            nodelist_push(&receiver->type_args, type_arg);
        } while (match_token(parser, TOK_COMMA));

        consume_token(parser, TOK_GT, "Expected '>' after type arguments");
    }

    consume_token(parser, TOK_RPAREN, "Expected ')' after receiver type");
    return 1;
}

// Initializes function-declaration fields that are common to all parsed functions.
static void init_func_decl_defaults(func_decl_node* fdn, int is_public) {
    fdn->is_public                = is_public;
    fdn->is_extern                = 0;
    fdn->type_params              = NULL;
    fdn->type_param_bounds        = NULL;
    fdn->type_param_count         = 0;
    fdn->extern_name              = NULL;
    fdn->extern_name_length       = 0;
    fdn->is_varargs               = 0;
    fdn->return_type              = NULL;
    fdn->return_is_const          = 0;
    fdn->body                     = NULL;
    fdn->accessible_modules       = NULL;
    fdn->accessible_modules_count = 0;
    nodelist_init(&fdn->params);
}

// Parses generic type parameters declared directly on a function.
static void parse_func_type_params(Parser* parser, func_decl_node* fdn) {
    if (!match_token(parser, TOK_LT)) {
        return;
    }

    int capacity           = 4;
    fdn->type_params       = xmalloc(capacity * sizeof(char*));
    fdn->type_param_bounds = xmalloc(capacity * sizeof(char*));

    do {
        Token param_name = parser->current;
        consume_token(parser, TOK_IDENT, "Expected type parameter name");

        if (fdn->type_param_count >= capacity) {
            capacity *= 2;
            fdn->type_params       = xrealloc(fdn->type_params, capacity * sizeof(char*));
            fdn->type_param_bounds = xrealloc(fdn->type_param_bounds, capacity * sizeof(char*));
        }

        fdn->type_params[fdn->type_param_count] = copy_token_string(&param_name);
        if (match_token(parser, TOK_COLON)) {
            Token bound_name = parser->current;
            consume_token(parser, TOK_IDENT, "Expected trait name after ':'");
            fdn->type_param_bounds[fdn->type_param_count] = copy_token_string(&bound_name);
        } else {
            fdn->type_param_bounds[fdn->type_param_count] = NULL;
        }

        fdn->type_param_count++;
    } while (match_token(parser, TOK_COMMA));

    consume_token(parser, TOK_GT, "Expected '>' after type parameters");
}

// Parses function parameters, including optional `const` and trailing varargs.
static void parse_func_params(Parser* parser, func_decl_node* fdn) {
    if (check_token(parser, TOK_RPAREN)) {
        return;
    }

    if (check_token(parser, TOK_ELLIPSIS)) {
        fdn->is_varargs = 1;
        advance_token(parser);
        return;
    }

    do {
        if (check_token(parser, TOK_ELLIPSIS)) {
            fdn->is_varargs = 1;
            advance_token(parser);
            break;
        }

        int   param_is_const = match_token(parser, TOK_CONST);
        Token param_name     = parser->current;
        consume_token(parser, TOK_IDENT, "Expected parameter name");

        Node* param                 = node_new(NODE_PARAM, param_name.line, param_name.column);
        param->as.param.name        = copy_token_string(&param_name);
        param->as.param.name_length = param_name.length;
        param->as.param.type        = NULL;
        param->as.param.is_const    = param_is_const;

        if (match_token(parser, TOK_COLON)) {
            param->as.param.type = parse_type(parser);
        }

        nodelist_push(&fdn->params, param);
    } while (match_token(parser, TOK_COMMA));
}

// Parses an optional `->` return type and tracks `const` return modifiers.
static void parse_func_return_type(Parser* parser, func_decl_node* fdn) {
    if (!match_token(parser, TOK_ARROW)) {
        return;
    }

    if (match_token(parser, TOK_CONST)) {
        fdn->return_is_const = 1;
    }
    fdn->return_type = parse_type(parser);
}

// Copies file-level imports so this function can resolve accessible modules during checking.
static void copy_func_accessible_modules(Parser* parser, func_decl_node* fdn) {
    ModuleLoader* loader   = parser->loader;
    int           fi_count = loader ? loader->file_imports_count : 0;

    fdn->accessible_modules_count = fi_count;
    if (fi_count <= 0) {
        fdn->accessible_modules = NULL;
        return;
    }

    fdn->accessible_modules = xmalloc(fi_count * sizeof(char*));
    for (int i = 0; i < fi_count; i++) {
        fdn->accessible_modules[i] = xstrdup(loader->file_imports[i]);
    }
}

// Parses either an extern-style declaration tail or an in-file function body block.
static void parse_func_body_or_extern(Parser* parser, func_decl_node* fdn) {
    if (!check_token(parser, TOK_LBRACE)) {
        if (match_token(parser, TOK_AS)) {
            Token alias = parser->current;
            consume_token(parser, TOK_IDENT, "Expected identifier after 'as'");
            fdn->extern_name        = fdn->name;
            fdn->extern_name_length = fdn->name_length;
            fdn->name               = copy_token_string(&alias);
            fdn->name_length        = alias.length;
        }

        consume_token(parser, TOK_SEMICOLON, "Expected ';' after function declaration");
        fdn->body = NULL;
        return;
    }

    consume_token(parser, TOK_LBRACE, "Expected '{' before function body");
    fdn->body = parse_block(parser);
    copy_func_accessible_modules(parser, fdn);
}

static Node* parse_func_decl(Parser* parser, int is_public) {
    FuncReceiverInfo receiver;
    init_func_receiver_info(&receiver);
    if (!parse_optional_func_receiver(parser, &receiver)) {
        return NULL;
    }

    Token name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected function name");

    Node*           node = node_new(NODE_FUNC_DECL, name.line, name.column);
    func_decl_node* fdn  = &node->as.func_decl;
    init_func_decl_defaults(fdn, is_public);

    fdn->receiver_type      = receiver.type;
    fdn->receiver_type_len  = receiver.type_len;
    fdn->receiver_is_const  = receiver.is_const;
    fdn->receiver_type_args = receiver.type_args;
    fdn->name               = copy_token_string(&name);
    fdn->name_length        = name.length;

    parse_func_type_params(parser, fdn);

    consume_token(parser, TOK_LPAREN, "Expected '(' after function name");
    parse_func_params(parser, fdn);
    consume_token(parser, TOK_RPAREN, "Expected ')' after parameters");

    parse_func_return_type(parser, fdn);
    parse_func_body_or_extern(parser, fdn);
    return node;
}

static Node* parse_struct_decl(Parser* parser, int is_public) {
    Token name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected struct name");

    Node* node                       = node_new(NODE_STRUCT_DECL, name.line, name.column);
    node->as.struct_decl.is_public   = is_public;
    node->as.struct_decl.name        = copy_token_string(&name);
    node->as.struct_decl.name_length = name.length;
    nodelist_init(&node->as.struct_decl.fields);

    // Parse type parameters if present: struct Box<T> or struct Pair<K, V>
    node->as.struct_decl.type_params       = NULL;
    node->as.struct_decl.type_param_bounds = NULL;
    node->as.struct_decl.type_param_count  = 0;

    if (match_token(parser, TOK_LT)) {
        // Parse type parameter names
        int capacity                           = 4;
        node->as.struct_decl.type_params       = xmalloc(capacity * sizeof(char*));
        node->as.struct_decl.type_param_bounds = xmalloc(capacity * sizeof(char*));

        do {
            Token param_name = parser->current;
            consume_token(parser, TOK_IDENT, "Expected type parameter name");

            // Grow arrays if needed
            if (node->as.struct_decl.type_param_count >= capacity) {
                capacity *= 2;
                node->as.struct_decl.type_params =
                    xrealloc(node->as.struct_decl.type_params, capacity * sizeof(char*));
                node->as.struct_decl.type_param_bounds =
                    xrealloc(node->as.struct_decl.type_param_bounds, capacity * sizeof(char*));
            }

            node->as.struct_decl.type_params[node->as.struct_decl.type_param_count] =
                copy_token_string(&param_name);

            // Check for trait bound: T: TraitName
            if (match_token(parser, TOK_COLON)) {
                Token bound_name = parser->current;
                consume_token(parser, TOK_IDENT, "Expected trait name after ':'");
                node->as.struct_decl.type_param_bounds[node->as.struct_decl.type_param_count] =
                    copy_token_string(&bound_name);
            } else {
                node->as.struct_decl.type_param_bounds[node->as.struct_decl.type_param_count] =
                    NULL;
            }

            node->as.struct_decl.type_param_count++;
        } while (match_token(parser, TOK_COMMA));

        consume_token(parser, TOK_GT, "Expected '>' after type parameters");
    }

    consume_token(parser, TOK_LBRACE, "Expected '{' after struct name");

    while (!check_token(parser, TOK_RBRACE) && !check_token(parser, TOK_EOF)) {
        int field_is_private = 0;
        if (match_token(parser, TOK_PRIVATE)) {
            field_is_private = 1;
        }

        int field_is_const = 0;
        if (match_token(parser, TOK_CONST)) {
            field_is_const = 1;
        }

        Token field_name = parser->current;
        consume_token(parser, TOK_IDENT, "Expected field name");

        Node* field                 = node_new(NODE_FIELD, field_name.line, field_name.column);
        field->as.field.name        = copy_token_string(&field_name);
        field->as.field.name_length = field_name.length;
        field->as.field.is_const    = field_is_const;
        field->as.field.is_private  = field_is_private;

        consume_token(parser, TOK_COLON, "Expected ':' after field name");
        field->as.field.type = parse_type(parser);

        if (!check_token(parser, TOK_RBRACE)) {
            consume_token(parser, TOK_COMMA, "Expected ',' or '}' after field");
        } else {
            match_token(parser, TOK_COMMA); // Allow trailing comma
        }

        nodelist_push(&node->as.struct_decl.fields, field);
    }

    consume_token(parser, TOK_RBRACE, "Expected '}' after struct fields");
    return node;
}

static Node* parse_trait_decl(Parser* parser, int is_public) {
    Token name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected trait name");

    Node* node                      = node_new(NODE_TRAIT_DECL, name.line, name.column);
    node->as.trait_decl.is_public   = is_public;
    node->as.trait_decl.name        = copy_token_string(&name);
    node->as.trait_decl.name_length = name.length;
    nodelist_init(&node->as.trait_decl.methods);

    consume_token(parser, TOK_LBRACE, "Expected '{' after trait name");

    while (!check_token(parser, TOK_RBRACE) && !check_token(parser, TOK_EOF)) {
        // Accept 'const func' or 'func' in trait declarations
        int method_is_const = 0;
        if (check_token(parser, TOK_CONST)) {
            advance_token(parser); // consume 'const'
            method_is_const = 1;
        }

        if (!match_token(parser, TOK_FUNC)) {
            parse_error(parser, "Expected 'func' in trait declaration");
            return NULL;
        }
        Node* method = parse_func_decl(parser, 0);
        if (method) {
            method->as.func_decl.receiver_is_const = method_is_const;
            nodelist_push(&node->as.trait_decl.methods, method);
        }
    }

    consume_token(parser, TOK_RBRACE, "Expected '}' after trait methods");
    return node;
}

static Node* parse_impl_decl(Parser* parser) {
    Token first_name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected name after 'impl'");

    Node* node = node_new(NODE_IMPL_DECL, first_name.line, first_name.column);
    nodelist_init(&node->as.impl_decl.type_args);
    nodelist_init(&node->as.impl_decl.methods);

    if (check_token(parser, TOK_FOR)) {
        // Trait impl: impl Trait for Type { ... }
        advance_token(parser); // consume 'for'
        Token type_name = parser->current;
        consume_token(parser, TOK_IDENT, "Expected type name after 'for'");
        node->as.impl_decl.trait_name        = copy_token_string(&first_name);
        node->as.impl_decl.trait_name_length = first_name.length;
        node->as.impl_decl.type_name         = copy_token_string(&type_name);
        node->as.impl_decl.type_name_length  = type_name.length;
    } else {
        // Inherent impl: impl Type { ... } or impl Type<T> { ... }
        node->as.impl_decl.trait_name        = NULL;
        node->as.impl_decl.trait_name_length = 0;
        node->as.impl_decl.type_name         = copy_token_string(&first_name);
        node->as.impl_decl.type_name_length  = first_name.length;
    }

    // Parse optional type args: impl Drop for Box<T> { ... } or impl Box<T> { ... }
    if (match_token(parser, TOK_LT)) {
        do {
            Node* type_arg = parse_type(parser);
            if (!type_arg) {
                node_free(node);
                return NULL;
            }
            nodelist_push(&node->as.impl_decl.type_args, type_arg);
        } while (match_token(parser, TOK_COMMA));
        consume_token(parser, TOK_GT, "Expected '>' after type arguments");
    }

    consume_token(parser, TOK_LBRACE, "Expected '{' after type name in impl block");

    while (!check_token(parser, TOK_RBRACE) && !check_token(parser, TOK_EOF)) {
        // Parse optional visibility modifier (default: public)
        int method_is_public = 1;
        if (match_token(parser, TOK_PUBLIC)) {
            method_is_public = 1;
        } else if (match_token(parser, TOK_PRIVATE)) {
            method_is_public = 0;
        }

        // Check for 'const func' (immutable receiver) or 'func' (mutable receiver)
        int method_is_const = 0;
        if (check_token(parser, TOK_CONST)) {
            // Peek ahead: if next is 'func', this is 'const func' (const receiver method)
            // If next is something else, it's an error
            advance_token(parser); // consume 'const'
            method_is_const = 1;
        }

        if (!match_token(parser, TOK_FUNC)) {
            parse_error(parser, "Expected 'func' in impl block");
            return NULL;
        }

        // Parse the function without a receiver (no `(Type)` prefix)
        Node* method = parse_func_decl(parser, method_is_public);
        if (method) {
            // Fill in receiver from the impl block context
            func_decl_node* fdn    = &method->as.func_decl;
            fdn->receiver_type     = xstrdup(node->as.impl_decl.type_name);
            fdn->receiver_type_len = node->as.impl_decl.type_name_length;
            fdn->receiver_is_const = method_is_const;

            // Copy type args from impl decl to receiver
            nodelist_init(&fdn->receiver_type_args);
            for (int i = 0; i < node->as.impl_decl.type_args.count; i++) {
                nodelist_push(&fdn->receiver_type_args, node->as.impl_decl.type_args.nodes[i]);
            }

            nodelist_push(&node->as.impl_decl.methods, method);
        }
    }

    consume_token(parser, TOK_RBRACE, "Expected '}' after impl methods");
    return node;
}

// Initializes a freshly parsed enum declaration node with its default fields.
static Node* create_enum_decl_node(Token* name, int is_public) {
    Node* node                     = node_new(NODE_ENUM_DECL, name->line, name->column);
    node->as.enum_decl.is_public   = is_public;
    node->as.enum_decl.name        = copy_token_string(name);
    node->as.enum_decl.name_length = name->length;
    nodelist_init(&node->as.enum_decl.values);
    node->as.enum_decl.type_params       = NULL;
    node->as.enum_decl.type_param_bounds = NULL;
    node->as.enum_decl.type_param_count  = 0;
    return node;
}

// Parses optional generic parameters for enum declarations like `enum Result<T, E>`.
static void parse_enum_type_params(Parser* parser, Node* node) {
    if (!match_token(parser, TOK_LT)) {
        return;
    }

    int capacity                         = 4;
    node->as.enum_decl.type_params       = xmalloc(capacity * sizeof(char*));
    node->as.enum_decl.type_param_bounds = xmalloc(capacity * sizeof(char*));

    do {
        Token param_name = parser->current;
        consume_token(parser, TOK_IDENT, "Expected type parameter name");

        if (node->as.enum_decl.type_param_count >= capacity) {
            capacity *= 2;
            node->as.enum_decl.type_params =
                xrealloc(node->as.enum_decl.type_params, capacity * sizeof(char*));
            node->as.enum_decl.type_param_bounds =
                xrealloc(node->as.enum_decl.type_param_bounds, capacity * sizeof(char*));
        }

        node->as.enum_decl.type_params[node->as.enum_decl.type_param_count] =
            copy_token_string(&param_name);
        if (match_token(parser, TOK_COLON)) {
            Token bound_name = parser->current;
            consume_token(parser, TOK_IDENT, "Expected trait name after ':'");
            node->as.enum_decl.type_param_bounds[node->as.enum_decl.type_param_count] =
                copy_token_string(&bound_name);
        } else {
            node->as.enum_decl.type_param_bounds[node->as.enum_decl.type_param_count] = NULL;
        }

        node->as.enum_decl.type_param_count++;
    } while (match_token(parser, TOK_COMMA));

    consume_token(parser, TOK_GT, "Expected '>' after type parameters");
}

// Parses the numeric base and literal digits for enum explicit-value integer tokens.
static int parse_enum_explicit_value_base(const Token* token, const char** start) {
    *start   = token->start;
    int base = 10;

    if (token->length > 2 && (*start)[0] == '0') {
        if ((*start)[1] == 'x' || (*start)[1] == 'X') {
            base = 16;
            *start += 2;
        } else if ((*start)[1] == 'b' || (*start)[1] == 'B') {
            base = 2;
            *start += 2;
        } else if ((*start)[1] == 'o' || (*start)[1] == 'O') {
            base = 8;
            *start += 2;
        }
    }

    return base;
}

// Parses optional payload types for an enum variant like `Some(T)` or `Pair(K, V)`.
static int parse_enum_variant_payload_types(Parser* parser, Node* value) {
    if (!match_token(parser, TOK_LPAREN)) {
        return 1;
    }

    while (!check_token(parser, TOK_RPAREN) && !check_token(parser, TOK_EOF)) {
        Node* type_node = parse_type(parser);
        if (!type_node) {
            return 0;
        }
        nodelist_push(&value->as.enum_variant.types, type_node);

        if (!check_token(parser, TOK_RPAREN)) {
            consume_token(parser, TOK_COMMA, "Expected ',' or ')' after variant type");
        }
    }

    consume_token(parser, TOK_RPAREN, "Expected ')' after variant types");
    return 1;
}

// Parses an optional explicit integer assignment for an enum variant.
static int parse_enum_variant_explicit_value(Parser* parser, Node* value) {
    if (!match_token(parser, TOK_EQ)) {
        return 1;
    }

    int   is_negative = match_token(parser, TOK_MINUS);
    Token value_token = parser->current;
    consume_token(parser, TOK_INT, "Expected integer literal after '=' in enum variant");
    if (value_token.type != TOK_INT) {
        return 0;
    }

    const char* start;
    int         base = parse_enum_explicit_value_base(&value_token, &start);
    errno            = 0;
    char* endptr;
    long  explicit_value = strtol(start, &endptr, base);
    if (errno == ERANGE) {
        parse_error_at(parser, &value_token, "Enum variant value out of range");
    }
    if (is_negative) {
        explicit_value = -explicit_value;
    }

    value->as.enum_variant.has_explicit_value = 1;
    value->as.enum_variant.explicit_value     = explicit_value;
    return 1;
}

// Parses one enum variant, including optional payload types and explicit numeric value.
static Node* parse_enum_variant(Parser* parser) {
    Token value_name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected enum value name");

    Node* value                 = node_new(NODE_ENUM_VARIANT, value_name.line, value_name.column);
    value->as.enum_variant.name = copy_token_string(&value_name);
    value->as.enum_variant.name_length = value_name.length;
    nodelist_init(&value->as.enum_variant.types);
    value->as.enum_variant.has_explicit_value = 0;
    value->as.enum_variant.explicit_value     = 0;

    if (!parse_enum_variant_payload_types(parser, value)) {
        node_free(value);
        return NULL;
    }
    if (!parse_enum_variant_explicit_value(parser, value)) {
        node_free(value);
        return NULL;
    }

    return value;
}

// Parses the comma-separated enum variant list up to the closing brace.
static int parse_enum_variants(Parser* parser, Node* node) {
    while (!check_token(parser, TOK_RBRACE) && !check_token(parser, TOK_EOF)) {
        Node* value = parse_enum_variant(parser);
        if (!value) {
            return 0;
        }

        nodelist_push(&node->as.enum_decl.values, value);
        if (!check_token(parser, TOK_RBRACE)) {
            consume_token(parser, TOK_COMMA, "Expected ',' or '}' after enum value");
        } else {
            match_token(parser, TOK_COMMA); // Allow trailing comma
        }
    }

    return 1;
}

static Node* parse_enum_decl(Parser* parser, int is_public) {
    Token name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected enum name");

    Node* node = create_enum_decl_node(&name, is_public);
    parse_enum_type_params(parser, node);

    consume_token(parser, TOK_LBRACE, "Expected '{' after enum name");
    if (!parse_enum_variants(parser, node)) {
        return NULL;
    }

    consume_token(parser, TOK_RBRACE, "Expected '}' after enum values");
    return node;
}

static Node* parse_type_alias(Parser* parser, int is_public) {
    Token name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected type alias name");

    Node* node                      = node_new(NODE_TYPE_ALIAS, name.line, name.column);
    node->as.type_alias.is_public   = is_public;
    node->as.type_alias.name        = copy_token_string(&name);
    node->as.type_alias.name_length = name.length;

    // Parse optional type parameters: type StringMap<V> = ...
    node->as.type_alias.type_params       = NULL;
    node->as.type_alias.type_param_bounds = NULL;
    node->as.type_alias.type_param_count  = 0;

    if (match_token(parser, TOK_LT)) {
        int capacity                          = 4;
        node->as.type_alias.type_params       = xmalloc(capacity * sizeof(char*));
        node->as.type_alias.type_param_bounds = xmalloc(capacity * sizeof(char*));

        do {
            Token param_name = parser->current;
            consume_token(parser, TOK_IDENT, "Expected type parameter name");

            if (node->as.type_alias.type_param_count >= capacity) {
                capacity *= 2;
                node->as.type_alias.type_params =
                    xrealloc(node->as.type_alias.type_params, capacity * sizeof(char*));
                node->as.type_alias.type_param_bounds =
                    xrealloc(node->as.type_alias.type_param_bounds, capacity * sizeof(char*));
            }

            node->as.type_alias.type_params[node->as.type_alias.type_param_count] =
                copy_token_string(&param_name);

            // Check for trait bound: V: TraitName
            if (match_token(parser, TOK_COLON)) {
                Token bound_name = parser->current;
                consume_token(parser, TOK_IDENT, "Expected trait name after ':'");
                node->as.type_alias.type_param_bounds[node->as.type_alias.type_param_count] =
                    copy_token_string(&bound_name);
            } else {
                node->as.type_alias.type_param_bounds[node->as.type_alias.type_param_count] = NULL;
            }

            node->as.type_alias.type_param_count++;
        } while (match_token(parser, TOK_COMMA));

        consume_token(parser, TOK_GT, "Expected '>' after type parameters");
    }

    consume_token(parser, TOK_EQ, "Expected '=' after type alias name");
    node->as.type_alias.target_type = parse_type(parser);
    consume_token(parser, TOK_SEMICOLON, "Expected ';' after type alias");
    return node;
}

static Node* parse_extern_decls(Parser* parser, int is_public) {
    Token module_name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected module name string after 'extern'");

    Node* node = node_new(NODE_EXTERN_MODULE, module_name.line, module_name.column);
    node->as.extern_module.module_name        = copy_token_string(&module_name);
    node->as.extern_module.module_name_length = module_name.length;
    nodelist_init(&node->as.extern_module.decls);
    consume_token(parser, TOK_LBRACE, "Expected '{' after extern module name");

    while (!check_token(parser, TOK_RBRACE) && !check_token(parser, TOK_EOF)) {
        // Parse optional function-level visibility (overrides block default)
        int func_is_public = is_public;
        if (match_token(parser, TOK_PUBLIC)) {
            func_is_public = 1;
        } else if (match_token(parser, TOK_PRIVATE)) {
            func_is_public = 0;
        }

        if (!match_token(parser, TOK_FUNC)) {
            parse_error(parser, "Expected 'func' in extern block");
            return NULL;
        }

        Node* func_node = parse_func_decl(parser, func_is_public);
        if (!func_node) {
            return NULL;
        }
        func_node->as.func_decl.is_extern = 1;
        nodelist_push(&node->as.extern_module.decls, func_node);
    }

    consume_token(parser, TOK_RBRACE, "Expected '}' after extern module declarations");
    return node;
}

// ============================================================================
// Import Handling
// ============================================================================

// Parse a use statement: use module::symbol; or use module::{sym1, sym2}; or use module::*;
Node* parse_use_stmt(Parser* parser) {
    Token module_token = parser->current;
    consume_token(parser, TOK_IDENT, "Expected module name after 'use'");

    consume_token(parser, TOK_COLON_COLON, "Expected '::' after module name in use statement");

    // Allocate arrays for symbol names
    int    capacity     = 4;
    char** symbol_names = xmalloc(capacity * sizeof(char*));
    int*   name_lengths = xmalloc(capacity * sizeof(int));
    int    count        = 0;
    int    is_wildcard  = 0;

    if (match_token(parser, TOK_STAR)) {
        // Wildcard: use module::*
        is_wildcard = 1;
    } else if (match_token(parser, TOK_LBRACE)) {
        // Grouped: use module::{sym1, sym2}
        while (!check_token(parser, TOK_RBRACE) && !check_token(parser, TOK_EOF)) {
            Token sym = parser->current;
            consume_token(parser, TOK_IDENT, "Expected symbol name in use group");
            if (count >= capacity) {
                capacity *= 2;
                symbol_names = xrealloc(symbol_names, capacity * sizeof(char*));
                name_lengths = xrealloc(name_lengths, capacity * sizeof(int));
            }
            symbol_names[count] = copy_token_string(&sym);
            name_lengths[count] = sym.length;
            count++;
            if (!check_token(parser, TOK_RBRACE)) {
                consume_token(parser, TOK_COMMA, "Expected ',' or '}' in use group");
            }
        }
        consume_token(parser, TOK_RBRACE, "Expected '}' after use group");
    } else {
        // Single: use module::symbol
        Token sym = parser->current;
        consume_token(parser, TOK_IDENT, "Expected symbol name after '::'");
        symbol_names[0] = copy_token_string(&sym);
        name_lengths[0] = sym.length;
        count           = 1;
    }

    consume_token(parser, TOK_SEMICOLON, "Expected ';' after use statement");

    Node* node                    = node_new(NODE_USE_DECL, module_token.line, module_token.column);
    node->as.use_decl.module_name = copy_token_string(&module_token);
    node->as.use_decl.module_name_length  = module_token.length;
    node->as.use_decl.symbol_names        = symbol_names;
    node->as.use_decl.symbol_name_lengths = name_lengths;
    node->as.use_decl.symbol_count        = count;
    node->as.use_decl.is_wildcard         = is_wildcard;
    return node;
}

int parse_import_stmt(Parser* parser, Node* program, Node* current_module) {
    Token import_token = parser->current;

    if (parser->current.type != TOK_IDENT) {
        parse_error(parser,
                    "Expected module name after 'import' (use 'include' for relative paths)");
        return 0;
    }
    advance_token(parser);

    consume_token(parser, TOK_SEMICOLON, "Expected ';' after import statement");

    return module_loader_import(parser->loader, parser, program, current_module, import_token.start,
                                import_token.length, /*is_relative=*/0);
}

int parse_include_stmt(Parser* parser, Node* program, Node* current_module) {
    Token include_token = parser->current;

    if (parser->current.type != TOK_STRING) {
        parse_error(parser, "Expected relative path string after 'include'");
        return 0;
    }
    advance_token(parser);

    const char* path   = include_token.start + 1;
    size_t      length = include_token.length - 2;

    if (!module_loader_is_relative_path(path, length)) {
        parse_error(parser, "Include paths must be relative (start with ./ or ../)");
        return 0;
    }

    consume_token(parser, TOK_SEMICOLON, "Expected ';' after include statement");

    return module_loader_import(parser->loader, parser, program, current_module, path, length,
                                /*is_relative=*/1);
}

// ============================================================================
// Test Declaration Parsing
// ============================================================================

static Node* parse_test_decl(Parser* parser) {
    int line = parser->previous.line;
    int col  = parser->previous.column;

    if (parser->current.type != TOK_STRING) {
        parse_error(parser, "Expected test name string after 'test'");
        return NULL;
    }
    advance_token(parser);

    // Extract the string contents (without quotes)
    const char* name_start  = parser->previous.start + 1;       // skip opening quote
    int         name_length = (int)parser->previous.length - 2; // exclude both quotes

    Node* node              = node_new(NODE_TEST_DECL, line, col);
    node->as.test_decl.name = xmalloc(name_length + 1);
    memcpy(node->as.test_decl.name, name_start, name_length);
    node->as.test_decl.name[name_length] = '\0';
    node->as.test_decl.name_length       = name_length;

    consume_token(parser, TOK_LBRACE, "Expected '{' after test name");
    node->as.test_decl.body = parse_block(parser);

    return node;
}

// ============================================================================
// Top-level Declaration Parsing
// ============================================================================

Node* parse_declaration(Parser* parser) {
    // test blocks have no visibility modifier
    if (check_token(parser, TOK_TEST)) {
        advance_token(parser);
        return parse_test_decl(parser);
    }

    int is_public      = match_token(parser, TOK_PUBLIC);
    int has_visibility = is_public;

    if (!is_public) {
        has_visibility = match_token(parser, TOK_PRIVATE);
        is_public      = !has_visibility; // default to public if no modifier
    }

    // Error if visibility modifier used with test
    if (has_visibility && check_token(parser, TOK_TEST)) {
        parse_error(parser, "Test blocks cannot have visibility modifiers");
        return NULL;
    }

    if (match_token(parser, TOK_EXTERN)) {
        // Extern defaults to private when no visibility modifier
        return parse_extern_decls(parser, has_visibility ? is_public : 0);
    }
    if (match_token(parser, TOK_FUNC)) {
        return parse_func_decl(parser, is_public);
    }
    if (match_token(parser, TOK_STRUCT)) {
        return parse_struct_decl(parser, is_public);
    }
    if (match_token(parser, TOK_ENUM)) {
        return parse_enum_decl(parser, is_public);
    }
    if (match_token(parser, TOK_TRAIT)) {
        return parse_trait_decl(parser, is_public);
    }
    if (match_token(parser, TOK_TYPE)) {
        return parse_type_alias(parser, is_public);
    }
    if (match_token(parser, TOK_IMPL)) {
        return parse_impl_decl(parser);
    }
    if (match_token(parser, TOK_VAR)) {
        return parse_var_decl(parser, 0, is_public);
    }
    if (match_token(parser, TOK_CONST)) {
        return parse_var_decl(parser, 1, is_public);
    }

    parse_error(parser, "Expected declaration");
    return NULL;
}
