// Ported from NetBSD libc setjmp tests and Android bionic setjmp smoke tests.
// Sources:
// - freebsd-src/contrib/netbsd-tests/lib/libc/setjmp/t_setjmp.c
// - freebsd-src/contrib/netbsd-tests/lib/libc/setjmp/t_threadjmp.c
// - bionic/tests/setjmp_test.cpp (subset)

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#if !defined(__wasilibc_unmodified_upstream) && !defined(__wasm_exception_handling__)
extern int sigsetjmp(sigjmp_buf, int);
extern void siglongjmp(sigjmp_buf, int);
#endif

static void set_sigabrt_blocked(int blocked) {
    sigset_t ss;
    assert(sigemptyset(&ss) == 0);
    if (blocked) {
        assert(sigaddset(&ss, SIGABRT) == 0);
    }
    assert(sigprocmask(SIG_SETMASK, &ss, NULL) == 0);
}

static int is_sigabrt_blocked(void) {
    sigset_t ss;
    assert(sigprocmask(SIG_SETMASK, NULL, &ss) == 0);
    return sigismember(&ss, SIGABRT) == 1;
}

static void test_setjmp_longjmp_value(void) {
    jmp_buf jb;
    int value = setjmp(jb);
    if (value == 0) {
        longjmp(jb, 123);
        assert(0 && "longjmp should not return");
    }
    assert(value == 123);
}

static void test_longjmp_zero_returns_one(void) {
    jmp_buf jb;
    int value = setjmp(jb);
    if (value == 0) {
        longjmp(jb, 0);
        assert(0 && "longjmp should not return");
    }
    assert(value == 1);
}

static void test_sigsetjmp_siglongjmp_value(void) {
    sigjmp_buf sjb;
    int value = sigsetjmp(sjb, 0);
    if (value == 0) {
        siglongjmp(sjb, 789);
        assert(0 && "siglongjmp should not return");
    }
    assert(value == 789);

    value = sigsetjmp(sjb, 1);
    if (value == 0) {
        siglongjmp(sjb, 0xabc);
        assert(0 && "siglongjmp should not return");
    }
    assert(value == 0xabc);
}

static void test_signal_mask_behavior(void) {
    jmp_buf jb;
    sigjmp_buf sjb;

    // sigsetjmp(0)/siglongjmp: no mask save/restore.
    set_sigabrt_blocked(1);
    if (sigsetjmp(sjb, 0) == 0) {
        set_sigabrt_blocked(0);
        siglongjmp(sjb, 1);
        assert(0 && "siglongjmp should not return");
    }
    assert(is_sigabrt_blocked() == 0);
    set_sigabrt_blocked(0);

    // sigsetjmp(1)/siglongjmp: mask save/restore.
    set_sigabrt_blocked(1);
    if (sigsetjmp(sjb, 1) == 0) {
        set_sigabrt_blocked(0);
        siglongjmp(sjb, 1);
        assert(0 && "siglongjmp should not return");
    }
    assert(is_sigabrt_blocked() == 1);
    set_sigabrt_blocked(0);
}

static void test_thread_self_consistency(void) {
    pthread_t self = pthread_self();
    jmp_buf jb;
    sigjmp_buf sjb;

    if (setjmp(jb) == 0) {
        longjmp(jb, 1);
        assert(0 && "longjmp should not return");
    }
    assert(pthread_equal(self, pthread_self()));

    if (sigsetjmp(sjb, 0) == 0) {
        siglongjmp(sjb, 1);
        assert(0 && "siglongjmp should not return");
    }
    assert(pthread_equal(self, pthread_self()));

    if (sigsetjmp(sjb, 1) == 0) {
        siglongjmp(sjb, 1);
        assert(0 && "siglongjmp should not return");
    }
    assert(pthread_equal(self, pthread_self()));
}

int main(void) {
    test_setjmp_longjmp_value();
    test_longjmp_zero_returns_one();
    test_sigsetjmp_siglongjmp_value();
    test_signal_mask_behavior();
    test_thread_self_consistency();
    return 0;
}
