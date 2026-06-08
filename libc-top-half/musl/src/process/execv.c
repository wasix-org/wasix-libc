#include <unistd.h>
#include <string.h>

#ifdef __wasilibc_unmodified_upstream
extern char **__environ;
#else
#include <stdlib.h>
#include <errno.h>
#include <wasi/api.h>
#endif

size_t __wasilibc_count_strings(char *const strings[]);

int execv(const char *path, char *const argv[])
{
#ifdef __wasilibc_unmodified_upstream
	return execve(path, argv, __environ);
#else
	size_t argc = __wasilibc_count_strings(argv);

	int e = __wasi_proc_exec4(
		path,
		(const uint8_t **)argv,
		argc,
		NULL,
		0,
		__WASI_BOOL_FALSE,
		"");
#ifdef __wasm_exception_handling__
	extern _Noreturn void __vfork_restore();
	if (e == 0) {
		__vfork_restore();
	}
#endif

	// A return from proc_exec automatically means it failed
	errno = e;
	return -1;
#endif
}
