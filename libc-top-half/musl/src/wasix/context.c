#include <errno.h>
#include <wasi/api_wasi.h>
#include <wasi/api_wasix.h>
#include <wasix/context.h>

// Points to the last context.
wasix_context_id_t context_main_context;

int wasix_context_new(
    wasix_context_id_t* new_context_ptr,
    void (*entrypoint_fn)()
) {
    return __wasi_context_new(new_context_ptr, (__wasi_function_pointer_t)entrypoint_fn);
}

int wasix_context_switch(
    wasix_context_id_t next_context
) {
    return __wasi_context_switch(next_context);
}

int wasix_context_delete(
    wasix_context_id_t context
) {
    return __wasi_context_delete(context);
}