#ifndef wasix_context_h
#define wasix_context_h
// Wrappers for the WASIX context switching interface
//
// Currently they are just aliases for the syscalls, but once the
// stack switching proposal is well-supported, these could be directly implemented
// here (and in the crt) instead.

#include <wasi/api_wasi.h>
#include <wasi/api_wasix.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef __wasi_context_id_t wasix_context_id_t;

extern wasix_context_id_t context_main_context;

int wasix_context_new(
    // The new context id will be written here.
    wasix_context_id_t* new_context_ptr,
    // An index into the indirect function table.
    void (*entrypoint_fn)()
);

int wasix_context_switch(
    // The context id to switch to.
    wasix_context_id_t next_context
);

int wasix_context_delete(
    // The context id to delete.
    wasix_context_id_t context
);

#ifdef __cplusplus
}
#endif
#endif