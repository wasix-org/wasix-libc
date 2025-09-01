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

#endif /* defined(__wasilibc_unmodified_upstream) || !defined(__wasm_exception_handling__) */