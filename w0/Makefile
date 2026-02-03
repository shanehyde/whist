CC = cc
CFLAGS = -Wall -Wextra -std=c11 -g -D_POSIX_C_SOURCE=200809L -MMD -MP
LDFLAGS =

SRC = main.c lexer.c parser.c ast.c types.c checker.c codegen.c print_ast.c
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