#include <errno.h>
#include <wasi/api_wasi.h>
#include <wasi/api_wasix.h>
#include <wasix/continuation.h>

// Points to the last continuation context.
wasix_continuation_id_t continuation_main_context;

int wasix_continuation_context(
    wasix_continuation_id_t* new_continuation_ptr,
    void (*context_fn)()
) {
    int result = __wasi_continuation_context(new_continuation_ptr, (__wasi_function_pointer_t)context_fn);
    return result;
}

int wasix_continuation_new(
    wasix_continuation_id_t* new_continuation_ptr,
    void (*entrypoint_fn)()
) {
    return __wasi_continuation_new(new_continuation_ptr, (__wasi_function_pointer_t)entrypoint_fn);
}

int wasix_continuation_switch(
    wasix_continuation_id_t next_continuation
) {
    return __wasi_continuation_switch(next_continuation);
}

int wasix_continuation_delete(
    wasix_continuation_id_t continuation
) {
    return __wasi_continuation_delete(continuation);
}