#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

extern char **__wasilibc_environ;

#ifdef __wasilibc_unmodified_upstream
int __execvpe(const char *file, char *const argv[], char *const envp[])
{
	const char *p, *z, *path = getenv("PATH");
	size_t l, k;
	int seen_eacces = 0;

	errno = ENOENT;
	if (!*file)
		return -1;

	if (strchr(file, '/'))
		return execve(file, argv, envp);

	if (!path)
		path = "/usr/local/bin:/bin:/usr/bin";
	k = strnlen(file, NAME_MAX + 1);
	if (k > NAME_MAX)
	{
		errno = ENAMETOOLONG;
		return -1;
	}
	l = strnlen(path, PATH_MAX - 1) + 1;

	for (p = path;; p = z)
	{
		char b[l + k + 1];
		z = __strchrnul(p, ':');
		if (z - p >= l)
		{
			if (!*z++)
				break;
			continue;
		}
		memcpy(b, p, z - p);
		b[z - p] = '/';
		memcpy(b + (z - p) + (z > p), file, k + 1);
		execve(b, argv, envp);
		switch (errno)
		{
		case EACCES:
			seen_eacces = 1;
		case ENOENT:
		case ENOTDIR:
			break;
		default:
			return -1;
		}
		if (!*z++)
			break;
	}
	if (seen_eacces)
		errno = EACCES;
	return -1;
}
#else
size_t __wasilibc_count_strings(char *const strings[])
{
	size_t count = 0;
	if (!strings)
		return 0;
	while (strings[count])
		count++;
	return count;
}

int __execvpe(const char *path, char *const argv[], char *const envp[], uint8_t use_path)
{
	size_t argc = __wasilibc_count_strings(argv);
	size_t envc = __wasilibc_count_strings(envp);
	const char *path_env = "";

	if (use_path) {
		path_env = getenv("PATH");
		if (!path_env)
			path_env = "/usr/local/bin:/bin:/usr/bin";
	}

	int e = __wasi_proc_exec4(
		path,
		(const uint8_t **)argv,
		argc,
		envp ? (const uint8_t **)envp : NULL,
		envc,
		use_path ? __WASI_BOOL_TRUE : __WASI_BOOL_FALSE,
		path_env);
#ifdef __wasm_exception_handling__
	extern _Noreturn void __vfork_restore();
	if (e == 0) {
		__vfork_restore();
	}
#endif

	// A return from proc_exec automatically means it failed
	errno = e;
	return -1;
}
#endif

int __execvp(const char *file, char *const argv[])
{
#ifndef __wasilibc_unmodified_upstream
	__wasilibc_ensure_environ();
#endif
	return __execvpe(file, argv, __wasilibc_environ, 1);
}

weak_alias(__execvp, execvp);
weak_alias(__execvp, execvpe);
