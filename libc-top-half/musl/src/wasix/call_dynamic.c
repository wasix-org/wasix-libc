#include <wasix/call_dynamic.h>
#include <errno.h>

int wasix_call_dynamic(
    wasix_function_pointer_t function_id,
    wasix_raw_value_with_type_t *values,
    size_t values_len,
    wasix_raw_value_with_type_t *results,
    size_t *results_len,
    bool strict)
{
    int err = __wasi_call_dynamic2(
        function_id,
        values,
        values_len,
        results,
        (__wasi_pointersize_t*)results_len,
        strict ? __WASI_BOOL_TRUE : __WASI_BOOL_FALSE);

    if (err != __WASI_ERRNO_SUCCESS)
    {
        errno = err;
        return -1;
    }

    return 0;
}