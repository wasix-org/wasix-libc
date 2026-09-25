#include <sys/socket.h>
#include <__header_netinet_in.h>

#include <wasi/api.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

int getsockopt(int socket, int level, int option_name,
               void *restrict option_value, socklen_t *restrict option_len) {
  if (level == IPPROTO_TCP &&
      (option_name == TCP_KEEPIDLE || option_name == TCP_KEEPINTVL ||
       option_name == TCP_KEEPCNT)) {
    if (option_len == NULL || (*option_len != 0 && option_value == NULL)) {
      errno = EFAULT;
      return -1;
    }
    __wasi_sock_option_t option = option_name == TCP_KEEPIDLE
        ? __WASI_SOCK_OPTION_TCP_KEEP_IDLE
        : option_name == TCP_KEEPINTVL
        ? __WASI_SOCK_OPTION_TCP_KEEP_INTERVAL
        : __WASI_SOCK_OPTION_TCP_KEEP_COUNT;
    __wasi_filesize_t size;
    __wasi_errno_t error = __wasi_sock_get_opt_size(socket, option, &size);
    if (error != 0) {
      errno = error;
      return -1;
    }
    if (size > INT_MAX) {
      errno = EOVERFLOW;
      return -1;
    }
    int value = size;
    if (*option_len > sizeof(value))
      *option_len = sizeof(value);
    if (*option_len != 0)
      memcpy(option_value, &value, *option_len);
    return 0;
  }
  if (level == IPPROTO_IPV6 && option_name == IPV6_V6ONLY) {
    level = SOL_SOCKET;
    option_name = SO_ONLYV6;
  }
  if (level == IPPROTO_TCP && option_name == TCP_NODELAY) {
    level = SOL_SOCKET;
    option_name = SO_NODELAY;
  }

  if (level != SOL_SOCKET) {
    errno = ENOPROTOOPT;
    return -1;
  }

  switch (option_name) {
    case SO_ACCEPTCONN:
    case SO_BROADCAST:
    case SO_DONTROUTE:
    case SO_NODELAY:
    case SO_OOBINLINE:
    case SO_ONLYV6:
    case SO_REUSEPORT:
    case SO_REUSEADDR:
    case SO_MCASTLOOPV4:
    case SO_MCASTLOOPV6:
    case SO_KEEPALIVE: {
      __wasi_bool_t on = 0;
      __wasi_errno_t error = __wasi_sock_get_opt_flag(socket, option_name, &on);
      if (error != 0) {
        errno = error;
        return -1;
      }
      
      int value = on == __WASI_BOOL_TRUE ? 1 : 0;
      memcpy(option_value, &value, *option_len < sizeof(int) ? *option_len : sizeof(int));
      *option_len = sizeof(int);
      return 0;
    }
    case SO_ERROR: {
      __wasi_filesize_t fs;
      __wasi_errno_t error = __wasi_sock_get_opt_size(socket, option_name, &fs);
      if (error != 0) {
        errno = error;
        return -1;
      }

      int value = fs;
      memcpy(option_value, &value, *option_len < sizeof(int) ? *option_len : sizeof(int));
      *option_len = sizeof(int);
      return 0;
    }
    case SO_LINGER: {
      __wasi_option_timestamp_t tm;
      __wasi_errno_t error = __wasi_sock_get_opt_time(socket, option_name, &tm);
      if (error != 0) {
        errno = error;
        return -1;
      }

      struct linger linger;
      linger.l_onoff = tm.tag == __WASI_OPTION_SOME ? 1 : 0;
      linger.l_linger = tm.u.some / 1000000000ULL;
      memcpy(option_value, &linger, *option_len < sizeof(struct linger) ? *option_len : sizeof(struct linger));
      *option_len = sizeof(struct linger);
      return 0;
    }
    case SO_RCVTIMEO:
    case SO_SNDTIMEO:
    case SO_CONNTIMEO:
    case SO_ACCPTIMEO: {
      __wasi_option_timestamp_t tm;
      __wasi_errno_t error = __wasi_sock_get_opt_time(socket, option_name, &tm);
      if (error != 0) {
        errno = error;
        return -1;
      }

      struct timeval tv;
      memset(&tv, 0, sizeof(struct timeval));
      if (tm.tag == __WASI_OPTION_SOME) {
        tv.tv_sec = tm.u.some / 1000000000ULL;
        tv.tv_usec = (tm.u.some % 1000000000ULL) / 1000ULL;
      }
      memcpy(option_value, &tv, *option_len < sizeof(struct timeval) ? *option_len : sizeof(struct timeval));
      *option_len = sizeof(struct timeval);
      return 0;
    }
    case SO_RCVBUF:
    case SO_SNDBUF:
    case SO_TTL:
    case SO_MCASTTTLV4: {
      __wasi_filesize_t fs;
      __wasi_errno_t error = __wasi_sock_get_opt_size(socket, option_name, &fs);
      if (error != 0) {
        errno = error;
        return -1;
      }
      socklen_t len = fs;
      memcpy(option_value, &len, *option_len < sizeof(socklen_t ) ? *option_len : sizeof(socklen_t ));
      *option_len = sizeof(socklen_t );
      return 0;
    }
    case SO_PROTOCOL: {
      __wasi_filesize_t fs;
      __wasi_errno_t error = __wasi_sock_get_opt_size(socket, option_name, &fs);
      if (error != 0) {
        errno = error;
        return -1;
      }
      int value = 0;
      memcpy(option_value, &value, *option_len < sizeof(int) ? *option_len : sizeof(int));
      *option_len = sizeof(int);
      return 0;
    }
    case SO_TYPE: {
      __wasi_fdstat_t fsb;
      if (__wasi_fd_fdstat_get(socket, &fsb) != 0) {
        errno = EBADF;
        return -1;
      }
      if (fsb.fs_filetype != __WASI_FILETYPE_SOCKET_DGRAM &&
          fsb.fs_filetype != __WASI_FILETYPE_SOCKET_STREAM &&
          fsb.fs_filetype != __WASI_FILETYPE_SOCKET_SEQPACKET &&
          fsb.fs_filetype != __WASI_FILETYPE_SOCKET_RAW) {
        errno = ENOTSOCK;
        return -1;
      }
      int value = fsb.fs_filetype;
      switch(fsb.fs_filetype) {
        case __WASI_FILETYPE_SOCKET_DGRAM: value = SOCK_DGRAM; break;
        case __WASI_FILETYPE_SOCKET_STREAM: value = SOCK_STREAM; break;
        case __WASI_FILETYPE_SOCKET_SEQPACKET: value = SOCK_SEQPACKET; break;
        case __WASI_FILETYPE_SOCKET_RAW: value = SOCK_RAW; break;
      }
      memcpy(option_value, &value, *option_len < sizeof(int) ? *option_len : sizeof(int));
      *option_len = sizeof(int);
      return 0;
    }
  }

  errno = ENOPROTOOPT;
  return -1;
}
