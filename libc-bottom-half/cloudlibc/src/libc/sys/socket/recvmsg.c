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

static int convert_control_from_wasi(struct msghdr* msg,
                                     const uint8_t* control,
                                     size_t control_len) {
  if (msg->msg_control == NULL || msg->msg_controllen == 0) {
    if (control_len != 0)
      msg->msg_flags |= MSG_CTRUNC;
    msg->msg_controllen = 0;
    return 0;
  }

  uint8_t* out = msg->msg_control;
  size_t out_capacity = msg->msg_controllen;
  size_t out_used = 0;
  size_t in_offset = 0;

  while (in_offset + sizeof(__wasi_sock_cmsg_t) <= control_len) {
    const __wasi_sock_cmsg_t* wasi_cmsg =
        (const __wasi_sock_cmsg_t*)(control + in_offset);
    if (wasi_cmsg->cmsg_len < sizeof(__wasi_sock_cmsg_t) ||
        in_offset + wasi_cmsg->cmsg_len > control_len) {
      msg->msg_flags |= MSG_CTRUNC;
      break;
    }

    size_t payload_len = wasi_cmsg->cmsg_len - sizeof(__wasi_sock_cmsg_t);
    size_t posix_space = CMSG_SPACE(payload_len);
    size_t posix_len = CMSG_LEN(payload_len);
    if (out_used + posix_space > out_capacity) {
      msg->msg_flags |= MSG_CTRUNC;
      break;
    }

    struct cmsghdr* cmsg = (struct cmsghdr*)(out + out_used);
    if (wasi_cmsg->cmsg_level == __WASI_SOCK_CMSG_LEVEL_SOCKET &&
        wasi_cmsg->cmsg_type == __WASI_SOCK_CMSG_TYPE_RIGHTS) {
      cmsg->cmsg_level = SOL_SOCKET;
      cmsg->cmsg_type = SCM_RIGHTS;
    } else {
      msg->msg_flags |= MSG_CTRUNC;
      break;
    }

    cmsg->cmsg_len = posix_len;
    memcpy(CMSG_DATA(cmsg),
           control + in_offset + wasi_cmsg_align(sizeof(__wasi_sock_cmsg_t)),
           payload_len);
    out_used += posix_space;
    in_offset += wasi_cmsg_align(sizeof(__wasi_sock_cmsg_t)) + wasi_cmsg_align(payload_len);
  }

  msg->msg_controllen = out_used;
  return 0;
}

ssize_t recvmsg(int socket, struct msghdr *restrict msg, int flags) {
  __wasi_iovec_t *ri_data = (__wasi_iovec_t *)msg->msg_iov;
  size_t ri_data_len = msg->msg_iovlen;
  __wasi_riflags_t ri_flags = 0;

  if ((flags & MSG_PEEK) != 0) { ri_flags |= __WASI_RIFLAGS_RECV_PEEK; }
  if ((flags & MSG_WAITALL) != 0) { ri_flags |= __WASI_RIFLAGS_RECV_WAITALL; }
  if ((flags & MSG_TRUNC) != 0) { ri_flags |= __WASI_RIFLAGS_RECV_DATA_TRUNCATED; }
  if ((flags & MSG_DONTWAIT) != 0) { ri_flags |= __WASI_RIFLAGS_RECV_DONT_WAIT; }

  __wasi_size_t ro_datalen;
  __wasi_roflags_t ro_flags;
  __wasi_size_t ro_control_len = 0;
  __wasi_errno_t error;

  uint8_t* control = NULL;
  size_t control_capacity = msg->msg_control == NULL ? 0 : msg->msg_controllen;
  if (control_capacity != 0) {
    control = malloc(control_capacity);
    if (control == NULL) {
      errno = ENOMEM;
      return -1;
    }
  }

  if (control_capacity != 0) {
    __wasi_addr_port_t peer_addr;
    __wasi_addr_port_t* peer_addr_ptr = msg->msg_name == NULL ? NULL : &peer_addr;
    error = __wasi_sock_recv_msg(socket,
									ri_data, ri_data_len, ri_flags,
									peer_addr_ptr,
									control, control_capacity,
									&ro_datalen,
									&ro_flags,
									&ro_control_len);
    if (error == 0 && msg->msg_name != NULL) {
      struct sockaddr *addr = (struct sockaddr *)msg->msg_name;
      socklen_t *addrlen = &msg->msg_namelen;
      error = wasi_to_sockaddr(&peer_addr, addr, addrlen);
    }
  } else if (msg->msg_name == NULL) {
    error = __wasi_sock_recv(socket,
									ri_data, ri_data_len, ri_flags,
									&ro_datalen,
									&ro_flags);
  } else {
    __wasi_addr_port_t peer_addr;
    error = __wasi_sock_recv_from(socket,
								ri_data, ri_data_len, ri_flags,
								&ro_datalen,
								&ro_flags,
								&peer_addr);
    if (error != 0) {
      errno = error;
      return -1;
    }
    
    struct sockaddr *addr = (struct sockaddr *)msg->msg_name;
    socklen_t *addrlen = &msg->msg_namelen;
	    error = wasi_to_sockaddr(&peer_addr, addr, addrlen);
  }
  msg->msg_flags = 0;
  if ((ro_flags & __WASI_ROFLAGS_RECV_DATA_TRUNCATED) != 0)
    msg->msg_flags |= MSG_TRUNC;
  if (error == 0 && control_capacity != 0) {
    if (ro_control_len > control_capacity)
      msg->msg_flags |= MSG_CTRUNC;
    convert_control_from_wasi(msg, control, ro_control_len > control_capacity ? control_capacity : ro_control_len);
  } else if (control_capacity == 0) {
    msg->msg_controllen = 0;
  }
  free(control);
  
  if (error != 0) {
    errno = error;
    return -1;
  }
  return ro_datalen;
}
