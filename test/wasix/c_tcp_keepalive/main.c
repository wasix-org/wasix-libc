#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wasi/api.h>

_Static_assert(__WASI_SOCK_OPTION_TCP_KEEP_IDLE == 27, "keep idle ABI");
_Static_assert(__WASI_SOCK_OPTION_TCP_KEEP_INTERVAL == 28, "keep interval ABI");
_Static_assert(__WASI_SOCK_OPTION_TCP_KEEP_COUNT == 29, "keep count ABI");

static const int options[] = {TCP_KEEPIDLE, TCP_KEEPINTVL, TCP_KEEPCNT};
static const int values[] = {31, 7, 4};

static void check_options(int fd) {
  for (unsigned i = 0; i < sizeof(options) / sizeof(options[0]); i++) {
    int value = 0;
    socklen_t len = sizeof(value);
    assert(getsockopt(fd, IPPROTO_TCP, options[i], &value, &len) == 0);
    assert(len == sizeof(value));
    assert(value == values[i]);
  }
}

static void check_validation(int fd) {
  for (unsigned i = 0; i < sizeof(options) / sizeof(options[0]); i++) {
    int value = 0;
    errno = 0;
    assert(setsockopt(fd, IPPROTO_TCP, options[i], &value, sizeof(value)) == -1);
    assert(errno == EINVAL);
    value = -1;
    errno = 0;
    assert(setsockopt(fd, IPPROTO_TCP, options[i], &value, sizeof(value)) == -1);
    assert(errno == EINVAL);
    value = 1;
    errno = 0;
    assert(setsockopt(fd, IPPROTO_TCP, options[i], &value, sizeof(value) - 1) == -1);
    assert(errno == EINVAL);
    errno = 0;
    assert(setsockopt(fd, IPPROTO_TCP, options[i], NULL, sizeof(value)) == -1);
    assert(errno == EFAULT);
  }
  check_options(fd);
}

static void check_output_lengths(int fd) {
  unsigned char output[sizeof(int) + 1];
  memset(output, 0xa5, sizeof(output));
  socklen_t len = 1;
  assert(getsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, output, &len) == 0);
  assert(len == 1);
  assert(output[0] == ((const unsigned char *)&values[0])[0]);
  assert(output[1] == 0xa5);

  len = sizeof(output);
  assert(getsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, output, &len) == 0);
  assert(len == sizeof(int));
  assert(memcmp(output, &values[0], sizeof(int)) == 0);
  assert(output[sizeof(int)] == 0xa5);

  len = 0;
  assert(getsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, NULL, &len) == 0);
  assert(len == 0);
  len = sizeof(int);
  errno = 0;
  assert(getsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, NULL, &len) == -1);
  assert(errno == EFAULT);
  errno = 0;
  assert(getsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, output, NULL) == -1);
  assert(errno == EFAULT);
}

int main(void) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  assert(fd >= 0);
  int enabled = 1;
  assert(setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enabled, sizeof(enabled)) == 0);
  for (unsigned i = 0; i < sizeof(options) / sizeof(options[0]); i++)
    assert(setsockopt(fd, IPPROTO_TCP, options[i], &values[i], sizeof(values[i])) == 0);
  check_options(fd);
  check_validation(fd);
  check_output_lengths(fd);

  int server = socket(AF_INET, SOCK_STREAM, 0);
  assert(server >= 0);
  struct sockaddr_in address = {0};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  assert(bind(server, (struct sockaddr *)&address, sizeof(address)) == 0);
  assert(listen(server, 1) == 0);
  socklen_t len = sizeof(address);
  assert(getsockname(server, (struct sockaddr *)&address, &len) == 0);
  assert(connect(fd, (struct sockaddr *)&address, sizeof(address)) == 0);
  int peer = accept(server, NULL, NULL);
  assert(peer >= 0);
  check_options(fd);
  check_validation(fd);

  int updated = 9;
  assert(setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &updated, sizeof(updated)) == 0);
  int actual = 0;
  len = sizeof(actual);
  assert(getsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &actual, &len) == 0);
  assert(actual == updated);

  assert(close(peer) == 0);
  assert(close(server) == 0);
  assert(close(fd) == 0);
  errno = 0;
  assert(setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &updated, sizeof(updated)) == -1);
  assert(errno == EBADF);
  errno = 0;
  len = sizeof(actual);
  assert(getsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &actual, &len) == -1);
  assert(errno == EBADF);
  puts("TCP keepalive options passed");
  return 0;
}
