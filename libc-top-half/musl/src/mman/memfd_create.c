#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

static int create_unlinked_temp(char *template_path, unsigned int flags)
{
	int fd;
	if (flags & MFD_CLOEXEC) {
		fd = mkostemp(template_path, O_CLOEXEC);
	} else {
		fd = mkstemp(template_path);
	}
	if (fd < 0) return -1;

	if (unlink(template_path) != 0) {
		int saved_errno = errno;
		close(fd);
		errno = saved_errno;
		return -1;
	}

	return fd;
}

int memfd_create(const char *name, unsigned int flags)
{
	(void)name;

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

	char tmp_template[] = "/tmp/.memfd-XXXXXX";
	int fd = create_unlinked_temp(tmp_template, flags);
	if (fd >= 0) return fd;

	char cwd_template[] = "./.memfd-XXXXXX";
	return create_unlinked_temp(cwd_template, flags);
}
