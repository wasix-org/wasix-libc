#ifndef SIGMASK_H
#define SIGMASK_H

#include <signal.h>

/* Meaningful extent of a sigset_t. The structure is padded well past the
 * number of signals that exist, and k_sigaction stores only this much, so all
 * mask arithmetic is done over this many bytes. */
#define __SIGSET_BYTES (_NSIG/8)

static inline void __sigset_or(sigset_t *dst, const void *bits)
{
	unsigned char *d = (unsigned char *)dst;
	const unsigned char *s = (const unsigned char *)bits;
	for (unsigned i = 0; i < __SIGSET_BYTES; i++) d[i] |= s[i];
}

static inline void __sigset_andnot(sigset_t *dst, const void *bits)
{
	unsigned char *d = (unsigned char *)dst;
	const unsigned char *s = (const unsigned char *)bits;
	for (unsigned i = 0; i < __SIGSET_BYTES; i++) d[i] &= ~s[i];
}

static inline void __sigset_copy(sigset_t *dst, const void *bits)
{
	unsigned char *d = (unsigned char *)dst;
	const unsigned char *s = (const unsigned char *)bits;
	for (unsigned i = 0; i < __SIGSET_BYTES; i++) d[i] = s[i];
}

/* Unlike sigaddset()/sigismember(), these neither validate `sig` nor touch
 * errno, so they are safe on the signal delivery path -- clobbering errno
 * there would corrupt the error reporting of whatever syscall the delivery
 * interrupted. Callers must keep `sig` within 1.._NSIG-1. */
static inline int __sigset_test(const sigset_t *set, int sig)
{
	unsigned s = sig-1;
	return !!(set->__bits[s/8/sizeof *set->__bits] & 1UL<<(s&8*sizeof *set->__bits-1));
}

static inline void __sigset_add(sigset_t *set, int sig)
{
	unsigned s = sig-1;
	set->__bits[s/8/sizeof *set->__bits] |= 1UL<<(s&8*sizeof *set->__bits-1);
}

static inline void __sigset_del(sigset_t *set, int sig)
{
	unsigned s = sig-1;
	set->__bits[s/8/sizeof *set->__bits] &= ~(1UL<<(s&8*sizeof *set->__bits-1));
}

#endif
