#ifndef wasix_context_h
#define wasix_context_h

#include <wasi/api_wasi.h>
#include <wasi/api_wasix.h>

#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------------
// WASIX Context Switching API
// --------------------------------------------------------------------------
//
// The WASIX context switching API provides lightweight execution contexts
// with independent call stacks.
//
// A context represents an execution state with its own call stack. It may
// be in one of the following states:
//
// - **active**
//   Currently executing. Only one context is active within a given
//   context-switching environment.
// - **suspended**
//   Execution is suspended and may be resumed with `wasix_context_switch()`.
// - **terminated**
//   The entrypoint has returned, thrown an uncaught exception, or triggered a
//   trap.
// - **deleted**
//   All resources associated with the context have been released; equivalent to
//   a context that was never created.
//
// Contexts exist within a context-switching environment. A context may only
// be resumed in the environment in which it was created. Future versions
// of this API may relax this restriction.
//
// Each context-switching environment has a single main context which is always
// the first active context in an context-switching environment. The main
// context cannot be deleted. If the main context terminates, the entire
// context-switching environment terminates.
//
// If no context-switching environment is available, functions in this API fail
// with `EAGAIN`. The conditions under which a context-switching environment is
// available are undefined.
//
// Behavior not specified here is implementation-defined and may be specified
// in future revisions of this API.
//
// #### Wasmer implementation note:
// In wasmer each thread corresponds to a context-switching environment
// beginning at the program’s `main` function. During early initialization, no
// context-switching environment may be available. This behavior may change in
// future versions.

// Opaque identifier referring to a WASIX context.
//
// A context identifier is valid only within the context-switching environment
// from which it was obtained with `wasix_context_create()` or
// `wasix_context_main`.
typedef __wasi_context_id_t wasix_context_id_t;

// Identifier of the main context of the current context-switching
// environment.
//
// If no context-switching environment is active, accessing
// `wasix_context_main` yields an undefined identifier.
#define wasix_context_main (__wasix_context_main())
extern wasix_context_id_t __wasix_context_main(void);

// Create a new context.
//
// Creates a new context in the suspended state. On its first resumption,
// `entrypoint` is invoked within that context.
//
// The entrypoint function must not return, raise an uncaught exception, or
// terminate with a trap. If it does, the context enters the terminated state.
// This typically causes termination of the WASIX program unless host code
// handles the trap or error. Exact behavior may change in future versions.
//
// #### Wasmer implementation note
// In the current Wasmer implementation, termination of the entrypoint always
// resumes the main context and produces a trap:
//
// - Entrypoint terminated with a trap:
//   The same trap is produced in the main context.
// - Entrypoint returned normally:
//   A custom user trap is produced in the main context.
// - Entrypoint threw an uncaught exception:
//   The exception is re-thrown in the main context.
//
// This behavior may change in future versions.
//
// #### Parameters
// - `context_id` pointer to where the new context identifier will be stored
// - `entrypoint` function invoked when the context is first resumed
//
// #### Return Value
// - `0` on success
// - `-1` on failure, with `errno` set
//
// #### Errors
// - `EINVAL` entrypoint is not a valid function with the required signature
// - `EAGAIN` not in a context-switching environment
// - `<memory-error>` Implementations may set `errno` to one of the
// WASIX-defined
//   memory error codes if `context_id` or memory reachable from the arguments
//   is invalid or inaccessible.
int wasix_context_create(wasix_context_id_t *context_id,
                         void (*entrypoint)(void));

// Destroy a suspended or terminated context.
//
// Destroys a context that is suspended or terminated. After a successful call,
// the context enters the deleted state; its identifier becomes invalid and its
// resources are released. Destroying an already deleted context is a no-op.
//
// The main context cannot be destroyed.
//
// Terminated contexts are effectively equivalent to deleted contexts, but
// `wasix_context_destroy()` must still be called to release resources.
//
// #### Wasmer implementation note
// Destroying a suspended context may produce an opaque user trap in the target
// context, requiring the entrypoint to terminate. External functions that call
// back into WASIX must propagate the trap unchanged; otherwise, behavior is
// undefined.
//
// #### Parameters
// - `context_id` identifier of the context to destroy
//
// #### Return Value
// - `0` on success
// - `-1` on failure, with `errno` set
//
// #### Errors
// - `EINVAL` can not destroy the active context
// - `EINVAL` can not destroy the main context
// - `EAGAIN` not in a context-switching environment
int wasix_context_destroy(wasix_context_id_t context_id);

// Suspend the active context and resume another.
//
// Suspends the active context and resumes execution in `target_context_id`.
// The resumed context continues from where it was last suspended, or from its
// entrypoint if it has never been resumed.
//
// If `target_context_id` refers to the active context, the call is a no-op.
//
// #### Wasmer implementation note
// In the current Wasmer implementation, calling `wasix_context_switch()` in
// the main context may produce a trap if another context terminates while the
// main context is suspended. See the implementation note in
// `wasix_context_create()`.
//
// #### Parameters
// - `target_context_id` identifier of the suspended context to resume
//
// #### Return Value
// - `0` on success
// - `-1` on error, with `errno` set
//
// #### Errors
// - `EINVAL` target context is not suspended
// - `EAGAIN` not in a context-switching environment
int wasix_context_switch(wasix_context_id_t target_context_id);

#ifdef __cplusplus
}
#endif
#endif