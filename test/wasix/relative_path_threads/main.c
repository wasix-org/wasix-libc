// Concurrent lookups must preserve each thread's own relative-path result.
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
    if (!iterations || count < 1 || count > 2 || chdir(".") != 0)
        return 2;
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
