#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

/* Keep the child alive until the parent has finished polling it. */
#define CHILD_GATE_FD 9

static int spawn_gated_child(pid_t *child, int *release_fd)
{
	int gate[2];
	if (pipe(gate) != 0) return errno;

	posix_spawn_file_actions_t actions;
	int err = posix_spawn_file_actions_init(&actions);
	if (err) {
		close(gate[0]);
		close(gate[1]);
		return err;
	}
	if (!err) err = posix_spawn_file_actions_adddup2(&actions, gate[0], CHILD_GATE_FD);
	if (!err) {
		char write_fd_arg[16];
		snprintf(write_fd_arg, sizeof(write_fd_arg), "%d", gate[1]);
		char *child_argv[] = { "/app/main", "child", write_fd_arg, NULL };
		err = posix_spawn(child, child_argv[0], &actions, NULL, child_argv, environ);
	}
	posix_spawn_file_actions_destroy(&actions);
	close(gate[0]);
	if (err) {
		close(gate[1]);
		return err;
	}
	*release_fd = gate[1];
	return 0;
}

static int release_child(int release_fd)
{
	char ready = 1;
	int ok = write(release_fd, &ready, 1) == 1;
	close(release_fd);
	return ok ? 0 : -1;
}

int main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "child") == 0) {
		if (argc != 3) return 25;
		int write_fd = atoi(argv[2]);
		if (write_fd != CHILD_GATE_FD) close(write_fd);
		char ready;
		return read(CHILD_GATE_FD, &ready, 1) == 1 ? 23 : 24;
	}

	pid_t child;
	int release_fd;
	int err = spawn_gated_child(&child, &release_fd);
	if (err) {
		fprintf(stderr, "posix_spawn failed: %d\n", err);
		return 1;
	}

	/* POSIX allows a null status pointer, and a live child must return zero. */
	errno = 0;
	pid_t waited = waitpid(child, NULL, WNOHANG);
	if (waited != 0 || errno != 0) {
		fprintf(stderr, "early nonblocking wait returned pid=%d errno=%d, expected zero\n",
			(int)waited, errno);
		close(release_fd);
		return 2;
	}

	/* Any-child polling must also leave a pending child registered. */
	int status = -1;
	errno = 0;
	waited = waitpid(-1, &status, WNOHANG);
	if (waited != 0 || status != -1 || errno != 0) {
		fprintf(stderr, "early any-child wait returned pid=%d status=%d errno=%d\n",
			(int)waited, status, errno);
		close(release_fd);
		return 3;
	}

	if (release_child(release_fd) != 0) {
		perror("release first child");
		return 4;
	}
	status = -1;
	waited = waitpid(child, &status, 0);
	if (waited != child || !WIFEXITED(status) || WEXITSTATUS(status) != 23) {
		fprintf(stderr, "completed wait returned pid=%d status=%d, expected pid=%d exit=23\n",
			(int)waited, status, (int)child);
		return 5;
	}

	/* A completed child must be waitable even when the status is discarded. */
	pid_t ignored_status_child;
	err = spawn_gated_child(&ignored_status_child, &release_fd);
	if (err) {
		fprintf(stderr, "second posix_spawn failed: %d\n", err);
		return 6;
	}
	if (release_child(release_fd) != 0) {
		perror("release second child");
		return 7;
	}
	waited = waitpid(ignored_status_child, NULL, 0);
	if (waited != ignored_status_child) {
		fprintf(stderr, "null-status wait returned pid=%d, expected pid=%d\n",
			(int)waited, (int)ignored_status_child);
		return 8;
	}

	puts("WAITPID_NONBLOCKING_OK");
	return 0;
}
