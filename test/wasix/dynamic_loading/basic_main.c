#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main_test_function(int x)
{
    return x + 5;
}

int main(void)
{
    dlerror();
    void *self = dlopen(NULL, RTLD_NOW);
    if (!self) {
        fprintf(stderr, "dlopen(NULL) failed: %s\n", dlerror());
    }
    assert(self != NULL);

    dlerror();
    int (*self_fn)(int) = (int (*)(int))dlsym(self, "main_test_function");
    char *err = dlerror();
    if (err) {
        fprintf(stderr, "dlsym(main_test_function) failed: %s\n", err);
    }
    assert(err == NULL);
    assert(self_fn != NULL);
    assert(self_fn(2) == 7);

    assert(dlclose(self) == 0);

    dlerror();
    void *noload = dlopen("./libbasic.so", RTLD_NOW | RTLD_NOLOAD);
    err = dlerror();
    assert(noload == NULL);
    assert(err && *err);

    dlerror();
    void *missing_file = dlopen("./missing.so", RTLD_NOW);
    err = dlerror();
    assert(missing_file == NULL);
    assert(err && *err);

    void *handle_local = dlopen("./libbasic.so", RTLD_NOW | RTLD_LOCAL);
    if (!handle_local) {
        fprintf(stderr, "dlopen(RTLD_LOCAL) failed: %s\n", dlerror());
    }
    assert(handle_local != NULL);

    dlerror();
    void *local_default = dlsym(RTLD_DEFAULT, "side_func");
    err = dlerror();
    assert(local_default == NULL);
    assert(err && *err);

    dlerror();
    int (*side_func_local)(int) = (int (*)(int))dlsym(handle_local, "side_func");
    err = dlerror();
    if (err) {
        fprintf(stderr, "dlsym(side_func) failed: %s\n", err);
    }
    assert(err == NULL);
    assert(side_func_local != NULL);
    assert(side_func_local(42) == 84);

    assert(dlclose(handle_local) == 0);

    void *handle = dlopen("./libbasic.so", RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        fprintf(stderr, "dlopen(RTLD_GLOBAL) failed: %s\n", dlerror());
    }
    assert(handle != NULL);

    dlerror();
    void *global_default = dlsym(RTLD_DEFAULT, "side_func");
    err = dlerror();
    assert(err == NULL);
    assert(global_default != NULL);

    dlerror();
    void *noload_after = dlopen("./libbasic.so", RTLD_NOW | RTLD_NOLOAD);
    err = dlerror();
    assert(err == NULL);
    assert(noload_after != NULL);
    assert(noload_after == handle);

    dlerror();
    int (*side_func)(int) = (int (*)(int))dlsym(handle, "side_func");
    err = dlerror();
    if (err) {
        fprintf(stderr, "dlsym(side_func) failed: %s\n", err);
    }
    assert(err == NULL);
    assert(side_func != NULL);
    assert(side_func(42) == 84);

    dlerror();
    void *empty_sym = dlsym(handle, "");
    err = dlerror();
    assert(empty_sym == NULL);
    assert(err && *err);

    dlerror();
    void *missing = dlsym(handle, "missing_symbol");
    err = dlerror();
    assert(missing == NULL);
    assert(err && *err);

    dlerror();
    void *main_handle = dlopen(NULL, RTLD_NOW);
    if (!main_handle) {
        fprintf(stderr, "dlopen(NULL) failed: %s\n", dlerror());
    }
    assert(main_handle != NULL);
    dlerror();
    int (*main_handle_fn)(int) = (int (*)(int))dlsym(main_handle, "main_test_function");
    err = dlerror();
    assert(err == NULL);
    assert(main_handle_fn != NULL);
    assert(main_handle_fn(3) == 8);
    assert(dlclose(main_handle) == 0);

    assert(dlclose(noload_after) == 0);
    assert(dlclose(handle) == 0);

    dlerror();
    void *closed_sym = dlsym(handle, "side_func");
    err = dlerror();
    assert(closed_sym == NULL);
    assert(err && *err);

    int bad = dlclose((void *)0xffffff);
    assert(bad != 0);
    err = dlerror();
    assert(err && *err);

    int fd = open("empty.so", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        fprintf(stderr, "open(empty.so) failed: %s\n", strerror(errno));
    }
    assert(fd >= 0);
    close(fd);

    dlerror();
    void *empty = dlopen("./empty.so", RTLD_NOW);
    err = dlerror();
    assert(empty == NULL);
    assert(err && *err);
    unlink("empty.so");

    return 0;
}
