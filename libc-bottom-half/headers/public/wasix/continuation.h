#ifndef wasix_continuation_h
#define wasix_continuation_h
// Wrappers for the continuations interface
//
// Currently they are just aliases for the syscalls, but once the
// stack switching proposal is well-supported, these could be directly implemented
// here instead

#include <wasi/api_wasi.h>
#include <wasi/api_wasix.h>
#include <wasix/function_pointer.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int wasix_continuation_id_t;


int wasix_continuation_context(
    /**
     * An index into the indirect function table.
     */
    wasix_function_pointer_t function_id
);

int wasix_continuation_new(
    /**
     * The new continuation id will be written here.
     */
    wasix_continuation_id_t* new_coroutine_ptr,
    /**
     * An index into the indirect function table.
     */
    wasix_function_pointer_t entrypoint
);

int wasix_continuation_switch(
    /**
     * The continuation id to switch to.
     */
    wasix_continuation_id_t next_coroutine
);

int wasix_continuation_delete(
    /**
     * The continuation id to delete.
     */
    wasix_continuation_id_t coroutine
);

#ifdef __cplusplus
}
#endif
#endif