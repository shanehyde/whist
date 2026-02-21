W0 = ../w0/bin/w0
LIB = ../lib
BIN = bin
TARGET = $(BIN)/wc

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.w | $(BIN)
	$(W0) --lib-path $(LIB) main.w | cc -x c -I$(LIB)/include -o $@ - $(LIB)/whist_runtime.c

$(BIN):
	mkdir -p $(BIN)

clean:
	rm -rf $(BIN)
