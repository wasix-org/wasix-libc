#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#ifdef __wasilibc_unmodified_upstream
#include "syscall.h"
#endif
#include "pthread_impl.h"

_Noreturn void siglongjmp(sigjmp_buf buf, int ret)
{
#if defined(__wasilibc_unmodified_upstream) || defined(__wasm_exception_handling__)
	longjmp(buf, ret);
#else
	__wasi_stack_snapshot_t *snapshot = buf;
	if (snapshot->user != 0) {
		sigset_t *mask = (sigset_t *)(uintptr_t)snapshot->user;
		sigprocmask(SIG_SETMASK, mask, NULL);
	}
	longjmp(buf, ret);
#endif
}
