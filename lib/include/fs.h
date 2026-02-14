/*
 * fs.h — File services for the Whist standard library
 *
 * Static inline functions using whist-compatible types.
 * FILE* handles are passed as void* (opaque pointers).
 * A handle value of NULL represents an invalid/null file handle.
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
#include <dirent.h>
#include <limits.h>
#include <errno.h>

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

static inline void* fs__open(const char* path, const char* mode) {
    FILE* fp = fopen(path, mode);
    if (!fp) return NULL;
    return (void*)fp;
}

static inline int32_t fs__close(void* handle) {
    FILE* fp = (FILE*)handle;
    if (!fp) return -1;
    return (fclose(fp) == 0) ? 0 : -1;
}

static inline const char* fs__read_line(void* handle) {
    FILE* fp = (FILE*)handle;
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

static inline int32_t fs__write_string(void* handle, const char* content) {
    FILE* fp = (FILE*)handle;
    if (!fp) return -1;

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, fp);
    return (written == len) ? 0 : -1;
}

static inline int32_t fs__flush(void* handle) {
    FILE* fp = (FILE*)handle;
    if (!fp) return -1;
    return (fflush(fp) == 0) ? 0 : -1;
}

static inline int32_t fs__seek(void* handle, int64_t offset, int32_t whence) {
    FILE* fp = (FILE*)handle;
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

static inline int64_t fs__tell(void* handle) {
    FILE* fp = (FILE*)handle;
    if (!fp) return -1;
    return (int64_t)ftell(fp);
}

static inline bool fs__eof(void* handle) {
    FILE* fp = (FILE*)handle;
    if (!fp) return true;
    return feof(fp) != 0;
}

/* ---------- Directory operations ---------- */

static inline int32_t fs__mkdir(const char* path) {
    return (mkdir(path, 0755) == 0) ? 0 : -1;
}

static inline int32_t fs__mkdir_all(const char* path) {
    char tmp[PATH_MAX];
    size_t len = strlen(path);
    if (len == 0 || len >= PATH_MAX) return -1;

    memcpy(tmp, path, len + 1);

    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
            tmp[i] = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

static inline int32_t fs__rmdir(const char* path) {
    return (rmdir(path) == 0) ? 0 : -1;
}

static inline bool fs__is_dir(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

static inline bool fs__is_file(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

static inline const char* fs__cwd(void) {
    char* result = getcwd(NULL, 0);
    if (!result) return strdup("");
    return result;
}

static inline int32_t fs__chdir(const char* path) {
    return (chdir(path) == 0) ? 0 : -1;
}

/* ---------- Directory iteration (handle-based) ---------- */

static inline void* fs__open_dir(const char* path) {
    DIR* d = opendir(path);
    return (void*)d;
}

static inline const char* fs__read_dir(void* handle) {
    DIR* d = (DIR*)handle;
    if (!d) return strdup("");
    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        return strdup(entry->d_name);
    }
    return strdup("");
}

static inline int32_t fs__close_dir(void* handle) {
    DIR* d = (DIR*)handle;
    if (!d) return -1;
    return (closedir(d) == 0) ? 0 : -1;
}

/* ---------- Path utilities ---------- */

static inline const char* fs__join_path(const char* a, const char* b) {
    size_t alen = strlen(a);
    size_t blen = strlen(b);
    /* Strip trailing slash from a */
    while (alen > 0 && a[alen - 1] == '/') alen--;
    /* Strip leading slash from b */
    while (blen > 0 && *b == '/') { b++; blen--; }

    char* result = (char*)malloc(alen + 1 + blen + 1);
    if (!result) return NULL;
    memcpy(result, a, alen);
    result[alen] = '/';
    memcpy(result + alen + 1, b, blen);
    result[alen + 1 + blen] = '\0';
    return result;
}

static inline const char* fs__dirname(const char* path) {
    size_t len = strlen(path);
    /* Strip trailing slashes */
    while (len > 1 && path[len - 1] == '/') len--;
    /* Find last slash */
    size_t i = len;
    while (i > 0 && path[i - 1] != '/') i--;
    if (i == 0) return strdup(".");
    /* Strip trailing slashes from parent */
    while (i > 1 && path[i - 1] == '/') i--;
    char* result = (char*)malloc(i + 1);
    if (!result) return NULL;
    memcpy(result, path, i);
    result[i] = '\0';
    return result;
}

static inline const char* fs__basename(const char* path) {
    size_t len = strlen(path);
    /* Strip trailing slashes */
    while (len > 1 && path[len - 1] == '/') len--;
    /* Find last slash */
    size_t i = len;
    while (i > 0 && path[i - 1] != '/') i--;
    size_t name_len = len - i;
    char* result = (char*)malloc(name_len + 1);
    if (!result) return NULL;
    memcpy(result, path + i, name_len);
    result[name_len] = '\0';
    return result;
}

static inline const char* fs__extension(const char* path) {
    size_t len = strlen(path);
    /* Find last slash to avoid dots in directory names */
    size_t last_slash = 0;
    for (size_t i = 0; i < len; i++) {
        if (path[i] == '/') last_slash = i;
    }
    /* Find last dot after last slash */
    const char* dot = NULL;
    for (size_t i = last_slash; i < len; i++) {
        if (path[i] == '.') dot = path + i;
    }
    if (!dot || dot == path + len - 1) return strdup("");
    return strdup(dot);
}

static inline const char* fs__abs_path(const char* path) {
    char* resolved = realpath(path, NULL);
    if (!resolved) return strdup("");
    return resolved;
}

/* ---------- Metadata & temp ---------- */

static inline int64_t fs__modified_time(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (int64_t)st.st_mtime;
}

static inline const char* fs__temp_dir(void) {
    const char* tmp = getenv("TMPDIR");
    if (tmp && tmp[0] != '\0') return strdup(tmp);
    return strdup("/tmp");
}

#endif /* WHIST_FS_H */
