#ifndef __wasi_polyfill_h
#define __wasi_polyfill_h

#include <stddef.h>
#include <stdint.h>

/**
 * WASIX extension to the preview1 event type namespace. Indicates that a file
 * descriptor has an exceptional condition pending, such as TCP urgent data.
 */
#define __WASI_EVENTTYPE_FD_EXCEPT (UINT8_C(3))

#ifdef _REENTRANT
/**
 * Request a new thread to be created by the host.
 *
 * The host will create a new instance of the current module sharing its
 * memory, find an exported entry function--`wasi_thread_start`--, and call the
 * entry function with `start_arg` in the new thread.
 *
 * @see https://github.com/WebAssembly/wasi-threads/#readme
 */
size_t __wasi_thread_spawn(
    /**
     * A pointer to an opaque struct to be passed to the module's entry
     * function.
     */
    void *start_arg
)  __attribute__((__warn_unused_result__));
#endif

#endif