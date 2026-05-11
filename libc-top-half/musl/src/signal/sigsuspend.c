#include <signal.h>
#include <errno.h>
#include "syscall.h"

int sigsuspend(const sigset_t *mask)
{
#ifdef __wasilibc_unmodified_upstream
	return syscall_cp(SYS_rt_sigsuspend, mask, _NSIG/8);
#else
	(void)mask;
	return __syscall_ret(-EINVAL);
#endif
}
