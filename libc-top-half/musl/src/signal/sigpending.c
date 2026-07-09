#include <signal.h>
#include <errno.h>
#include "syscall.h"

int sigpending(sigset_t *set)
{
#ifdef __wasilibc_unmodified_upstream
	return syscall(SYS_rt_sigpending, set, _NSIG/8);
#else
	(void)set;
	return __syscall_ret(-EINVAL);
#endif
}
