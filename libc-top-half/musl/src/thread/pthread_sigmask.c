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
#include "pthread_impl.h"
#include "sigmask.h"

/* WASIX has no kernel to hold the mask, so it lives in the thread structure
 * and is consulted by __wasm_signal() when the host delivers a signal. */
int pthread_sigmask(int how, const sigset_t *restrict set, sigset_t *restrict old)
{
	pthread_t self = __pthread_self();

	if (set && (unsigned)how - SIG_BLOCK > 2U) return EINVAL;

	if (old) {
		__sigset_copy(old, &self->sigmask);
		/* The libc-internal signals are not the application's business. */
		if (sizeof old->__bits[0] == 8) {
			old->__bits[0] &= ~0x380000000ULL;
		} else {
			old->__bits[0] &= ~0x80000000UL;
			old->__bits[1] &= ~0x3UL;
		}
	}

	if (set) {
		sigset_t next = self->sigmask;
		switch (how) {
		case SIG_BLOCK:
			__sigset_or(&next, set);
			break;
		case SIG_UNBLOCK:
			__sigset_andnot(&next, set);
			break;
		case SIG_SETMASK:
			__sigset_copy(&next, set);
			break;
		}

		/* An application must never be able to block the signals libc uses
		 * internally, or pthread_cancel() and timers stop working. */
		__sigset_del(&next, SIGTIMER);
		__sigset_del(&next, SIGCANCEL);
		__sigset_del(&next, SIGSYNCCALL);

		self->sigmask = next;

		/* Unblocking has to deliver: signals only ever arrive at syscall
		 * boundaries, so nothing else would pick these up. */
		if (how != SIG_BLOCK) __sig_deliver_pending();
	}

	return 0;
}
#endif