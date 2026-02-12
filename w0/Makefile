CC = cc
CFLAGS = -Wall -Wextra -std=c11 -g -D_POSIX_C_SOURCE=200809L -MMD -MP
LDFLAGS =

SRC = main.c lexer.c parser.c module_loader.c ast.c types.c sem_info.c checker.c checker_types.c checker_expr.c codegen.c codegen_types.c codegen_emit.c codegen_rc.c codegen_expr.c codegen_stmt.c print_ast.c
OBJ = $(SRC:.c=.o)
DEPS = $(SRC:.c=.d)
BIN = bin
TARGET = $(BIN)/w0

.PHONY: all clean test test-verbose test-valid test-errors format metrics complexity

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

metrics:
	@lizard *.c --sort nloc

complexity:
	@./scripts/complexity_report.sh

-include $(DEPS)
