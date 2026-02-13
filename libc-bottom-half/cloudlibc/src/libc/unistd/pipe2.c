#include <wasi/api.h>
#include <wasi/libc.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

int pipe2(int fd[2], int flag)
{
	int fd1;
	int fd2;
    __wasi_errno_t error = __wasi_fd_pipe(&fd1, &fd2);
    if (error != 0) {
        errno = error;
        return -1;
    }

    // Set flags if requested
    if (flag & O_CLOEXEC) {
        if (fcntl(fd1, F_SETFD, FD_CLOEXEC) < 0 || fcntl(fd2, F_SETFD, FD_CLOEXEC) < 0) {
            int saved_errno = errno;
            close(fd1);
            close(fd2);
            errno = saved_errno;
            return -1;
        }
    }
    if (flag & O_NONBLOCK) {
        int flags1 = fcntl(fd1, F_GETFL);
        int flags2 = fcntl(fd2, F_GETFL);
        if (flags1 < 0 || flags2 < 0) {
            int saved_errno = errno;
            close(fd1);
            close(fd2);
            errno = saved_errno;
            return -1;
        }
        if (fcntl(fd1, F_SETFL, flags1 | O_NONBLOCK) < 0 ||
            fcntl(fd2, F_SETFL, flags2 | O_NONBLOCK) < 0) {
            int saved_errno = errno;
            close(fd1);
            close(fd2);
            errno = saved_errno;
            return -1;
        }
    }

    fd[0] = fd1;
	fd[1] = fd2;
    return 0;
}
