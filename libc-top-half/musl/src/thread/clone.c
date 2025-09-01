#include <errno.h>
#include <sched.h>
#include <unistd.h>
#include "pthread_impl.h"

// WASIX uses a separate syscall for posix_spawn, so __clone isn't needed
#if defined(__wasilibc_unmodified_upstream)
#ifndef CLONE_VFORK
#define CLONE_VFORK	0x00004000
#endif

int __clone(int (*func)(void *), void *stack, int flags, void *arg, ...)
{
	int pid;
	if ((flags & CLONE_VFORK) != 0) {
		pid = vfork();
	} else {
		pid = fork();
	}
	if (pid == -1) {
		return -pid;
	}
	if (pid == 0) {
		return func(arg);
	}
	return pid;
}
#endif /* defined(__wasilibc_unmodified_upstream) */