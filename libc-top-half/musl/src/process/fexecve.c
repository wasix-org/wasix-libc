#define _GNU_SOURCE
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifdef __wasilibc_unmodified_upstream
#include "syscall.h"
#endif

int fexecve(int fd, char *const argv[], char *const envp[])
{
#ifdef __wasilibc_unmodified_upstream
	int r = __syscall(SYS_execveat, fd, "", argv, envp, AT_EMPTY_PATH);
	if (r != -ENOSYS) return __syscall_ret(r);
	char buf[15 + 3*sizeof(int)];
	__procfdname(buf, fd);
	execve(buf, argv, envp);
	if (errno == ENOENT) errno = EBADF;
	return -1;
#else
	if (fd < 0) {
		errno = EBADF;
		return -1;
	}

	struct stat st;
	if (fstat(fd, &st) == 0 && S_ISDIR(st.st_mode)) {
		errno = EACCES;
		return -1;
	}

	/* Create a temporary copy of the executable, since WASIX exec
	 * operates by path and does not currently support exec-by-fd. */
	char template_path[] = "./.fexecve-XXXXXX";
	int tmpfd = mkstemp(template_path);
	if (tmpfd < 0) {
		return -1;
	}
	(void)fchmod(tmpfd, 0700);

	off_t saved = lseek(fd, 0, SEEK_CUR);
	(void)lseek(fd, 0, SEEK_SET);

	char buf[8192];
	for (;;) {
		ssize_t n = read(fd, buf, sizeof(buf));
		if (n == 0) break;
		if (n < 0) {
			int err = errno;
			close(tmpfd);
			unlink(template_path);
			if (saved != (off_t)-1) (void)lseek(fd, saved, SEEK_SET);
			errno = err;
			return -1;
		}
		ssize_t off = 0;
		while (off < n) {
			ssize_t w = write(tmpfd, buf + off, (size_t)(n - off));
			if (w < 0) {
				int err = errno;
				close(tmpfd);
				unlink(template_path);
				if (saved != (off_t)-1) (void)lseek(fd, saved, SEEK_SET);
				errno = err;
				return -1;
			}
			off += w;
		}
	}

	if (saved != (off_t)-1) {
		(void)lseek(fd, saved, SEEK_SET);
	}
	close(tmpfd);

	execve(template_path, argv, envp);
	int err = errno;
	unlink(template_path);
	errno = err;
	return -1;
#endif
}
