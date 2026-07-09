#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int write_all(int fd, const char *buf, size_t len) {
    while (len > 0) {
        ssize_t wrote = write(fd, buf, len);
        if (wrote < 0) {
            return -1;
        }
        buf += (size_t)wrote;
        len -= (size_t)wrote;
    }
    return 0;
}

static void write_file(const char *path, const char *data, size_t len) {
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    assert(fd >= 0);
    assert(write_all(fd, data, len) == 0);
    close(fd);
}

static char *read_file(const char *path) {
    int fd = open(path, O_RDONLY);
    assert(fd >= 0);
    struct stat st;
    assert(fstat(fd, &st) == 0);
    size_t size = (st.st_size > 0) ? (size_t)st.st_size : 0;
    char *buf = (char *)malloc(size + 1);
    assert(buf != NULL);
    size_t off = 0;
    while (off < size) {
        ssize_t got = read(fd, buf + off, size - off);
        assert(got >= 0);
        if (got == 0) {
            break;
        }
        off += (size_t)got;
    }
    buf[off] = '\0';
    close(fd);
    return buf;
}

static void test_fork_basic(void) {
    const char *path = "fork_child_pid.txt";
    unlink(path);
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        char buf[64];
        int len = snprintf(buf, sizeof(buf), "%d\n", (int)getpid());
        assert(len > 0);
        write_file(path, buf, (size_t)len);
        _exit(42);
    }
    int status = 0;
    pid_t waited = waitpid(pid, &status, 0);
    assert(waited == pid);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 42);
    char *content = read_file(path);
    int child_pid = atoi(content);
    free(content);
    assert(child_pid == pid);
    unlink(path);
}

static const char *env_names[] = {"TERM", "NoTSetzWq", "TESTPROG"};
static const char *env_not_set = "getenv() does not find variable set";

static char *build_env_dump(void) {
    size_t total = 0;
    size_t count = sizeof(env_names) / sizeof(env_names[0]);
    for (size_t i = 0; i < count; ++i) {
        const char *val = getenv(env_names[i]);
        if (!val) {
            val = env_not_set;
        }
        total += strlen(env_names[i]) + 1 + strlen(val) + 1;
    }
    char *buf = (char *)malloc(total + 1);
    assert(buf != NULL);
    size_t off = 0;
    for (size_t i = 0; i < count; ++i) {
        const char *val = getenv(env_names[i]);
        if (!val) {
            val = env_not_set;
        }
        int written = snprintf(buf + off, total + 1 - off, "%s:%s\n", env_names[i], val);
        assert(written > 0);
        off += (size_t)written;
    }
    buf[off] = '\0';
    return buf;
}

static void test_fork_env_inheritance(void) {
    const char *path = "fork_env.out";
    unlink(path);
    unsetenv("NoTSetzWq");
    setenv("TESTPROG", "FRKTCS04", 1);
    char *expected = build_env_dump();
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        char *child_dump = build_env_dump();
        write_file(path, child_dump, strlen(child_dump));
        free(child_dump);
        _exit(0);
    }
    int status = 0;
    pid_t waited = waitpid(pid, &status, 0);
    assert(waited == pid);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);
    char *actual = read_file(path);
    assert(strcmp(actual, expected) == 0);
    free(actual);
    free(expected);
    unlink(path);
}

static void test_fork_fd_offset_shared(void) {
    const char *path = "fork_offset.txt";
    unlink(path);
    const char *data = "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n";
    write_file(path, data, strlen(data));
    int fd = open(path, O_RDONLY);
    assert(fd >= 0);

    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        off_t off = lseek(fd, 10, SEEK_SET);
        assert(off == 10);
        _exit(0);
    }
    int status = 0;
    pid_t waited = waitpid(pid, &status, 0);
    assert(waited == pid);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);

    pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        char c = '\0';
        ssize_t got = read(fd, &c, 1);
        assert(got == 1);
        assert(c == 'K');
        _exit(0);
    }
    waited = waitpid(pid, &status, 0);
    assert(waited == pid);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);

    char c = '\0';
    ssize_t got = read(fd, &c, 1);
    assert(got == 1);
    assert(c == 'L');

    close(fd);
    unlink(path);
}

struct attrs {
    uid_t ruid;
    uid_t euid;
    gid_t rgid;
    gid_t egid;
    mode_t mask;
    char cwd[PATH_MAX];
    dev_t root_dev;
    ino_t root_ino;
    dev_t cwd_dev;
    ino_t cwd_ino;
};

static volatile int vfork_child_error_step = 0;
static volatile int vfork_child_error_errno = 0;
static volatile int vfork_child_reached = 0;
static struct attrs vfork_parent_attrs;
static struct attrs vfork_child_attrs;
static pid_t vfork_pid;
static pid_t vfork_waited;
static int vfork_status;
static int vfork_exit_status;

static void fill_attrs(struct attrs *out) {
    out->ruid = getuid();
    out->euid = geteuid();
    out->rgid = getgid();
    out->egid = getegid();
    mode_t mask = umask(0);
    umask(mask);
    out->mask = mask;
    assert(getcwd(out->cwd, sizeof(out->cwd)) != NULL);
    struct stat st;
    assert(stat("/", &st) == 0);
    out->root_dev = st.st_dev;
    out->root_ino = st.st_ino;
    assert(stat(out->cwd, &st) == 0);
    out->cwd_dev = st.st_dev;
    out->cwd_ino = st.st_ino;
}

static void test_vfork_attributes(void) {
    vfork_child_error_step = 0;
    vfork_child_error_errno = 0;
    vfork_child_reached = 0;
    fill_attrs(&vfork_parent_attrs);
    vfork_pid = vfork();
    assert(vfork_pid >= 0);
    if (vfork_pid == 0) {
        vfork_child_reached = 1;
        vfork_child_attrs.ruid = getuid();
        vfork_child_attrs.euid = geteuid();
        vfork_child_attrs.rgid = getgid();
        vfork_child_attrs.egid = getegid();
        mode_t mask = umask(0);
        umask(mask);
        vfork_child_attrs.mask = mask;
        if (getcwd(vfork_child_attrs.cwd, sizeof(vfork_child_attrs.cwd)) == NULL) {
            vfork_child_error_step = 1;
            vfork_child_error_errno = errno;
            _exit(1);
        }
        struct stat st;
        if (stat("/", &st) != 0) {
            vfork_child_error_step = 2;
            vfork_child_error_errno = errno;
            _exit(1);
        }
        vfork_child_attrs.root_dev = st.st_dev;
        vfork_child_attrs.root_ino = st.st_ino;
        if (stat(vfork_child_attrs.cwd, &st) != 0) {
            vfork_child_error_step = 3;
            vfork_child_error_errno = errno;
            _exit(1);
        }
        vfork_child_attrs.cwd_dev = st.st_dev;
        vfork_child_attrs.cwd_ino = st.st_ino;
        _exit(0);
    }
    vfork_status = 0;
    vfork_waited = waitpid(vfork_pid, &vfork_status, 0);
    assert(vfork_waited == vfork_pid);
    assert(WIFEXITED(vfork_status));
    vfork_exit_status = WEXITSTATUS(vfork_status);
    if (vfork_exit_status != 0) {
        fprintf(stderr, "vfork child failed (reached=%d) step %d errno %d status=0x%x exit=%d\n",
                vfork_child_reached, vfork_child_error_step,
                vfork_child_error_errno, vfork_status, vfork_exit_status);
        assert(vfork_exit_status == 0);
    }

    assert(vfork_parent_attrs.ruid == vfork_child_attrs.ruid);
    assert(vfork_parent_attrs.euid == vfork_child_attrs.euid);
    assert(vfork_parent_attrs.rgid == vfork_child_attrs.rgid);
    assert(vfork_parent_attrs.egid == vfork_child_attrs.egid);
    assert(vfork_parent_attrs.mask == vfork_child_attrs.mask);
    assert(strcmp(vfork_parent_attrs.cwd, vfork_child_attrs.cwd) == 0);
    assert(vfork_parent_attrs.root_dev == vfork_child_attrs.root_dev);
    assert(vfork_parent_attrs.root_ino == vfork_child_attrs.root_ino);
    assert(vfork_parent_attrs.cwd_dev == vfork_child_attrs.cwd_dev);
    assert(vfork_parent_attrs.cwd_ino == vfork_child_attrs.cwd_ino);
}

static void test_setgroups_basic(void) {
    assert(geteuid() == 0);
    int ret = setgroups(0, NULL);
    if (ret != 0) {
        fprintf(stderr, "setgroups(0, NULL) failed errno %d\n", errno);
    }
    assert(ret == 0);
}

static void run_fexecve_child(char *const argv[], char *const envp[]) {
    int fd = open("./exec_helper", O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "open exec_helper failed errno %d\n", errno);
        assert(fd >= 0);
    }
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        fexecve(fd, argv, envp);
        _exit(errno);
    }
    close(fd);
    int status = 0;
    pid_t waited = waitpid(pid, &status, 0);
    assert(waited == pid);
    assert(WIFEXITED(status));
    if (WEXITSTATUS(status) == ENOSYS) {
        assert(!"fexecve is unimplemented");
    }
    assert(WEXITSTATUS(status) == 0);
}

static void test_fexecve_errors(void) {
    char *argv[] = {(char *)"exec_helper", (char *)"echo", (char *)"out", (char *)"x", NULL};
    char *envp[] = {NULL};
    errno = 0;
    assert(fexecve(-1, argv, envp) == -1);
    if (errno != EBADF) {
        fprintf(stderr, "fexecve(-1) errno %d (expected EBADF)\n", errno);
    }
    assert(errno == EBADF);

    int fd = open("/", O_RDONLY);
    assert(fd >= 0);
    errno = 0;
    assert(fexecve(fd, argv, envp) == -1);
    if (errno != EACCES) {
        fprintf(stderr, "fexecve(dirfd) errno %d (expected EACCES)\n", errno);
    }
    assert(errno == EACCES);
    close(fd);
}

static void test_fexecve_success(void) {
    char *envp[] = {(char *)"A=B", NULL};

    const char *echo_out = "exec_echo.out";
    unlink(echo_out);
    char *argv_echo[] = {(char *)"exec_helper", (char *)"echo", (char *)echo_out,
                         (char *)"hello", (char *)"world", NULL};
    run_fexecve_child(argv_echo, envp);
    char *echo_content = read_file(echo_out);
    assert(strcmp(echo_content, "hello world\n") == 0);
    free(echo_content);
    unlink(echo_out);

    const char *env_out = "exec_env.out";
    unlink(env_out);
    char *argv_env[] = {(char *)"exec_helper", (char *)"env", (char *)env_out,
                        (char *)"A", NULL};
    run_fexecve_child(argv_env, envp);
    char *env_content = read_file(env_out);
    assert(strcmp(env_content, "B\n") == 0);
    free(env_content);
    unlink(env_out);

    const char *touch_path = "exec_touch.txt";
    unlink(touch_path);
    char *argv_touch[] = {(char *)"exec_helper", (char *)"touch", (char *)touch_path, NULL};
    run_fexecve_child(argv_touch, envp);
    assert(access(touch_path, F_OK) == 0);
    unlink(touch_path);
}

static void test_wasix_proc_snapshot(void) {
    int ret = wasix_proc_snapshot();
    assert(ret == 0);
}

int main(void) {
    struct {
        const char *name;
        void (*fn)(void);
    } cases[] = {
        {"fork_basic", test_fork_basic},
        {"fork_env_inheritance", test_fork_env_inheritance},
        {"fork_fd_offset_shared", test_fork_fd_offset_shared},
        {"fexecve_errors", test_fexecve_errors},
        {"fexecve_success", test_fexecve_success},
        {"wasix_proc_snapshot", test_wasix_proc_snapshot},
        {"vfork_attributes", test_vfork_attributes},
        {"setgroups_basic", test_setgroups_basic},
    };

    int failures = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        pid_t pid = fork();
        assert(pid >= 0);
        if (pid == 0) {
            cases[i].fn();
            _exit(0);
        }
        int status = 0;
        pid_t waited = waitpid(pid, &status, 0);
        assert(waited == pid);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            printf("%s ok\n", cases[i].name);
        } else if (WIFSIGNALED(status)) {
            printf("%s failed (signal %d)\n", cases[i].name, WTERMSIG(status));
            failures++;
        } else {
            printf("%s failed (exit %d)\n", cases[i].name, WEXITSTATUS(status));
            failures++;
        }
        fflush(stdout);
    }

    if (failures == 0) {
        printf("c_process_control test passed\n");
        return 0;
    }

    printf("c_process_control test failed (%d failure%s)\n", failures,
           failures == 1 ? "" : "s");
    return 1;
}
