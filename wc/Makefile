W0 = ../w0/bin/w0
LIB = ../lib
BIN = bin
SRC = main.w
OBJ = $(SRC:.w=.o)
TARGET = $(BIN)/wc

.PHONY: all clean

all: $(TARGET)

%.o: %.w
	$(W0) -c $< -o $@ --rc-debug --lib-path $(LIB)

$(TARGET): $(OBJ) | $(BIN)
	$(W0) $(OBJ) -o $@ --lib-path $(LIB)

$(BIN):
	mkdir -p $(BIN)

clean:
	rm -f $(OBJ)
	rm -rf $(BIN)
