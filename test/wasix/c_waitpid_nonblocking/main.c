#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

int main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "child") == 0) {
		usleep(100000);
		return 23;
	}

	char *child_argv[] = { "/app/main", "child", NULL };
	pid_t child;
	int err = posix_spawn(&child, child_argv[0], NULL, NULL, child_argv, environ);
	if (err) {
		fprintf(stderr, "posix_spawn failed: %d\n", err);
		return 1;
	}

	/* POSIX allows a null status pointer, and a live child must return zero. */
	pid_t waited = waitpid(child, NULL, WNOHANG);
	if (waited != 0) {
		fprintf(stderr, "early nonblocking wait returned %d, expected 0\n", (int)waited);
		return 2;
	}

	int status = -1;
	waited = waitpid(child, &status, 0);
	if (waited != child || !WIFEXITED(status) || WEXITSTATUS(status) != 23) {
		fprintf(stderr, "completed wait returned pid=%d status=%d, expected pid=%d exit=23\n",
			(int)waited, status, (int)child);
		return 3;
	}

	/* A completed child must be waitable even when the status is discarded. */
	pid_t ignored_status_child;
	err = posix_spawn(&ignored_status_child, child_argv[0], NULL, NULL, child_argv, environ);
	if (err) {
		fprintf(stderr, "second posix_spawn failed: %d\n", err);
		return 4;
	}
	waited = waitpid(ignored_status_child, NULL, 0);
	if (waited != ignored_status_child) {
		fprintf(stderr, "null-status wait returned pid=%d, expected pid=%d\n",
			(int)waited, (int)ignored_status_child);
		return 5;
	}

	puts("WAITPID_NONBLOCKING_OK");
	return 0;
}
