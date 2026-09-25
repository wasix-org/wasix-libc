#include <sys/socket.h>
#include <__header_netinet_in.h>

#include <assert.h>
#include <wasi/api.h>
#include <errno.h>
#include <string.h>

int setsockopt(int socket, int level, int option_name, const void *restrict option_value, socklen_t option_len) {
  if (level == IPPROTO_IPV6 && option_name == IPV6_V6ONLY) {
    level = SOL_SOCKET;
    option_name = SO_ONLYV6;
  }
  if (level == IPPROTO_TCP) {
    switch (option_name) {
      case TCP_NODELAY: option_name = SO_NODELAY; break;
      case TCP_KEEPIDLE: option_name = __WASI_SOCK_OPTION_TCP_KEEP_IDLE; break;
      case TCP_KEEPINTVL: option_name = __WASI_SOCK_OPTION_TCP_KEEP_INTERVAL; break;
      case TCP_KEEPCNT: option_name = __WASI_SOCK_OPTION_TCP_KEEP_COUNT; break;
      default: errno = ENOSYS; return -1;
    }
    level = SOL_SOCKET;
  }

  if(level!=SOL_SOCKET) {
    errno = ENOSYS;
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
      if (option_len < sizeof(int)) {
        errno = EINVAL;
        return -1;
      }
      on = (*(const int *)option_value) != 0 ? __WASI_BOOL_TRUE : __WASI_BOOL_FALSE;

      __wasi_errno_t error = __wasi_sock_set_opt_flag(socket, option_name, on);
      if (error != 0) {
        errno = error;
        return -1;
      }
      return 0;
    }
    case SO_LINGER: {
      __wasi_option_timestamp_t tm;
	  if (option_len >= sizeof(struct linger)) {
		struct linger *linger = (struct linger *)option_value;
		tm.tag = linger->l_onoff > 0 ? __WASI_OPTION_SOME : __WASI_OPTION_NONE;
		tm.u.some = linger->l_linger * 1000000000ULL;
	  } else {
		errno = EINVAL;
    	return -1;
	  }
      __wasi_errno_t error = __wasi_sock_set_opt_time(socket, option_name, &tm);
      if (error != 0) {
        errno = error;
        return -1;
      }
      return 0;
    }
    case SO_RCVTIMEO:
    case SO_SNDTIMEO:
    case SO_CONNTIMEO:
    case SO_ACCPTIMEO: {
	  __wasi_option_timestamp_t tm;
	  if (option_len >= sizeof(struct timeval)) {
		struct timeval *tv = (struct timeval *)option_value;
		tm.tag = tv->tv_sec > 0 || tv->tv_usec > 0 ? __WASI_OPTION_SOME : __WASI_OPTION_NONE;
		tm.u.some = (tv->tv_sec * 1000000000ULL) + (tv->tv_usec * 1000ULL);
	  } else {
		errno = EINVAL;
    	return -1;
	  }

	  __wasi_errno_t error = __wasi_sock_set_opt_time(socket, option_name, &tm);
      if (error != 0) {
        errno = error;
        return -1;
      }
      return 0;
    }
    case SO_RCVBUF:
    case SO_SNDBUF:
    case SO_TTL:
    case SO_MCASTTTLV4:
    case __WASI_SOCK_OPTION_TCP_KEEP_IDLE:
    case __WASI_SOCK_OPTION_TCP_KEEP_INTERVAL:
    case __WASI_SOCK_OPTION_TCP_KEEP_COUNT: {
      socklen_t value;
      if (option_len < sizeof(value)) {
        errno = EINVAL;
        return -1;
      }
      if (option_value == NULL) {
        errno = EFAULT;
        return -1;
      }
      memcpy(&value, option_value, sizeof(value));

      __wasi_errno_t error = __wasi_sock_set_opt_size(socket, option_name, value);
      if (error != 0) {
        errno = error;
        return -1;
      }
      return 0;
    }
  }

  errno = ENOPROTOOPT;
  return -1;
}
