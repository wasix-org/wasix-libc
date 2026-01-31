#include <stdio.h>

extern int side_needed_func(int);
extern int main_exported(void);
int (*main_exported_ptr)(void) = main_exported;

int side_func(int x)
{
    int (*side_func_ptr)(int) = side_func;
    if (side_func_ptr != side_func)
    {
        printf("side_needed_func pointer mismatch\n");
        return -1;
    }

    if (main_exported_ptr() != 85)
    {
        printf("main_exported returned unexpected value\n");
        return -1;
    }

    return side_needed_func(x) * 2;
}
