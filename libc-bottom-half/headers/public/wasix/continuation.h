#ifndef wasix_continuation_h
#define wasix_continuation_h
// Wrappers for the continuations interface
//
// Currently they are just aliases for the syscalls, but once the
// stack switching proposal is well-supported, these could be directly implemented
// here instead

#include <wasi/api_wasi.h>
#include <wasi/api_wasix.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int wasix_continuation_id_t;

extern wasix_continuation_id_t continuation_main_context;

int wasix_continuation_context(
    // The root continuation id will be written here.
    wasix_continuation_id_t* new_continuation_ptr,
    // An index into the indirect function table.
    void (*context_fn)()
);


int wasix_continuation_new(
    // The new continuation id will be written here.
    wasix_continuation_id_t* new_continuation_ptr,
    // An index into the indirect function table.
    void (*entrypoint_fn)()
);

int wasix_continuation_switch(
    // The continuation id to switch to.
    wasix_continuation_id_t next_continuation
);

int wasix_continuation_delete(
    // The continuation id to delete.
    wasix_continuation_id_t continuation
);

#ifdef __cplusplus
}
#endif
#endif