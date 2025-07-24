#define _GNU_SOURCE
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

char *getpass(const char *prompt)
{
	int fd;
	struct termios s, t;
	ssize_t l;
	static char password[128];

	#ifdef __wasilibc_unmodified_upstream // See TODO comment below
	if ((fd = open("/dev/tty", O_RDWR|O_NOCTTY|O_CLOEXEC)) < 0) return 0;
	#else
	// TODO: This implementation is wrong. It should not use stdin and stdout, but rather /dev/tty which we don't support yet.
	if ((fd = open("/dev/stdin", O_RDONLY|O_CLOEXEC)) < 0) return 0;
	#endif

	tcgetattr(fd, &t);
	s = t;
	t.c_lflag &= ~(ECHO|ISIG);
	t.c_lflag |= ICANON;
	t.c_iflag &= ~(INLCR|IGNCR);
	t.c_iflag |= ICRNL;
	tcsetattr(fd, TCSAFLUSH, &t);
	tcdrain(fd);

	#ifdef __wasilibc_unmodified_upstream // See TODO comment above
	dprintf(fd, "%s", prompt);
	#else
	fprintf(stderr, "%s", prompt);
	fflush(stderr);
	#endif

	l = read(fd, password, sizeof password);
	if (l >= 0) {
		if (l > 0 && password[l-1] == '\n' || l==sizeof password) l--;
		password[l] = 0;
	}

	tcsetattr(fd, TCSAFLUSH, &s);

	#ifdef __wasilibc_unmodified_upstream // See TODO comment above
	dprintf(fd, "\n");
	#else
	fprintf(stderr, "%s", prompt);
	fflush(stderr);
	#endif
	close(fd);

	return l<0 ? 0 : password;
}
