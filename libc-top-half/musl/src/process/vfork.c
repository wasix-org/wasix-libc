#define _GNU_SOURCE
#include <unistd.h>
#include <signal.h>
#ifdef __wasilibc_unmodified_upstream
#include "syscall.h"
#endif

#if defined(__wasilibc_unmodified_upstream) || !defined(__wasm_exception_handling__)

pid_t vfork(void)
{
#ifdef __wasilibc_unmodified_upstream
	/* vfork syscall cannot be made from C code */
#ifdef SYS_fork
	return syscall(SYS_fork);
#else
	return syscall(SYS_clone, SIGCHLD, 0);
#endif
#else
	return _fork_internal(0);
#endif
}

#elif defined(__wasm_exception_handling__)

#include <errno.h>

// __vfork_jump is the real jump buffer
static jmp_buf __vfork_jump;
// This is passed to setjmp and moved to __vfork_jump once we are in a new
// environment
jmp_buf __vfork_jump_temporary;
// The pid of the vforked process
static pid_t __child_pid;

// setjmp/longjmp based vfork implementation
//
// This implementation of vfork uses the wasix proc_vfork syscall to create a new
// process that shares the address space with the parent until proc_exec* or proc_exit2 is
// called. However the syscalls can not cause us to jump back to the point where vfork was called,
// so we use setjmp/longjmp to simulate that behavior.
//
// This should work fine, given that the guarantees of setjmp/longjmp and vfork mostly align with each other
// The only major caveat is that we must call the setjmp in the function that called vfork, so vfork must be a macro
// and not a real function. It expands to `__vfork_internal(setjmp(__vfork_jump_temporary))`.
//
// proc_exit and proc_exec both return in the parent process after the child has exited or execed, so we need to
// longjmp back to the original context in those cases.
pid_t __vfork_internal(int setjmp_result) {
  if (setjmp_result == 0) {
    // Swaps our env with a shallow clone
    int ret = __wasi_proc_fork_env(&__child_pid);
    if (ret != 0) {
      // Fork failed
      errno = ret;
      return (pid_t)-1;
    }

    // If the vfork was successful move the jump buffer to the real jumpbuffer
    __vfork_jump[0] = __vfork_jump_temporary[0];

    // If the vfork succeeded we are now in the child

    // In the child vfork returns 0
    return (pid_t)0;
  } else {
    // In the parent vfork returns 0
    return __child_pid;
  }
}

// This function must be called in case proc_exit2 or proc_exec return without error
[[ noreturn ]] void __vfork_restore() {
  // Longjmp back to the vfork call site in the parent
  longjmp(__vfork_jump, 1);
  __builtin_unreachable();
}

#endif /* defined(__wasilibc_unmodified_upstream) || !defined(__wasm_exception_handling__) */
