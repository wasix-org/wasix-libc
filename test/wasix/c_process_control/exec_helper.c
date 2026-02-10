#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int write_all(int fd, const char *buf, size_t len) {
    while (len > 0) {
        ssize_t wrote = write(fd, buf, len);
        if (wrote < 0) {
            return -1;
        }
        buf += (size_t)wrote;
        len -= (size_t)wrote;
    }
    return 0;
}

static int write_file(const char *path, const char *data, size_t len) {
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        return -1;
    }
    int result = write_all(fd, data, len);
    int saved = errno;
    close(fd);
    errno = saved;
    return result;
}

static int do_echo(int argc, char **argv) {
    if (argc < 4) {
        return 2;
    }
    const char *out = argv[2];
    size_t total = 0;
    for (int i = 3; i < argc; ++i) {
        total += strlen(argv[i]);
        if (i + 1 < argc) {
            total += 1;
        }
    }
    total += 1;
    char *buf = (char *)malloc(total + 1);
    if (!buf) {
        return 3;
    }
    size_t off = 0;
    for (int i = 3; i < argc; ++i) {
        size_t len = strlen(argv[i]);
        memcpy(buf + off, argv[i], len);
        off += len;
        if (i + 1 < argc) {
            buf[off++] = ' ';
        }
    }
    buf[off++] = '\n';
    buf[off] = '\0';
    int result = write_file(out, buf, off);
    int saved = errno;
    free(buf);
    errno = saved;
    return result == 0 ? 0 : 4;
}

static int do_env(int argc, char **argv) {
    if (argc < 4) {
        return 2;
    }
    const char *out = argv[2];
    const char *name = argv[3];
    const char *value = getenv(name);
    if (!value) {
        value = "";
    }
    size_t len = strlen(value);
    char *buf = (char *)malloc(len + 2);
    if (!buf) {
        return 3;
    }
    memcpy(buf, value, len);
    buf[len] = '\n';
    buf[len + 1] = '\0';
    int result = write_file(out, buf, len + 1);
    int saved = errno;
    free(buf);
    errno = saved;
    return result == 0 ? 0 : 4;
}

static int do_touch(int argc, char **argv) {
    if (argc < 3) {
        return 2;
    }
    const char *path = argv[2];
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        return 4;
    }
    close(fd);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        return 2;
    }
    if (strcmp(argv[1], "echo") == 0) {
        return do_echo(argc, argv);
    }
    if (strcmp(argv[1], "env") == 0) {
        return do_env(argc, argv);
    }
    if (strcmp(argv[1], "touch") == 0) {
        return do_touch(argc, argv);
    }
    return 2;
}
