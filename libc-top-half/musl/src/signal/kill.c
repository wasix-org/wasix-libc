#include <signal.h>
#ifdef __wasilibc_unmodified_upstream
#include "syscall.h"
#else
#include <errno.h>
#include <wasi/api.h>
#endif

int kill(pid_t pid, int sig)
{
#ifdef __wasilibc_unmodified_upstream
	return syscall(SYS_kill, pid, sig);
#else
	int r = __wasi_proc_signal(pid, (__wasi_signal_t)sig);
	if (r != 0) {
		errno = r;
		return -1;
	}
	return 0;
#endif
}