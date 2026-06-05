#include <sys/socket.h>
#include <__header_netinet_in.h>

#include <assert.h>
#include <wasi/api.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int socket(int domain, int ty, int protocol) {
  int fd;
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
  __wasi_errno_t error = __wasi_sock_open(domain, socktype, protocol, &fd);
  if (error != 0) {
    errno = error;
    return -1;
  }

  if (flags & SOCK_NONBLOCK) {
    __wasi_fdstat_t fds;
    error = __wasi_fd_fdstat_get(fd, &fds);
    if (error == 0) {
      error = __wasi_fd_fdstat_set_flags(
          fd, fds.fs_flags | __WASI_FDFLAGS_NONBLOCK);
    }
    if (error != 0) {
      close(fd);
      errno = error;
      return -1;
    }
  }

  if (flags & SOCK_CLOEXEC) {
    error = __wasi_fd_fdflags_set(fd, __WASI_FDFLAGSEXT_CLOEXEC);
    if (error != 0) {
      close(fd);
      errno = error;
      return -1;
    }
  }

  return fd;
}
