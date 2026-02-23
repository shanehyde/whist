W0 = ../w0/bin/w0
LIB = .

MODULES = std fs collections time
MOD_OBJ = $(MODULES:=.o)

# Debug (RC-traced) objects go into debug/ subdirectory
DEBUG_DIR = debug
DEBUG_MOD_OBJ = $(addprefix $(DEBUG_DIR)/,$(MOD_OBJ))

.PHONY: all debug clean

all: libwhist.a

debug: $(DEBUG_DIR)/libwhist.a

whist_runtime.o: whist_runtime.c include/whist_runtime.h
	cc -c -o $@ $< -Iinclude

%.o: %.w
	$(W0) -c $< -o $@ --module-name $* --lib-path $(LIB)

libwhist.a: $(MOD_OBJ) whist_runtime.o
	ar rcs $@ $^

# --- RC debug build ---

$(DEBUG_DIR)/.dir:
	@mkdir -p $(DEBUG_DIR)
	@test -L $(DEBUG_DIR)/include || ln -s ../include $(DEBUG_DIR)/include
	@for f in *.w; do test -L $(DEBUG_DIR)/$$f || ln -s ../$$f $(DEBUG_DIR)/$$f; done
	@touch $@

$(DEBUG_DIR)/whist_runtime.o: whist_runtime.c include/whist_runtime.h $(DEBUG_DIR)/.dir
	cc -c -o $@ $< -Iinclude -DWHIST_RC_DEBUG

$(DEBUG_DIR)/%.o: %.w $(DEBUG_DIR)/.dir
	$(W0) -c $< -o $@ --module-name $* --lib-path $(LIB) --rc-debug

$(DEBUG_DIR)/libwhist.a: $(DEBUG_MOD_OBJ) $(DEBUG_DIR)/whist_runtime.o
	ar rcs $@ $^

clean:
	rm -f $(MOD_OBJ) whist_runtime.o libwhist.a
	rm -rf $(DEBUG_DIR)
