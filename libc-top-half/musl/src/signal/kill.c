#include <signal.h>
#include "syscall.h"
#ifndef __wasilibc_unmodified_upstream
#include <wasi/api.h>
#endif

int kill(pid_t pid, int sig)
{
#ifdef __wasilibc_unmodified_upstream
	return syscall(SYS_kill, pid, sig);
#else
	int r = __wasi_proc_signal(pid, (__wasi_signal_t)sig);
	return __syscall_ret(-r);
#endif
}
