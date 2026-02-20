#include <assert.h>
#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

extern int main_needed_func(int);

int main_exported(void)
{
    return 85;
}

static void *thread_func(void *arg)
{
    (void)arg;
    for (;;) {
        sleep(1);
    }
    return NULL;
}

int main(void)
{
    pthread_t thread;
    int rc = pthread_create(&thread, NULL, thread_func, NULL);
    assert(rc == 0);

    assert(main_needed_func(42) == 43);

    void *handle = dlopen("./libside.so", RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
    }
    assert(handle != NULL);

    int (*side_func)(int) = (int (*)(int))dlsym(handle, "side_func");
    if (!side_func) {
        fprintf(stderr, "dlsym failed: %s\n", dlerror());
    }
    assert(side_func != NULL);

    int res = side_func(42);
    assert(res == 92);

    assert(dlclose(handle) == 0);

    pthread_kill(thread, SIGTERM);

    return 0;
}
