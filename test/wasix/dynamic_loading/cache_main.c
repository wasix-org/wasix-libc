#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>

int main(void)
{
    void *handle1 = dlopen("./libcache1.so", RTLD_NOW | RTLD_GLOBAL);
    if (!handle1) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
    }
    assert(handle1 != NULL);

    int (*side_func1)(int) = (int (*)(int))dlsym(handle1, "side_func");
    if (!side_func1) {
        fprintf(stderr, "dlsym failed: %s\n", dlerror());
    }
    assert(side_func1 != NULL);

    int res = side_func1(2);
    assert(res == 44);

    void *handle2 = dlopen("./libcache2.so", RTLD_NOW | RTLD_GLOBAL);
    if (!handle2) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
    }
    assert(handle2 != NULL);

    int (*side_func2)(int) = (int (*)(int))dlsym(handle2, "side_func");
    if (!side_func2) {
        fprintf(stderr, "dlsym failed: %s\n", dlerror());
    }
    assert(side_func2 != NULL);

    assert(side_func1 != side_func2);

    res = side_func2(2);
    assert(res == 4);

    assert(dlclose(handle1) == 0);
    assert(dlclose(handle2) == 0);

    return 0;
}
