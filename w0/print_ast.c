#include "print_ast.h"

#include <stdio.h>

#include "lexer.h"

static void print_indent(int depth) {
    for (int i = 0; i < depth; i++)
        printf("  ");
}

// Helper: print a node list with a label
static void print_node_list(const char* label, NodeList* list, int depth) {
    if (label) {
        print_indent(depth);
        printf("%s:\n", label);
    }
    for (int i = 0; i < list->count; i++) {
        print_ast(list->nodes[i], depth + 1);
    }
}

static void print_labeled_child(const char* label, Node* child, int depth) {
    print_indent(depth);
    printf("%s:\n", label);
    print_ast(child, depth + 1);
}

static void print_optional_labeled_child(const char* label, Node* child, int depth) {
    if (child) {
        print_labeled_child(label, child, depth);
    }
}

static void print_inline_type_params(char** type_params, int type_param_count) {
    if (type_param_count <= 0) {
        return;
    }

    printf("<");
    for (int i = 0; i < type_param_count; i++) {
        if (i > 0)
            printf(", ");
        printf("%s", type_params[i]);
    }
    printf(">");
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
    case PATTERN_STRUCT:
        printf("{");
        for (int i = 0; i < pattern->as.struc.count; i++) {
            if (i > 0)
                printf(", ");
            printf("%s", pattern->as.struc.field_names[i]);
        }
        printf("}");
        break;
    }
}

static void print_visibility(int depth, int is_public) {
    print_indent(depth);
    printf("Visibility: %s\n", is_public ? "public" : "private");
}

// --- Declaration helpers ---

static void print_func_decl(Node* node, int depth) {
    if (node->as.func_decl.receiver_type) {
        printf("MethodDecl: (%s%.*s) %.*s\n", node->as.func_decl.receiver_is_const ? "const " : "",
               node->as.func_decl.receiver_type_len, node->as.func_decl.receiver_type,
               node->as.func_decl.name_length, node->as.func_decl.name);
    } else if (node->as.func_decl.extern_name) {
        printf("FuncDecl: %.*s (extern: %.*s)\n", node->as.func_decl.name_length,
               node->as.func_decl.name, node->as.func_decl.extern_name_length,
               node->as.func_decl.extern_name);
    } else {
        printf("FuncDecl: %.*s\n", node->as.func_decl.name_length, node->as.func_decl.name);
    }
    print_visibility(depth + 1, node->as.func_decl.is_public);
    if (node->as.func_decl.receiver_type_args.count > 0) {
        print_node_list("ReceiverTypeArgs", &node->as.func_decl.receiver_type_args, depth + 1);
    }
    if (node->as.func_decl.params.count > 0) {
        print_node_list("Params", &node->as.func_decl.params, depth + 1);
    }
    print_optional_labeled_child("ReturnType", node->as.func_decl.return_type, depth + 1);
    print_labeled_child("Body", node->as.func_decl.body, depth + 1);
}

static void print_struct_decl(Node* node, int depth) {
    printf("StructDecl: %.*s\n", node->as.struct_decl.name_length, node->as.struct_decl.name);
    print_visibility(depth + 1, node->as.struct_decl.is_public);
    if (node->as.struct_decl.type_param_count > 0) {
        print_indent(depth + 1);
        printf("TypeParams: ");
        print_inline_type_params(node->as.struct_decl.type_params,
                                 node->as.struct_decl.type_param_count);
        printf("\n");
    }
    print_node_list(NULL, &node->as.struct_decl.fields, depth);
}

static void print_enum_decl(Node* node, int depth) {
    printf("EnumDecl: %.*s\n", node->as.enum_decl.name_length, node->as.enum_decl.name);
    print_visibility(depth + 1, node->as.enum_decl.is_public);
    print_node_list(NULL, &node->as.enum_decl.values, depth);
}

static void print_enum_variant(Node* node, int depth) {
    printf("EnumVariant: %.*s", node->as.enum_variant.name_length, node->as.enum_variant.name);
    if (node->as.enum_variant.has_explicit_value) {
        printf(" = %ld", node->as.enum_variant.explicit_value);
    }
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
    print_node_list(NULL, &node->as.enum_variant.types, depth);
}

static void print_var_decl(Node* node, int depth) {
    if (node->as.var_decl.destruct_pattern) {
        printf("VarDecl: ");
        print_destruct_pattern(node->as.var_decl.destruct_pattern);
        printf("%s\n", node->as.var_decl.is_const ? " (const)" : "");
    } else {
        printf("VarDecl: %.*s%s\n", node->as.var_decl.name_length, node->as.var_decl.name,
               node->as.var_decl.is_const ? " (const)" : "");
    }
    if (node->as.var_decl.type) {
        print_labeled_child("Type", node->as.var_decl.type, depth + 1);
    }
    if (node->as.var_decl.init) {
        print_labeled_child("Init", node->as.var_decl.init, depth + 1);
    }
}

static void print_param(Node* node, int depth) {
    printf("Param: %.*s\n", node->as.param.name_length, node->as.param.name);
    print_optional_labeled_child("Type", node->as.param.type, depth + 1);
}

static void print_trait_decl(Node* node, int depth) {
    printf("TraitDecl: %.*s\n", node->as.trait_decl.name_length, node->as.trait_decl.name);
    print_node_list(NULL, &node->as.trait_decl.methods, depth);
}

static void print_type_alias(Node* node, int depth) {
    printf("TypeAlias: %.*s", node->as.type_alias.name_length, node->as.type_alias.name);
    print_inline_type_params(node->as.type_alias.type_params, node->as.type_alias.type_param_count);
    printf("\n");
    print_visibility(depth + 1, node->as.type_alias.is_public);
    print_labeled_child("Target", node->as.type_alias.target_type, depth + 1);
}

static void print_impl_decl(Node* node, int depth) {
    if (node->as.impl_decl.trait_name) {
        printf("ImplDecl: %.*s for %.*s", node->as.impl_decl.trait_name_length,
               node->as.impl_decl.trait_name, node->as.impl_decl.type_name_length,
               node->as.impl_decl.type_name);
    } else {
        printf("ImplDecl: %.*s", node->as.impl_decl.type_name_length, node->as.impl_decl.type_name);
    }
    if (node->as.impl_decl.type_args.count > 0) {
        printf("<");
        for (int i = 0; i < node->as.impl_decl.type_args.count; i++) {
            if (i > 0)
                printf(", ");
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
    print_node_list(NULL, &node->as.impl_decl.methods, depth);
}

static void print_use_decl(Node* node) {
    printf("UseDecl: %.*s.", node->as.use_decl.module_name_length, node->as.use_decl.module_name);
    if (node->as.use_decl.symbol_count == 1) {
        printf("%.*s\n", node->as.use_decl.symbol_name_lengths[0],
               node->as.use_decl.symbol_names[0]);
    } else {
        printf("{");
        for (int i = 0; i < node->as.use_decl.symbol_count; i++) {
            if (i > 0)
                printf(", ");
            printf("%.*s", node->as.use_decl.symbol_name_lengths[i],
                   node->as.use_decl.symbol_names[i]);
        }
        printf("}\n");
    }
}

// --- Statement helpers ---

static void print_if_stmt(Node* node, int depth) {
    printf("If\n");
    print_labeled_child("Cond", node->as.if_stmt.cond, depth + 1);
    print_labeled_child("Then", node->as.if_stmt.then_block, depth + 1);
    if (node->as.if_stmt.else_block) {
        print_labeled_child("Else", node->as.if_stmt.else_block, depth + 1);
    }
}

static void print_while_stmt(Node* node, int depth) {
    printf("While\n");
    print_labeled_child("Cond", node->as.while_stmt.cond, depth + 1);
    print_labeled_child("Body", node->as.while_stmt.body, depth + 1);
}

static void print_for_stmt(Node* node, int depth) {
    printf("For\n");
    print_optional_labeled_child("Init", node->as.for_stmt.init, depth + 1);
    print_optional_labeled_child("Cond", node->as.for_stmt.cond, depth + 1);
    print_optional_labeled_child("Post", node->as.for_stmt.post, depth + 1);
    print_labeled_child("Body", node->as.for_stmt.body, depth + 1);
}

static void print_match_stmt(Node* node, int depth) {
    printf("Match\n");
    print_indent(depth + 1);
    printf("Expr:\n");
    print_ast(node->as.match_stmt.expr, depth + 2);
    print_node_list("Arms", &node->as.match_stmt.arms, depth + 1);
}

static void print_match_arm(Node* node, int depth) {
    if (node->as.match_arm.is_wildcard) {
        printf("MatchArm: _\n");
    } else if (node->as.match_arm.pattern_expr) {
        printf("MatchArm: <value>\n");
        print_labeled_child("Pattern", node->as.match_arm.pattern_expr, depth + 1);
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
    print_labeled_child("Body", node->as.match_arm.body, depth + 1);
}

// --- Expression helpers ---

static void print_call_expr(Node* node, int depth) {
    printf("Call\n");
    print_labeled_child("Func", node->as.call.func, depth + 1);
    if (node->as.call.args.count > 0) {
        print_node_list("Args", &node->as.call.args, depth + 1);
    }
}

static void print_enum_value(Node* node, int depth) {
    printf("EnumValue: %.*s::%.*s\n", node->as.enum_value.enum_name_length,
           node->as.enum_value.enum_name, node->as.enum_value.value_name_length,
           node->as.enum_value.value_name);
    if (node->as.enum_value.args.count > 0) {
        print_node_list("Args", &node->as.enum_value.args, depth + 1);
    }
}

static void print_new_expr(Node* node, int depth) {
    printf("NewExpr\n");
    print_labeled_child("Type", node->as.new_expr.type_node, depth + 1);
    print_labeled_child("Init", node->as.new_expr.init, depth + 1);
}

static void print_cast_expr(Node* node, int depth) {
    printf("Cast\n");
    print_labeled_child("Expr", node->as.cast_expr.expr, depth + 1);
    print_labeled_child("TargetType", node->as.cast_expr.type_node, depth + 1);
}

// --- Type helpers ---

static void print_array_type(Node* node, int depth) {
    printf("ArrayType\n");
    print_labeled_child("ElemType", node->as.array_type.elem_type, depth + 1);
    if (node->as.array_type.size) {
        print_labeled_child("Size", node->as.array_type.size, depth + 1);
    }
}

static void print_generic_type(Node* node, int depth) {
    printf("GenericType: %.*s\n", node->as.generic_type.base_name_length,
           node->as.generic_type.base_name);
    print_node_list("TypeArgs", &node->as.generic_type.type_args, depth + 1);
}

// --- Main dispatch ---

void print_ast(Node* node, int depth) {
    if (!node) {
        print_indent(depth);
        printf("(null)\n");
        return;
    }

    print_indent(depth);

    switch (node->type) {
    // Declarations
    case NODE_PROGRAM:
        printf("Program\n");
        print_node_list(NULL, &node->as.program.modules, depth);
        break;
    case NODE_MODULE:
        printf("Module: %.*s\n", node->as.module.name_length, node->as.module.name);
        print_node_list(NULL, &node->as.module.decls, depth);
        break;
    case NODE_EXTERN_MODULE:
        printf("ExternModule: %.*s\n", node->as.extern_module.module_name_length,
               node->as.extern_module.module_name);
        print_node_list(NULL, &node->as.extern_module.decls, depth);
        break;
    case NODE_FUNC_DECL:
        print_func_decl(node, depth);
        break;
    case NODE_PARAM:
        print_param(node, depth);
        break;
    case NODE_STRUCT_DECL:
        print_struct_decl(node, depth);
        break;
    case NODE_FIELD:
        printf("Field: %.*s\n", node->as.field.name_length, node->as.field.name);
        print_ast(node->as.field.type, depth + 1);
        break;
    case NODE_ENUM_DECL:
        print_enum_decl(node, depth);
        break;
    case NODE_ENUM_VARIANT:
        print_enum_variant(node, depth);
        break;
    case NODE_VAR_DECL:
        print_var_decl(node, depth);
        break;
    case NODE_TRAIT_DECL:
        print_trait_decl(node, depth);
        break;
    case NODE_TYPE_ALIAS:
        print_type_alias(node, depth);
        break;
    case NODE_IMPL_DECL:
        print_impl_decl(node, depth);
        break;
    case NODE_USE_DECL:
        print_use_decl(node);
        break;
    case NODE_TEST_DECL:
        printf("TestDecl: \"%.*s\"\n", node->as.test_decl.name_length, node->as.test_decl.name);
        print_labeled_child("Body", node->as.test_decl.body, depth + 1);
        break;

    // Statements
    case NODE_BLOCK:
        printf("Block\n");
        print_node_list(NULL, &node->as.block.stmts, depth);
        break;
    case NODE_IF:
        print_if_stmt(node, depth);
        break;
    case NODE_WHILE:
        print_while_stmt(node, depth);
        break;
    case NODE_FOR:
        print_for_stmt(node, depth);
        break;
    case NODE_RETURN:
        printf("Return\n");
        if (node->as.return_stmt.value)
            print_ast(node->as.return_stmt.value, depth + 1);
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
        print_match_stmt(node, depth);
        break;
    case NODE_MATCH_ARM:
        print_match_arm(node, depth);
        break;
    case NODE_EXPR_STMT:
        printf("ExprStmt\n");
        print_ast(node->as.expr_stmt.expr, depth + 1);
        break;

    // Expressions
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
        print_call_expr(node, depth);
        break;
    case NODE_INDEX:
        printf("Index\n");
        print_ast(node->as.index.object, depth + 1);
        if (node->as.index.index)
            print_ast(node->as.index.index, depth + 1);
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
        print_node_list(NULL, &node->as.struct_init.fields, depth);
        break;
    case NODE_FIELD_INIT:
        printf("FieldInit: %.*s\n", node->as.field_init.name_length, node->as.field_init.name);
        print_ast(node->as.field_init.value, depth + 1);
        break;
    case NODE_ENUM_VALUE:
        print_enum_value(node, depth);
        break;
    case NODE_NEW_EXPR:
        print_new_expr(node, depth);
        break;
    case NODE_CAST:
        print_cast_expr(node, depth);
        break;
    case NODE_TRY_EXPR:
        printf("TryExpr\n");
        print_ast(node->as.try_expr.expr, depth + 1);
        break;
    case NODE_LAMBDA:
        printf("Lambda (%d params%s)\n", node->as.lambda.params.count,
               node->as.lambda.is_expr_body ? ", expr" : "");
        print_node_list("Params", &node->as.lambda.params, depth);
        if (node->as.lambda.return_type) {
            print_indent(depth + 1);
            printf("ReturnType: ");
            print_ast(node->as.lambda.return_type, depth + 2);
        }
        print_indent(depth + 1);
        printf("Body: ");
        print_ast(node->as.lambda.body, depth + 2);
        break;
    case NODE_STRING_INTERP:
        printf("StringInterp (%d parts)\n", node->as.string_interp.parts.count);
        print_node_list(NULL, &node->as.string_interp.parts, depth);
        break;

    // Literals
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

    // Types
    case NODE_ARRAY_TYPE:
        print_array_type(node, depth);
        break;
    case NODE_GENERIC_TYPE:
        print_generic_type(node, depth);
        break;
    case NODE_TUPLE_TYPE:
        printf("TupleType\n");
        print_node_list(NULL, &node->as.tuple_type.elem_types, depth);
        break;
    case NODE_FUNC_TYPE:
        printf("FuncType\n");
        if (node->as.func_type.param_types.count > 0) {
            print_node_list("Params", &node->as.func_type.param_types, depth + 1);
        }
        print_optional_labeled_child("ReturnType", node->as.func_type.return_type, depth + 1);
        break;
    case NODE_TUPLE_LIT:
        printf("TupleLit\n");
        print_node_list(NULL, &node->as.tuple_lit.elements, depth);
        break;

    default:
        printf("Unknown node type: %d\n", node->type);
        break;
    }
}
