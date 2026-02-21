CC = cc
CFLAGS = -Wall -Wextra -std=c11 -g -D_POSIX_C_SOURCE=200809L -MMD -MP
LDFLAGS =

SRC = main.c lexer.c parser.c parser_core.c parser_types.c parser_expr.c parser_stmt.c parser_decl.c module_loader.c ast.c types.c sem_info.c checker.c checker_types.c checker_expr.c codegen.c codegen_types.c codegen_emit.c codegen_rc.c codegen_expr.c codegen_stmt.c print_ast.c
OBJ = $(SRC:.c=.o)
DEPS = $(SRC:.c=.d)
BIN = bin
TARGET = $(BIN)/w0

.PHONY: all clean test test-verbose test-valid test-run test-errors format metrics complexity

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

$(BIN)/test_runner: $(TARGET) tools/test_runner.w
	@$(TARGET) --lib-path ../lib tools/test_runner.w | cc -x c -I../lib/include -o $@ - ../lib/whist_runtime.c

test: $(BIN)/test_runner
	@$(BIN)/test_runner $(FILTER)

test-verbose: $(BIN)/test_runner
	@$(BIN)/test_runner --verbose $(FILTER)

test-run: $(BIN)/test_runner
	@$(BIN)/test_runner --run $(FILTER)

test-valid: $(BIN)/test_runner
	@$(BIN)/test_runner --run $(FILTER)

test-errors: $(BIN)/test_runner
	@$(BIN)/test_runner --errors $(FILTER)

format:
	clang-format -i *.c *.h

metrics:
	@lizard *.c --sort nloc

complexity:
	@./scripts/complexity_report.sh

-include $(DEPS)
