// Concurrent lookups must preserve each thread's own relative-path result.
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wasi/libc-find-relpath.h>

static atomic_int ready;
static atomic_int start;
static unsigned iterations;

struct worker {
    const char *path;
    char *expected;
    unsigned mismatches;
};

static int check_path(const char *path, const char *absolute)
{
    const char *expected_prefix, *expected_relative, *prefix;
    int expected_fd = __wasilibc_find_abspath(absolute, &expected_prefix, &expected_relative);
    if (expected_fd < 0)
        return 1;
    size_t length = strlen(expected_relative) + 1;
    char buffer[512], *relative = buffer;
    if (length > sizeof(buffer))
        return 1;

    // Fixed buffers must work at the exact required size and report ERANGE below it.
    if (__wasilibc_find_relpath(path, &prefix, &relative, length) != expected_fd ||
        relative != buffer || strcmp(relative, expected_relative) || strcmp(prefix, expected_prefix))
        return 1;
    errno = 0;
    if (__wasilibc_find_relpath(path, &prefix, &relative, length - 1) != -1 || errno != ERANGE)
        return 1;

    // ERANGE must not affect the following successful lookup.
    relative = NULL;
    size_t capacity = 0;
    int fd = __wasilibc_find_relpath_alloc(path, &prefix, &relative, &capacity, 1);
    int failed = fd != expected_fd || !relative || strcmp(relative, expected_relative) ||
                 strcmp(prefix, expected_prefix) || capacity < length;
    free(relative);
    return failed;
}

static void *run(void *arg)
{
    struct worker *worker = arg;
    char *relative = NULL;
    size_t capacity = 0;
    const char *prefix;
    atomic_fetch_add_explicit(&ready, 1, memory_order_release);
    while (!atomic_load_explicit(&start, memory_order_acquire)) {}
    for (unsigned i = 0; i < iterations; i++) {
        int fd = __wasilibc_find_relpath_alloc(worker->path, &prefix, &relative, &capacity, 1);
        if (fd < 0 || strcmp(relative, worker->expected) != 0)
            worker->mismatches++;
    }
    free(relative);
    return NULL;
}

int main(int argc, char **argv)
{
    iterations = argc > 1 ? (unsigned)strtoul(argv[1], NULL, 10) : 100000;
    int count = argc > 2 ? atoi(argv[2]) : 2;
    if (!iterations || count < 1 || count > 2)
        return 2;
    char cwd[256], absolute[512];
    if (!getcwd(cwd, sizeof(cwd)))
        return 2;
    snprintf(absolute, sizeof(absolute), "%s/relative-path/metadata.json", cwd);
    if (check_path("relative-path/metadata.json", absolute) ||
        check_path("./relative-path/metadata.json", absolute) ||
        check_path(absolute, absolute) || check_path("", cwd) ||
        check_path(".", cwd) || check_path("./", cwd)) {
        fprintf(stderr, "relative-path buffer contract failed\n");
        return 1;
    }
    struct worker workers[2] = {{.path = "relative-path/metadata.json"}, {.path = "."}};
    // Initialize the process cwd and collect single-threaded reference results.
    for (int i = 0; i < 2; i++) {
        size_t capacity = 0;
        const char *prefix;
        if (__wasilibc_find_relpath_alloc(workers[i].path, &prefix, &workers[i].expected, &capacity, 1) < 0)
            return 2;
    }
    pthread_t threads[2];
    int created = 0;
    for (; created < count; created++) {
        if (pthread_create(&threads[created], NULL, run, &workers[created]) != 0)
            break;
    }
    while (atomic_load_explicit(&ready, memory_order_acquire) != created) {}
    atomic_store_explicit(&start, 1, memory_order_release);
    for (int i = 0; i < created; i++) {
        if (pthread_join(threads[i], NULL) != 0)
            return 2;
    }
    unsigned mismatches = workers[0].mismatches + workers[1].mismatches;
    for (int i = 0; i < 2; i++)
        free(workers[i].expected);
    printf("{\"threads\":%d,\"iterations_per_thread\":%u,\"mismatches\":%u}\n", count, iterations, mismatches);
    return created != count ? 2 : mismatches != 0;
}
