#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>

int main(void)
{
    void *handle = dlopen("./libdata1.so", RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
    }
    assert(handle != NULL);

    dlerror();
    int *data_export = (int *)dlsym(handle, "data_export");
    char *err = dlerror();
    if (err) {
        fprintf(stderr, "dlsym(data_export) failed: %s\n", err);
    }
    assert(err == NULL);
    assert(data_export != NULL);
    assert(*data_export == 42);

    dlerror();
    int (*func_export)(void) = (int (*)(void))dlsym(handle, "func_export");
    err = dlerror();
    if (err) {
        fprintf(stderr, "dlsym(func_export) failed: %s\n", err);
    }
    assert(err == NULL);
    assert(func_export != NULL);
    assert(func_export() == 234);

    dlerror();
    void *local_sym = dlsym(handle, "local_function");
    err = dlerror();
    assert(local_sym == NULL);
    assert(err && *err);

    assert(dlclose(handle) == 0);

    return 0;
}
