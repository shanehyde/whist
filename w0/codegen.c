#include "codegen.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

static void emit_indent(CodeGen* gen) {
    for (int i = 0; i < gen->indent; i++) {
        fprintf(gen->out, "    ");
    }
}

static void emit(CodeGen* gen, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(gen->out, fmt, args);
    va_end(args);
}

static void  emit_type(CodeGen* gen, Node* type_node);
static void  emit_resolved_type(CodeGen* gen, Type* type);
static void  emit_expr(CodeGen* gen, Node* node);
static void  emit_stmt(CodeGen* gen, Node* node);
static void  emit_decl(CodeGen* gen, Node* node);
static void  emit_struct_init(CodeGen* gen, Node* node);
static int   tuple_types_equal(Type* a, Type* b);
static Type* type_from_node(Node* type_node);

static void defer_push(CodeGen* gen, Node* node) {
    if (gen->defer_count >= gen->defer_capacity) {
        int new_cap         = gen->defer_capacity == 0 ? 8 : gen->defer_capacity * 2;
        gen->defer_stack    = realloc(gen->defer_stack, new_cap * sizeof(Node*));
        gen->defer_capacity = new_cap;
    }
    gen->defer_stack[gen->defer_count++] = node;
}

static void defer_clear(CodeGen* gen) {
    gen->defer_count = 0;
}

// Check if a type node represents a struct (user-defined) type
static int is_struct_type(Node* type_node) {
    if (!type_node || type_node->type != NODE_IDENT)
        return 0;
    const char* name = type_node->as.ident.name;
    // Check against all built-in type names
    return strcmp(name, "void") != 0 && strcmp(name, "bool") != 0 && strcmp(name, "i64") != 0 &&
           strcmp(name, "i8") != 0 && strcmp(name, "i16") != 0 && strcmp(name, "i32") != 0 &&
           strcmp(name, "u64") != 0 && strcmp(name, "u8") != 0 && strcmp(name, "u16") != 0 &&
           strcmp(name, "u32") != 0 && strcmp(name, "f32") != 0 && strcmp(name, "f64") != 0 &&
           strcmp(name, "char") != 0 && strcmp(name, "string") != 0;
}

// Helper to emit function name with appropriate prefix (method or module)
// For methods: emits "StructName_funcname("
// For library functions: emits "module_funcname("
// For main module functions: emits "funcname("
static void emit_function_name(CodeGen* gen, const char* func_name, const char* receiver_type,
                               const char* module_name) {
    if (receiver_type != NULL) {
        // Method: prefix with struct name
        emit(gen, " %s_%s(", receiver_type, func_name);
    } else if (module_name != NULL) {
        // Library function: prefix with module name
        emit(gen, " %s_%s(", module_name, func_name);
    } else {
        // Main module function: no prefix
        emit(gen, " %s(", func_name);
    }
}

// Emit a type from a type annotation node
static void emit_type(CodeGen* gen, Node* type_node) {
    if (!type_node) {
        emit(gen, "void");
        return;
    }

    switch (type_node->type) {
    case NODE_IDENT: {
        const char* name = type_node->as.ident.name;
        // Map whist types to C types
        if (strcmp(name, "void") == 0) {
            emit(gen, "void");
        } else if (strcmp(name, "bool") == 0) {
            emit(gen, "bool");
        } else if (strcmp(name, "i64") == 0) {
            emit(gen, "int64_t");
        } else if (strcmp(name, "i8") == 0) {
            emit(gen, "int8_t");
        } else if (strcmp(name, "i16") == 0) {
            emit(gen, "int16_t");
        } else if (strcmp(name, "i32") == 0) {
            emit(gen, "int32_t");
        } else if (strcmp(name, "u64") == 0) {
            emit(gen, "uint64_t");
        } else if (strcmp(name, "u8") == 0) {
            emit(gen, "uint8_t");
        } else if (strcmp(name, "u16") == 0) {
            emit(gen, "uint16_t");
        } else if (strcmp(name, "u32") == 0) {
            emit(gen, "uint32_t");
        } else if (strcmp(name, "f32") == 0) {
            emit(gen, "float");
        } else if (strcmp(name, "f64") == 0) {
            emit(gen, "double");
        } else if (strcmp(name, "char") == 0) {
            emit(gen, "char");
        } else if (strcmp(name, "string") == 0) {
            emit(gen, "const char*");
        } else {
            // User-defined struct type - emit as pointer (struct references)
            emit(gen, "%s*", name);
        }
        break;
    }
    case NODE_UNARY:
        // Pointer types no longer supported in the language
        emit(gen, "/* pointer types not supported */");
        break;
    case NODE_INDEX:
        // Array type: [n]T -> T[n] or T*
        emit_type(gen, type_node->as.index.object);
        if (type_node->as.index.index) {
            emit(gen, "[");
            emit_expr(gen, type_node->as.index.index);
            emit(gen, "]");
        } else {
            emit(gen, "*");
        }
        break;
    case NODE_TUPLE_TYPE: {
        // Build a Type* and find the matching typedef
        Type* tuple = type_from_node(type_node);
        int   idx   = -1;
        for (int i = 0; i < gen->tuple_type_count; i++) {
            if (tuple_types_equal(gen->tuple_types[i], tuple)) {
                idx = i;
                break;
            }
        }
        if (idx >= 0) {
            emit(gen, "__tuple_t%d", idx);
        } else {
            // Fallback to inline struct
            emit(gen, "struct { ");
            for (int i = 0; i < type_node->as.tuple_type.elem_types.count; i++) {
                emit_type(gen, type_node->as.tuple_type.elem_types.nodes[i]);
                emit(gen, " _%d; ", i);
            }
            emit(gen, "}");
        }
        break;
    }
    default:
        emit(gen, "/* unknown type */");
        break;
    }
}

// Emit a resolved Type* (used for inferred types like tuples)
static void emit_resolved_type(CodeGen* gen, Type* type) {
    if (!type) {
        emit(gen, "void");
        return;
    }

    switch (type->kind) {
    case TYPE_VOID:
        emit(gen, "void");
        break;
    case TYPE_BOOL:
        emit(gen, "bool");
        break;
    case TYPE_INT64:
        emit(gen, "int64_t");
        break;
    case TYPE_INT8:
        emit(gen, "int8_t");
        break;
    case TYPE_INT16:
        emit(gen, "int16_t");
        break;
    case TYPE_INT32:
        emit(gen, "int32_t");
        break;
    case TYPE_UINT64:
        emit(gen, "uint64_t");
        break;
    case TYPE_UINT8:
        emit(gen, "uint8_t");
        break;
    case TYPE_UINT16:
        emit(gen, "uint16_t");
        break;
    case TYPE_UINT32:
        emit(gen, "uint32_t");
        break;
    case TYPE_F32:
        emit(gen, "float");
        break;
    case TYPE_F64:
        emit(gen, "double");
        break;
    case TYPE_CHAR:
        emit(gen, "char");
        break;
    case TYPE_STRING:
        emit(gen, "const char*");
        break;
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
    case TYPE_ENUM:
        emit(gen, "%s", type->as.enm.name);
        break;
    case TYPE_TUPLE: {
        // Find the typedef index for this tuple type
        int idx = -1;
        for (int i = 0; i < gen->tuple_type_count; i++) {
            if (tuple_types_equal(gen->tuple_types[i], type)) {
                idx = i;
                break;
            }
        }
        if (idx >= 0) {
            emit(gen, "__tuple_t%d", idx);
        } else {
            // Fallback to inline struct (shouldn't happen if collection is correct)
            emit(gen, "struct { ");
            for (int i = 0; i < type->as.tuple.elem_count; i++) {
                emit_resolved_type(gen, type->as.tuple.elem_types[i]);
                emit(gen, " _%d; ", i);
            }
            emit(gen, "}");
        }
        break;
    }
    default:
        emit(gen, "/* unknown type */");
        break;
    }
}

// Emit type with variable name (handles array syntax)
static void emit_type_with_name(CodeGen* gen, Node* type_node, const char* name) {
    if (!type_node) {
        emit(gen, "void %s", name);
        return;
    }

    if (type_node->type == NODE_INDEX && type_node->as.index.index) {
        // Array: T name[n]
        emit_type(gen, type_node->as.index.object);
        emit(gen, " %s[", name);
        emit_expr(gen, type_node->as.index.index);
        emit(gen, "]");
    } else {
        emit_type(gen, type_node);
        emit(gen, " %s", name);
    }
}

static const char* binary_op_str(TokenType op) {
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

static const char* unary_op_str(TokenType op) {
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
    case TOK_PLUS_PLUS:
        return "++";
    case TOK_MINUS_MINUS:
        return "--";
    default:
        return "?";
    }
}

static const char* assign_op_str(TokenType op) {
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

static void emit_expr(CodeGen* gen, Node* node) {
    if (!node)
        return;

    switch (node->type) {
    case NODE_INT_LIT:
        emit(gen, "%ldLL", node->as.int_lit.value);
        break;

    case NODE_FLOAT_LIT:
        emit(gen, "%g", node->as.float_lit.value);
        break;

    case NODE_STRING_LIT:
        emit(gen, "\"");
        // Escape special characters
        for (int i = 0; i < node->as.string_lit.length; i++) {
            char c = node->as.string_lit.value[i];
            switch (c) {
            case '\n':
                emit(gen, "\\n");
                break;
            case '\t':
                emit(gen, "\\t");
                break;
            case '\r':
                emit(gen, "\\r");
                break;
            case '\\':
                emit(gen, "\\\\");
                break;
            case '"':
                emit(gen, "\\\"");
                break;
            default:
                emit(gen, "%c", c);
                break;
            }
        }
        emit(gen, "\"");
        break;

    case NODE_CHAR_LIT:
        if (node->as.char_lit.value == '\n') {
            emit(gen, "'\\n'");
        } else if (node->as.char_lit.value == '\t') {
            emit(gen, "'\\t'");
        } else if (node->as.char_lit.value == '\r') {
            emit(gen, "'\\r'");
        } else if (node->as.char_lit.value == '\\') {
            emit(gen, "'\\\\'");
        } else if (node->as.char_lit.value == '\'') {
            emit(gen, "'\\''");
        } else {
            emit(gen, "'%c'", node->as.char_lit.value);
        }
        break;

    case NODE_BOOL_LIT:
        emit(gen, "%s", node->as.bool_lit.value ? "true" : "false");
        break;

    case NODE_NULL_LIT:
        emit(gen, "NULL");
        break;

    case NODE_IDENT:
        emit(gen, "%.*s", node->as.ident.length, node->as.ident.name);
        break;

    case NODE_ENUM_VALUE:
        // Emit just the value name - C enums use unqualified names
        emit(gen, "%.*s", node->as.enum_value.value_name_length, node->as.enum_value.value_name);
        break;

    case NODE_BINARY:
        emit(gen, "(");
        emit_expr(gen, node->as.binary.left);
        emit(gen, " %s ", binary_op_str(node->as.binary.op));
        emit_expr(gen, node->as.binary.right);
        emit(gen, ")");
        break;

    case NODE_UNARY:
        if (node->as.unary.postfix) {
            emit(gen, "(");
            emit_expr(gen, node->as.unary.operand);
            emit(gen, "%s)", unary_op_str(node->as.unary.op));
        } else {
            emit(gen, "(%s", unary_op_str(node->as.unary.op));
            emit_expr(gen, node->as.unary.operand);
            emit(gen, ")");
        }
        break;

    case NODE_CALL: {
        Node* func = node->as.call.func;
        // Check if this is a module-qualified call (e.g., std.print())
        if (func->type == NODE_MEMBER && func->as.member.module_name != NULL) {
            // Module-qualified call: emit module_func(args...)
            emit(gen, "%s_%.*s(", func->as.member.module_name, func->as.member.length,
                 func->as.member.name);
            for (int i = 0; i < node->as.call.args.count; i++) {
                if (i > 0)
                    emit(gen, ", ");
                emit_expr(gen, node->as.call.args.nodes[i]);
            }
            emit(gen, ")");
        } else if (func->type == NODE_MEMBER && func->as.member.struct_name != NULL) {
            // Method call: emit StructName_method(obj, args...)
            // With struct references, objects are already pointers
            emit(gen, "%s_%.*s(", func->as.member.struct_name, func->as.member.length,
                 func->as.member.name);
            // Emit the receiver as first argument (already a pointer)
            emit_expr(gen, func->as.member.object);
            // Emit remaining arguments
            for (int i = 0; i < node->as.call.args.count; i++) {
                emit(gen, ", ");
                emit_expr(gen, node->as.call.args.nodes[i]);
            }
            emit(gen, ")");
        } else {
            // Regular function call
            emit_expr(gen, func);
            emit(gen, "(");
            for (int i = 0; i < node->as.call.args.count; i++) {
                if (i > 0)
                    emit(gen, ", ");
                emit_expr(gen, node->as.call.args.nodes[i]);
            }
            emit(gen, ")");
        }
        break;
    }

    case NODE_INDEX:
        emit_expr(gen, node->as.index.object);
        if (node->as.index.is_tuple_index) {
            // Tuple indexing: obj._N
            emit(gen, "._%ld", node->as.index.index->as.int_lit.value);
        } else {
            // Array/string indexing: obj[index]
            emit(gen, "[");
            emit_expr(gen, node->as.index.index);
            emit(gen, "]");
        }
        break;

    case NODE_MEMBER:
        emit_expr(gen, node->as.member.object);
        // With struct references, always use -> for member access
        emit(gen, "->%.*s", node->as.member.length, node->as.member.name);
        break;

    case NODE_ASSIGN:
        emit(gen, "(");
        emit_expr(gen, node->as.assign.target);
        emit(gen, " %s ", assign_op_str(node->as.assign.op));
        emit_expr(gen, node->as.assign.value);
        emit(gen, ")");
        break;

    case NODE_STRUCT_INIT:
        emit_struct_init(gen, node);
        break;

    case NODE_TUPLE_LIT:
        // Tuple literal: (e1, e2, ...) -> {e1, e2, ...}
        emit(gen, "{");
        for (int i = 0; i < node->as.tuple_lit.elements.count; i++) {
            if (i > 0)
                emit(gen, ", ");
            emit_expr(gen, node->as.tuple_lit.elements.nodes[i]);
        }
        emit(gen, "}");
        break;

    default:
        emit(gen, "/* unknown expr %d */", node->type);
        break;
    }
}

static void emit_struct_init(CodeGen* gen, Node* node) {
    emit(gen, "{");
    for (int i = 0; i < node->as.struct_init.fields.count; i++) {
        Node* field = node->as.struct_init.fields.nodes[i];
        if (!field || field->type != NODE_FIELD_INIT) {
            continue;
        }
        if (i > 0) {
            emit(gen, ", ");
        }
        emit(gen, ".%s = ", field->as.field_init.name);
        emit_expr(gen, field->as.field_init.value);
    }
    emit(gen, "}");
}

static void emit_stmt(CodeGen* gen, Node* node) {
    if (!node)
        return;

    switch (node->type) {
    case NODE_EXPR_STMT:
        emit_indent(gen);
        emit_expr(gen, node->as.expr_stmt.expr);
        emit(gen, ";\n");
        break;

    case NODE_VAR_DECL: {
        // Handle destructuring: var (a, b) = tuple;
        if (node->as.var_decl.destruct_count > 0) {
            Type* tuple_type = (Type*)node->as.var_decl.destruct_tuple_type;

            // Emit a temporary variable to hold the tuple value
            emit_indent(gen);
            emit_resolved_type(gen, tuple_type);
            emit(gen, " __tuple%d = ", gen->temp_count);
            emit_expr(gen, node->as.var_decl.init);
            emit(gen, ";\n");
            int temp_id = gen->temp_count++;

            // Emit each destructured variable
            for (int i = 0; i < node->as.var_decl.destruct_count; i++) {
                emit_indent(gen);
                if (node->as.var_decl.is_const) {
                    emit(gen, "const ");
                }
                emit_resolved_type(gen, tuple_type->as.tuple.elem_types[i]);
                emit(gen, " %s = __tuple%d._%d;\n", node->as.var_decl.destruct_names[i], temp_id,
                     i);
            }
            break;
        }

        emit_indent(gen);
        if (node->as.var_decl.is_const) {
            emit(gen, "const ");
        }

        // Check if this is a struct type variable with initializer
        int struct_type = node->as.var_decl.type && is_struct_type(node->as.var_decl.type);

        if (node->as.var_decl.type) {
            emit_type_with_name(gen, node->as.var_decl.type, node->as.var_decl.name);
        } else {
            // Type inference - use auto or infer from init
            // For C, we need to determine the type from the initializer
            // For simplicity, use int64_t for int literals, float for f32 literals
            if (node->as.var_decl.init) {
                switch (node->as.var_decl.init->type) {
                case NODE_INT_LIT:
                    emit(gen, "int64_t %s", node->as.var_decl.name);
                    break;
                case NODE_FLOAT_LIT:
                    emit(gen, "float %s", node->as.var_decl.name);
                    break;
                case NODE_BOOL_LIT:
                    emit(gen, "bool %s", node->as.var_decl.name);
                    break;
                case NODE_STRING_LIT:
                    emit(gen, "const char* %s", node->as.var_decl.name);
                    break;
                case NODE_CHAR_LIT:
                    emit(gen, "char %s", node->as.var_decl.name);
                    break;
                case NODE_TUPLE_LIT: {
                    // Build a tuple type from the literal's elements
                    int    count = node->as.var_decl.init->as.tuple_lit.elements.count;
                    Type** elems = malloc(count * sizeof(Type*));
                    for (int i = 0; i < count; i++) {
                        Node* elem = node->as.var_decl.init->as.tuple_lit.elements.nodes[i];
                        switch (elem->type) {
                        case NODE_INT_LIT:
                            elems[i] = type_int64;
                            break;
                        case NODE_FLOAT_LIT:
                            elems[i] = type_f32;
                            break;
                        case NODE_BOOL_LIT:
                            elems[i] = type_bool;
                            break;
                        case NODE_STRING_LIT:
                            elems[i] = type_string;
                            break;
                        case NODE_CHAR_LIT:
                            elems[i] = type_char;
                            break;
                        case NODE_TUPLE_LIT:
                            // Nested tuple - would need recursive handling
                            elems[i] = type_int64; // Fallback
                            break;
                        default:
                            elems[i] = type_int64; // Default
                            break;
                        }
                    }
                    Type* tuple = type_tuple(elems, count);
                    // Find matching typedef
                    int idx = -1;
                    for (int i = 0; i < gen->tuple_type_count; i++) {
                        if (tuple_types_equal(gen->tuple_types[i], tuple)) {
                            idx = i;
                            break;
                        }
                    }
                    if (idx >= 0) {
                        emit(gen, "__tuple_t%d %s", idx, node->as.var_decl.name);
                    } else {
                        // Fallback to inline struct
                        emit(gen, "struct { ");
                        for (int i = 0; i < count; i++) {
                            emit_resolved_type(gen, elems[i]);
                            emit(gen, " _%d; ", i);
                        }
                        emit(gen, "} %s", node->as.var_decl.name);
                    }
                    break;
                }
                default:
                    // Default to auto if we can't determine
                    emit(gen, "int64_t %s", node->as.var_decl.name);
                    break;
                }
            } else {
                emit(gen, "int64_t %s", node->as.var_decl.name);
            }
        }
        if (node->as.var_decl.init) {
            if (struct_type && node->as.var_decl.init->type == NODE_STRUCT_INIT) {
                // Struct type with struct init: allocate and initialize
                // var p: Point = {...} => Point* p = malloc(sizeof(Point)); *p = (Point){...};
                const char* type_name = node->as.var_decl.type->as.ident.name;
                emit(gen, " = malloc(sizeof(%s));\n", type_name);
                emit_indent(gen);
                emit(gen, "*%s = (%s)", node->as.var_decl.name, type_name);
                emit_struct_init(gen, node->as.var_decl.init);
            } else if (struct_type && node->as.var_decl.init->type == NODE_NULL_LIT) {
                // Struct type with null: just assign NULL
                emit(gen, " = NULL");
            } else {
                emit(gen, " = ");
                emit_expr(gen, node->as.var_decl.init);
            }
        }
        emit(gen, ";\n");
        break;
    }

    case NODE_BLOCK:
        emit_indent(gen);
        emit(gen, "{\n");
        gen->indent++;
        for (int i = 0; i < node->as.block.stmts.count; i++) {
            emit_stmt(gen, node->as.block.stmts.nodes[i]);
        }
        gen->indent--;
        emit_indent(gen);
        emit(gen, "}\n");
        break;

    case NODE_IF:
        emit_indent(gen);
        emit(gen, "if (");
        emit_expr(gen, node->as.if_stmt.cond);
        emit(gen, ") {\n");
        gen->indent++;
        // Emit then block contents directly (it's already a block)
        if (node->as.if_stmt.then_block->type == NODE_BLOCK) {
            for (int i = 0; i < node->as.if_stmt.then_block->as.block.stmts.count; i++) {
                emit_stmt(gen, node->as.if_stmt.then_block->as.block.stmts.nodes[i]);
            }
        } else {
            emit_stmt(gen, node->as.if_stmt.then_block);
        }
        gen->indent--;
        emit_indent(gen);
        emit(gen, "}");

        if (node->as.if_stmt.else_block) {
            if (node->as.if_stmt.else_block->type == NODE_IF) {
                // else if
                emit(gen, " else ");
                // Remove indent for else if
                gen->indent--;
                emit_stmt(gen, node->as.if_stmt.else_block);
                gen->indent++;
            } else {
                emit(gen, " else {\n");
                gen->indent++;
                if (node->as.if_stmt.else_block->type == NODE_BLOCK) {
                    for (int i = 0; i < node->as.if_stmt.else_block->as.block.stmts.count; i++) {
                        emit_stmt(gen, node->as.if_stmt.else_block->as.block.stmts.nodes[i]);
                    }
                } else {
                    emit_stmt(gen, node->as.if_stmt.else_block);
                }
                gen->indent--;
                emit_indent(gen);
                emit(gen, "}\n");
            }
        } else {
            emit(gen, "\n");
        }
        break;

    case NODE_WHILE:
        emit_indent(gen);
        emit(gen, "while (");
        emit_expr(gen, node->as.while_stmt.cond);
        emit(gen, ") {\n");
        gen->indent++;
        if (node->as.while_stmt.body->type == NODE_BLOCK) {
            for (int i = 0; i < node->as.while_stmt.body->as.block.stmts.count; i++) {
                emit_stmt(gen, node->as.while_stmt.body->as.block.stmts.nodes[i]);
            }
        } else {
            emit_stmt(gen, node->as.while_stmt.body);
        }
        gen->indent--;
        emit_indent(gen);
        emit(gen, "}\n");
        break;

    case NODE_FOR:
        emit_indent(gen);
        emit(gen, "for (");
        // Init
        if (node->as.for_stmt.init) {
            if (node->as.for_stmt.init->type == NODE_VAR_DECL) {
                Node* v = node->as.for_stmt.init;
                if (v->as.var_decl.type) {
                    emit_type_with_name(gen, v->as.var_decl.type, v->as.var_decl.name);
                } else {
                    emit(gen, "int64_t %s", v->as.var_decl.name);
                }
                if (v->as.var_decl.init) {
                    emit(gen, " = ");
                    emit_expr(gen, v->as.var_decl.init);
                }
            } else {
                emit_expr(gen, node->as.for_stmt.init);
            }
        }
        emit(gen, "; ");
        // Cond
        if (node->as.for_stmt.cond) {
            emit_expr(gen, node->as.for_stmt.cond);
        }
        emit(gen, "; ");
        // Post
        if (node->as.for_stmt.post) {
            emit_expr(gen, node->as.for_stmt.post);
        }
        emit(gen, ") {\n");
        gen->indent++;
        if (node->as.for_stmt.body->type == NODE_BLOCK) {
            for (int i = 0; i < node->as.for_stmt.body->as.block.stmts.count; i++) {
                emit_stmt(gen, node->as.for_stmt.body->as.block.stmts.nodes[i]);
            }
        } else {
            emit_stmt(gen, node->as.for_stmt.body);
        }
        gen->indent--;
        emit_indent(gen);
        emit(gen, "}\n");
        break;

    case NODE_FOREACH:
        emit_indent(gen);
        // Generate: for (int64_t var = start; var <= end; var += step) {
        emit(gen, "for (int64_t %s = ", node->as.foreach_stmt.var_name);
        emit_expr(gen, node->as.foreach_stmt.start);
        emit(gen, "; %s <= ", node->as.foreach_stmt.var_name);
        emit_expr(gen, node->as.foreach_stmt.end);
        emit(gen, "; %s += ", node->as.foreach_stmt.var_name);
        emit_expr(gen, node->as.foreach_stmt.step);
        emit(gen, ") {\n");
        gen->indent++;
        if (node->as.foreach_stmt.body->type == NODE_BLOCK) {
            for (int i = 0; i < node->as.foreach_stmt.body->as.block.stmts.count; i++) {
                emit_stmt(gen, node->as.foreach_stmt.body->as.block.stmts.nodes[i]);
            }
        } else {
            emit_stmt(gen, node->as.foreach_stmt.body);
        }
        gen->indent--;
        emit_indent(gen);
        emit(gen, "}\n");
        break;
    case NODE_RETURN:
        emit_indent(gen);
        if (gen->defer_count > 0) {
            // With defers: store value in __ret and goto cleanup
            if (node->as.return_stmt.value) {
                emit(gen, "__ret = ");
                emit_expr(gen, node->as.return_stmt.value);
                emit(gen, ";\n");
            }
            emit_indent(gen);
            emit(gen, "goto __cleanup;\n");
        } else {
            // No defers: normal return
            emit(gen, "return");
            if (node->as.return_stmt.value) {
                emit(gen, " ");
                emit_expr(gen, node->as.return_stmt.value);
            }
            emit(gen, ";\n");
        }
        break;

    case NODE_DEFER:
        // Don't emit anything here - just push to defer stack
        defer_push(gen, node->as.defer_stmt.stmt);
        break;

    case NODE_BREAK:
        emit_indent(gen);
        emit(gen, "break;\n");
        break;

    case NODE_CONTINUE:
        emit_indent(gen);
        emit(gen, "continue;\n");
        break;

    default:
        emit_indent(gen);
        emit(gen, "/* unknown stmt %d */;\n", node->type);
        break;
    }
}

static void emit_decl(CodeGen* gen, Node* node) {
    if (!node)
        return;

    switch (node->type) {
    case NODE_EXTERN_MODULE:
        emit(gen, "\n#include <%s.h>\n", node->as.extern_module.module_name);
        break;

    case NODE_STRUCT_DECL:
        emit(gen, "typedef struct %s {\n", node->as.struct_decl.name);
        gen->indent++;
        for (int i = 0; i < node->as.struct_decl.fields.count; i++) {
            Node* field = node->as.struct_decl.fields.nodes[i];
            emit_indent(gen);
            emit_type_with_name(gen, field->as.field.type, field->as.field.name);
            emit(gen, ";\n");
        }
        gen->indent--;
        emit(gen, "} %s;\n\n", node->as.struct_decl.name);
        break;

    case NODE_ENUM_DECL:
        emit(gen, "typedef enum %s {\n", node->as.enum_decl.name);
        gen->indent++;
        for (int i = 0; i < node->as.enum_decl.values.count; i++) {
            Node* val = node->as.enum_decl.values.nodes[i];
            emit_indent(gen);
            emit(gen, "%.*s", val->as.ident.length, val->as.ident.name);
            if (i < node->as.enum_decl.values.count - 1) {
                emit(gen, ",");
            }
            emit(gen, "\n");
        }
        gen->indent--;
        emit(gen, "} %s;\n\n", node->as.enum_decl.name);
        break;

    case NODE_FUNC_DECL: {
        int is_method = (node->as.func_decl.receiver_type != NULL);

        // Check if function is void
        int is_void = !node->as.func_decl.return_type ||
                      (node->as.func_decl.return_type->type == NODE_IDENT &&
                       strcmp(node->as.func_decl.return_type->as.ident.name, "void") == 0);

        // Emit static for private functions (except main)
        if (!node->as.func_decl.is_public && strcmp(node->as.func_decl.name, "main") != 0) {
            emit(gen, "static ");
        }

        // Return type
        emit_type(gen, node->as.func_decl.return_type);

        // Function name (mangled for methods and library functions)
        emit_function_name(gen, node->as.func_decl.name,
                           is_method ? node->as.func_decl.receiver_type : NULL,
                           gen->current_module);

        // Parameters
        if (is_method) {
            // Emit self parameter first
            if (node->as.func_decl.receiver_is_const) {
                emit(gen, "const ");
            }
            emit(gen, "%s* self", node->as.func_decl.receiver_type);
            if (node->as.func_decl.params.count > 0) {
                emit(gen, ", ");
            }
        }

        if (node->as.func_decl.params.count == 0 && !is_method) {
            emit(gen, "void");
        } else {
            for (int i = 0; i < node->as.func_decl.params.count; i++) {
                if (i > 0)
                    emit(gen, ", ");
                Node* param = node->as.func_decl.params.nodes[i];
                if (param->as.param.is_const) {
                    emit(gen, "const ");
                }
                emit_type_with_name(gen, param->as.param.type, param->as.param.name);
            }
        }
        emit(gen, ") {\n");

        // Clear defer stack for this function
        defer_clear(gen);
        gen->current_return_type = node->as.func_decl.return_type;

        // First pass: count defers to know if we need __ret
        int has_defers = 0;
        if (node->as.func_decl.body) {
            for (int i = 0; i < node->as.func_decl.body->as.block.stmts.count; i++) {
                Node* stmt = node->as.func_decl.body->as.block.stmts.nodes[i];
                if (stmt && stmt->type == NODE_DEFER) {
                    has_defers = 1;
                    break;
                }
            }
        }

        // Body
        gen->indent++;

        // Declare __ret if function has defers and is non-void
        if (has_defers && !is_void) {
            emit_indent(gen);
            emit_type(gen, node->as.func_decl.return_type);
            emit(gen, " __ret;\n");
        }

        if (node->as.func_decl.body) {
            for (int i = 0; i < node->as.func_decl.body->as.block.stmts.count; i++) {
                emit_stmt(gen, node->as.func_decl.body->as.block.stmts.nodes[i]);
            }
        }

        // Emit cleanup section if there are defers
        if (gen->defer_count > 0) {
            emit(gen, "__cleanup:;\n");
            // Emit deferred statements in reverse order (LIFO)
            for (int i = gen->defer_count - 1; i >= 0; i--) {
                emit_stmt(gen, gen->defer_stack[i]);
            }
            // Emit final return
            emit_indent(gen);
            if (is_void) {
                emit(gen, "return;\n");
            } else {
                emit(gen, "return __ret;\n");
            }
        }

        gen->indent--;
        emit(gen, "}\n\n");

        // Clear defer stack
        defer_clear(gen);
        gen->current_return_type = NULL;
        break;
    }

    case NODE_VAR_DECL:
        // Global variable - emit static for private vars
        if (!node->as.var_decl.is_public) {
            emit(gen, "static ");
        }
        emit_stmt(gen, node);
        emit(gen, "\n");
        break;

    default:
        emit(gen, "/* unknown decl %d */\n", node->type);
        break;
    }
}

// Check if two tuple types are structurally equal
static int tuple_types_equal(Type* a, Type* b) {
    if (a->as.tuple.elem_count != b->as.tuple.elem_count)
        return 0;
    for (int i = 0; i < a->as.tuple.elem_count; i++) {
        if (!type_equals(a->as.tuple.elem_types[i], b->as.tuple.elem_types[i]))
            return 0;
    }
    return 1;
}

// Register a tuple type and return its index (for typedef name)
static int register_tuple_type(CodeGen* gen, Type* type) {
    // Check if already registered
    for (int i = 0; i < gen->tuple_type_count; i++) {
        if (tuple_types_equal(gen->tuple_types[i], type))
            return i;
    }
    // Add new
    if (gen->tuple_type_count >= gen->tuple_type_capacity) {
        int new_cap              = gen->tuple_type_capacity == 0 ? 8 : gen->tuple_type_capacity * 2;
        gen->tuple_types         = realloc(gen->tuple_types, new_cap * sizeof(Type*));
        gen->tuple_type_capacity = new_cap;
    }
    gen->tuple_types[gen->tuple_type_count] = type;
    return gen->tuple_type_count++;
}

// Recursively register tuple types from a Type
static void collect_tuple_types_from_type(CodeGen* gen, Type* type) {
    if (!type)
        return;
    if (type->kind == TYPE_TUPLE) {
        register_tuple_type(gen, type);
        for (int i = 0; i < type->as.tuple.elem_count; i++) {
            collect_tuple_types_from_type(gen, type->as.tuple.elem_types[i]);
        }
    } else if (type->kind == TYPE_ARRAY) {
        collect_tuple_types_from_type(gen, type->as.array.elem);
    }
}

// Collect tuple types from a type node
static void collect_tuple_types_from_node(CodeGen* gen, Node* type_node);

// Build a Type* from a tuple literal (for type collection)
static Type* type_from_tuple_lit(Node* node) {
    int    count = node->as.tuple_lit.elements.count;
    Type** elems = malloc(count * sizeof(Type*));
    for (int i = 0; i < count; i++) {
        Node* elem = node->as.tuple_lit.elements.nodes[i];
        switch (elem->type) {
        case NODE_INT_LIT:
            elems[i] = type_int64;
            break;
        case NODE_FLOAT_LIT:
            elems[i] = type_f32;
            break;
        case NODE_BOOL_LIT:
            elems[i] = type_bool;
            break;
        case NODE_STRING_LIT:
            elems[i] = type_string;
            break;
        case NODE_CHAR_LIT:
            elems[i] = type_char;
            break;
        case NODE_TUPLE_LIT:
            elems[i] = type_from_tuple_lit(elem);
            break;
        default:
            elems[i] = type_int64; // Default
            break;
        }
    }
    return type_tuple(elems, count);
}

// Collect tuple types from an expression node
static void collect_tuple_types_from_expr(CodeGen* gen, Node* node) {
    if (!node)
        return;
    switch (node->type) {
    case NODE_TUPLE_LIT: {
        // Build and register the tuple type
        Type* tuple = type_from_tuple_lit(node);
        register_tuple_type(gen, tuple);
        // Collect from nested tuples
        for (int i = 0; i < node->as.tuple_lit.elements.count; i++) {
            collect_tuple_types_from_expr(gen, node->as.tuple_lit.elements.nodes[i]);
        }
        break;
    }
    case NODE_BINARY:
        collect_tuple_types_from_expr(gen, node->as.binary.left);
        collect_tuple_types_from_expr(gen, node->as.binary.right);
        break;
    case NODE_UNARY:
        collect_tuple_types_from_expr(gen, node->as.unary.operand);
        break;
    case NODE_CALL:
        collect_tuple_types_from_expr(gen, node->as.call.func);
        for (int i = 0; i < node->as.call.args.count; i++) {
            collect_tuple_types_from_expr(gen, node->as.call.args.nodes[i]);
        }
        break;
    case NODE_INDEX:
        collect_tuple_types_from_expr(gen, node->as.index.object);
        collect_tuple_types_from_expr(gen, node->as.index.index);
        break;
    case NODE_MEMBER:
        collect_tuple_types_from_expr(gen, node->as.member.object);
        break;
    case NODE_ASSIGN:
        collect_tuple_types_from_expr(gen, node->as.assign.target);
        collect_tuple_types_from_expr(gen, node->as.assign.value);
        break;
    default:
        break;
    }
}

// Collect tuple types from a statement node
static void collect_tuple_types_from_stmt(CodeGen* gen, Node* node) {
    if (!node)
        return;
    switch (node->type) {
    case NODE_VAR_DECL:
        if (node->as.var_decl.type)
            collect_tuple_types_from_node(gen, node->as.var_decl.type);
        if (node->as.var_decl.init)
            collect_tuple_types_from_expr(gen, node->as.var_decl.init);
        // Also collect from destructuring type if present
        if (node->as.var_decl.destruct_tuple_type) {
            collect_tuple_types_from_type(gen, (Type*)node->as.var_decl.destruct_tuple_type);
        }
        break;
    case NODE_EXPR_STMT:
        collect_tuple_types_from_expr(gen, node->as.expr_stmt.expr);
        break;
    case NODE_BLOCK:
        for (int i = 0; i < node->as.block.stmts.count; i++) {
            collect_tuple_types_from_stmt(gen, node->as.block.stmts.nodes[i]);
        }
        break;
    case NODE_IF:
        collect_tuple_types_from_expr(gen, node->as.if_stmt.cond);
        collect_tuple_types_from_stmt(gen, node->as.if_stmt.then_block);
        if (node->as.if_stmt.else_block)
            collect_tuple_types_from_stmt(gen, node->as.if_stmt.else_block);
        break;
    case NODE_WHILE:
        collect_tuple_types_from_expr(gen, node->as.while_stmt.cond);
        collect_tuple_types_from_stmt(gen, node->as.while_stmt.body);
        break;
    case NODE_FOR:
        if (node->as.for_stmt.init)
            collect_tuple_types_from_stmt(gen, node->as.for_stmt.init);
        if (node->as.for_stmt.cond)
            collect_tuple_types_from_expr(gen, node->as.for_stmt.cond);
        if (node->as.for_stmt.post)
            collect_tuple_types_from_expr(gen, node->as.for_stmt.post);
        collect_tuple_types_from_stmt(gen, node->as.for_stmt.body);
        break;
    case NODE_RETURN:
        if (node->as.return_stmt.value)
            collect_tuple_types_from_expr(gen, node->as.return_stmt.value);
        break;
    case NODE_DEFER:
        collect_tuple_types_from_stmt(gen, node->as.defer_stmt.stmt);
        break;
    default:
        break;
    }
}

// Build a Type* from a type node (for codegen purposes, simplified)
static Type* type_from_node(Node* type_node) {
    if (!type_node)
        return type_void;

    switch (type_node->type) {
    case NODE_IDENT: {
        const char* name = type_node->as.ident.name;
        if (strcmp(name, "void") == 0)
            return type_void;
        if (strcmp(name, "bool") == 0)
            return type_bool;
        if (strcmp(name, "i64") == 0)
            return type_int64;
        if (strcmp(name, "i8") == 0)
            return type_int8;
        if (strcmp(name, "i16") == 0)
            return type_int16;
        if (strcmp(name, "i32") == 0)
            return type_int32;
        if (strcmp(name, "u64") == 0)
            return type_uint64;
        if (strcmp(name, "u8") == 0)
            return type_uint8;
        if (strcmp(name, "u16") == 0)
            return type_uint16;
        if (strcmp(name, "u32") == 0)
            return type_uint32;
        if (strcmp(name, "f32") == 0)
            return type_f32;
        if (strcmp(name, "f64") == 0)
            return type_f64;
        if (strcmp(name, "char") == 0)
            return type_char;
        if (strcmp(name, "string") == 0)
            return type_string;
        // User-defined type - return a struct type
        return type_struct(name);
    }
    case NODE_INDEX: {
        Type* elem = type_from_node(type_node->as.index.object);
        int   size = -1;
        if (type_node->as.index.index && type_node->as.index.index->type == NODE_INT_LIT) {
            size = (int)type_node->as.index.index->as.int_lit.value;
        }
        return type_array(elem, size);
    }
    case NODE_TUPLE_TYPE: {
        int    count = type_node->as.tuple_type.elem_types.count;
        Type** elems = malloc(count * sizeof(Type*));
        for (int i = 0; i < count; i++) {
            elems[i] = type_from_node(type_node->as.tuple_type.elem_types.nodes[i]);
        }
        return type_tuple(elems, count);
    }
    default:
        return type_error;
    }
}

// Collect tuple types from a type node
static void collect_tuple_types_from_node(CodeGen* gen, Node* type_node) {
    if (!type_node)
        return;
    if (type_node->type == NODE_TUPLE_TYPE) {
        // Build a Type* and register it
        Type* tuple = type_from_node(type_node);
        register_tuple_type(gen, tuple);
        // Recurse into element types
        for (int i = 0; i < type_node->as.tuple_type.elem_types.count; i++) {
            collect_tuple_types_from_node(gen, type_node->as.tuple_type.elem_types.nodes[i]);
        }
    } else if (type_node->type == NODE_INDEX) {
        // Array type - recurse into element type
        collect_tuple_types_from_node(gen, type_node->as.index.object);
    }
}

// Collect all tuple types from a declaration
static void collect_tuple_types_from_decl(CodeGen* gen, Node* decl) {
    if (!decl)
        return;
    switch (decl->type) {
    case NODE_FUNC_DECL:
        if (decl->as.func_decl.return_type)
            collect_tuple_types_from_node(gen, decl->as.func_decl.return_type);
        for (int i = 0; i < decl->as.func_decl.params.count; i++) {
            Node* param = decl->as.func_decl.params.nodes[i];
            if (param->as.param.type)
                collect_tuple_types_from_node(gen, param->as.param.type);
        }
        if (decl->as.func_decl.body)
            collect_tuple_types_from_stmt(gen, decl->as.func_decl.body);
        break;
    case NODE_VAR_DECL:
        if (decl->as.var_decl.type)
            collect_tuple_types_from_node(gen, decl->as.var_decl.type);
        if (decl->as.var_decl.init)
            collect_tuple_types_from_expr(gen, decl->as.var_decl.init);
        break;
    case NODE_STRUCT_DECL:
        for (int i = 0; i < decl->as.struct_decl.fields.count; i++) {
            Node* field = decl->as.struct_decl.fields.nodes[i];
            if (field->as.field.type)
                collect_tuple_types_from_node(gen, field->as.field.type);
        }
        break;
    default:
        break;
    }
}

void codegen_init(CodeGen* gen, FILE* out) {
    gen->out                 = out;
    gen->indent              = 0;
    gen->temp_count          = 0;
    gen->defer_stack         = NULL;
    gen->defer_count         = 0;
    gen->defer_capacity      = 0;
    gen->current_return_type = NULL;
    gen->tuple_types         = NULL;
    gen->tuple_type_count    = 0;
    gen->tuple_type_capacity = 0;
}

void codegen_emit(CodeGen* gen, Node* ast) {
    if (!ast || ast->type != NODE_PROGRAM)
        return;

    // First pass: collect all tuple types
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            collect_tuple_types_from_decl(gen, mod->as.module.decls.nodes[i]);
        }
    }

    // Emit header
    emit(gen, "/* Generated by whist compiler */\n");
    emit(gen, "#include <stdint.h>\n");
    emit(gen, "#include <stdbool.h>\n");
    emit(gen, "#include <stddef.h>\n");
    emit(gen, "#include <stdlib.h>\n");
    emit(gen, "\n");

    // Emit tuple typedefs
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

    // Forward declarations for structs
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type == NODE_STRUCT_DECL) {
                emit(gen, "typedef struct %s %s;\n", decl->as.struct_decl.name,
                     decl->as.struct_decl.name);
            }
        }
    }
    emit(gen, "\n");

    // Forward declarations for functions and methods
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        // Determine if this is a library module (not "main")
        const char* module_prefix = NULL;
        if (strcmp(mod->as.module.name, "main") != 0) {
            module_prefix = mod->as.module.name;
        }
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type == NODE_FUNC_DECL) {
                func_decl_node* fdn       = &decl->as.func_decl;
                int             is_method = (fdn->receiver_type != NULL);

                // Emit static for private functions (except main)
                if (!fdn->is_public && strcmp(fdn->name, "main") != 0) {
                    emit(gen, "static ");
                }

                emit_type(gen, fdn->return_type);

                // Emit function name with appropriate prefix
                emit_function_name(gen, fdn->name, is_method ? fdn->receiver_type : NULL,
                                   module_prefix);

                // Emit self parameter for methods
                if (is_method) {
                    if (fdn->receiver_is_const) {
                        emit(gen, "const ");
                    }
                    emit(gen, "%s* self", fdn->receiver_type);
                    if (fdn->params.count > 0) {
                        emit(gen, ", ");
                    }
                }

                if (fdn->params.count == 0 && !is_method) {
                    emit(gen, "void");
                } else {
                    for (int j = 0; j < fdn->params.count; j++) {
                        if (j > 0)
                            emit(gen, ", ");
                        Node* param = fdn->params.nodes[j];
                        emit_type_with_name(gen, param->as.param.type, param->as.param.name);
                    }
                }
                emit(gen, ");\n");
            }
        }
    }
    emit(gen, "\n");

    // Emit all declarations
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        // Set current module context (NULL for "main", module name for library imports)
        gen->current_module = strcmp(mod->as.module.name, "main") == 0 ? NULL : mod->as.module.name;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            emit_decl(gen, mod->as.module.decls.nodes[i]);
        }
    }
    gen->current_module = NULL;
}
