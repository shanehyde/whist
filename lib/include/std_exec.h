/*
 * std_exec.h — Command execution with output capture for the Whist standard library
 *
 * Provides an opaque-handle API so Whist extern functions can extract
 * exit code, stdout, and stderr from a subprocess without needing to
 * return a Whist struct directly from C.
 *
 * Functions use the std__ prefix (double underscore) to avoid
 * colliding with the module-prefixed wrapper names (std_*) generated
 * by the whist compiler.
 *
 * Usage: compile with -Ilib/include so #include <std_exec.h> resolves.
 */

#ifndef WHIST_STD_EXEC_H
#define WHIST_STD_EXEC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>

typedef struct {
    int32_t exit_code;
    char* out;
    char* err;
} __ExecResult;

static char* __exec_read_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) {
        char* empty = (char*)malloc(1);
        empty[0] = '\0';
        return empty;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(len + 1);
    if (len > 0) {
        size_t n = fread(buf, 1, len, f);
        buf[n] = '\0';
    } else {
        buf[0] = '\0';
    }
    fclose(f);
    return buf;
}

static inline void* std__exec(const char* cmd) {
    char stdout_tmpl[] = "/tmp/whist_exec_out_XXXXXX";
    char stderr_tmpl[] = "/tmp/whist_exec_err_XXXXXX";

    int out_fd = mkstemp(stdout_tmpl);
    int err_fd = mkstemp(stderr_tmpl);
    if (out_fd < 0 || err_fd < 0) {
        __ExecResult* r = (__ExecResult*)malloc(sizeof(__ExecResult));
        r->exit_code = -1;
        r->out = strdup("");
        r->err = strdup("failed to create temp files");
        return r;
    }
    close(out_fd);
    close(err_fd);

    /* Build: (cmd) > stdout_tmp 2> stderr_tmp
     * The subshell ensures internal redirections (e.g. >&2)
     * resolve before our outer capture redirections. */
    size_t cmdlen = strlen(cmd) + strlen(stdout_tmpl) + strlen(stderr_tmpl) + 16;
    char* full_cmd = (char*)malloc(cmdlen);
    snprintf(full_cmd, cmdlen, "(%s) > %s 2> %s", cmd, stdout_tmpl, stderr_tmpl);

    int status = system(full_cmd);
    free(full_cmd);

    __ExecResult* r = (__ExecResult*)malloc(sizeof(__ExecResult));
    if (WIFEXITED(status)) {
        r->exit_code = WEXITSTATUS(status);
    } else {
        r->exit_code = -1;
    }
    r->out = __exec_read_file(stdout_tmpl);
    r->err = __exec_read_file(stderr_tmpl);

    unlink(stdout_tmpl);
    unlink(stderr_tmpl);

    return r;
}

static inline int32_t std__exec_exit_code(void* handle) {
    return ((__ExecResult*)handle)->exit_code;
}

static inline const char* std__exec_output(void* handle) {
    return strdup(((__ExecResult*)handle)->out);
}

static inline const char* std__exec_error_output(void* handle) {
    return strdup(((__ExecResult*)handle)->err);
}

static inline void std__exec_free(void* handle) {
    __ExecResult* r = (__ExecResult*)handle;
    free(r->out);
    free(r->err);
    free(r);
}

#endif
