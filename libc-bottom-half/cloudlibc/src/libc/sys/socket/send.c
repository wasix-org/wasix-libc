// Copyright (c) 2015-2017 Nuxi, https://nuxi.nl/
//
// SPDX-License-Identifier: BSD-2-Clause

#include <sys/socket.h>

#include <assert.h>
#include <wasi/api.h>
#include <errno.h>

ssize_t send(int socket, const void *buffer, size_t length, int flags) {
  if (buffer == NULL) {
	  errno = EINVAL;
	  return -1;
  }

  // Prepare input parameters.
  __wasi_ciovec_t iov = {.buf = buffer, .buf_len = length};
  __wasi_ciovec_t *si_data = &iov;
  size_t si_data_len = 1;
  __wasi_siflags_t si_flags = 0;

  if ((flags & MSG_DONTWAIT) != 0) { si_flags |= __WASI_SIFLAGS_SEND_DONT_WAIT; }

  // Perform system call.
  __wasi_size_t so_datalen;
  __wasi_errno_t error = __wasi_sock_send(socket, si_data, si_data_len, si_flags, &so_datalen);
  if (error != 0) {
    errno = error;
    return -1;
  }
  return so_datalen;
}
