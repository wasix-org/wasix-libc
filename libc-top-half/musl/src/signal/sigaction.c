#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sysexits.h>
#include "syscall.h"
#include "pthread_impl.h"
#include "libc.h"
#include "lock.h"
#include "ksigaction.h"

static int unmask_done;
static unsigned long handler_set[_NSIG/(8*sizeof(long))];
#ifdef __wasilibc_unmodified_upstream
#else
static volatile int __eintr_callback_registered = 0;
static volatile struct k_sigaction __eintr_handler_callbacks[_NSIG];
volatile int __eintr_handler_lock[1];
#endif

void __get_handler_set(sigset_t *set)
{
	memcpy(set, handler_set, sizeof handler_set);
}

_Noreturn
static void core_handler(int sig) {
    fprintf(stderr, "Program recieved fatal signal: %s\n", strsignal(sig));
    abort();
}

_Noreturn
static void terminate_handler(int sig) {
    fprintf(stderr, "Program recieved termination signal: %s\n", strsignal(sig));
    abort();
}

_Noreturn
static void stop_handler(int sig) {
    fprintf(stderr, "Program recieved stop signal: %s\n", strsignal(sig));
    abort();
}

static void continue_handler(int sig) {
    // do nothing
}

#ifdef __wasilibc_unmodified_upstream
typedef void (*sighandler_t)(int);
static const sighandler_t default_handlers[_NSIG] = {
    // Default behavior: "core".
    [SIGABRT] = core_handler,
    [SIGBUS] = core_handler,
    [SIGFPE] = core_handler,
    [SIGILL] = core_handler,
#if SIGIOT != SIGABRT
    [SIGIOT] = core_handler,
#endif
    [SIGQUIT] = core_handler,
    [SIGSEGV] = core_handler,
    [SIGSYS] = core_handler,
    [SIGTRAP] = core_handler,
    [SIGXCPU] = core_handler,
    [SIGXFSZ] = core_handler,
#if defined(SIGUNUSED) && SIGUNUSED != SIGSYS
    [SIGUNUSED] = core_handler,
#endif

    // Default behavior: ignore.
    [SIGCHLD] = SIG_IGN,
#if defined(SIGCLD) && SIGCLD != SIGCHLD
    [SIGCLD] = SIG_IGN,
#endif
    [SIGURG] = SIG_IGN,
    [SIGWINCH] = SIG_IGN,

    // Default behavior: "continue".
    [SIGCONT] = continue_handler,

    // Default behavior: "stop".
    [SIGSTOP] = stop_handler,
    [SIGTSTP] = stop_handler,
    [SIGTTIN] = stop_handler,
    [SIGTTOU] = stop_handler,

    // Default behavior: "terminate".
    [SIGHUP] = terminate_handler,
    [SIGINT] = terminate_handler,
    [SIGKILL] = terminate_handler,
    [SIGUSR1] = terminate_handler,
    [SIGUSR2] = terminate_handler,
    [SIGPIPE] = terminate_handler,
    [SIGALRM] = terminate_handler,
    [SIGTERM] = terminate_handler,
    [SIGSTKFLT] = terminate_handler,
    [SIGVTALRM] = terminate_handler,
    [SIGPROF] = terminate_handler,
    [SIGIO] = terminate_handler,
#if SIGPOLL != SIGIO
    [SIGPOLL] = terminate_handler,
#endif
    [SIGPWR] = terminate_handler,
};
#else
typedef void (*sighandler_t)(int);
static sighandler_t default_handler = NULL;

static void __default_handler(int sig) {
	switch (sig) {
		// Default behavior: "core".
		case SIGABRT:
		case SIGBUS:
		case SIGFPE:
		case SIGILL:
	#if SIGIOT != SIGABRT
		case SIGIOT:
	#endif
		case SIGQUIT:
		case SIGSEGV:
		case SIGSYS:
		case SIGTRAP:
		case SIGXCPU:
		case SIGXFSZ:
	#if defined(SIGUNUSED) && SIGUNUSED != SIGSYS
		case SIGUNUSED:
	#endif
			core_handler(sig);
			break;

		// Default behavior: ignore.
		case SIGCHLD:
#if defined(SIGCLD) && SIGCLD != SIGCHLD
		case SIGCLD:
#endif
		case SIGURG:
		case SIGWINCH:
			SIG_IGN(sig);
			break;

		// Default behavior: "continue".
		case SIGCONT:
			continue_handler(sig);
			break;

		// Default behavior: "stop".
		case SIGSTOP:
		case SIGTSTP:
		case SIGTTIN:
		case SIGTTOU:
			stop_handler(sig);
			break;

		// Default behavior: "terminate".
		case SIGHUP:
		case SIGINT:
		case SIGKILL:
		case SIGUSR1:
		case SIGUSR2:
		case SIGPIPE:
		case SIGALRM:
		case SIGTERM:
		case SIGSTKFLT:
		case SIGVTALRM:
		case SIGPROF:
		case SIGIO:
#if SIGPOLL != SIGIO
		case SIGPOLL:
#endif
		case SIGPWR:
			terminate_handler(sig);
			break;
	}
}
#endif

static sighandler_t handlers[_NSIG];

volatile int __eintr_valid_flag;

#ifdef __wasilibc_unmodified_upstream
#else
__attribute__((export_name("__wasm_signal")))
void __wasm_signal(int sig) {
	if (sig-32U < 3 || sig-1U >= _NSIG-1) {
		return;
	}
	LOCK(__eintr_handler_lock);
	struct k_sigaction ksa = __eintr_handler_callbacks[sig];
	UNLOCK(__eintr_handler_lock);

	if (ksa.handler != 0) {
		ksa.handler(sig);
	} else {
		unsigned long set[_NSIG/(8*sizeof(long))];
		__block_all_sigs(&set);
		default_handler(sig);
		__restore_sigs(&set);
	}
}
#endif

int __libc_sigaction(int sig, const struct sigaction *restrict sa, struct sigaction *restrict old)
{
	struct k_sigaction ksa, ksa_old;
	if (sa) {
		if ((uintptr_t)sa->sa_handler > 1UL) {
			a_or_l(handler_set+(sig-1)/(8*sizeof(long)),
				1UL<<(sig-1)%(8*sizeof(long)));

			/* If pthread_create has not yet been called,
			 * implementation-internal signals might not
			 * yet have been unblocked. They must be
			 * unblocked before any signal handler is
			 * installed, so that an application cannot
			 * receive an illegal sigset_t (with them
			 * blocked) as part of the ucontext_t passed
			 * to the signal handler. */
			if (!libc.threaded && !unmask_done) {
#ifdef __wasilibc_unmodified_upstream
				__syscall(SYS_rt_sigprocmask, SIG_UNBLOCK,
					SIGPT_SET, 0, _NSIG/8);
#endif
				unmask_done = 1;
			}

			if (!(sa->sa_flags & SA_RESTART)) {
				a_store(&__eintr_valid_flag, 1);
			}
		}
		ksa.handler = sa->sa_handler;
		ksa.flags = sa->sa_flags | SA_RESTORER;
		ksa.restorer = (sa->sa_flags & SA_SIGINFO) ? __restore_rt : __restore;
		memcpy(&ksa.mask, &sa->sa_mask, _NSIG/8);
	}
#ifdef __wasilibc_unmodified_upstream
	int r = __syscall(SYS_rt_sigaction, sig, sa?&ksa:0, old?&ksa_old:0, _NSIG/8);
#else
	if (a_cas(&__eintr_callback_registered, 0, 1) == 0) {
		__wasi_callback_signal("__wasm_signal");
	}
	int r = 0;
	if (sig-32U < 3 || sig-1U >= _NSIG-1) {
		r = EINVAL;
	} else {
		LOCK(__eintr_handler_lock);
		ksa_old = __eintr_handler_callbacks[sig];
		if (sa) {
			__eintr_handler_callbacks[sig] = ksa;
		}
		UNLOCK(__eintr_handler_lock);
		r = 0;
	}
#endif
	if (old && !r) {
		old->sa_handler = ksa_old.handler;
		old->sa_flags = ksa_old.flags;
		memcpy(&old->sa_mask, &ksa_old.mask, _NSIG/8);
	}
	return __syscall_ret(r);
}

#ifdef __wasilibc_unmodified_upstream
int __sigaction(int sig, const struct sigaction *restrict sa, struct sigaction *restrict old)
{
	unsigned long set[_NSIG/(8*sizeof(long))];

	if (sig-32U < 3 || sig-1U >= _NSIG-1) {
		errno = EINVAL;
		return -1;
	}

	/* Doing anything with the disposition of SIGABRT requires a lock,
	 * so that it cannot be changed while abort is terminating the
	 * process and so any change made by abort can't be observed. */
	if (sig == SIGABRT) {
		__block_all_sigs(&set);
		LOCK(__abort_lock);
	}
	int r = __libc_sigaction(sig, sa, old);
	if (sig == SIGABRT) {
		UNLOCK(__abort_lock);
		__restore_sigs(&set);
	}
	return r;
}
#else
static int __sigaction_inner(int sig, const struct sigaction *restrict sa, struct sigaction *restrict old)
{
	unsigned long set[_NSIG/(8*sizeof(long))];

	if (sig-32U < 3 || sig-1U >= _NSIG-1) {
		errno = EINVAL;
		return -1;
	}

	/* Doing anything with the disposition of SIGABRT requires a lock,
	 * so that it cannot be changed while abort is terminating the
	 * process and so any change made by abort can't be observed. */
	if (sig == SIGABRT) {
		__block_all_sigs(&set);
		LOCK(__abort_lock);
	}
	int r = __libc_sigaction(sig, sa, old);
	if (sig == SIGABRT) {
		UNLOCK(__abort_lock);
		__restore_sigs(&set);
	}
	return r;
}

int __sigaction(int sig, const struct sigaction *restrict sa, struct sigaction *restrict old)
{
	if (default_handler == NULL) default_handler = &__default_handler;
	return __sigaction_inner(sig, sa, old);
}
#endif

weak_alias(__sigaction, sigaction);

#ifdef __wasilibc_unmodified_upstream
#else
/* Currently, core_handler cannot be compiled in a rust program.
 * To keep that out of the compilation, the rust libc does not
 * use __sigaction above (which references __default_handler),
 * instead using this function to pass in a default handler of
 * its own. */
int __sigaction_external_default(int sig, const struct sigaction *restrict sa, struct sigaction *restrict old, sighandler_t _default_handler)
{
	if (default_handler == NULL) default_handler = _default_handler;
	return __sigaction_inner(sig, sa, old);
}

weak_alias(__sigaction_external_default, sigaction_external_default);

__attribute__((export_name("__wasm_sigaction")))
int __wasm_sigaction(int sig, int action) {
	void (*a)(int);

	switch (action) {
		case __WASI_DISPOSITION_DEFAULT:
			a = SIG_DFL;
			break;
		case __WASI_DISPOSITION_IGNORE:
			a = SIG_IGN;
			break;
		default:
			return -1;
	}

	struct sigaction sa = { .sa_handler = a, .sa_flags = SA_RESTART };
	if (__sigaction(sig, &sa, NULL) < 0) {
		return -1;
	}

	return 0;
}

void __wasi_init_signals() {
    __wasi_errno_t err;
	int sigaction_ret;

    __wasi_size_t signal_count;
    err = __wasi_proc_signals_sizes_get(&signal_count);
    if (err != __WASI_ERRNO_SUCCESS) {
        _Exit(EX_OSERR);
    }
	
	__wasi_signal_disposition_t *sig_dispositions = calloc(signal_count, sizeof(__wasi_signal_disposition_t));
    if (sig_dispositions == NULL) {
        _Exit(EX_SOFTWARE);
    }

    err = __wasi_proc_signals_get((uint8_t *)sig_dispositions);
    if (err != __WASI_ERRNO_SUCCESS) {
        free(sig_dispositions);
        _Exit(EX_OSERR);
    }

	for (int i = 0; i < signal_count; ++i) {
		sigaction_ret = __wasm_sigaction((int)sig_dispositions[i].sig, (int)sig_dispositions[i].disp);
		if (sigaction_ret == -1) {
			free(sig_dispositions);
			_Exit(EX_OSERR);
		}
	}

	free(sig_dispositions);

	// Unconditionally register the signal handler at startup - otherwise, the host will
	// eat up signals that are sent before the first sigaction call.
	if (a_cas(&__eintr_callback_registered, 0, 1) == 0) {
		__wasi_callback_signal("__wasm_signal");
	}
}
#endif
