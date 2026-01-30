CC = cc
CFLAGS = -Wall -Wextra -std=c11
LDFLAGS =

SRC = main.c lexer.c parser.c ast.c types.c checker.c codegen.c
OBJ = $(SRC:.c=.o)
BIN = bin
TARGET = $(BIN)/w0

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJ) | $(BIN)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

$(BIN):
	mkdir -p $(BIN)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ)
	rm -rf $(BIN)

test: $(TARGET)
	@echo "=== Type checking valid programs ==="
	@for f in test/*.w; do \
		case "$$f" in test/error_*) continue;; esac; \
		printf "$$f: "; \
		$(TARGET) --check "$$f" 2>&1; \
	done
	@echo ""
	@echo "=== Testing error cases ==="
	@for f in test/error_*.w; do \
		echo "--- $$f ---"; \
		$(TARGET) --check "$$f" 2>&1 | grep "Error:"; \
	done

# Dependencies
main.o: main.c lexer.h parser.h checker.h codegen.h
lexer.o: lexer.c lexer.h
parser.o: parser.c parser.h lexer.h ast.h
ast.o: ast.c ast.h lexer.h
types.o: types.c types.h
checker.o: checker.c checker.h ast.h types.h lexer.h
codegen.o: codegen.c codegen.h ast.h types.h checker.h
