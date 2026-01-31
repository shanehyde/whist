CC = cc
CFLAGS = -Wall -Wextra -std=c11 -g -D_POSIX_C_SOURCE=200809L -MMD -MP
LDFLAGS =

SRC = main.c lexer.c parser.c parser_util.c parse_enum_decl.c parse_struct_decl.c \
		parse_func_decl.c parse_expression.c parse_while_stmt.c parse_type.c parse_block.c parse_if_stmt.c parse_primary.c \
		parse_for_stmt.c parse_statement.c parse_foreach_stmt.c parse_return_stmt.c parse_var_decl.c parse_struct_init.c \
		parse_defer_stmt.c parse_extern_decls.c ast.c types.c checker.c codegen.c print_ast.c \
		checker_util.c check_decl.c check_statement.c check_expression.c check_struct_init.c
OBJ = $(SRC:.c=.o)
DEPS = $(SRC:.c=.d)
BIN = bin
TARGET = $(BIN)/w0

.PHONY: all clean test test-verbose test-valid test-errors format

all: $(TARGET)

$(TARGET): $(OBJ) | $(BIN)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

$(BIN):
	mkdir -p $(BIN)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(DEPS)
	rm -rf $(BIN)

test: $(TARGET)
	@./scripts/run_tests.sh

test-verbose: $(TARGET)
	@./scripts/run_tests.sh --verbose

test-valid: $(TARGET)
	@./scripts/run_tests.sh --valid

test-errors: $(TARGET)
	@./scripts/run_tests.sh --errors

format:
	clang-format -i *.c *.h

-include $(DEPS)