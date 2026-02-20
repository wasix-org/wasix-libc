#include <signal.h>
#include <errno.h>
#ifdef __wasilibc_unmodified_upstream
#include "syscall.h"

int pthread_sigmask(int how, const sigset_t *restrict set, sigset_t *restrict old)
{
	int ret;
	if (set && (unsigned)how - SIG_BLOCK > 2U) return EINVAL;
	ret = -__syscall(SYS_rt_sigprocmask, how, set, old, _NSIG/8);
	if (!ret && old) {
		if (sizeof old->__bits[0] == 8) {
			old->__bits[0] &= ~0x380000000ULL;
		} else {
			old->__bits[0] &= ~0x80000000UL;
			old->__bits[1] &= ~0x3UL;
		}
	}
	return ret;
}
#else
int pthread_sigmask(int how, const sigset_t *restrict set, sigset_t *restrict old)
{
	static __thread sigset_t sigmask;

	if (set && (unsigned)how - SIG_BLOCK > 2U) return EINVAL;
	if (old) *old = sigmask;

	if (set) {
		size_t n = sizeof(sigmask.__bits) / sizeof(sigmask.__bits[0]);
		switch (how) {
		case SIG_BLOCK:
			for (size_t i = 0; i < n; i++) sigmask.__bits[i] |= set->__bits[i];
			break;
		case SIG_UNBLOCK:
			for (size_t i = 0; i < n; i++) sigmask.__bits[i] &= ~set->__bits[i];
			break;
		case SIG_SETMASK:
			for (size_t i = 0; i < n; i++) sigmask.__bits[i] = set->__bits[i];
			break;
		}
	}

	return 0;
}
#endif
