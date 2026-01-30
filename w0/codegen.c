#include "codegen.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

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

static void emit_type(CodeGen* gen, Node* type_node);
static void emit_expr(CodeGen* gen, Node* node);
static void emit_stmt(CodeGen* gen, Node* node);
static void emit_decl(CodeGen* gen, Node* node);
static void emit_struct_init(CodeGen* gen, Node* node);

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
            // User-defined type (struct/enum)
            emit(gen, "%s", name);
        }
        break;
    }
    case NODE_UNARY:
        // Pointer type: *T -> T*
        if (type_node->as.unary.op == TOK_STAR) {
            emit_type(gen, type_node->as.unary.operand);
            emit(gen, "*");
        }
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

    case NODE_CALL:
        emit_expr(gen, node->as.call.func);
        emit(gen, "(");
        for (int i = 0; i < node->as.call.args.count; i++) {
            if (i > 0)
                emit(gen, ", ");
            emit_expr(gen, node->as.call.args.nodes[i]);
        }
        emit(gen, ")");
        break;

    case NODE_INDEX:
        emit_expr(gen, node->as.index.object);
        emit(gen, "[");
        emit_expr(gen, node->as.index.index);
        emit(gen, "]");
        break;

    case NODE_MEMBER:
        emit_expr(gen, node->as.member.object);
        emit(gen, "%s%.*s", node->as.member.arrow ? "->" : ".", node->as.member.length,
             node->as.member.name);
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
        emit_indent(gen);
        if (node->as.var_decl.is_const) {
            emit(gen, "const ");
        }
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
            emit(gen, " = ");
            emit_expr(gen, node->as.var_decl.init);
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

    case NODE_RETURN:
        emit_indent(gen);
        emit(gen, "return");
        if (node->as.return_stmt.value) {
            emit(gen, " ");
            emit_expr(gen, node->as.return_stmt.value);
        }
        emit(gen, ";\n");
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
        // Return type
        emit_type(gen, node->as.func_decl.return_type);
        emit(gen, " %s(", node->as.func_decl.name);

        // Parameters
        if (node->as.func_decl.params.count == 0) {
            emit(gen, "void");
        } else {
            for (int i = 0; i < node->as.func_decl.params.count; i++) {
                if (i > 0)
                    emit(gen, ", ");
                Node* param = node->as.func_decl.params.nodes[i];
                emit_type_with_name(gen, param->as.param.type, param->as.param.name);
            }
        }
        emit(gen, ") {\n");

        // Body
        gen->indent++;
        if (node->as.func_decl.body) {
            for (int i = 0; i < node->as.func_decl.body->as.block.stmts.count; i++) {
                emit_stmt(gen, node->as.func_decl.body->as.block.stmts.nodes[i]);
            }
        }
        gen->indent--;
        emit(gen, "}\n\n");
        break;
    }

    case NODE_VAR_DECL:
        // Global variable
        emit_stmt(gen, node);
        emit(gen, "\n");
        break;

    default:
        emit(gen, "/* unknown decl %d */\n", node->type);
        break;
    }
}

void codegen_init(CodeGen* gen, FILE* out) {
    gen->out        = out;
    gen->indent     = 0;
    gen->temp_count = 0;
}

void codegen_emit(CodeGen* gen, Node* ast) {
    if (!ast || ast->type != NODE_PROGRAM)
        return;

    // Emit header
    emit(gen, "/* Generated by whist compiler */\n");
    emit(gen, "#include <stdint.h>\n");
    emit(gen, "#include <stdbool.h>\n");
    emit(gen, "#include <stddef.h>\n");
    emit(gen, "\n");

    // Forward declarations for structs
    for (int i = 0; i < ast->as.program.decls.count; i++) {
        Node* decl = ast->as.program.decls.nodes[i];
        if (decl->type == NODE_STRUCT_DECL) {
            emit(gen, "typedef struct %s %s;\n", decl->as.struct_decl.name,
                 decl->as.struct_decl.name);
        }
    }
    emit(gen, "\n");

    // Forward declarations for functions
    for (int i = 0; i < ast->as.program.decls.count; i++) {
        Node* decl = ast->as.program.decls.nodes[i];
        if (decl->type == NODE_FUNC_DECL) {
            emit_type(gen, decl->as.func_decl.return_type);
            emit(gen, " %s(", decl->as.func_decl.name);
            if (decl->as.func_decl.params.count == 0) {
                emit(gen, "void");
            } else {
                for (int j = 0; j < decl->as.func_decl.params.count; j++) {
                    if (j > 0)
                        emit(gen, ", ");
                    Node* param = decl->as.func_decl.params.nodes[j];
                    emit_type_with_name(gen, param->as.param.type, param->as.param.name);
                }
            }
            emit(gen, ");\n");
        }
    }
    emit(gen, "\n");

    // Emit all declarations
    for (int i = 0; i < ast->as.program.decls.count; i++) {
        emit_decl(gen, ast->as.program.decls.nodes[i]);
    }
}
