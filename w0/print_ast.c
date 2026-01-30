#include "print_ast.h"
#include "lexer.h"
#include <stdio.h>

static void print_indent(int depth) {
    for (int i = 0; i < depth; i++)
        printf("  ");
}

void print_ast(Node* node, int depth) {
    if (!node) {
        print_indent(depth);
        printf("(null)\n");
        return;
    }

    print_indent(depth);

    switch (node->type) {
    case NODE_PROGRAM:
        printf("Program\n");
        for (int i = 0; i < node->as.program.decls.count; i++) {
            print_ast(node->as.program.decls.nodes[i], depth + 1);
        }
        break;

    case NODE_FUNC_DECL:
        printf("FuncDecl: %.*s\n", node->as.func_decl.name_length, node->as.func_decl.name);
        if (node->as.func_decl.params.count > 0) {
            print_indent(depth + 1);
            printf("Params:\n");
            for (int i = 0; i < node->as.func_decl.params.count; i++) {
                print_ast(node->as.func_decl.params.nodes[i], depth + 2);
            }
        }
        if (node->as.func_decl.return_type) {
            print_indent(depth + 1);
            printf("ReturnType:\n");
            print_ast(node->as.func_decl.return_type, depth + 2);
        }
        print_indent(depth + 1);
        printf("Body:\n");
        print_ast(node->as.func_decl.body, depth + 2);
        break;

    case NODE_PARAM:
        printf("Param: %.*s", node->as.param.name_length, node->as.param.name);
        if (node->as.param.type) {
            printf("\n");
            print_indent(depth + 1);
            printf("Type:\n");
            print_ast(node->as.param.type, depth + 2);
        } else {
            printf("\n");
        }
        break;

    case NODE_STRUCT_DECL:
        printf("StructDecl: %.*s\n", node->as.struct_decl.name_length, node->as.struct_decl.name);
        for (int i = 0; i < node->as.struct_decl.fields.count; i++) {
            print_ast(node->as.struct_decl.fields.nodes[i], depth + 1);
        }
        break;

    case NODE_FIELD:
        printf("Field: %.*s\n", node->as.field.name_length, node->as.field.name);
        print_ast(node->as.field.type, depth + 1);
        break;

    case NODE_ENUM_DECL:
        printf("EnumDecl: %.*s\n", node->as.enum_decl.name_length, node->as.enum_decl.name);
        for (int i = 0; i < node->as.enum_decl.values.count; i++) {
            print_ast(node->as.enum_decl.values.nodes[i], depth + 1);
        }
        break;

    case NODE_VAR_DECL:
        printf("VarDecl: %.*s%s\n", node->as.var_decl.name_length, node->as.var_decl.name,
               node->as.var_decl.is_const ? " (const)" : "");
        if (node->as.var_decl.type) {
            print_indent(depth + 1);
            printf("Type:\n");
            print_ast(node->as.var_decl.type, depth + 2);
        }
        if (node->as.var_decl.init) {
            print_indent(depth + 1);
            printf("Init:\n");
            print_ast(node->as.var_decl.init, depth + 2);
        }
        break;

    case NODE_BLOCK:
        printf("Block\n");
        for (int i = 0; i < node->as.block.stmts.count; i++) {
            print_ast(node->as.block.stmts.nodes[i], depth + 1);
        }
        break;

    case NODE_IF:
        printf("If\n");
        print_indent(depth + 1);
        printf("Cond:\n");
        print_ast(node->as.if_stmt.cond, depth + 2);
        print_indent(depth + 1);
        printf("Then:\n");
        print_ast(node->as.if_stmt.then_block, depth + 2);
        if (node->as.if_stmt.else_block) {
            print_indent(depth + 1);
            printf("Else:\n");
            print_ast(node->as.if_stmt.else_block, depth + 2);
        }
        break;

    case NODE_WHILE:
        printf("While\n");
        print_indent(depth + 1);
        printf("Cond:\n");
        print_ast(node->as.while_stmt.cond, depth + 2);
        print_indent(depth + 1);
        printf("Body:\n");
        print_ast(node->as.while_stmt.body, depth + 2);
        break;

    case NODE_FOR:
        printf("For\n");
        if (node->as.for_stmt.init) {
            print_indent(depth + 1);
            printf("Init:\n");
            print_ast(node->as.for_stmt.init, depth + 2);
        }
        if (node->as.for_stmt.cond) {
            print_indent(depth + 1);
            printf("Cond:\n");
            print_ast(node->as.for_stmt.cond, depth + 2);
        }
        if (node->as.for_stmt.post) {
            print_indent(depth + 1);
            printf("Post:\n");
            print_ast(node->as.for_stmt.post, depth + 2);
        }
        print_indent(depth + 1);
        printf("Body:\n");
        print_ast(node->as.for_stmt.body, depth + 2);
        break;

    case NODE_RETURN:
        printf("Return\n");
        if (node->as.return_stmt.value) {
            print_ast(node->as.return_stmt.value, depth + 1);
        }
        break;

    case NODE_BREAK:
        printf("Break\n");
        break;

    case NODE_CONTINUE:
        printf("Continue\n");
        break;

    case NODE_EXPR_STMT:
        printf("ExprStmt\n");
        print_ast(node->as.expr_stmt.expr, depth + 1);
        break;

    case NODE_BINARY:
        printf("Binary: %s\n", token_type_name(node->as.binary.op));
        print_ast(node->as.binary.left, depth + 1);
        print_ast(node->as.binary.right, depth + 1);
        break;

    case NODE_UNARY:
        printf("Unary: %s%s\n", token_type_name(node->as.unary.op),
               node->as.unary.postfix ? " (postfix)" : "");
        print_ast(node->as.unary.operand, depth + 1);
        break;

    case NODE_CALL:
        printf("Call\n");
        print_indent(depth + 1);
        printf("Func:\n");
        print_ast(node->as.call.func, depth + 2);
        if (node->as.call.args.count > 0) {
            print_indent(depth + 1);
            printf("Args:\n");
            for (int i = 0; i < node->as.call.args.count; i++) {
                print_ast(node->as.call.args.nodes[i], depth + 2);
            }
        }
        break;

    case NODE_INDEX:
        printf("Index\n");
        print_ast(node->as.index.object, depth + 1);
        if (node->as.index.index) {
            print_ast(node->as.index.index, depth + 1);
        }
        break;

    case NODE_MEMBER:
        printf("Member: %.*s%s\n", node->as.member.length, node->as.member.name,
               node->as.member.arrow ? " (->)" : "");
        print_ast(node->as.member.object, depth + 1);
        break;

    case NODE_ASSIGN:
        printf("Assign: %s\n", token_type_name(node->as.assign.op));
        print_ast(node->as.assign.target, depth + 1);
        print_ast(node->as.assign.value, depth + 1);
        break;

    case NODE_STRUCT_INIT:
        printf("StructInit\n");
        for (int i = 0; i < node->as.struct_init.fields.count; i++) {
            print_ast(node->as.struct_init.fields.nodes[i], depth + 1);
        }
        break;

    case NODE_FIELD_INIT:
        printf("FieldInit: %.*s\n", node->as.field_init.name_length, node->as.field_init.name);
        print_ast(node->as.field_init.value, depth + 1);
        break;

    case NODE_INT_LIT:
        printf("Int: %ld\n", node->as.int_lit.value);
        break;

    case NODE_FLOAT_LIT:
        printf("Float: %g\n", node->as.float_lit.value);
        break;

    case NODE_STRING_LIT:
        printf("String: \"%.*s\"\n", node->as.string_lit.length, node->as.string_lit.value);
        break;

    case NODE_CHAR_LIT:
        printf("Char: '%c'\n", node->as.char_lit.value);
        break;

    case NODE_BOOL_LIT:
        printf("Bool: %s\n", node->as.bool_lit.value ? "true" : "false");
        break;

    case NODE_NULL_LIT:
        printf("Null\n");
        break;

    case NODE_IDENT:
        printf("Ident: %.*s\n", node->as.ident.length, node->as.ident.name);
        break;

    default:
        printf("Unknown node type: %d\n", node->type);
        break;
    }
}
