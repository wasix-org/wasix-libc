#include <sys/socket.h>
#include <__header_netinet_in.h>

#include <assert.h>
#include <wasi/api.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

static __wasi_errno_t socketpair_set_flags(int fd, int flags) {
  __wasi_errno_t error;

  if (flags & SOCK_NONBLOCK) {
    __wasi_fdstat_t fds;
    error = __wasi_fd_fdstat_get(fd, &fds);
    if (error != 0)
      return error;
    error = __wasi_fd_fdstat_set_flags(
        fd, fds.fs_flags | __WASI_FDFLAGS_NONBLOCK);
    if (error != 0)
      return error;
  }

  if (flags & SOCK_CLOEXEC) {
    error = __wasi_fd_fdflags_set(fd, __WASI_FDFLAGSEXT_CLOEXEC);
    if (error != 0)
      return error;
  }

  return 0;
}

int socketpair(int domain, int ty, int protocol, int *restrict socket_vector) {
  int fd1, fd2;
  int flags = ty & (SOCK_NONBLOCK | SOCK_CLOEXEC);
  int socktype = ty & ~(SOCK_NONBLOCK | SOCK_CLOEXEC);

  if(!protocol) {
    switch (socktype)
    {
    case SOCK_STREAM:
      protocol = IPPROTO_TCP;
      break;
    case SOCK_DGRAM:
      protocol = IPPROTO_UDP;
      break;
    }
  }
  __wasi_errno_t error = __wasi_sock_pair(domain, socktype, protocol, &fd1, &fd2);
  if (error != 0) {
    errno = error;
    return -1;
  }

  error = socketpair_set_flags(fd1, flags);
  if (error == 0)
    error = socketpair_set_flags(fd2, flags);
  if (error != 0) {
    close(fd1);
    close(fd2);
    errno = error;
    return -1;
  }

  socket_vector[0] = fd1;
  socket_vector[1] = fd2;
  return 0;
}
