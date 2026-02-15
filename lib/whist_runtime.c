#include "include/whist_runtime.h"

int    __w0_argc = 0;
char** __w0_argv = NULL;

int64_t std__argc(void) {
    return (int64_t)__w0_argc;
}

const char* std__argv(int64_t i) {
    if (i < 0 || i >= (int64_t)__w0_argc)
        return __rc_strdup("");
    return __rc_strdup((const char*)__w0_argv[i]);
}
