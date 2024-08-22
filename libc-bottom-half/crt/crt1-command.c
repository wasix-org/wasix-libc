#ifdef _REENTRANT
#include <stdatomic.h>
extern void __wasi_init_tp(void);
#endif
#include <wasi/api.h>
extern void __wasm_call_ctors(void);
extern int __main_void(void);
extern void __wasm_call_dtors(void);

// We use `volatile` here to prevent the store to `started` from being
// sunk past any subsequent code, and to prevent any compiler from
// optimizing based on the knowledge that `_start` is the program
// entrypoint.
#ifdef _REENTRANT
static volatile _Atomic int __wasix_started = 0;
static volatile _Atomic int __wasix_resumed = 0;
#else
static volatile int __wasix_started = 0;
static volatile int __wasix_resumed = 0;
#endif

__attribute__((export_name("_start")))
void _start(void) {
    // Commands should only be called once per instance. This simple check
    // ensures that the `_start` function isn't started more than once.
#ifdef _REENTRANT
    int expected = 0;
    int resume_expected = 0;
    if (!atomic_compare_exchange_strong(&__wasix_started, &expected, 1)) {
        __builtin_trap();
    }
    if (!atomic_compare_exchange_strong(&__wasix_resumed, &resume_expected, 1)) {
        __builtin_trap();
    }
#else
    if (__wasix_started != 0) {
        __builtin_trap();
    }
    __wasix_started = 1;
    if (__wasix_resumed != 0) {
        __builtin_trap();
    }
    __wasix_resumed = 1;
#endif

#ifdef _REENTRANT
	__wasi_init_tp();
#endif

    // The linker synthesizes this to call constructors.
    __wasm_call_ctors();

    // Call `__main_void` which will either be the application's zero-argument
    // `__main_void` function or a libc routine which obtains the command-line
    // arguments and calls `__main_argv_argc`.
    int r = __main_void();

    // Call atexit functions, destructors, stdio cleanup, etc.
    __wasm_call_dtors();

    // If main exited successfully, just return, otherwise call
    // `__wasi_proc_exit`.
    if (r != 0) {
        __wasi_proc_exit(r);
    }
}

// This function acts as the top "half" of _start, up until the call to
// __main_void. This is useful when a module needs to do initialization
// separately, e.g. when using partial evaluation tools. When used with
// __wasix_resume, they perform the same logic as calling _start once.
__attribute__((export_name("__wasix_init")))
void __wasix_init(void) {
#ifdef _REENTRANT
    int expected = 0;
    if (!atomic_compare_exchange_strong(&__wasix_started, &expected, 1)) {
	__builtin_trap();
    }
#else
    if (__wasix_started != 0) {
        __builtin_trap();
    }
    __wasix_started = 1;
#endif

#ifdef _REENTRANT
	__wasi_init_tp();
#endif

    __wasm_call_ctors();
}

// This is the bottom "half" of _start, which can be called after
// __wasix_init to continue executing the module normally. This
// function also checks the value of __wasix_started to make sure
// __wasix_init was called beforehand.
__attribute__((export_name("__wasix_resume")))
void __wasix_resume(void) {
#ifdef _REENTRANT
    if (atomic_load(&__wasix_started) != 1) {
        __builtin_trap();
    }
    int expected = 0;
    if (!atomic_compare_exchange_strong(&__wasix_resumed, &expected, 1)) {
	__builtin_trap();
    }
#else
    if (__wasix_started != 1) {
        __builtin_trap();
    }
    if (__wasix_resumed != 0) {
        __builtin_trap();
    }
    __wasix_resumed = 1;
#endif

    int r = __main_void();

    __wasm_call_dtors();

    if (r != 0) {
        __wasi_proc_exit(r);
    }
}