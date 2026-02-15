#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "parser_internal.h"

static Node* parse_func_decl(Parser* parser, int is_public) {
    // Check for method receiver: func (Type) or func (const Type) or func (Box<T>)
    // or func (Pair<i32, Box<T>>)
    char*    receiver_type     = NULL;
    int      receiver_type_len = 0;
    int      receiver_is_const = 0;
    NodeList receiver_type_args;
    nodelist_init(&receiver_type_args);

    if (check_token(parser, TOK_LPAREN)) {
        advance_token(parser); // consume '('

        // Check for 'const' modifier
        if (match_token(parser, TOK_CONST)) {
            receiver_is_const = 1;
        }

        // Expect struct type name
        Token recv_type = parser->current;
        consume_token(parser, TOK_IDENT, "Expected receiver type name");
        receiver_type     = copy_token_string(&recv_type);
        receiver_type_len = recv_type.length;

        // Check for generic type args: Box<T> or Pair<K, V> or Pair<i32, Box<T>>
        if (match_token(parser, TOK_LT)) {
            do {
                // Parse full type (can be identifier, generic type, etc.)
                Node* type_arg = parse_type(parser);
                if (!type_arg) {
                    free(receiver_type);
                    return NULL;
                }
                nodelist_push(&receiver_type_args, type_arg);
            } while (match_token(parser, TOK_COMMA));

            consume_token(parser, TOK_GT, "Expected '>' after type arguments");
        }

        consume_token(parser, TOK_RPAREN, "Expected ')' after receiver type");
    }

    Token name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected function name");

    Node*           node = node_new(NODE_FUNC_DECL, name.line, name.column);
    func_decl_node* fdn  = &node->as.func_decl;

    fdn->is_public          = is_public;
    fdn->is_extern          = 0;
    fdn->receiver_type      = receiver_type;
    fdn->receiver_type_len  = receiver_type_len;
    fdn->receiver_is_const  = receiver_is_const;
    fdn->receiver_type_args = receiver_type_args;
    fdn->name               = copy_token_string(&name);
    fdn->name_length        = name.length;
    fdn->type_params        = NULL;
    fdn->type_param_bounds  = NULL;
    fdn->type_param_count   = 0;
    fdn->extern_name        = NULL;
    fdn->extern_name_length = 0;
    fdn->is_varargs         = 0;
    fdn->return_is_const    = 0;
    nodelist_init(&fdn->params);

    // Parse type parameters for generic free functions: func identity<T>(x: T): T
    // Only for free functions (not methods with receivers)
    if (!receiver_type && match_token(parser, TOK_LT)) {
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

            // Check for trait bound: T: TraitName
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

    consume_token(parser, TOK_LPAREN, "Expected '(' after function name");

    // Parameters
    if (!check_token(parser, TOK_RPAREN)) {
        // Check for leading ... (no named params)
        if (check_token(parser, TOK_ELLIPSIS)) {
            fdn->is_varargs = 1;
            advance_token(parser);
        } else {
            do {
                // Check for ... after a comma (varargs after named params)
                if (check_token(parser, TOK_ELLIPSIS)) {
                    fdn->is_varargs = 1;
                    advance_token(parser);
                    break;
                }

                // Check for 'const' modifier
                int param_is_const = 0;
                if (match_token(parser, TOK_CONST)) {
                    param_is_const = 1;
                }

                Token param_name = parser->current;
                consume_token(parser, TOK_IDENT, "Expected parameter name");

                Node* param          = node_new(NODE_PARAM, param_name.line, param_name.column);
                param->as.param.name = copy_token_string(&param_name);
                param->as.param.name_length = param_name.length;
                param->as.param.type        = NULL;
                param->as.param.is_const    = param_is_const;

                if (match_token(parser, TOK_COLON)) {
                    param->as.param.type = parse_type(parser);
                }

                nodelist_push(&fdn->params, param);
            } while (match_token(parser, TOK_COMMA));
        }
    }

    consume_token(parser, TOK_RPAREN, "Expected ')' after parameters");

    // Return type
    fdn->return_type = NULL;
    if (match_token(parser, TOK_COLON)) {
        if (match_token(parser, TOK_CONST)) {
            fdn->return_is_const = 1;
        }
        fdn->return_type = parse_type(parser);
    }

    // Body
    if (check_token(parser, TOK_LBRACE) == 0) {
        // Check for 'as <alias>' renaming (extern functions only)
        if (match_token(parser, TOK_AS)) {
            Token alias = parser->current;
            consume_token(parser, TOK_IDENT, "Expected identifier after 'as'");
            fdn->extern_name        = fdn->name;
            fdn->extern_name_length = fdn->name_length;
            fdn->name               = copy_token_string(&alias);
            fdn->name_length        = alias.length;
        }
        consume_token(parser, TOK_SEMICOLON, "Expected ';' after function declaration");
        // extern function declaration
        fdn->body = NULL;
        return node;
    }

    consume_token(parser, TOK_LBRACE, "Expected '{' before function body");
    fdn->body = parse_block(parser);

    // Copy current file's imports to accessible_modules
    // This determines which library modules this function can access
    ModuleLoader* loader          = parser->loader;
    int           fi_count        = loader ? loader->file_imports_count : 0;
    fdn->accessible_modules_count = fi_count;
    if (fi_count > 0) {
        fdn->accessible_modules = xmalloc(fi_count * sizeof(char*));
        for (int i = 0; i < fi_count; i++) {
            fdn->accessible_modules[i] = xstrdup(loader->file_imports[i]);
        }
    } else {
        fdn->accessible_modules = NULL;
    }

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
        Node* method = parse_func_decl(parser, 0);
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

static Node* parse_enum_decl(Parser* parser, int is_public) {
    Token name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected enum name");

    Node* node                     = node_new(NODE_ENUM_DECL, name.line, name.column);
    node->as.enum_decl.is_public   = is_public;
    node->as.enum_decl.name        = copy_token_string(&name);
    node->as.enum_decl.name_length = name.length;
    nodelist_init(&node->as.enum_decl.values);

    // Parse type parameters if present: enum Option<T> or enum Result<T, E>
    node->as.enum_decl.type_params       = NULL;
    node->as.enum_decl.type_param_bounds = NULL;
    node->as.enum_decl.type_param_count  = 0;

    if (match_token(parser, TOK_LT)) {
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

    consume_token(parser, TOK_LBRACE, "Expected '{' after enum name");
    while (!check_token(parser, TOK_RBRACE) && !check_token(parser, TOK_EOF)) {
        Token value_name = parser->current;
        consume_token(parser, TOK_IDENT, "Expected enum value name");

        Node* value = node_new(NODE_ENUM_VARIANT, value_name.line, value_name.column);
        value->as.enum_variant.name        = copy_token_string(&value_name);
        value->as.enum_variant.name_length = value_name.length;
        nodelist_init(&value->as.enum_variant.types);
        value->as.enum_variant.has_explicit_value = 0;
        value->as.enum_variant.explicit_value     = 0;

        // Check for payload types: VariantName(Type1, Type2, ...)
        if (match_token(parser, TOK_LPAREN)) {
            while (!check_token(parser, TOK_RPAREN) && !check_token(parser, TOK_EOF)) {
                Node* type_node = parse_type(parser);
                if (!type_node)
                    return NULL;
                nodelist_push(&value->as.enum_variant.types, type_node);
                if (!check_token(parser, TOK_RPAREN)) {
                    consume_token(parser, TOK_COMMA, "Expected ',' or ')' after variant type");
                }
            }
            consume_token(parser, TOK_RPAREN, "Expected ')' after variant types");
        }
        // Optional explicit integer value: VariantName = 43
        if (match_token(parser, TOK_EQ)) {
            int   is_negative = match_token(parser, TOK_MINUS);
            Token value_token = parser->current;
            consume_token(parser, TOK_INT, "Expected integer literal after '=' in enum variant");
            if (value_token.type != TOK_INT)
                return NULL;

            const char* start = value_token.start;
            int         base  = 10;
            if (value_token.length > 2) {
                if (start[0] == '0' && (start[1] == 'x' || start[1] == 'X')) {
                    base = 16;
                    start += 2;
                } else if (start[0] == '0' && (start[1] == 'b' || start[1] == 'B')) {
                    base = 2;
                    start += 2;
                } else if (start[0] == '0' && (start[1] == 'o' || start[1] == 'O')) {
                    base = 8;
                    start += 2;
                }
            }

            errno = 0;
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
        }

        nodelist_push(&node->as.enum_decl.values, value);

        if (!check_token(parser, TOK_RBRACE)) {
            consume_token(parser, TOK_COMMA, "Expected ',' or '}' after enum value");
        } else {
            match_token(parser, TOK_COMMA); // Allow trailing comma
        }
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

// Parse a use statement: use module.symbol; or use module.{sym1, sym2};
Node* parse_use_stmt(Parser* parser) {
    Token module_token = parser->current;
    consume_token(parser, TOK_IDENT, "Expected module name after 'use'");

    consume_token(parser, TOK_DOT, "Expected '.' after module name in use statement");

    // Allocate arrays for symbol names
    int    capacity     = 4;
    char** symbol_names = xmalloc(capacity * sizeof(char*));
    int*   name_lengths = xmalloc(capacity * sizeof(int));
    int    count        = 0;

    if (match_token(parser, TOK_LBRACE)) {
        // Grouped: use module.{sym1, sym2}
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
        // Single: use module.symbol
        Token sym = parser->current;
        consume_token(parser, TOK_IDENT, "Expected symbol name after '.'");
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
    return node;
}

int parse_import_stmt(Parser* parser, Node* program, Node* current_module) {
    Token       import_token = parser->current;
    const char* module_name;
    size_t      module_length;
    int         is_relative = 0;

    if (parser->current.type == TOK_STRING) {
        advance_token(parser);
        module_name   = import_token.start + 1;
        module_length = import_token.length - 2;
        is_relative   = module_loader_is_relative_path(module_name, module_length);
        if (!is_relative) {
            parse_error(parser, "String imports must be relative paths (start with ./ or ../)");
            return 0;
        }
    } else if (parser->current.type == TOK_IDENT) {
        advance_token(parser);
        module_name   = import_token.start;
        module_length = import_token.length;
    } else {
        parse_error(parser, "Expected module name or path after 'import'");
        return 0;
    }

    consume_token(parser, TOK_SEMICOLON, "Expected ';' after import statement");

    return module_loader_import(parser->loader, parser, program, current_module, module_name,
                                module_length, is_relative);
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
