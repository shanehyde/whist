/*
 * fs.h — File services for the Whist standard library
 *
 * Static inline functions using whist-compatible types.
 * Manages FILE* handles internally via an integer-indexed table.
 *
 * Public functions use the fs__ prefix (double underscore) to avoid
 * colliding with the module-prefixed wrapper names (fs_*) generated
 * by the whist compiler.
 *
 * Usage: compile with -Ilib/include so #include <fs.h> resolves.
 */

#ifndef WHIST_FS_H
#define WHIST_FS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>

/* ---------- Handle table ---------- */

#define FS__MAX_HANDLES 256

static FILE* fs__handles[FS__MAX_HANDLES];
static bool  fs__initialized = false;

static inline void fs__init(void) {
    if (!fs__initialized) {
        for (int i = 0; i < FS__MAX_HANDLES; i++) {
            fs__handles[i] = NULL;
        }
        fs__initialized = true;
    }
}

static inline int64_t fs__alloc_handle(FILE* fp) {
    fs__init();
    for (int i = 0; i < FS__MAX_HANDLES; i++) {
        if (fs__handles[i] == NULL) {
            fs__handles[i] = fp;
            return (int64_t)i;
        }
    }
    return -1;  /* no free slots */
}

static inline FILE* fs__get_handle(int64_t handle) {
    if (handle < 0 || handle >= FS__MAX_HANDLES) return NULL;
    return fs__handles[handle];
}

/* ---------- Convenience API (no handles) ---------- */

static inline const char* fs__read_file(const char* path) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size < 0) {
        fclose(fp);
        return NULL;
    }

    char* buf = (char*)malloc((size_t)size + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }

    size_t read = fread(buf, 1, (size_t)size, fp);
    buf[read] = '\0';
    fclose(fp);
    return buf;
}

static inline int32_t fs__write_file(const char* path, const char* content) {
    FILE* fp = fopen(path, "wb");
    if (!fp) return -1;

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, fp);
    fclose(fp);
    return (written == len) ? 0 : -1;
}

static inline int32_t fs__append_file(const char* path, const char* content) {
    FILE* fp = fopen(path, "ab");
    if (!fp) return -1;

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, fp);
    fclose(fp);
    return (written == len) ? 0 : -1;
}

static inline bool fs__file_exists(const char* path) {
    return access(path, F_OK) == 0;
}

static inline int32_t fs__remove_file(const char* path) {
    return (remove(path) == 0) ? 0 : -1;
}

static inline int32_t fs__rename_file(const char* old_path, const char* new_path) {
    return (rename(old_path, new_path) == 0) ? 0 : -1;
}

static inline int64_t fs__file_size(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (int64_t)st.st_size;
}

/* ---------- Handle-based API ---------- */

static inline int64_t fs__open(const char* path, const char* mode) {
    FILE* fp = fopen(path, mode);
    if (!fp) return -1;
    int64_t handle = fs__alloc_handle(fp);
    if (handle < 0) {
        fclose(fp);
    }
    return handle;
}

static inline int32_t fs__close(int64_t handle) {
    FILE* fp = fs__get_handle(handle);
    if (!fp) return -1;
    int result = fclose(fp);
    fs__handles[handle] = NULL;
    return (result == 0) ? 0 : -1;
}

static inline const char* fs__read_line(int64_t handle) {
    FILE* fp = fs__get_handle(handle);
    if (!fp) return NULL;

    size_t cap = 256;
    size_t len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;

    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        if (len + 2 > cap) {
            cap *= 2;
            char* tmp = (char*)realloc(buf, cap);
            if (!tmp) { free(buf); return NULL; }
            buf = tmp;
        }
        buf[len++] = (char)ch;
        if (ch == '\n') break;
    }

    if (len == 0) {
        free(buf);
        return NULL;  /* EOF, nothing read */
    }

    buf[len] = '\0';
    return buf;
}

static inline int32_t fs__write_string(int64_t handle, const char* content) {
    FILE* fp = fs__get_handle(handle);
    if (!fp) return -1;

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, fp);
    return (written == len) ? 0 : -1;
}

static inline int32_t fs__flush(int64_t handle) {
    FILE* fp = fs__get_handle(handle);
    if (!fp) return -1;
    return (fflush(fp) == 0) ? 0 : -1;
}

static inline int32_t fs__seek(int64_t handle, int64_t offset, int32_t whence) {
    FILE* fp = fs__get_handle(handle);
    if (!fp) return -1;

    int w;
    switch (whence) {
        case 0: w = SEEK_SET; break;
        case 1: w = SEEK_CUR; break;
        case 2: w = SEEK_END; break;
        default: return -1;
    }
    return (fseek(fp, (long)offset, w) == 0) ? 0 : -1;
}

static inline int64_t fs__tell(int64_t handle) {
    FILE* fp = fs__get_handle(handle);
    if (!fp) return -1;
    return (int64_t)ftell(fp);
}

static inline bool fs__eof(int64_t handle) {
    FILE* fp = fs__get_handle(handle);
    if (!fp) return true;
    return feof(fp) != 0;
}

#endif /* WHIST_FS_H */
