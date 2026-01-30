#include "check_statement.h"

#include "check_expression.h"
#include "check_struct_init.h"
#include "checker_util.h"

void check_statement(Checker* checker, Node* node) {
    if (!node)
        return;

    switch (node->type) {
    case NODE_EXPR_STMT:
        check_expression(checker, node->as.expr_stmt.expr);
        break;

    case NODE_VAR_DECL: {
        const char* name = node->as.var_decl.name;

        // Check for redefinition
        if (checker_lookup_local(checker, name)) {
            check_error(checker, node->line, node->column, "Redefinition of '%s'", name);
            return;
        }

        Type* decl_type = NULL;
        Type* init_type = NULL;

        if (node->as.var_decl.type) {
            decl_type = resolve_type(checker, node->as.var_decl.type);
        }

        if (node->as.var_decl.init) {
            if (node->as.var_decl.init->type == NODE_STRUCT_INIT) {
                if (!decl_type) {
                    check_error(checker, node->line, node->column,
                                "Struct initializer requires an explicit type");
                    init_type = type_error;
                } else {
                    init_type = check_struct_init(checker, node->as.var_decl.init, decl_type);
                }
            } else {
                init_type = check_expression(checker, node->as.var_decl.init);
            }
        }

        Type* var_type;
        if (decl_type && init_type) {
            // Both specified - check compatibility
            if (!type_assignable(decl_type, init_type)) {
                check_error(checker, node->line, node->column,
                            "Cannot initialize '%s' of type '%s' with '%s'", name,
                            type_name(decl_type), type_name(init_type));
            }
            var_type = decl_type;
        } else if (decl_type) {
            var_type = decl_type;
        } else if (init_type) {
            var_type = init_type;
        } else {
            check_error(checker, node->line, node->column,
                        "Variable '%s' needs type annotation or initializer", name);
            var_type = type_error;
        }

        checker_define(checker, name, SYM_VAR, var_type, node->as.var_decl.is_const);
        break;
    }

    case NODE_BLOCK:
        checker_push_scope(checker);
        for (int i = 0; i < node->as.block.stmts.count; i++) {
            check_statement(checker, node->as.block.stmts.nodes[i]);
        }
        checker_pop_scope(checker);
        break;

    case NODE_IF: {
        Type* cond = check_expression(checker, node->as.if_stmt.cond);
        if (cond->kind != TYPE_BOOL && cond->kind != TYPE_ERROR) {
            check_error(checker, node->as.if_stmt.cond->line, node->as.if_stmt.cond->column,
                        "If condition must be bool, got '%s'", type_name(cond));
        }
        check_statement(checker, node->as.if_stmt.then_block);
        if (node->as.if_stmt.else_block) {
            check_statement(checker, node->as.if_stmt.else_block);
        }
        break;
    }

    case NODE_WHILE: {
        Type* cond = check_expression(checker, node->as.while_stmt.cond);
        if (cond->kind != TYPE_BOOL && cond->kind != TYPE_ERROR) {
            check_error(checker, node->as.while_stmt.cond->line, node->as.while_stmt.cond->column,
                        "While condition must be bool, got '%s'", type_name(cond));
        }
        int was_in_loop  = checker->in_loop;
        checker->in_loop = 1;
        check_statement(checker, node->as.while_stmt.body);
        checker->in_loop = was_in_loop;
        break;
    }

    case NODE_FOR: {
        checker_push_scope(checker); // New scope for init var

        if (node->as.for_stmt.init) {
            check_statement(checker, node->as.for_stmt.init);
        }

        if (node->as.for_stmt.cond) {
            Type* cond = check_expression(checker, node->as.for_stmt.cond);
            if (cond->kind != TYPE_BOOL && cond->kind != TYPE_ERROR) {
                check_error(checker, node->as.for_stmt.cond->line, node->as.for_stmt.cond->column,
                            "For condition must be bool, got '%s'", type_name(cond));
            }
        }

        if (node->as.for_stmt.post) {
            check_expression(checker, node->as.for_stmt.post);
        }

        int was_in_loop  = checker->in_loop;
        checker->in_loop = 1;
        check_statement(checker, node->as.for_stmt.body);
        checker->in_loop = was_in_loop;

        checker_pop_scope(checker);
        break;
    }

    case NODE_FOREACH: {
        checker_push_scope(checker); // New scope for loop variable

        // Check that start and end are integers
        Type* start_type = check_expression(checker, node->as.foreach_stmt.start);
        Type* end_type   = check_expression(checker, node->as.foreach_stmt.end);

        if (start_type->kind != TYPE_INT64 && start_type->kind != TYPE_ERROR) {
            check_error(checker, node->as.foreach_stmt.start->line,
                        node->as.foreach_stmt.start->column,
                        "Foreach range start must be int, got '%s'", type_name(start_type));
        }

        if (end_type->kind != TYPE_INT64 && end_type->kind != TYPE_ERROR) {
            check_error(checker, node->as.foreach_stmt.end->line, node->as.foreach_stmt.end->column,
                        "Foreach range end must be int, got '%s'", type_name(end_type));
        }

        // Add the loop variable as a const int64 (immutable)
        Symbol* sym =
            checker_define(checker, node->as.foreach_stmt.var_name, SYM_VAR, type_int64, 1);
        if (!sym) {
            check_error(checker, node->line, node->column,
                        "Variable '%s' already declared in this scope",
                        node->as.foreach_stmt.var_name);
        }

        int was_in_loop  = checker->in_loop;
        checker->in_loop = 1;
        check_statement(checker, node->as.foreach_stmt.body);
        checker->in_loop = was_in_loop;

        checker_pop_scope(checker);
        break;
    }

    case NODE_RETURN: {
        Type* expected = checker->current_func_return;
        if (!expected) {
            check_error(checker, node->line, node->column, "Return outside of function");
            return;
        }

        if (node->as.return_stmt.value) {
            Type* actual = check_expression(checker, node->as.return_stmt.value);
            if (!type_assignable(expected, actual)) {
                check_error(checker, node->line, node->column,
                            "Return type mismatch: expected '%s', got '%s'", type_name(expected),
                            type_name(actual));
            }
        } else if (expected->kind != TYPE_VOID) {
            check_error(checker, node->line, node->column,
                        "Return without value in non-void function");
        }
        break;
    }

    case NODE_BREAK:
        if (!checker->in_loop) {
            check_error(checker, node->line, node->column, "Break outside of loop");
        }
        break;

    case NODE_CONTINUE:
        if (!checker->in_loop) {
            check_error(checker, node->line, node->column, "Continue outside of loop");
        }
        break;

    case NODE_DEFER:
        if (!checker->current_func_return) {
            check_error(checker, node->line, node->column, "Defer outside of function");
            return;
        }
        check_statement(checker, node->as.defer_stmt.stmt);
        break;

    default:
        check_error(checker, node->line, node->column, "Unknown statement type %d", node->type);
        break;
    }
}
