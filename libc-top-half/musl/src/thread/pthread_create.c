#define _GNU_SOURCE
#include "pthread_impl.h"
#include "stdio_impl.h"
#include "libc.h"
#include "lock.h"
#ifdef __wasilibc_unmodified_upstream
#include <sys/mman.h>
#endif
#include <string.h>
#include <stddef.h>
#ifndef __wasilibc_unmodified_upstream
#include <stdatomic.h>
#include <wasi/api.h>
#include <bits/signal.h>
#endif

#include <stdalign.h>

static void dummy_0()
{
}
#ifdef __wasilibc_unmodified_upstream
weak_alias(dummy_0, __acquire_ptc);
weak_alias(dummy_0, __release_ptc);
weak_alias(dummy_0, __pthread_tsd_run_dtors);
weak_alias(dummy_0, __do_orphaned_stdio_locks);
weak_alias(dummy_0, __dl_thread_cleanup);
weak_alias(dummy_0, __membarrier_init);
#endif

static int tl_lock_count;
static int tl_lock_waiters;

void __tl_lock(void)
{
	int tid = __pthread_self()->tid;
	int val = __thread_list_lock;
	if (val == tid) {
		tl_lock_count++;
		return;
	}
	while ((val = a_cas(&__thread_list_lock, 0, tid)))
		__wait(&__thread_list_lock, &tl_lock_waiters, val, 0);
}

void __tl_unlock(void)
{
	if (tl_lock_count) {
		tl_lock_count--;
		return;
	}
	a_store(&__thread_list_lock, 0);
	if (tl_lock_waiters) __wake(&__thread_list_lock, 1, 0);
}

void __tl_sync(pthread_t td)
{
	a_barrier();
	int val = __thread_list_lock;
	if (!val) return;
	__wait(&__thread_list_lock, &tl_lock_waiters, val, 0);
	if (tl_lock_waiters) __wake(&__thread_list_lock, 1, 0);
}

_Noreturn void __pthread_exit(void *result)
{
	pthread_t self = __pthread_self();
	sigset_t set;

	self->canceldisable = 1;
	self->cancelasync = 0;
	self->result = result;

	while (self->cancelbuf) {
		void (*f)(void *) = self->cancelbuf->__f;
		void *x = self->cancelbuf->__x;
		self->cancelbuf = self->cancelbuf->__next;
		f(x);
	}

	__pthread_tsd_run_dtors();

#ifdef __wasilibc_unmodified_upstream
	__block_app_sigs(&set);
#endif

	/* This atomic potentially competes with a concurrent pthread_detach
	 * call; the loser is responsible for freeing thread resources. */
	int state = a_cas(&self->detach_state, DT_JOINABLE, DT_EXITING);

	if (state==DT_DETACHED && self->map_base) {
		/* Since __unmapself bypasses the normal munmap code path,
		 * explicitly wait for vmlock holders first. This must be
		 * done before any locks are taken, to avoid lock ordering
		 * issues that could lead to deadlock. */
#ifdef __wasilibc_unmodified_upstream
		__vm_wait();
#endif
	}

	/* Access to target the exiting thread with syscalls that use
	 * its kernel tid is controlled by killlock. For detached threads,
	 * any use past this point would have undefined behavior, but for
	 * joinable threads it's a valid usage that must be handled.
	 * Signals must be blocked since pthread_kill must be AS-safe. */
	LOCK(self->killlock);

	/* The thread list lock must be AS-safe, and thus depends on
	 * application signals being blocked above. */
	__tl_lock();

	/* If this is the only thread in the list, don't proceed with
	 * termination of the thread, but restore the previous lock and
	 * signal state to prepare for exit to call atexit handlers. */
	if (self->next == self) {
		__tl_unlock();
		UNLOCK(self->killlock);
		self->detach_state = state;
#ifdef __wasilibc_unmodified_upstream
		__restore_sigs(&set);
#endif
		exit(0);
	}

	/* At this point we are committed to thread termination. */

#ifdef __wasilibc_unmodified_upstream
	/* Process robust list in userspace to handle non-pshared mutexes
	 * and the detached thread case where the robust list head will
	 * be invalid when the kernel would process it. */
	__vm_lock();
#endif
	volatile void *volatile *rp;
	while ((rp=self->robust_list.head) && rp != &self->robust_list.head) {
		pthread_mutex_t *m = (void *)((char *)rp
			- offsetof(pthread_mutex_t, _m_next));
		int waiters = m->_m_waiters;
		int priv = (m->_m_type & 128) ^ 128;
		self->robust_list.pending = rp;
		self->robust_list.head = *rp;
		int cont = a_swap(&m->_m_lock, 0x40000000);
		self->robust_list.pending = 0;
		if (cont < 0 || waiters)
			__wake(&m->_m_lock, 1, priv);
	}
#ifdef __wasilibc_unmodified_upstream
	__vm_unlock();
#endif

	__do_orphaned_stdio_locks();
#ifdef __wasilibc_unmodified_upstream
	__dl_thread_cleanup();
#endif

	/* Last, unlink thread from the list. This change will not be visible
	 * until the lock is released, which only happens after SYS_exit
	 * has been called, via the exit futex address pointing at the lock.
	 * This needs to happen after any possible calls to LOCK() that might
	 * skip locking if process appears single-threaded. */
	if (!--libc.threads_minus_1) libc.need_locks = -1;
	self->next->prev = self->prev;
	self->prev->next = self->next;
	self->prev = self->next = self;

#ifndef __wasilibc_unmodified_upstream
	/* On Linux, the thread is created with CLONE_CHILD_CLEARTID,
	 * and this lock will unlock by kernel when this thread terminates.
	 * So we should unlock it here in WebAssembly.
	 * See also set_tid_address(2) */
	__tl_unlock();
#endif

#ifdef __wasilibc_unmodified_upstream
	if (state==DT_DETACHED && self->map_base) {
		/* Detached threads must block even implementation-internal
		 * signals, since they will not have a stack in their last
		 * moments of existence. */
		__block_all_sigs(&set);

		/* Robust list will no longer be valid, and was already
		 * processed above, so unregister it with the kernel. */
		if (self->robust_list.off)
			__syscall(SYS_set_robust_list, 0, 3*sizeof(long));

		/* The following call unmaps the thread's stack mapping
		 * and then exits without touching the stack. */
		__unmapself(self->map_base, self->map_size);
	}
#else
	if (state==DT_DETACHED && self->map_base) {
		// __syscall(SYS_exit) would unlock the thread, list
		// do it manually here
		__tl_unlock();
		free(self->map_base);
		for (;;) __wasi_thread_exit(0);
	}
#endif

	/* Wake any joiner. */
	a_store(&self->detach_state, DT_EXITED);
	__wake(&self->detach_state, 1, 1);

	/* After the kernel thread exits, its tid may be reused. Clear it
	 * to prevent inadvertent use and inform functions that would use
	 * it that it's no longer available. */
	self->tid = 0;
	UNLOCK(self->killlock);

#ifdef __wasilibc_unmodified_upstream
	for (;;) __syscall(SYS_exit, 0);
#else
	// __syscall(SYS_exit) would unlock the thread, list
	// do it manually here
	__tl_unlock();
	for (;;) __wasi_thread_exit(0);
#endif
}

void __do_cleanup_push(struct __ptcb *cb)
{
	struct pthread *self = __pthread_self();
	cb->__next = self->cancelbuf;
	self->cancelbuf = cb;
}

void __do_cleanup_pop(struct __ptcb *cb)
{
	__pthread_self()->cancelbuf = cb->__next;
}

struct start_args {
#ifdef __wasilibc_unmodified_upstream
	void *(*start_func)(void *);
	void *start_arg;
	volatile int control;
	unsigned long sig_mask[_NSIG/8/sizeof(long)];
#else
	/*
	 * Note: the offset of the "stack" and "tls_base" members
	 * in this structure is hardcoded in wasi_thread_start.
	 */
	void *stack;
	void *tls_base;
	void *(*start_func)(void *);
	void *start_arg;

	/*
	 * When generating PIC code, clang generates an "internal GOT" (Global Offset
	 * Table), which includes TLS symbols as well. Each TLS symbol gets its own global,
	 * which is filled in according to the value of __tls_base, within the
	 * __wasm_init_tls function that clang also generates.
	 *
	 * Previously, pthread_create would use __copy_tls, which would assume that
	 * the only global that's modified by __wasm_init_tls is __tls_base, so it would
	 * back __tls_base up and restore it afterwards. This can't happen with the
	 * internal GOT since we can't know what other globals there are and so we can't
	 * restore them. Even if we did, the new thread would need to get its globals
	 * initialized anyway, and that's also impossible without a call to __wasm_init_tls.
	 * The only option is thus to defer initialization of TLS to the new thread itself.
	 * This happens in the wasi_thread_start function.
	 *
	 * Now, this creates a second problem: The pthread struct for the new thread is
	 * initialized by the parent thread in pthread_create. Even if we were to put enough
	 * data into start_args for the new thread to initialize its own pthread struct,
	 * as long as the struct lives in the TLS area, __wasm_init_tls will write to it
	 * with non-atomic writes. This breaks pthread_join, which expects the detach_state
	 * member of pthread to only ever be accessed atomically.
	 *
	 * As such, the only real way to solve this is to make sure the pthread struct for
	 * a thread does not live in the TLS section. Instead, we keep a pointer to a
	 * separately-allocated pthread struct (the corresponding code is in pthread_arch.h).
	 * The pointer is then passed to the new thread, so it can be put in place after
	 * the call to __wasm_init_tls.
	 */
	void *pthread_self_ptr;

	void *reserved[9];	// this is reserved for future WASI changes, makes the struct 64 bytes or 128 bytes on 64bit
	size_t stack_size;
	size_t guard_size;
#endif
};

#ifdef __wasilibc_unmodified_upstream
static int start(void *p)
{
	struct start_args *args = p;
	int state = args->control;
	if (state) {
		if (a_cas(&args->control, 1, 2)==1)
			__wait(&args->control, 0, 2, 1);
		if (args->control) {
#ifdef __wasilibc_unmodified_upstream
			__syscall(SYS_set_tid_address, &args->control);
			for (;;) __syscall(SYS_exit, 0);
#endif
		}
	}
#ifdef __wasilibc_unmodified_upstream
	__syscall(SYS_rt_sigprocmask, SIG_SETMASK, &args->sig_mask, 0, _NSIG/8);
#endif
	__pthread_exit(args->start_func(args->start_arg));
	return 0;
}

static int start_c11(void *p)
{
	struct start_args *args = p;
	int (*start)(void*) = (int(*)(void*)) args->start_func;
	__pthread_exit((void *)(uintptr_t)start(args->start_arg));
	return 0;
}
#else

/*
 * We want to ensure wasi_thread_start is linked whenever
 * pthread_create is used. The following reference is to ensure that.
 * Otherwise, the linker doesn't notice the dependency because
 * wasi_thread_start is used indirectly via a wasm export.
 */
void wasi_thread_start(int tid, void *p);
hidden void *__dummy_reference = wasi_thread_start;

extern void __set_tp(uintptr_t p);

hidden void __wasi_thread_start_C(int tid, void *p)
{
	struct start_args *args = p;

	pthread_t self = args->pthread_self_ptr;
	__set_tp((uintptr_t)self);

	// Set the thread ID (TID) on the pthread structure. The TID is stored
	// atomically since it is also stored by the parent thread; this way,
	// whichever thread (parent or child) reaches this point first can proceed
	// without waiting.
	atomic_store((atomic_int *) &(self->tid), tid);
	// Execute the user's start function.
	__pthread_exit(args->start_func(args->start_arg));
}
#endif

#ifdef __wasilibc_unmodified_upstream
#define ROUND(x) (((x)+PAGE_SIZE-1)&-PAGE_SIZE)
#else
/*
 * As we allocate stack with malloc() instead of mmap/mprotect,
 * there is no point to round it up to PAGE_SIZE.
 * Instead, round up to a sane alignment.
 * Note: PAGE_SIZE is rather big on WASM. (65536)
 */
// As wasmer uses mmap on linux, rounding to a linux page size
// makes sense
#define ROUND(x) (((x)+LINUX_PAGE_SIZE-1)&-LINUX_PAGE_SIZE)
#endif

/* pthread_key_create.c overrides this */
static volatile size_t dummy = 0;
weak_alias(dummy, __pthread_tsd_size);
static void *dummy_tsd[1] = { 0 };
weak_alias(dummy_tsd, __pthread_tsd_main);

static FILE *volatile dummy_file = 0;
weak_alias(dummy_file, __stdin_used);
weak_alias(dummy_file, __stdout_used);
weak_alias(dummy_file, __stderr_used);

static void init_file_lock(FILE *f)
{
	if (f && f->lock<0) f->lock = 0;
}

int __pthread_create(pthread_t *restrict res, const pthread_attr_t *restrict attrp, void *(*entry)(void *), void *restrict arg)
{
	int ret, c11 = (attrp == __ATTRP_C11_THREAD);
	size_t size, guard;
	struct pthread *self, *new;
	unsigned char *map = 0, *stack = 0, *tsd = 0, *stack_limit;
#ifdef __wasilibc_unmodified_upstream
	unsigned flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND
		| CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS
		| CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID | CLONE_DETACHED;
#endif
	pthread_attr_t attr = { 0 };
	sigset_t set;
#ifndef __wasilibc_unmodified_upstream
	size_t tls_size = __builtin_wasm_tls_size();
	size_t tls_align = __builtin_wasm_tls_align();
	void* tls_base = __builtin_wasm_tls_base();
	size_t pthread_size = sizeof(struct pthread);
	size_t pthread_align = _Alignof(struct pthread);
	void* new_tls_base;
	void* new_pthread;
	size_t tls_offset;
	tls_size += tls_align;
	pthread_size += pthread_align;
#endif

#ifdef __wasilibc_unmodified_upstream
	if (!libc.can_do_threads) return ENOSYS;
#endif
	self = __pthread_self();
	if (!libc.threaded) {
		for (FILE *f=*__ofl_lock(); f; f=f->next)
			init_file_lock(f);
		__ofl_unlock();
		init_file_lock(__stdin_used);
		init_file_lock(__stdout_used);
		init_file_lock(__stderr_used);
#ifdef __wasilibc_unmodified_upstream
		__syscall(SYS_rt_sigprocmask, SIG_UNBLOCK, SIGPT_SET, 0, _NSIG/8);
#endif
		self->tsd = (void **)__pthread_tsd_main;
#ifdef __wasilibc_unmodified_upstream
		__membarrier_init();
#endif
		libc.threaded = 1;
	}
	if (attrp && !c11) attr = *attrp;

	__acquire_ptc();
	if (!attrp || c11) {
		attr._a_stacksize = __default_stacksize;
		attr._a_guardsize = __default_guardsize;
	}

	if (attr._a_stackaddr) {
#ifdef __wasilibc_unmodified_upstream
		size_t need = libc.tls_size + __pthread_tsd_size;
#else
		size_t need = tls_size + pthread_size + __pthread_tsd_size;
#endif
		size = attr._a_stacksize;
		stack = (void *)(attr._a_stackaddr & -16);
		stack_limit = (void *)(attr._a_stackaddr - size);
		/* Use application-provided stack for TLS only when
		 * it does not take more than ~12% or 2k of the
		 * application's stack space. */
		if (need < size/8 && need < 2048) {
			tsd = stack - __pthread_tsd_size;
#ifdef __wasilibc_unmodified_upstream
			stack = tsd - libc.tls_size;
#else
			new_pthread = tsd - pthread_size;
			stack = new_pthread - tls_size;
#endif
			memset(stack, 0, need);
		} else {
			size = ROUND(need);
		}
		guard = 0;
	} else {
		guard = ROUND(attr._a_guardsize);
		size = guard + ROUND(attr._a_stacksize
#ifdef __wasilibc_unmodified_upstream
			+ libc.tls_size +  __pthread_tsd_size);
#else
			+ tls_size + pthread_size +  __pthread_tsd_size);
#endif
	}

	if (!tsd) {
#ifdef __wasilibc_unmodified_upstream
		if (guard) {
			map = __mmap(0, size, PROT_NONE, MAP_PRIVATE|MAP_ANON, -1, 0);
			if (map == MAP_FAILED) goto fail;
			if (__mprotect(map+guard, size-guard, PROT_READ|PROT_WRITE)
			    && errno != ENOSYS) {
				__munmap(map, size);
				goto fail;
			}
		} else {
			map = __mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
			if (map == MAP_FAILED) goto fail;
		}
#else
		if (guard) {
			map = malloc(guard + size + guard);
			if (!map) goto fail;
		} else {
			map = malloc(size);
			if (!map) goto fail;
		}
#endif
		tsd = map + size - __pthread_tsd_size;
		new_pthread = tsd - pthread_size;
		if (!stack) {
#ifdef __wasilibc_unmodified_upstream
			stack = tsd - libc.tls_size;
#else
			stack = new_pthread - tls_size;
#endif
			stack_limit = map + guard;
		}
	}

#ifdef __wasilibc_unmodified_upstream
	new = __copy_tls(tsd - libc.tls_size);
#else
	// See comments on start_args.pthread_self_ptr for how TLS is handled in WASIX threads.
	new_tls_base = new_pthread - tls_size;
	new_tls_base += tls_align;
	new_tls_base -= (uintptr_t)new_tls_base & (tls_align - 1);

	new = new_pthread;
	new += pthread_align;
	new -= (uintptr_t)new & (pthread_align - 1);
	
#endif
	new->map_base = map;
	new->map_size = size;
	new->stack = stack;
	new->stack_size = stack - stack_limit;
	new->guard_size = guard;
	new->self = new;
	new->tsd = (void *)tsd;
	new->locale = &libc.global_locale;
	if (attr._a_detach) {
		new->detach_state = DT_DETACHED;
	} else {
		new->detach_state = DT_JOINABLE;
	}
	new->robust_list.head = &new->robust_list.head;
	new->canary = self->canary;
	new->sysinfo = self->sysinfo;

	/* Setup argument structure for the new thread on its stack.
	 * It's safe to access from the caller only until the thread
	 * list is unlocked. */
#ifdef __wasilibc_unmodified_upstream
	stack -= (uintptr_t)stack % sizeof(uintptr_t);
	stack -= sizeof(struct start_args);
	struct start_args *args = (void *)stack;
	args->start_func = entry;
	args->start_arg = arg;
	args->control = attr._a_sched ? 1 : 0;

	/* Application signals (but not the synccall signal) must be
	 * blocked before the thread list lock can be taken, to ensure
	 * that the lock is AS-safe. */
	__block_app_sigs(&set);

	/* Ensure SIGCANCEL is unblocked in new thread. This requires
	 * working with a copy of the set so we can restore the
	 * original mask in the calling thread. */
	memcpy(&args->sig_mask, &set, sizeof args->sig_mask);
	args->sig_mask[(SIGCANCEL- 1) /8/sizeof(long)] &=
		~(1UL<<((SIGCANCEL-1)%(8*sizeof(long))));
#else
	/* Align the stack to struct start_args */
	stack -= sizeof(struct start_args);
	stack -= (uintptr_t)stack % alignof(struct start_args);
	struct start_args *args = (void *)stack;
	
	/* Align the stack to 16 and store it */
	new->stack = (void *)((uintptr_t) stack & -16);
	/* Correct the stack size */
	new->stack_size = stack - stack_limit;

	args->stack = new->stack; /* just for convenience of asm trampoline */
	args->start_func = entry;
	args->start_arg = arg;
	args->tls_base = (void*)new_tls_base;
	args->pthread_self_ptr = (void *)new;
	args->stack_size = new->stack_size;		// used by WASIX for stack migration and asyncify support
	args->guard_size = new->guard_size;		// used by WASIX for stack overflow guards using mmap
#endif

	__tl_lock();
	if (!libc.threads_minus_1++) libc.need_locks = 1;
#ifdef __wasilibc_unmodified_upstream
	ret = __clone((c11 ? start_c11 : start), stack, flags, args, &new->tid, TP_ADJ(new), &__thread_list_lock);
#else
	/* Instead of `__clone`, WASI uses a host API to instantiate a new version
	 * of the current module and start executing the entry function. The
	 * wasi-threads specification requires the module to export a
	 * `wasi_thread_start` function, which is invoked with `args`. */
	ret = __wasi_thread_spawn((void *) args);
#endif

#ifdef __wasilibc_unmodified_upstream
	/* All clone failures translate to EAGAIN. If explicit scheduling
	 * was requested, attempt it before unlocking the thread list so
	 * that the failed thread is never exposed and so that we can
	 * clean up all transient resource usage before returning. */
	if (ret < 0) {
		ret = -EAGAIN;
	} else if (attr._a_sched) {
		ret = __syscall(SYS_sched_setscheduler,
			new->tid, attr._a_policy, &attr._a_prio);
		if (a_swap(&args->control, ret ? 3 : 0)==2)
			__wake(&args->control, 1, 1);
		if (ret)
			__wait(&args->control, 0, 3, 0);
	}
#else
	/* `wasi_thread_spawn` will either return a host-provided thread ID (TID)
	 * (`>= 0`) or an error code (`< 0`). As in the unmodified version, all
	 * spawn failures translate to EAGAIN; unlike the modified version, there is
	 * no need to "start up" the child thread--the host does this. If the spawn
	 * did succeed, then we store the TID atomically, since this parent thread
	 * is racing with the child thread to set this field; this way, whichever
	 * thread reaches this point first can continue without waiting. */
	if (ret < 0) {
		ret = -EAGAIN;
	} else {
		atomic_store((atomic_int *) &(new->tid), ret);
	}
#endif

	if (ret >= 0) {
		new->next = self->next;
		new->prev = self;
		new->next->prev = new;
		new->prev->next = new;
	} else {
		if (!--libc.threads_minus_1) libc.need_locks = 0;
	}
	__tl_unlock();
#ifdef __wasilibc_unmodified_upstream
	__restore_sigs(&set);
#endif
	__release_ptc();

	if (ret < 0) {
#ifdef __wasilibc_unmodified_upstream
		if (map) __munmap(map, size);
#else
		free(map);
#endif
		return -ret;
	}

	*res = new;
	return 0;
fail:
	__release_ptc();
	return EAGAIN;
}

weak_alias(__pthread_exit, pthread_exit);
weak_alias(__pthread_create, pthread_create);
