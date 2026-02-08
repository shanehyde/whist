#include "print_ast.h"

#include <stdio.h>

#include "lexer.h"

static void print_indent(int depth) {
    for (int i = 0; i < depth; i++)
        printf("  ");
}

// Print a destructuring pattern (recursive)
static void print_destruct_pattern(DestructPattern* pattern) {
    if (!pattern)
        return;

    switch (pattern->kind) {
    case PATTERN_IDENT:
        printf("%.*s", pattern->as.ident.name_length, pattern->as.ident.name);
        break;
    case PATTERN_TUPLE:
        printf("(");
        for (int i = 0; i < pattern->as.tuple.count; i++) {
            if (i > 0)
                printf(", ");
            print_destruct_pattern(pattern->as.tuple.elements[i]);
        }
        printf(")");
        break;
    }
}

static void print_visibility(int depth, int is_public) {
    print_indent(depth);
    if (is_public) {
        printf("Visibility: public\n");
    } else {
        printf("Visibility: private\n");
    }
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
        for (int i = 0; i < node->as.program.modules.count; i++) {
            print_ast(node->as.program.modules.nodes[i], depth + 1);
        }
        break;
    case NODE_MODULE:
        printf("Module: %.*s\n", node->as.module.name_length, node->as.module.name);
        for (int i = 0; i < node->as.module.decls.count; i++) {
            print_ast(node->as.module.decls.nodes[i], depth + 1);
        }
        break;
    case NODE_EXTERN_MODULE:
        printf("ExternModule: %.*s\n", node->as.extern_module.module_name_length,
               node->as.extern_module.module_name);
        for (int i = 0; i < node->as.extern_module.decls.count; i++) {
            print_ast(node->as.extern_module.decls.nodes[i], depth + 1);
        }
        break;
    case NODE_FUNC_DECL:
        if (node->as.func_decl.receiver_type) {
            printf("MethodDecl: (%s%.*s) %.*s\n",
                   node->as.func_decl.receiver_is_const ? "const " : "",
                   node->as.func_decl.receiver_type_len, node->as.func_decl.receiver_type,
                   node->as.func_decl.name_length, node->as.func_decl.name);
        } else {
            printf("FuncDecl: %.*s\n", node->as.func_decl.name_length, node->as.func_decl.name);
        }
        print_visibility(depth + 1, node->as.func_decl.is_public);
        if (node->as.func_decl.receiver_type_args.count > 0) {
            print_indent(depth + 1);
            printf("ReceiverTypeArgs:\n");
            for (int i = 0; i < node->as.func_decl.receiver_type_args.count; i++) {
                print_ast(node->as.func_decl.receiver_type_args.nodes[i], depth + 2);
            }
        }
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
        print_visibility(depth + 1, node->as.struct_decl.is_public);
        if (node->as.struct_decl.type_param_count > 0) {
            print_indent(depth + 1);
            printf("TypeParams: <");
            for (int i = 0; i < node->as.struct_decl.type_param_count; i++) {
                if (i > 0)
                    printf(", ");
                printf("%s", node->as.struct_decl.type_params[i]);
            }
            printf(">\n");
        }
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
        print_visibility(depth + 1, node->as.enum_decl.is_public);
        for (int i = 0; i < node->as.enum_decl.values.count; i++) {
            print_ast(node->as.enum_decl.values.nodes[i], depth + 1);
        }
        break;

    case NODE_ENUM_VARIANT:
        printf("EnumVariant: %.*s", node->as.enum_variant.name_length, node->as.enum_variant.name);
        if (node->as.enum_variant.types.count > 0) {
            printf("(");
            for (int i = 0; i < node->as.enum_variant.types.count; i++) {
                if (i > 0)
                    printf(", ");
                printf("...");
            }
            printf(")");
        }
        printf("\n");
        for (int i = 0; i < node->as.enum_variant.types.count; i++) {
            print_ast(node->as.enum_variant.types.nodes[i], depth + 1);
        }
        break;

    case NODE_VAR_DECL:
        if (node->as.var_decl.destruct_pattern) {
            printf("VarDecl: ");
            print_destruct_pattern(node->as.var_decl.destruct_pattern);
            printf("%s\n", node->as.var_decl.is_const ? " (const)" : "");
        } else {
            printf("VarDecl: %.*s%s\n", node->as.var_decl.name_length, node->as.var_decl.name,
                   node->as.var_decl.is_const ? " (const)" : "");
        }
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

    case NODE_DEFER:
        printf("Defer\n");
        print_ast(node->as.defer_stmt.stmt, depth + 1);
        break;

    case NODE_MATCH:
        printf("Match\n");
        print_indent(depth + 1);
        printf("Expr:\n");
        print_ast(node->as.match_stmt.expr, depth + 2);
        print_indent(depth + 1);
        printf("Arms:\n");
        for (int i = 0; i < node->as.match_stmt.arms.count; i++) {
            print_ast(node->as.match_stmt.arms.nodes[i], depth + 2);
        }
        break;

    case NODE_MATCH_ARM:
        if (node->as.match_arm.is_wildcard) {
            printf("MatchArm: _\n");
        } else {
            printf("MatchArm: ");
            if (node->as.match_arm.enum_name) {
                printf("%.*s::", node->as.match_arm.enum_name_length, node->as.match_arm.enum_name);
            }
            printf("%.*s", node->as.match_arm.variant_name_length, node->as.match_arm.variant_name);
            if (node->as.match_arm.binding_count > 0) {
                printf("(");
                for (int i = 0; i < node->as.match_arm.binding_count; i++) {
                    if (i > 0)
                        printf(", ");
                    printf("%s", node->as.match_arm.bindings[i]);
                }
                printf(")");
            }
            printf("\n");
        }
        print_indent(depth + 1);
        printf("Body:\n");
        print_ast(node->as.match_arm.body, depth + 2);
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
        printf("Unary: %s\n", token_type_name(node->as.unary.op));
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
        printf("Member: %.*s\n", node->as.member.length, node->as.member.name);
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

    case NODE_ENUM_VALUE:
        printf("EnumValue: %.*s::%.*s\n", node->as.enum_value.enum_name_length,
               node->as.enum_value.enum_name, node->as.enum_value.value_name_length,
               node->as.enum_value.value_name);
        if (node->as.enum_value.args.count > 0) {
            print_indent(depth + 1);
            printf("Args:\n");
            for (int i = 0; i < node->as.enum_value.args.count; i++) {
                print_ast(node->as.enum_value.args.nodes[i], depth + 2);
            }
        }
        break;

    case NODE_NEW_EXPR:
        printf("NewExpr\n");
        print_indent(depth + 1);
        printf("Type:\n");
        print_ast(node->as.new_expr.type_node, depth + 2);
        print_indent(depth + 1);
        printf("Init:\n");
        print_ast(node->as.new_expr.init, depth + 2);
        break;

    case NODE_TUPLE_TYPE:
        printf("TupleType\n");
        for (int i = 0; i < node->as.tuple_type.elem_types.count; i++) {
            print_ast(node->as.tuple_type.elem_types.nodes[i], depth + 1);
        }
        break;

    case NODE_TUPLE_LIT:
        printf("TupleLit\n");
        for (int i = 0; i < node->as.tuple_lit.elements.count; i++) {
            print_ast(node->as.tuple_lit.elements.nodes[i], depth + 1);
        }
        break;

    case NODE_ARRAY_TYPE:
        printf("ArrayType\n");
        print_indent(depth + 1);
        printf("ElemType:\n");
        print_ast(node->as.array_type.elem_type, depth + 2);
        if (node->as.array_type.size) {
            print_indent(depth + 1);
            printf("Size:\n");
            print_ast(node->as.array_type.size, depth + 2);
        }
        break;

    case NODE_GENERIC_TYPE:
        printf("GenericType: %.*s\n", node->as.generic_type.base_name_length,
               node->as.generic_type.base_name);
        print_indent(depth + 1);
        printf("TypeArgs:\n");
        for (int i = 0; i < node->as.generic_type.type_args.count; i++) {
            print_ast(node->as.generic_type.type_args.nodes[i], depth + 2);
        }
        break;

    case NODE_TRAIT_DECL:
        printf("TraitDecl: %.*s\n", node->as.trait_decl.name_length, node->as.trait_decl.name);
        for (int i = 0; i < node->as.trait_decl.methods.count; i++) {
            print_ast(node->as.trait_decl.methods.nodes[i], depth + 1);
        }
        break;

    case NODE_TYPE_ALIAS:
        printf("TypeAlias: %.*s", node->as.type_alias.name_length, node->as.type_alias.name);
        if (node->as.type_alias.type_param_count > 0) {
            printf("<");
            for (int i = 0; i < node->as.type_alias.type_param_count; i++) {
                if (i > 0)
                    printf(", ");
                printf("%s", node->as.type_alias.type_params[i]);
            }
            printf(">");
        }
        printf("\n");
        print_visibility(depth + 1, node->as.type_alias.is_public);
        print_indent(depth + 1);
        printf("Target:\n");
        print_ast(node->as.type_alias.target_type, depth + 2);
        break;

    case NODE_IMPL_DECL:
        printf("ImplDecl: %.*s for %.*s", node->as.impl_decl.trait_name_length,
               node->as.impl_decl.trait_name, node->as.impl_decl.type_name_length,
               node->as.impl_decl.type_name);
        if (node->as.impl_decl.type_args.count > 0) {
            printf("<");
            for (int i = 0; i < node->as.impl_decl.type_args.count; i++) {
                if (i > 0)
                    printf(", ");
                // Print type arg inline (simplified)
                Node* ta = node->as.impl_decl.type_args.nodes[i];
                if (ta->type == NODE_IDENT) {
                    printf("%.*s", ta->as.ident.length, ta->as.ident.name);
                } else {
                    printf("?");
                }
            }
            printf(">");
        }
        printf("\n");
        for (int i = 0; i < node->as.impl_decl.methods.count; i++) {
            print_ast(node->as.impl_decl.methods.nodes[i], depth + 1);
        }
        break;

    default:
        printf("Unknown node type: %d\n", node->type);
        break;
    }
}
