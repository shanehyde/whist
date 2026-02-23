W0 = ../w0/bin/w0
LIB = .

MODULES = std fs collections time
MOD_OBJ = $(MODULES:=.o)

.PHONY: all clean

all: libwhist.a

whist_runtime.o: whist_runtime.c include/whist_runtime.h
	cc -c -o $@ $< -Iinclude

%.o: %.w
	$(W0) -c $< -o $@ --module-name $* --lib-path $(LIB)

libwhist.a: $(MOD_OBJ) whist_runtime.o
	ar rcs $@ $^

clean:
	rm -f $(MOD_OBJ) whist_runtime.o libwhist.a
