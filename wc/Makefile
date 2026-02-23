W0 = ../w0/bin/w0
LIB = ../lib
LIB_DEBUG = ../lib/debug
BIN = bin
SRC = main.w lexer.w
OBJ = $(SRC:.w=.o)
TARGET = $(BIN)/wc

.PHONY: all clean

all: $(TARGET)

%.o: %.w
	$(W0) -c $< -o $@ --rc-debug --lib-path $(LIB_DEBUG)

$(TARGET): $(OBJ) | $(BIN)
	$(W0) $(OBJ) -o $@ --lib-path $(LIB_DEBUG)

$(BIN):
	mkdir -p $(BIN)

clean:
	rm -f $(OBJ)
	rm -rf $(BIN)

main.w: lexer.w