#include "pthread_impl.h"
#ifdef __wasilibc_unmodified_upstream
#include "syscall.h"
#else
#include <wasi/api.h>
#endif
#include <signal.h>
#ifdef __wasilibc_unmodified_upstream
#else
#include "sigmask.h"
#endif

static const unsigned long all_mask[] = {
#if ULONG_MAX == 0xffffffff && _NSIG == 129
	-1UL, -1UL, -1UL, -1UL
#elif ULONG_MAX == 0xffffffff
	-1UL, -1UL
#else
	-1UL
#endif
};

static const unsigned long app_mask[] = {
#if ULONG_MAX == 0xffffffff
#if _NSIG == 65
	0x7fffffff, 0xfffffffc
#else
	0x7fffffff, 0xfffffffc, -1UL, -1UL
#endif
#else
#if _NSIG == 65
	0xfffffffc7fffffff
#else
	0xfffffffc7fffffff, -1UL
#endif
#endif
};

void __block_all_sigs(void *set)
{
#ifdef __wasilibc_unmodified_upstream
	__syscall(SYS_rt_sigprocmask, SIG_BLOCK, &all_mask, set, _NSIG/8);
#else
	pthread_t self = __pthread_self();
	if (set) __sigset_copy((sigset_t *)set, &self->sigmask);
	__sigset_or(&self->sigmask, all_mask);
#endif
}

void __block_app_sigs(void *set)
{
#ifdef __wasilibc_unmodified_upstream
	__syscall(SYS_rt_sigprocmask, SIG_BLOCK, &app_mask, set, _NSIG/8);
#else
	pthread_t self = __pthread_self();
	if (set) __sigset_copy((sigset_t *)set, &self->sigmask);
	__sigset_or(&self->sigmask, app_mask);
#endif
}

void __restore_sigs(void *set)
{
#ifdef __wasilibc_unmodified_upstream
	__syscall(SYS_rt_sigprocmask, SIG_SETMASK, set, 0, _NSIG/8);
#else
	pthread_t self = __pthread_self();
	if (set) __sigset_copy(&self->sigmask, (const sigset_t *)set);
	/* Restoring can unblock, and nothing else will come along to deliver:
	 * WASIX only delivers signals at syscall boundaries. */
	__sig_deliver_pending();
#endif
}