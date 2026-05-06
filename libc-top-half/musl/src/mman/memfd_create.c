#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define MEMFD_NAME_MAX 249

static int create_unlinked_temp(char *template_path, unsigned int flags) {
  int fd;
  if (flags & MFD_CLOEXEC) {
    fd = mkostemp(template_path, O_CLOEXEC);
  } else {
    fd = mkstemp(template_path);
  }
  if (fd < 0)
    return -1;

  if (unlink(template_path) != 0) {
    int saved_errno = errno;
    close(fd);
    errno = saved_errno;
    return -1;
  }

  return fd;
}

// Returns the number of characters copied, excluding any null terminator.
// If the returned length == dst_len, src contained at least dst_len
// non-null characters and dst is not null-terminated (no room for a terminator).
static int sanitize_name(char *dst, size_t dst_len, const char *src) {
  size_t i = 0;

  while (i < dst_len && src[i] != '\0') {
    unsigned char c = (unsigned char)src[i];
    if (isalnum(c) || c == '_' || c == '-' || c == '.') {
      dst[i] = (char)c;
    } else {
      dst[i] = '_';
    }
    i++;
  }

  return i;
}

int memfd_create(const char *name, unsigned int flags) {
  if (flags & ~(MFD_CLOEXEC | MFD_ALLOW_SEALING | MFD_HUGETLB)) {
    errno = EINVAL;
    return -1;
  }

  if (flags & (MFD_HUGETLB | MFD_ALLOW_SEALING)) {
    /* Seals are not currently supported by the WASIX fcntl subset. */
    /* hugetlbfs is not currently supported by the WASIX filesystem subset. */
    errno = ENOTSUP;
    return -1;
  }

  if (name == NULL) {
    errno = EFAULT;
    return -1;
  }

  // Construct a string like "/tmp/.memfd-<name>-XXXXXX"
  char tmp_template[sizeof("/tmp/.memfd-") - 1 + MEMFD_NAME_MAX +
                    sizeof("-XXXXXX") - 1 + 1];
  memcpy(tmp_template, "/tmp/.memfd-", sizeof("/tmp/.memfd-") - 1);
  int name_len = sanitize_name(tmp_template + sizeof("/tmp/.memfd-") - 1,
                               MEMFD_NAME_MAX + 1, name);
  if (name_len > MEMFD_NAME_MAX) {
    errno = EINVAL;
    return -1;
  }
  memcpy(tmp_template + sizeof("/tmp/.memfd-") - 1 + name_len, "-XXXXXX",
         sizeof("-XXXXXX"));

  int fd = create_unlinked_temp(tmp_template, flags);
  if (fd >= 0)
    return fd;

  // If that failed, try ".memfd-<name>-XXXXXX" in the current working
  // directory. Restore the -XXXXXX placeholder because it got overwritten by
  // the previous call to mkstemp.
  memcpy(tmp_template + sizeof("/tmp/.memfd-") - 1 + name_len, "-XXXXXX",
         sizeof("-XXXXXX"));
  return create_unlinked_temp(tmp_template + sizeof("/tmp/") - 1, flags);
}
