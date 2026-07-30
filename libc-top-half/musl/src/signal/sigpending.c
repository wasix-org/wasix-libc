#include <signal.h>
#include <errno.h>
#ifdef __wasilibc_unmodified_upstream
#include "syscall.h"
#else
#include <wasi/api.h>
#include "pthread_impl.h"
#include "sigmask.h"
#endif

int sigpending(sigset_t *set)
{
#ifdef __wasilibc_unmodified_upstream
	return syscall(SYS_rt_sigpending, set, _NSIG/8);
#else
	/* A signal the host has queued but not yet handed over is not in our
	 * pending set yet. Delivery happens at syscall boundaries, so yielding
	 * flushes the queue into it (blocked ones land in sigpending, deliverable
	 * ones run their handlers) and makes the answer current. */
	(void)__wasi_sched_yield();
	__sigset_copy(set, &__pthread_self()->sigpending);
	return 0;
#endif
}