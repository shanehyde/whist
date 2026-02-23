#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "codegen_internal.h"
#include "sem_info.h"
#include "types.h"
#include "vec.h"

// Emit indentation spaces (4 per level) at the current indent depth
void emit_indent(CodeGen* gen) {
    for (int i = 0; i < gen->out.indent; i++) {
        fprintf(gen->out.file, "    ");
    }
}

// Emit formatted text to the output stream
void emit(CodeGen* gen, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(gen->out.file, fmt, args);
    va_end(args);
}

// Push a deferred statement onto the defer stack for later LIFO emission
void defer_push(CodeGen* gen, Node* node) {
    VEC_GROW(gen->defer.stack, gen->defer.count, gen->defer.capacity);
    gen->defer.stack[gen->defer.count++] = node;
}

// Reset the defer stack (called at function boundaries)
void defer_clear(CodeGen* gen) {
    gen->defer.count = 0;
}

int codegen_return_type_is_void(Node* return_type) {
    return !return_type ||
           (return_type->type == NODE_IDENT && strcmp(return_type->as.ident.name, "void") == 0);
}

void emit_func_return_type(CodeGen* gen, func_decl_node* fdn) {
    if (fdn->return_is_const && !codegen_return_type_is_void(fdn->return_type)) {
        emit(gen, "const ");
    }
    emit_type(gen, fdn->return_type);
}

int codegen_find_tuple_type_index(CodeGen* gen, Type* tuple_type) {
    if (!tuple_type) {
        return -1;
    }
    for (int i = 0; i < gen->tuple_type_count; i++) {
        if (type_equals(gen->tuple_types[i], tuple_type)) {
            return i;
        }
    }
    return -1;
}

const char* codegen_enum_value_resolved_name(CodeGen* gen, Node* enum_value) {
    const char* name = sem_info_get_enum_value_resolved_name(gen->checker.sem, enum_value,
                                                             enum_value->as.enum_value.enum_name);
    return name ? name : "";
}

int codegen_enum_value_resolved_name_length(CodeGen* gen, Node* enum_value) {
    const char* name = sem_info_get_enum_value_resolved_name(gen->checker.sem, enum_value,
                                                             enum_value->as.enum_value.enum_name);
    return name ? (int)strlen(name) : 0;
}

// Extract additional type bindings from matching a method's receiver pattern against concrete args
// For example, matching <i32, Box<T>> against [i32, Box_i64] extracts T=i64
// Returns 1 on match (and fills out_* params), 0 on no match
int codegen_extract_method_bindings(NodeList* pattern_args, Type** concrete_args, int arg_count,
                                    char*** out_params, Type*** out_args, int* out_count) {
    int    capacity = 4;
    int    count    = 0;
    char** params   = xmalloc(capacity * sizeof(char*));
    Type** args     = xmalloc(capacity * sizeof(Type*));

    for (int i = 0; i < arg_count; i++) {
        Node* pattern  = pattern_args->nodes[i];
        Type* concrete = concrete_args[i];

        if (pattern->type == NODE_IDENT) {
            const char* name = pattern->as.ident.name;
            // If it's a type variable, bind it
            if (codegen_is_type_variable(name)) {
                if (count >= capacity) {
                    capacity *= 2;
                    params = xrealloc(params, capacity * sizeof(char*));
                    args   = xrealloc(args, capacity * sizeof(Type*));
                }
                params[count] = xstrdup(name);
                args[count]   = concrete;
                count++;
            }
            // If it's a concrete type, check it matches (for partial specialization)
            // For now, we trust that the checker validated this
        } else if (pattern->type == NODE_GENERIC_TYPE) {
            // Pattern is like Box<T>, concrete is like Box_i64
            if (concrete->kind != TYPE_STRUCT)
                continue;

            const char* pattern_base  = pattern->as.generic_type.base_name;
            const char* concrete_name = concrete->as.struc.name;

            // Check the base matches
            size_t base_len = strlen(pattern_base);
            if (strncmp(concrete_name, pattern_base, base_len) != 0 ||
                concrete_name[base_len] != '_') {
                for (int k = 0; k < count; k++)
                    free(params[k]);
                free(params);
                free(args);
                return 0;
            }

            // For single type arg, extract the binding
            if (pattern->as.generic_type.type_args.count == 1) {
                Node* nested = pattern->as.generic_type.type_args.nodes[0];
                if (nested->type == NODE_IDENT && codegen_is_type_variable(nested->as.ident.name)) {
                    // Extract type from mangled name suffix
                    const char* suffix    = concrete_name + base_len + 1;
                    Type*       extracted = type_builtin_from_name(suffix);
                    if (!extracted) {
                        // Assume it's a struct type
                        extracted = type_struct(suffix);
                    }

                    if (count >= capacity) {
                        capacity *= 2;
                        params = xrealloc(params, capacity * sizeof(char*));
                        args   = xrealloc(args, capacity * sizeof(Type*));
                    }
                    params[count] = xstrdup(nested->as.ident.name);
                    args[count]   = extracted;
                    count++;
                }
            }
        }
    }

    *out_params = params;
    *out_args   = args;
    *out_count  = count;
    return 1;
}

// Helper to emit function name with appropriate prefix (method or module)
// For methods: emits "StructName_funcname("
// For library functions: emits "module_funcname("
// For main module functions: emits "funcname("
void emit_function_name(CodeGen* gen, const char* func_name, const char* receiver_type,
                        const char* module_name) {
    if (receiver_type != NULL) {
        // Method: prefix with struct name
        emit(gen, " %s_%s(", receiver_type, func_name);
    } else if (module_name == NULL && strcmp(func_name, "main") == 0) {
        // User main is lowered to an internal symbol; wrapper C main is emitted separately.
        emit(gen, " __w0_user_main(");
    } else if (module_name != NULL) {
        // Library function: prefix with module name
        emit(gen, " %s_%s(", module_name, func_name);
    } else {
        // Main module function: no prefix
        emit(gen, " %s(", func_name);
    }
}

static Node* find_alias_target(CodeGen* gen, const char* name) {
    for (int i = 0; i < gen->aliases.type_count; i++) {
        if (strcmp(gen->aliases.types[i], name) == 0) {
            return gen->aliases.type_targets[i];
        }
    }
    return NULL;
}

static void emit_ident_type(CodeGen* gen, Node* type_node) {
    const char* name = type_node->as.ident.name;

    Type* resolved = subst_lookup(gen, name);
    if (resolved) {
        emit_resolved_type(gen, resolved);
        return;
    }

    const char* c_type = type_c_name(name);
    if (c_type) {
        emit(gen, "%s", c_type);
        return;
    }

    if (is_enum_type_name(gen, name)) {
        emit(gen, "%s", name);
        return;
    }

    Node* alias_target = find_alias_target(gen, name);
    if (alias_target) {
        emit_type(gen, alias_target);
        return;
    }

    emit(gen, "%s*", name);
}

static void emit_vec_or_span_type_arg(CodeGen* gen, Node* arg) {
    if (arg->type == NODE_IDENT) {
        const char* arg_name = arg->as.ident.name;
        Type*       resolved = subst_lookup(gen, arg_name);
        if (resolved) {
            emit(gen, "%s", type_mangle_name(resolved));
        } else {
            emit(gen, "%s", arg_name);
        }
        return;
    }

    if (arg->type == NODE_GENERIC_TYPE) {
        // Nested generic like Vec<HashEntry<V>>.
        char* mangled = build_mangled_name_from_generic_node(gen, arg);
        emit(gen, "%s", mangled);
        free(mangled);
    }
}

static void emit_regular_generic_type(CodeGen* gen, Node* type_node) {
    char* mangled = build_mangled_name_from_generic_node(gen, type_node);
    emit(gen, "%s", mangled);
    if (!is_enum_type_name(gen, mangled)) {
        emit(gen, "*");
    }
    free(mangled);
}

// Emit an array type annotation as either sized array syntax or pointer syntax.
static void emit_array_type(CodeGen* gen, Node* type_node) {
    emit_type(gen, type_node->as.array_type.elem_type);
    if (type_node->as.array_type.size) {
        emit(gen, "[");
        emit_expr(gen, type_node->as.array_type.size);
        emit(gen, "]");
    } else {
        emit(gen, "*");
    }
}

// Return the registered tuple typedef index for a tuple type node, or -1 if missing.
static int find_tuple_type_index(CodeGen* gen, Node* tuple_type_node) {
    Type* tuple = type_from_node(tuple_type_node);
    return codegen_find_tuple_type_index(gen, tuple);
}

// Emit an inline struct form for a tuple type node.
static void emit_inline_tuple_type(CodeGen* gen, Node* tuple_type_node) {
    emit(gen, "struct { ");
    for (int i = 0; i < tuple_type_node->as.tuple_type.elem_types.count; i++) {
        emit_type(gen, tuple_type_node->as.tuple_type.elem_types.nodes[i]);
        emit(gen, " _%d; ", i);
    }
    emit(gen, "}");
}

// Emit a tuple type annotation as either a typedef name or inline struct fallback.
static void emit_tuple_type(CodeGen* gen, Node* type_node) {
    int idx = find_tuple_type_index(gen, type_node);
    if (idx >= 0) {
        emit(gen, "__tuple_t%d", idx);
        return;
    }
    emit_inline_tuple_type(gen, type_node);
}

// Emit a generic type annotation, handling Vec/Span special cases.
static void emit_generic_type(CodeGen* gen, Node* type_node) {
    const char* base    = type_node->as.generic_type.base_name;
    int         is_span = (strcmp(base, "Span") == 0);
    int         is_vec  = (strcmp(base, "Vec") == 0);
    int         is_box  = (strcmp(base, "Box") == 0);

    if (is_vec || is_span) {
        emit(gen, "%s", is_vec ? "__Vec_" : "__Span_");
        emit_vec_or_span_type_arg(gen, type_node->as.generic_type.type_args.nodes[0]);
        if (is_vec)
            emit(gen, "*"); // Vec is RC-managed pointer
        return;
    }

    if (is_box && gen->checker.box_count > 0) {
        // Only use builtin Box emission if checker produced BoxInstances
        // (user-defined Box<T> structs go through regular generic path)
        emit(gen, "__Box_");
        emit_vec_or_span_type_arg(gen, type_node->as.generic_type.type_args.nodes[0]);
        emit(gen, "*"); // Box is RC-managed pointer
        return;
    }

    emit_regular_generic_type(gen, type_node);
}

// Emit an anonymous function pointer type annotation.
static void emit_func_type_anon(CodeGen* gen, Node* type_node) {
    if (type_node->as.func_type.return_type)
        emit_type(gen, type_node->as.func_type.return_type);
    else
        emit(gen, "void");
    emit(gen, " (*)(");
    for (int i = 0; i < type_node->as.func_type.param_types.count; i++) {
        if (i > 0)
            emit(gen, ", ");
        emit_type(gen, type_node->as.func_type.param_types.nodes[i]);
    }
    if (type_node->as.func_type.param_types.count == 0)
        emit(gen, "void");
    emit(gen, ")");
}

// Emit a type from a type annotation node
void emit_type(CodeGen* gen, Node* type_node) {
    if (!type_node) {
        emit(gen, "void");
        return;
    }

    switch (type_node->type) {
    case NODE_IDENT:
        emit_ident_type(gen, type_node);
        break;
    case NODE_UNARY:
        // Pointer types no longer supported in the language
        emit(gen, "/* pointer types not supported */");
        break;
    case NODE_ARRAY_TYPE:
        emit_array_type(gen, type_node);
        break;
    case NODE_TUPLE_TYPE:
        emit_tuple_type(gen, type_node);
        break;
    case NODE_GENERIC_TYPE:
        emit_generic_type(gen, type_node);
        break;
    case NODE_FUNC_TYPE:
        emit_func_type_anon(gen, type_node);
        break;
    default:
        emit(gen, "/* unknown type */");
        break;
    }
}

// Emit a resolved Type* (used for inferred types like tuples)
static const char* resolved_builtin_c_type(TypeKind kind) {
    switch (kind) {
    case TYPE_VOID:
        return "void";
    case TYPE_BOOL:
        return "bool";
    case TYPE_INT64:
        return "int64_t";
    case TYPE_INT8:
        return "int8_t";
    case TYPE_INT16:
        return "int16_t";
    case TYPE_INT32:
        return "int32_t";
    case TYPE_UINT64:
        return "uint64_t";
    case TYPE_UINT8:
        return "uint8_t";
    case TYPE_UINT16:
        return "uint16_t";
    case TYPE_UINT32:
        return "uint32_t";
    case TYPE_F32:
        return "float";
    case TYPE_F64:
        return "double";
    case TYPE_CHAR:
        return "char";
    case TYPE_STRING:
        return "const char*";
    case TYPE_VOIDPTR:
        return "void*";
    case TYPE_STRINGBUILDER:
        return "__StringBuilder*";
    default:
        return NULL;
    }
}

static void emit_resolved_tuple_type(CodeGen* gen, Type* type) {
    int idx = codegen_find_tuple_type_index(gen, type);
    if (idx >= 0) {
        emit(gen, "__tuple_t%d", idx);
        return;
    }

    // Fallback to inline struct (shouldn't happen if collection is correct)
    emit(gen, "struct { ");
    for (int i = 0; i < type->as.tuple.elem_count; i++) {
        emit_resolved_type(gen, type->as.tuple.elem_types[i]);
        emit(gen, " _%d; ", i);
    }
    emit(gen, "}");
}

static void emit_resolved_func_type(CodeGen* gen, Type* type) {
    (void)type;
    emit(gen, "__Closure");
}

void emit_resolved_type(CodeGen* gen, Type* type) {
    if (!type) {
        emit(gen, "void");
        return;
    }

    const char* builtin = resolved_builtin_c_type(type->kind);
    if (builtin) {
        emit(gen, "%s", builtin);
        return;
    }

    switch (type->kind) {
    case TYPE_ARRAY:
        emit_resolved_type(gen, type->as.array.elem);
        if (type->as.array.size >= 0) {
            emit(gen, "[%d]", type->as.array.size);
        } else {
            emit(gen, "*");
        }
        break;
    case TYPE_STRUCT:
        emit(gen, "%s*", type->as.struc.name);
        break;
    case TYPE_SPAN:
        emit(gen, "__Span_%s", type_mangle_name(type->as.span.elem));
        break;
    case TYPE_VEC:
        emit(gen, "__Vec_%s*", type_mangle_name(type->as.vec.elem));
        break;
    case TYPE_BOX:
        emit(gen, "__Box_%s*", type_mangle_name(type->as.box.elem));
        break;
    case TYPE_ENUM:
        emit(gen, "%s", type->as.enm.name);
        break;
    case TYPE_TUPLE:
        emit_resolved_tuple_type(gen, type);
        break;
    case TYPE_FUNC:
        emit_resolved_func_type(gen, type);
        break;
    default:
        emit(gen, "/* unknown type */");
        break;
    }
}

// Emit type with variable name (handles array syntax)
void emit_type_with_name(CodeGen* gen, Node* type_node, const char* name) {
    if (!type_node) {
        emit(gen, "void %s", name);
        return;
    }

    if (type_node->type == NODE_FUNC_TYPE) {
        // Closure: __Closure name
        emit(gen, "__Closure %s", name);
    } else if (type_node->type == NODE_ARRAY_TYPE && type_node->as.array_type.size) {
        // Array: T name[n]
        emit_type(gen, type_node->as.array_type.elem_type);
        emit(gen, " %s[", name);
        emit_expr(gen, type_node->as.array_type.size);
        emit(gen, "]");
    } else {
        emit_type(gen, type_node);
        emit(gen, " %s", name);
    }
}

// Map a binary operator token to its C operator string
const char* binary_op_str(TokenType op) {
    switch (op) {
    case TOK_PLUS:
        return "+";
    case TOK_MINUS:
        return "-";
    case TOK_STAR:
        return "*";
    case TOK_SLASH:
        return "/";
    case TOK_PERCENT:
        return "%";
    case TOK_AMP:
        return "&";
    case TOK_PIPE:
        return "|";
    case TOK_CARET:
        return "^";
    case TOK_LT_LT:
        return "<<";
    case TOK_GT_GT:
        return ">>";
    case TOK_EQ_EQ:
        return "==";
    case TOK_BANG_EQ:
        return "!=";
    case TOK_LT:
        return "<";
    case TOK_GT:
        return ">";
    case TOK_LT_EQ:
        return "<=";
    case TOK_GT_EQ:
        return ">=";
    case TOK_AMP_AMP:
        return "&&";
    case TOK_PIPE_PIPE:
        return "||";
    default:
        return "?";
    }
}

// Map a unary operator token to its C operator string
const char* unary_op_str(TokenType op) {
    switch (op) {
    case TOK_MINUS:
        return "-";
    case TOK_BANG:
        return "!";
    case TOK_TILDE:
        return "~";
    case TOK_AMP:
        return "&";
    case TOK_STAR:
        return "*";
    default:
        return "?";
    }
}

// Map an assignment operator token to its C operator string
const char* assign_op_str(TokenType op) {
    switch (op) {
    case TOK_EQ:
        return "=";
    case TOK_PLUS_EQ:
        return "+=";
    case TOK_MINUS_EQ:
        return "-=";
    case TOK_STAR_EQ:
        return "*=";
    case TOK_SLASH_EQ:
        return "/=";
    case TOK_PERCENT_EQ:
        return "%=";
    case TOK_AMP_EQ:
        return "&=";
    case TOK_PIPE_EQ:
        return "|=";
    case TOK_CARET_EQ:
        return "^=";
    case TOK_LT_LT_EQ:
        return "<<=";
    case TOK_GT_GT_EQ:
        return ">>=";
    default:
        return "=";
    }
}

void emit_value_match_cond(CodeGen* gen, int match_id, Node* pattern, Type* expr_type) {
    if (expr_type->kind == TYPE_STRING) {
        emit(gen, "strcmp(__match%d, ", match_id);
        emit_expr(gen, pattern);
        emit(gen, ") == 0");
    } else {
        emit(gen, "__match%d == ", match_id);
        emit_expr(gen, pattern);
    }
}

// Emit an extern module: #include directive and register function aliases/names
static void emit_extern_module(CodeGen* gen, Node* node) {
    emit(gen, "\n#include <%s.h>\n", node->as.extern_module.module_name);
    // Register extern function aliases and names
    for (int i = 0; i < node->as.extern_module.decls.count; i++) {
        Node* decl = node->as.extern_module.decls.nodes[i];
        if (decl->type == NODE_FUNC_DECL) {
            // Track all extern function names (for intra-module call prefixing)
            VEC_GROW(gen->aliases.extern_funcs, gen->aliases.extern_func_count,
                     gen->aliases.extern_func_capacity);
            gen->aliases.extern_funcs[gen->aliases.extern_func_count++] = decl->as.func_decl.name;
            // Register aliases (Whist name -> C name) for renamed externs
            if (decl->as.func_decl.extern_name) {
                VEC_GROW(gen->aliases.externs, gen->aliases.extern_count,
                         gen->aliases.extern_capacity);
                gen->aliases.externs[gen->aliases.extern_count].whist_name =
                    decl->as.func_decl.name;
                gen->aliases.externs[gen->aliases.extern_count].c_name =
                    decl->as.func_decl.extern_name;
                gen->aliases.extern_count++;
            }
        }
    }
}

static int should_skip_func_decl(CodeGen* gen, func_decl_node* fdn, int is_method) {
    // Skip body-less non-extern methods (duck-type trait declarations).
    if (fdn->body == NULL && !fdn->is_extern) {
        return 1;
    }

    // In test mode, skip user's main function.
    if (gen->test_mode && !is_method && strcmp(fdn->name, "main") == 0) {
        return 1;
    }

    // Skip generic method templates - they get instantiated separately.
    if (is_method && fdn->receiver_type_args.count > 0) {
        return 1;
    }

    // Skip generic free function templates - they get instantiated separately.
    if (!is_method && fdn->type_param_count > 0) {
        return 1;
    }

    return 0;
}

static void emit_method_self_param(CodeGen* gen, func_decl_node* fdn) {
    if (fdn->receiver_is_const && strcmp(fdn->receiver_type, "string") != 0) {
        emit(gen, "const ");
    }
    if (type_is_builtin_name(fdn->receiver_type)) {
        emit(gen, "%s self", type_c_name(fdn->receiver_type));
    } else {
        emit(gen, "%s* self", fdn->receiver_type);
    }
}

static void emit_func_params(CodeGen* gen, func_decl_node* fdn, int is_method) {
    if (is_method) {
        emit_method_self_param(gen, fdn);
        if (fdn->params.count > 0) {
            emit(gen, ", ");
        }
    }

    if (fdn->params.count == 0 && !is_method) {
        emit(gen, "void");
        return;
    }

    for (int i = 0; i < fdn->params.count; i++) {
        if (i > 0) {
            emit(gen, ", ");
        }
        Node* param = fdn->params.nodes[i];
        if (param->as.param.is_const) {
            emit(gen, "const ");
        }
        emit_type_with_name(gen, param->as.param.type, param->as.param.name);
    }
}

static int func_body_has_top_level_defer(func_decl_node* fdn) {
    if (!fdn->body) {
        return 0;
    }
    for (int i = 0; i < fdn->body->as.block.stmts.count; i++) {
        Node* stmt = fdn->body->as.block.stmts.nodes[i];
        if (stmt && stmt->type == NODE_DEFER) {
            return 1;
        }
    }
    return 0;
}

static void emit_func_body_stmts(CodeGen* gen, func_decl_node* fdn) {
    if (!fdn->body) {
        return;
    }
    for (int i = 0; i < fdn->body->as.block.stmts.count; i++) {
        emit_stmt(gen, fdn->body->as.block.stmts.nodes[i]);
    }
}

static void emit_implicit_return_rc_cleanup(CodeGen* gen, func_decl_node* fdn) {
    if (gen->rc.count <= 0 || !fdn->body) {
        return;
    }
    int   stmt_count = fdn->body->as.block.stmts.count;
    Node* last       = stmt_count > 0 ? fdn->body->as.block.stmts.nodes[stmt_count - 1] : NULL;
    if (!last || last->type != NODE_RETURN) {
        rc_cleanup_all(gen, NULL);
    }
}

static void emit_defer_cleanup_section(CodeGen* gen, int is_void) {
    if (gen->defer.count <= 0) {
        return;
    }

    emit(gen, "__cleanup:;\n");
    // Emit deferred statements in reverse order (LIFO).
    for (int i = gen->defer.count - 1; i >= 0; i--) {
        emit_stmt(gen, gen->defer.stack[i]);
    }

    emit_indent(gen);
    if (is_void) {
        emit(gen, "return;\n");
    } else {
        emit(gen, "return __ret;\n");
    }
}

// Emit a function declaration: signature, body, defer cleanup, and RC cleanup
static void emit_func_decl(CodeGen* gen, Node* node) {
    func_decl_node* fdn       = &node->as.func_decl;
    int             is_method = (fdn->receiver_type != NULL);
    if (should_skip_func_decl(gen, fdn, is_method)) {
        return;
    }

    int is_void = codegen_return_type_is_void(fdn->return_type);

    if (gen->line_directives && node->line > 0) {
        emit(gen, "#line %d \"%s\"\n", node->line, gen->source_file);
    }

    // Emit static for private functions (except main), and for all non-target module
    // functions in separate compilation mode.
    if (gen->force_static || (!fdn->is_public && strcmp(fdn->name, "main") != 0)) {
        emit(gen, "static ");
    }

    emit_func_return_type(gen, fdn);

    // Function name (mangled for methods and library functions).
    emit_function_name(gen, fdn->name, is_method ? fdn->receiver_type : NULL, gen->current_module);
    emit_func_params(gen, fdn, is_method);
    emit(gen, ") {\n");

    // Clear defer stack for this function.
    defer_clear(gen);
    gen->defer.return_type = fdn->return_type;
    int has_defers         = func_body_has_top_level_defer(fdn);

    gen->out.indent++;

    // Declare __ret if function has defers and is non-void.
    if (has_defers && !is_void) {
        emit_indent(gen);
        emit_type(gen, fdn->return_type);
        emit(gen, " __ret;\n");
    }

    // Track if we're inside an enum method body (for match(self) dereference).
    int was_in_enum_method = gen->in_enum_method;
    if (is_method && is_enum_type_name(gen, fdn->receiver_type)) {
        gen->in_enum_method = 1;
    }

    emit_func_body_stmts(gen, fdn);

    gen->in_enum_method = was_in_enum_method;
    emit_implicit_return_rc_cleanup(gen, fdn);
    emit_defer_cleanup_section(gen, is_void);

    gen->out.indent--;
    emit(gen, "}\n\n");

    // Clear defer stack and RC tracking.
    defer_clear(gen);
    rc_clear_all(gen);
    gen->defer.return_type = NULL;
}

// Emit a top-level declaration: function, struct, enum, extern, impl, or global var
void emit_decl(CodeGen* gen, Node* node) {
    if (!node)
        return;

    switch (node->type) {
    case NODE_EXTERN_MODULE:
        emit_extern_module(gen, node);
        break;

    case NODE_STRUCT_DECL:
        // Struct body typedefs are emitted in codegen_emit before __rc_dec_TypeName
        break;

    case NODE_ENUM_DECL:
        // Enum typedefs are emitted in the early pass in codegen_emit
        break;

    case NODE_TRAIT_DECL:
        // Traits produce no C code - they are purely a type-checking concept
        break;

    case NODE_TYPE_ALIAS:
        // Type aliases produce no C code - they are resolved during type checking
        break;

    case NODE_USE_DECL:
        // Use aliases are pre-collected in collect_use_aliases() before declarations
        // are emitted, so no work needed here.
        break;

    case NODE_IMPL_DECL:
        // Emit each method in the impl block as a regular function
        for (int i = 0; i < node->as.impl_decl.methods.count; i++) {
            emit_decl(gen, node->as.impl_decl.methods.nodes[i]);
        }
        break;

    case NODE_TEST_DECL:
        if (gen->test_mode) {
            emit(gen, "static void __test_%d(void) {\n", gen->test_index++);
            gen->out.indent++;
            emit_block_contents(gen, node->as.test_decl.body);
            rc_cleanup_all(gen, NULL);
            gen->out.indent--;
            emit(gen, "}\n\n");
            rc_clear_all(gen);
        }
        // In normal mode: skip entirely
        break;

    case NODE_FUNC_DECL:
        emit_func_decl(gen, node);
        break;

    case NODE_VAR_DECL: {
        // Global variable - emit #line before static to avoid "static #line" in output
        int saved_ld = gen->line_directives;
        if (gen->line_directives && node->line > 0) {
            emit(gen, "#line %d \"%s\"\n", node->line, gen->source_file);
            gen->line_directives = 0;
        }
        if (gen->force_static || !node->as.var_decl.is_public) {
            emit(gen, "static ");
        }
        emit_stmt(gen, node);
        gen->line_directives = saved_ld;
        emit(gen, "\n");
        break;
    }

    default:
        emit(gen, "/* unknown decl %d */\n", node->type);
        break;
    }
}

// =============================================================================
// Typedef Emission — bounds checks, string helpers, typedefs, forward decls
// =============================================================================

// Bounds-checking helpers are now provided by whist_runtime.h.
void emit_bounds_checks(CodeGen* gen) {
    (void)gen;
}

// String method helpers are now provided by whist_runtime.h.
void emit_string_helpers(CodeGen* gen) {
    (void)gen;
}

// Emit C struct typedefs for each collected tuple type (__tuple_tN)
void emit_tuple_typedefs(CodeGen* gen) {
    for (int i = 0; i < gen->tuple_type_count; i++) {
        emit(gen, "typedef struct { ");
        Type* tuple = gen->tuple_types[i];
        for (int j = 0; j < tuple->as.tuple.elem_count; j++) {
            emit_resolved_type(gen, tuple->as.tuple.elem_types[j]);
            emit(gen, " _%d; ", j);
        }
        emit(gen, "} __tuple_t%d;\n", i);
    }
    if (gen->tuple_type_count > 0)
        emit(gen, "\n");
}

// Emit forward typedef declarations for all structs (non-generic and instantiated)
void emit_struct_forward_decls(CodeGen* gen, Node* ast) {
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type == NODE_STRUCT_DECL) {
                // Skip generic struct templates
                if (decl->as.struct_decl.type_param_count > 0) {
                    continue;
                }
                emit(gen, "typedef struct %s %s;\n", decl->as.struct_decl.name,
                     decl->as.struct_decl.name);
            }
        }
    }
    // Forward declarations for instantiated generic structs and data enums
    for (int i = 0; i < gen->checker.instance_count; i++) {
        GenericInstance* inst = &gen->checker.instances[i];
        if (inst->type->kind == TYPE_ENUM) {
            // Forward-declare data enums so Vec/Span typedefs can reference them
            if (inst->type->as.enm.has_data) {
                emit(gen, "typedef struct %s %s;\n", inst->mangled_name, inst->mangled_name);
            }
            continue;
        }
        emit(gen, "typedef struct %s %s;\n", inst->mangled_name, inst->mangled_name);
    }
    emit(gen, "\n");
}

// Emit struct typedefs for each instantiated Span type (__Span_T)
void emit_span_typedefs(CodeGen* gen) {
    for (int i = 0; i < gen->checker.span_count; i++) {
        SpanInstance* inst = &gen->checker.spans[i];
        emit(gen, "typedef struct {\n");
        emit(gen, "    ");
        if (inst->elem_type->kind != TYPE_STRING)
            emit(gen, "const ");
        emit_resolved_type(gen, inst->elem_type);
        emit(gen, "* data;\n");
        emit(gen, "    uint64_t count;\n");
        emit(gen, "} __Span_%s;\n\n", type_mangle_name(inst->elem_type));
    }
}

// Emit struct typedefs for each instantiated Vec type (__Vec_T)
void emit_vec_typedefs(CodeGen* gen) {
    for (int i = 0; i < gen->checker.vec_count; i++) {
        VecInstance* inst       = &gen->checker.vecs[i];
        Type*        elem_type  = inst->elem_type;
        const char*  elem_tname = type_mangle_name(elem_type);

        // Vec<string> typedef is provided by whist_runtime.h
        if (elem_type->kind == TYPE_STRING)
            continue;

        emit(gen, "typedef struct {\n");
        emit(gen, "    ");
        emit_resolved_type(gen, elem_type);
        emit(gen, "* data;\n");
        emit(gen, "    int64_t count;\n");
        emit(gen, "    int64_t capacity;\n");
        emit(gen, "} __Vec_%s;\n\n", elem_tname);
    }
}

// Emit forward declarations for each instantiated Box type (__Box_T)
// These must come early so that pointer types (__Box_T*) can be used
// in enum/struct definitions before the full Box typedef is emitted.
void emit_box_forward_decls(CodeGen* gen) {
    for (int i = 0; i < gen->checker.box_count; i++) {
        BoxInstance* inst       = &gen->checker.boxes[i];
        const char*  elem_tname = type_mangle_name(inst->elem_type);
        emit(gen, "typedef struct __Box_%s __Box_%s;\n", elem_tname, elem_tname);
    }
    if (gen->checker.box_count > 0)
        emit(gen, "\n");
}

// Emit full struct definitions for each instantiated Box type (__Box_T)
// Must come after enum and struct typedefs since Box embeds its element by value.
void emit_box_typedefs(CodeGen* gen) {
    for (int i = 0; i < gen->checker.box_count; i++) {
        BoxInstance* inst       = &gen->checker.boxes[i];
        Type*        elem_type  = inst->elem_type;
        const char*  elem_tname = type_mangle_name(elem_type);

        emit(gen, "struct __Box_%s {\n", elem_tname);
        emit(gen, "    ");
        emit_resolved_type(gen, elem_type);
        emit(gen, " value;\n");
        emit(gen, "};\n\n");
    }
}

// Emit C enum/tagged-union typedefs for non-generic enum declarations
void emit_enum_typedefs(CodeGen* gen, Node* ast) {
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type != NODE_ENUM_DECL)
                continue;
            if (decl->as.enum_decl.type_param_count > 0)
                continue; // Skip generic enum templates

            const char* ename       = decl->as.enum_decl.name;
            int         value_count = decl->as.enum_decl.values.count;

            // Determine if this is a data enum (any variant has types)
            int has_data = 0;
            for (int v = 0; v < value_count; v++) {
                Node* var = decl->as.enum_decl.values.nodes[v];
                if (var->as.enum_variant.types.count > 0) {
                    has_data = 1;
                    break;
                }
            }

            if (!has_data) {
                // Simple enum: typedef enum Name { ... } Name;
                emit(gen, "typedef enum %s {\n", ename);
                gen->out.indent++;
                for (int v = 0; v < value_count; v++) {
                    Node* var = decl->as.enum_decl.values.nodes[v];
                    emit_indent(gen);
                    emit(gen, "%s_%.*s", ename, var->as.enum_variant.name_length,
                         var->as.enum_variant.name);
                    if (var->as.enum_variant.has_explicit_value) {
                        emit(gen, " = %ld", var->as.enum_variant.explicit_value);
                    }
                    if (v < value_count - 1)
                        emit(gen, ",");
                    emit(gen, "\n");
                }
                gen->out.indent--;
                emit(gen, "} %s;\n\n", ename);
            } else {
                // Data enum: tag enum + tagged union struct
                // 1. Tag enum
                emit(gen, "typedef enum %s_Tag {\n", ename);
                gen->out.indent++;
                for (int v = 0; v < value_count; v++) {
                    Node* var = decl->as.enum_decl.values.nodes[v];
                    emit_indent(gen);
                    emit(gen, "%s_%.*s", ename, var->as.enum_variant.name_length,
                         var->as.enum_variant.name);
                    if (var->as.enum_variant.has_explicit_value) {
                        emit(gen, " = %ld", var->as.enum_variant.explicit_value);
                    }
                    if (v < value_count - 1)
                        emit(gen, ",");
                    emit(gen, "\n");
                }
                gen->out.indent--;
                emit(gen, "} %s_Tag;\n\n", ename);

                // 2. Tagged union struct
                emit(gen, "typedef struct %s {\n", ename);
                gen->out.indent++;
                emit_indent(gen);
                emit(gen, "%s_Tag tag;\n", ename);
                emit_indent(gen);
                emit(gen, "union {\n");
                gen->out.indent++;
                for (int v = 0; v < value_count; v++) {
                    Node* var        = decl->as.enum_decl.values.nodes[v];
                    int   type_count = var->as.enum_variant.types.count;
                    if (type_count == 0)
                        continue;
                    emit_indent(gen);
                    emit(gen, "struct {");
                    for (int t = 0; t < type_count; t++) {
                        emit(gen, " ");
                        emit_type(gen, var->as.enum_variant.types.nodes[t]);
                        emit(gen, " f%d;", t);
                    }
                    emit(gen, " } %.*s;\n", var->as.enum_variant.name_length,
                         var->as.enum_variant.name);
                }
                gen->out.indent--;
                emit_indent(gen);
                emit(gen, "};\n");
                gen->out.indent--;
                emit(gen, "} %s;\n\n", ename);
            }
        }
    }
}

// Emit C enum/tagged-union typedefs for instantiated generic enum types
void emit_generic_enum_typedefs(CodeGen* gen, Node* ast) {
    for (int gi = 0; gi < gen->checker.instance_count; gi++) {
        GenericInstance* info = &gen->checker.instances[gi];
        if (info->type->kind != TYPE_ENUM)
            continue;
        Node* template = find_generic_enum_decl(ast, info->base_name);
        if (!template)
            continue;

        const char* ename       = info->mangled_name;
        int         value_count = template->as.enum_decl.values.count;

        // Build substitution context
        int              param_count = template->as.enum_decl.type_param_count;
        TypeSubstContext subst;
        subst.type_params = template->as.enum_decl.type_params;
        subst.type_args   = info->type_args;
        subst.count       = param_count;

        TypeSubstContext* old_subst = gen->generics.subst;
        gen->generics.subst         = &subst;

        // Determine if this is a data enum
        int has_data = 0;
        for (int v = 0; v < value_count; v++) {
            Node* var = template->as.enum_decl.values.nodes[v];
            if (var->as.enum_variant.types.count > 0) {
                has_data = 1;
                break;
            }
        }

        if (!has_data) {
            // Simple enum
            emit(gen, "typedef enum %s {\n", ename);
            gen->out.indent++;
            for (int v = 0; v < value_count; v++) {
                Node* var = template->as.enum_decl.values.nodes[v];
                emit_indent(gen);
                emit(gen, "%s_%.*s", ename, var->as.enum_variant.name_length,
                     var->as.enum_variant.name);
                if (var->as.enum_variant.has_explicit_value) {
                    emit(gen, " = %ld", var->as.enum_variant.explicit_value);
                }
                if (v < value_count - 1)
                    emit(gen, ",");
                emit(gen, "\n");
            }
            gen->out.indent--;
            emit(gen, "} %s;\n\n", ename);
        } else {
            // Data enum: tag enum + tagged union struct
            emit(gen, "typedef enum %s_Tag {\n", ename);
            gen->out.indent++;
            for (int v = 0; v < value_count; v++) {
                Node* var = template->as.enum_decl.values.nodes[v];
                emit_indent(gen);
                emit(gen, "%s_%.*s", ename, var->as.enum_variant.name_length,
                     var->as.enum_variant.name);
                if (var->as.enum_variant.has_explicit_value) {
                    emit(gen, " = %ld", var->as.enum_variant.explicit_value);
                }
                if (v < value_count - 1)
                    emit(gen, ",");
                emit(gen, "\n");
            }
            gen->out.indent--;
            emit(gen, "} %s_Tag;\n\n", ename);

            emit(gen, "typedef struct %s {\n", ename);
            gen->out.indent++;
            emit_indent(gen);
            emit(gen, "%s_Tag tag;\n", ename);
            emit_indent(gen);
            emit(gen, "union {\n");
            gen->out.indent++;
            for (int v = 0; v < value_count; v++) {
                Node* var        = template->as.enum_decl.values.nodes[v];
                int   type_count = var->as.enum_variant.types.count;
                if (type_count == 0)
                    continue;
                emit_indent(gen);
                emit(gen, "struct {");
                for (int t = 0; t < type_count; t++) {
                    emit(gen, " ");
                    emit_type(gen, var->as.enum_variant.types.nodes[t]);
                    emit(gen, " f%d;", t);
                }
                emit(gen, " } %.*s;\n", var->as.enum_variant.name_length,
                     var->as.enum_variant.name);
            }
            gen->out.indent--;
            emit_indent(gen);
            emit(gen, "};\n");
            gen->out.indent--;
            emit(gen, "} %s;\n\n", ename);
        }

        gen->generics.subst = old_subst;
    }
}

// Emit a field comparison expression for enum eq helpers.
// Resolves type substitution, then emits: StructName_eq for structs, strcmp for strings, == else.
static void emit_enum_field_eq(CodeGen* gen, Node* tnode, const char* vname, int vname_len,
                               int field_idx) {
    Node* resolved = resolve_alias(gen, tnode);

    // Check type parameter substitution first (for generic enums)
    if (resolved->type == NODE_IDENT) {
        Type* sub = subst_lookup(gen, resolved->as.ident.name);
        if (sub) {
            if (sub->kind == TYPE_STRUCT) {
                emit(gen, "%s_eq(a.%.*s.f%d, b.%.*s.f%d)", sub->as.struc.name, vname_len, vname,
                     field_idx, vname_len, vname, field_idx);
                return;
            }
            if (sub->kind == TYPE_STRING) {
                emit(gen, "(strcmp(a.%.*s.f%d, b.%.*s.f%d) == 0)", vname_len, vname, field_idx,
                     vname_len, vname, field_idx);
                return;
            }
            // Primitive or other — use ==
            emit(gen, "(a.%.*s.f%d == b.%.*s.f%d)", vname_len, vname, field_idx, vname_len, vname,
                 field_idx);
            return;
        }
    }

    // Non-substituted types
    if (is_struct_type(gen, tnode)) {
        const char* sname     = NULL;
        int         need_free = 0;
        if (resolved->type == NODE_IDENT) {
            sname = resolved->as.ident.name;
        } else if (resolved->type == NODE_GENERIC_TYPE) {
            sname     = build_mangled_name_from_generic_node(gen, resolved);
            need_free = 1;
        }
        if (sname) {
            emit(gen, "%s_eq(a.%.*s.f%d, b.%.*s.f%d)", sname, vname_len, vname, field_idx,
                 vname_len, vname, field_idx);
        }
        if (need_free)
            free((char*)sname);
    } else if (resolved->type == NODE_IDENT && strcmp(resolved->as.ident.name, "string") == 0) {
        emit(gen, "(strcmp(a.%.*s.f%d, b.%.*s.f%d) == 0)", vname_len, vname, field_idx, vname_len,
             vname, field_idx);
    } else {
        emit(gen, "(a.%.*s.f%d == b.%.*s.f%d)", vname_len, vname, field_idx, vname_len, vname,
             field_idx);
    }
}

// Emit a single __EnumName_eq function for a data enum
static void emit_single_enum_eq(CodeGen* gen, const char* ename, Node* enum_decl) {
    emit(gen, "static inline bool __%s_eq(%s a, %s b) {\n", ename, ename, ename);
    emit(gen, "    if (a.tag != b.tag) return false;\n");
    emit(gen, "    switch (a.tag) {\n");
    for (int v = 0; v < enum_decl->as.enum_decl.values.count; v++) {
        Node*       var       = enum_decl->as.enum_decl.values.nodes[v];
        int         tc        = var->as.enum_variant.types.count;
        const char* vname     = var->as.enum_variant.name;
        int         vname_len = var->as.enum_variant.name_length;

        emit(gen, "    case %s_%.*s:", ename, vname_len, vname);
        if (tc == 0) {
            emit(gen, " return true;\n");
        } else {
            emit(gen, " return ");
            for (int t = 0; t < tc; t++) {
                if (t > 0)
                    emit(gen, " && ");
                emit_enum_field_eq(gen, var->as.enum_variant.types.nodes[t], vname, vname_len, t);
            }
            emit(gen, ";\n");
        }
    }
    emit(gen, "    default: return false;\n");
    emit(gen, "    }\n");
    emit(gen, "}\n\n");
}

// Emit __EnumName_eq helpers for data enums that support equality comparison
void emit_enum_eq_helpers(CodeGen* gen, Node* ast) {
    // Non-generic data enums
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type != NODE_ENUM_DECL)
                continue;
            if (decl->as.enum_decl.type_param_count > 0)
                continue;
            const char* ename = decl->as.enum_decl.name;
            if (!enum_has_eq(gen, ename))
                continue;
            emit_single_enum_eq(gen, ename, decl);
        }
    }

    // Generic data enum instances
    for (int gi = 0; gi < gen->checker.instance_count; gi++) {
        GenericInstance* info = &gen->checker.instances[gi];
        if (info->type->kind != TYPE_ENUM)
            continue;
        if (!enum_has_eq(gen, info->mangled_name))
            continue;
        Node* tmpl = find_generic_enum_decl(ast, info->base_name);
        if (!tmpl)
            continue;

        TypeSubstContext subst;
        subst.type_params = tmpl->as.enum_decl.type_params;
        subst.type_args   = info->type_args;
        subst.count       = tmpl->as.enum_decl.type_param_count;

        TypeSubstContext* old_subst = gen->generics.subst;
        gen->generics.subst         = &subst;

        emit_single_enum_eq(gen, info->mangled_name, tmpl);

        gen->generics.subst = old_subst;
    }
}

// Emit struct body typedefs (field definitions) for non-generic and generic structs
void emit_struct_body_typedefs(CodeGen* gen, Node* ast) {
    // Emit body typedefs for non-generic structs (must come before __rc_dec_TypeName)
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type == NODE_STRUCT_DECL) {
                if (decl->as.struct_decl.type_param_count > 0)
                    continue;
                emit(gen, "typedef struct %s {\n", decl->as.struct_decl.name);
                gen->out.indent++;
                for (int f = 0; f < decl->as.struct_decl.fields.count; f++) {
                    Node* field = decl->as.struct_decl.fields.nodes[f];
                    emit_indent(gen);
                    emit_type_with_name(gen, field->as.field.type, field->as.field.name);
                    emit(gen, ";\n");
                }
                gen->out.indent--;
                emit(gen, "} %s;\n\n", decl->as.struct_decl.name);
            }
        }
    }

    // Emit typedefs for instantiated generic structs
    for (int i = 0; i < gen->checker.instance_count; i++) {
        GenericInstance* info     = &gen->checker.instances[i];
        Node*            template = find_generic_struct_decl(ast, info->base_name);
        if (!template) {
            continue;
        }
        emit(gen, "typedef struct %s {\n", info->mangled_name);
        gen->out.indent++;

        // Set up substitution context so emit_type handles all type params
        TypeSubstContext subst_ctx;
        subst_ctx.type_params = template->as.struct_decl.type_params;
        subst_ctx.type_args   = info->type_args;
        subst_ctx.count       = template->as.struct_decl.type_param_count;
        gen->generics.subst   = &subst_ctx;

        // Emit fields with substituted types
        for (int f = 0; f < template->as.struct_decl.fields.count; f++) {
            Node* field = template->as.struct_decl.fields.nodes[f];
            emit_indent(gen);
            emit_type(gen, field->as.field.type);
            emit(gen, " %s;\n", field->as.field.name);
        }

        gen->generics.subst = NULL;

        gen->out.indent--;
        emit(gen, "} %s;\n\n", info->mangled_name);
    }
}

static const char* module_forward_prefix(Node* mod) {
    if (strcmp(mod->as.module.name, "main") == 0) {
        return NULL;
    }
    return mod->as.module.name;
}

static void collect_decl_functions(Node* decl, Node*** funcs, int* func_count, int* func_cap) {
    if (decl->type == NODE_FUNC_DECL) {
        VEC_GROW(*funcs, *func_count, *func_cap);
        (*funcs)[(*func_count)++] = decl;
        return;
    }
    if (decl->type != NODE_IMPL_DECL) {
        return;
    }
    for (int i = 0; i < decl->as.impl_decl.methods.count; i++) {
        VEC_GROW(*funcs, *func_count, *func_cap);
        (*funcs)[(*func_count)++] = decl->as.impl_decl.methods.nodes[i];
    }
}

static int should_emit_non_generic_forward_decl(CodeGen* gen, func_decl_node* fdn) {
    int is_method = (fdn->receiver_type != NULL);

    if (fdn->body == NULL && !fdn->is_extern) {
        return 0;
    }
    if (is_method && fdn->receiver_type_args.count > 0) {
        return 0;
    }
    if (!is_method && fdn->type_param_count > 0) {
        return 0;
    }
    if (gen->test_mode && !is_method && strcmp(fdn->name, "main") == 0) {
        return 0;
    }
    return 1;
}

static void emit_non_generic_forward_decl(CodeGen* gen, func_decl_node* fdn,
                                          const char* module_prefix) {
    int is_method = (fdn->receiver_type != NULL);

    if (gen->force_static || (!fdn->is_public && strcmp(fdn->name, "main") != 0)) {
        emit(gen, "static ");
    }

    emit_func_return_type(gen, fdn);
    emit_function_name(gen, fdn->name, is_method ? fdn->receiver_type : NULL, module_prefix);

    if (is_method) {
        if (fdn->receiver_is_const && strcmp(fdn->receiver_type, "string") != 0) {
            emit(gen, "const ");
        }
        if (type_is_builtin_name(fdn->receiver_type)) {
            emit(gen, "%s self", type_c_name(fdn->receiver_type));
        } else {
            emit(gen, "%s* self", fdn->receiver_type);
        }
        if (fdn->params.count > 0) {
            emit(gen, ", ");
        }
    }

    if (fdn->params.count == 0 && !is_method) {
        emit(gen, "void");
    } else {
        for (int i = 0; i < fdn->params.count; i++) {
            if (i > 0)
                emit(gen, ", ");
            Node* param = fdn->params.nodes[i];
            emit_type_with_name(gen, param->as.param.type, param->as.param.name);
        }
    }
    emit(gen, ");\n");
}

static void emit_non_generic_function_forward_decls(CodeGen* gen, Node* ast) {
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;

        int is_non_target =
            (gen->target_module != NULL && strcmp(mod->as.module.name, gen->target_module) != 0);

        // For separately compiled modules (sibling or library), emit extern prototypes
        // for public functions only (private functions are not needed in this .o).
        // For non-separate non-target modules, force everything to static.
        int is_separate = (is_non_target && mod->as.module.is_sibling) ||
                          ((is_non_target || gen->use_lib_archive) && mod->as.module.is_library);
        if (is_separate) {
            gen->force_static = 0; // Public functions get implicit extern linkage
        } else {
            gen->force_static = is_non_target;
        }

        int skip_private = is_separate;

        // For sibling modules, use no prefix (their .o defines bare names).
        // For library modules, keep the module prefix (their .o uses prefixed names).
        const char* module_prefix =
            (is_non_target && mod->as.module.is_sibling) ? NULL : module_forward_prefix(mod);
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];

            Node** funcs      = NULL;
            int    func_count = 0;
            int    func_cap   = 0;
            collect_decl_functions(decl, &funcs, &func_count, &func_cap);

            for (int fi = 0; fi < func_count; fi++) {
                func_decl_node* fdn = &funcs[fi]->as.func_decl;
                if (!should_emit_non_generic_forward_decl(gen, fdn)) {
                    continue;
                }
                // Skip private functions from separately compiled modules
                if (skip_private && !fdn->is_public) {
                    continue;
                }
                emit_non_generic_forward_decl(gen, fdn, module_prefix);
            }

            free(funcs);
        }
    }
    gen->force_static = 0;
}

static int find_generic_type_template(Node* ast, const char* base_name, Node** out_template,
                                      int* out_is_struct, int* out_param_count) {
    Node* template = find_generic_struct_decl(ast, base_name);
    if (template) {
        *out_template    = template;
        *out_is_struct   = 1;
        *out_param_count = template->as.struct_decl.type_param_count;
        return 1;
    }

    template = find_generic_enum_decl(ast, base_name);
    if (!template) {
        return 0;
    }
    *out_template    = template;
    *out_is_struct   = 0;
    *out_param_count = template->as.enum_decl.type_param_count;
    return 1;
}

static void emit_single_generic_method_forward_decl(CodeGen* gen, GenericInstance* info,
                                                    Node* template, int is_struct, int param_count,
                                                    Node* method) {
    func_decl_node* fdn = &method->as.func_decl;

    char** method_params     = NULL;
    Type** method_args       = NULL;
    int    method_bind_count = 0;
    codegen_extract_method_bindings(&fdn->receiver_type_args, info->type_args, info->type_arg_count,
                                    &method_params, &method_args, &method_bind_count);

    int    combined_count  = param_count + method_bind_count;
    char** combined_params = xmalloc(combined_count * sizeof(char*));
    Type** combined_args   = xmalloc(combined_count * sizeof(Type*));

    for (int i = 0; i < param_count; i++) {
        if (is_struct) {
            combined_params[i] = template->as.struct_decl.type_params[i];
        } else {
            combined_params[i] = template->as.enum_decl.type_params[i];
        }
        combined_args[i] = info->type_args[i];
    }
    for (int i = 0; i < method_bind_count; i++) {
        combined_params[param_count + i] = method_params[i];
        combined_args[param_count + i]   = method_args[i];
    }

    TypeSubstContext  subst_ctx;
    TypeSubstContext* old_subst = gen->generics.subst;
    subst_ctx.type_params       = combined_params;
    subst_ctx.type_args         = combined_args;
    subst_ctx.count             = combined_count;
    gen->generics.subst         = &subst_ctx;

    if (gen->target_module)
        emit(gen, "static ");
    emit_func_return_type(gen, fdn);
    emit(gen, " %s_%s(", info->mangled_name, fdn->name);

    if (fdn->receiver_is_const) {
        emit(gen, "const ");
    }
    emit(gen, "%s* self", info->mangled_name);

    for (int i = 0; i < fdn->params.count; i++) {
        emit(gen, ", ");
        Node* param = fdn->params.nodes[i];
        if (param->as.param.is_const) {
            emit(gen, "const ");
        }
        emit_type_with_name(gen, param->as.param.type, param->as.param.name);
    }
    emit(gen, ");\n");

    gen->generics.subst = old_subst;
    free(combined_params);
    free(combined_args);
    for (int i = 0; i < method_bind_count; i++) {
        free(method_params[i]);
    }
    free(method_params);
    free(method_args);
}

static void emit_generic_method_forward_decls(CodeGen* gen, Node* ast) {
    for (int i = 0; i < gen->checker.instance_count; i++) {
        GenericInstance* info = &gen->checker.instances[i];

        Node* template    = NULL;
        int   is_struct   = 0;
        int   param_count = 0;
        if (!find_generic_type_template(ast, info->base_name, &template, &is_struct,
                                        &param_count)) {
            continue;
        }

        Node** methods      = NULL;
        int    method_count = 0;
        collect_generic_methods(ast, info->base_name, &methods, &method_count);

        for (int j = 0; j < method_count; j++) {
            emit_single_generic_method_forward_decl(gen, info, template, is_struct, param_count,
                                                    methods[j]);
        }

        free(methods);
    }
}

static void emit_single_generic_func_forward_decl(CodeGen* gen, GenericFuncInstance* inst,
                                                  func_decl_node* fdn) {
    TypeSubstContext  subst_ctx;
    TypeSubstContext* old_subst = gen->generics.subst;
    subst_ctx.type_params       = fdn->type_params;
    subst_ctx.type_args         = inst->type_args;
    subst_ctx.count             = inst->type_arg_count;
    gen->generics.subst         = &subst_ctx;

    if (gen->target_module)
        emit(gen, "static ");
    emit_func_return_type(gen, fdn);
    emit(gen, " %s(", inst->mangled_name);

    if (fdn->params.count == 0) {
        emit(gen, "void");
    } else {
        for (int i = 0; i < fdn->params.count; i++) {
            if (i > 0)
                emit(gen, ", ");
            Node* param = fdn->params.nodes[i];
            if (param->as.param.is_const) {
                emit(gen, "const ");
            }
            emit_type_with_name(gen, param->as.param.type, param->as.param.name);
        }
    }
    emit(gen, ");\n");
    gen->generics.subst = old_subst;
}

static void emit_single_generic_method_func_forward_decl(CodeGen* gen, GenericFuncInstance* inst,
                                                         func_decl_node* fdn) {
    GenericFuncDef* def = lookup_generic_func_def_for_instance(gen, inst->base_name);
    if (!def || def->type_param_count != inst->type_arg_count) {
        return;
    }

    TypeSubstContext  subst_ctx;
    TypeSubstContext* old_subst = gen->generics.subst;
    subst_ctx.type_params       = def->type_params;
    subst_ctx.type_args         = inst->type_args;
    subst_ctx.count             = def->type_param_count;
    gen->generics.subst         = &subst_ctx;

    if (gen->target_module)
        emit(gen, "static ");
    emit_func_return_type(gen, fdn);
    emit(gen, " %s(", inst->mangled_name);

    // Self parameter
    if (fdn->receiver_is_const) {
        emit(gen, "const ");
    }
    emit_resolved_type(gen, inst->receiver_concrete);
    emit(gen, " self");

    // Other parameters
    for (int p = 0; p < fdn->params.count; p++) {
        emit(gen, ", ");
        Node* param = fdn->params.nodes[p];
        if (param->as.param.is_const) {
            emit(gen, "const ");
        }
        emit_type_with_name(gen, param->as.param.type, param->as.param.name);
    }
    emit(gen, ");\n");

    gen->generics.subst = old_subst;
}

static void emit_generic_func_forward_decls(CodeGen* gen, Node* ast) {
    for (int i = 0; i < gen->checker.func_instance_count; i++) {
        GenericFuncInstance* inst = &gen->checker.func_instances[i];

        if (inst->is_method) {
            // Method-level generic: parse "Vec.map" key
            char recv_name[128], method_name_buf[128];
            if (!parse_method_key(inst->base_name, recv_name, sizeof(recv_name), method_name_buf,
                                  sizeof(method_name_buf)))
                continue;

            Node* tmpl = find_generic_method_func_decl(ast, recv_name, method_name_buf);
            if (!tmpl)
                continue;
            emit_single_generic_method_func_forward_decl(gen, inst, &tmpl->as.func_decl);
            continue;
        }

        Node* tmpl = find_generic_func_decl(ast, inst->base_name);
        if (!tmpl) {
            continue;
        }
        emit_single_generic_func_forward_decl(gen, inst, &tmpl->as.func_decl);
    }
}

static void emit_vec_user_method_forward_decls(CodeGen* gen, Node* ast) {
    for (int i = 0; i < gen->checker.vec_count; i++) {
        VecInstance* inst = &gen->checker.vecs[i];
        if (inst->method_count == 0) {
            continue;
        }

        const char* elem_tname = type_mangle_name(inst->elem_type);

        // Find the Vec GenericDef's methods in the AST to get parameter type nodes
        Node** methods      = NULL;
        int    method_count = 0;
        collect_generic_methods(ast, "Vec", &methods, &method_count);

        for (int m = 0; m < inst->method_count && m < method_count; m++) {
            func_decl_node* fdn = &methods[m]->as.func_decl;

            // Set up substitution: T -> elem_type
            TypeSubstContext  subst_ctx;
            TypeSubstContext* old_subst = gen->generics.subst;
            char*             tp        = "T";
            subst_ctx.type_params       = &tp;
            subst_ctx.type_args         = &inst->elem_type;
            subst_ctx.count             = 1;
            gen->generics.subst         = &subst_ctx;

            if (gen->target_module)
                emit(gen, "static ");
            emit_func_return_type(gen, fdn);
            emit(gen, " __Vec_%s_%s(__Vec_%s* self", elem_tname, fdn->name, elem_tname);

            for (int p = 0; p < fdn->params.count; p++) {
                emit(gen, ", ");
                Node* param = fdn->params.nodes[p];
                if (param->as.param.is_const) {
                    emit(gen, "const ");
                }
                emit_type_with_name(gen, param->as.param.type, param->as.param.name);
            }
            emit(gen, ");\n");

            gen->generics.subst = old_subst;
        }

        free(methods);
    }
}

// Emit forward declarations for all functions, methods, and generic method instances
void emit_function_forward_decls(CodeGen* gen, Node* ast) {
    emit_non_generic_function_forward_decls(gen, ast);
    emit_generic_method_forward_decls(gen, ast);
    emit_generic_func_forward_decls(gen, ast);
    emit_vec_user_method_forward_decls(gen, ast);
    emit(gen, "\n");
}
