int data_export = 42;

extern int data_export2;
extern int func_export2(void);

static void local_function(int *i)
{
    *i += 1;
}

int func_export(void)
{
    int x = 123;
    local_function(&x);

    int res = func_export2();
    res += data_export2;
    (void)res;

    return 234;
}
