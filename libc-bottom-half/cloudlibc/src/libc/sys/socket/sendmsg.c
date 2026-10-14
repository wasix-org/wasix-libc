#include <errno.h>
#include <common/net.h>

#include <sys/socket.h>
#include <__struct_msghdr.h>

#include <assert.h>
#include <wasi/api.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

static size_t wasi_cmsg_align(size_t len) {
  return (len + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1);
}

static int convert_control_to_wasi(const struct msghdr* msg,
                                   uint8_t** out_control,
                                   size_t* out_control_len) {
  *out_control = NULL;
  *out_control_len = 0;

  if (msg->msg_control == NULL || msg->msg_controllen == 0)
    return 0;

  size_t len = 0;
  for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(msg);
       cmsg != NULL;
       cmsg = CMSG_NXTHDR((struct msghdr*)msg, cmsg)) {
    if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
      errno = ENOTSUP;
      return -1;
    }
    if (cmsg->cmsg_len < CMSG_LEN(0)) {
      errno = EINVAL;
      return -1;
    }
    size_t payload_len = cmsg->cmsg_len - CMSG_LEN(0);
    len += wasi_cmsg_align(sizeof(__wasi_sock_cmsg_t)) + wasi_cmsg_align(payload_len);
  }

  if (len == 0)
    return 0;

  uint8_t* control = malloc(len);
  if (control == NULL) {
    errno = ENOMEM;
    return -1;
  }

  uint8_t* out = control;
  for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(msg);
       cmsg != NULL;
       cmsg = CMSG_NXTHDR((struct msghdr*)msg, cmsg)) {
    size_t payload_len = cmsg->cmsg_len - CMSG_LEN(0);
    __wasi_sock_cmsg_t* wasi_cmsg = (__wasi_sock_cmsg_t*)out;
    wasi_cmsg->cmsg_len = sizeof(__wasi_sock_cmsg_t) + payload_len;
    wasi_cmsg->cmsg_level = __WASI_SOCK_CMSG_LEVEL_SOCKET;
    wasi_cmsg->cmsg_type = __WASI_SOCK_CMSG_TYPE_RIGHTS;
    memcpy(out + wasi_cmsg_align(sizeof(__wasi_sock_cmsg_t)),
           CMSG_DATA(cmsg),
           payload_len);
    out += wasi_cmsg_align(sizeof(__wasi_sock_cmsg_t)) + wasi_cmsg_align(payload_len);
  }

  *out_control = control;
  *out_control_len = len;
  return 0;
}

ssize_t sendmsg(int socket, const struct msghdr* msg, int flags) {
  if (msg->msg_iov == NULL) {
	errno = EINVAL;
	return -1;
  }

  __wasi_ciovec_t *si_data = (__wasi_ciovec_t *)msg->msg_iov;
  size_t si_data_len = msg->msg_iovlen;

  __wasi_size_t so_datalen;
  __wasi_siflags_t si_flags = 0;
  __wasi_errno_t error;

  if ((flags & MSG_DONTWAIT) != 0) { si_flags |= __WASI_SIFLAGS_SEND_DONT_WAIT; }

  uint8_t* control = NULL;
  size_t control_len = 0;
  if (convert_control_to_wasi(msg, &control, &control_len) != 0)
    return -1;

  if (msg->msg_name == NULL) {
    error = control_len == 0
      ? __wasi_sock_send(socket, si_data, si_data_len, si_flags, &so_datalen)
      : __wasi_sock_send_msg(socket, si_data, si_data_len, si_flags, NULL,
                             control, control_len, &so_datalen);
  } else {
    struct sockaddr *addr = (struct sockaddr *)msg->msg_name;
    socklen_t addrlen = msg->msg_namelen;
    __wasi_addr_port_t peer_addr;
    error = sockaddr_to_wasi(addr, addrlen, &peer_addr);
    if (error != 0) {
      free(control);
      errno = error;
      return -1;
    }
    error = control_len == 0
      ? __wasi_sock_send_to(socket, si_data, si_data_len, si_flags, &peer_addr, &so_datalen)
      : __wasi_sock_send_msg(socket, si_data, si_data_len, si_flags, &peer_addr,
                             control, control_len, &so_datalen);
  }
  free(control);

  if (error != 0) {
    errno = error;
    return -1;
  }
  return so_datalen;
}
