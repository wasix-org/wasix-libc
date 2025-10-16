#include <errno.h>
#include <wasix/continuation.h>

int wasix_continuation_context(
    wasix_function_pointer_t function_id
) {
    return __wasi_coroutine_context(function_id);
}

int wasix_continuation_new(
    wasix_continuation_id_t* new_coroutine_ptr,
    wasix_function_pointer_t entrypoint
) {
    return __wasi_coroutine_new(new_coroutine_ptr, entrypoint);
}

int wasix_continuation_switch(
    wasix_continuation_id_t next_coroutine
) {
    return __wasi_coroutine_switch(next_coroutine);
}

int wasix_continuation_delete(
    wasix_continuation_id_t coroutine
) {
    return __wasi_coroutine_delete(coroutine);
}