#ifndef	_SETJMP_H
#define	_SETJMP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <features.h>

#ifdef __wasilibc_unmodified_upstream
#include <bits/setjmp.h>

typedef struct __jmp_buf_tag {
	__jmp_buf __jb;
	unsigned long __fl;
	unsigned long __ss[128/sizeof(long)];
} jmp_buf[1];

#if defined(_POSIX_SOURCE) || defined(_POSIX_C_SOURCE) \
 || defined(_XOPEN_SOURCE) || defined(_GNU_SOURCE) \
 || defined(_BSD_SOURCE)
typedef jmp_buf sigjmp_buf;
int sigsetjmp (sigjmp_buf, int);
_Noreturn void siglongjmp (sigjmp_buf, int);
#endif

#if defined(_XOPEN_SOURCE) || defined(_GNU_SOURCE) \
 || defined(_BSD_SOURCE)
int _setjmp (jmp_buf);
_Noreturn void _longjmp (jmp_buf, int);
#endif
#elif defined(__wasm_exception_handling__)
    typedef int __jmp_buf[6];

	typedef struct __jmp_buf_tag
	{
		__jmp_buf __jb;
		unsigned long __fl;
		unsigned long __ss[128 / sizeof(long)];
	} jmp_buf[1];

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
#define __setjmp_attr __attribute__((__returns_twice__))
#else
#define __setjmp_attr
#endif

#if defined(_POSIX_SOURCE) || defined(_POSIX_C_SOURCE) || \
	defined(_XOPEN_SOURCE) || defined(_GNU_SOURCE) || defined(_BSD_SOURCE)
	typedef jmp_buf sigjmp_buf;
	int sigsetjmp(sigjmp_buf, int) __setjmp_attr;
	_Noreturn void siglongjmp(sigjmp_buf, int);
#endif

#if defined(_XOPEN_SOURCE) || defined(_GNU_SOURCE) || defined(_BSD_SOURCE)
	int _setjmp(jmp_buf) __setjmp_attr;
	_Noreturn void _longjmp(jmp_buf, int);
#endif

#undef __setjmp_attr
#else
#include <wasi/api.h>
typedef __wasi_stack_snapshot_t __jmp_buf_tag;
typedef __wasi_stack_snapshot_t jmp_buf[1];
typedef jmp_buf sigjmp_buf;
#endif

int setjmp (jmp_buf);
_Noreturn void longjmp (jmp_buf, int);

#define setjmp setjmp

#ifdef __cplusplus
}
#endif

#endif
