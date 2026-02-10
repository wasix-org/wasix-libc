#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void unlink_if_exists(const char *path)
{
    if (unlink(path) != 0 && errno != ENOENT) {
        perror("unlink");
        assert(0);
    }
}

static void rmdir_if_exists(const char *path)
{
    if (rmdir(path) != 0 && errno != ENOENT) {
        perror("rmdir");
        assert(0);
    }
}

static void ensure_clean(void)
{
    unlink_if_exists("dirent_tmp/file1");
    unlink_if_exists("dirent_tmp/file2");
    rmdir_if_exists("dirent_tmp/subdir");
    rmdir_if_exists("dirent_tmp");
}

static void write_file(const char *path)
{
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    assert(fd >= 0);
    const char byte = 'x';
    ssize_t written = write(fd, &byte, 1);
    assert(written == 1);
    assert(close(fd) == 0);
}

int main(void)
{
    ensure_clean();
    assert(mkdir("dirent_tmp", 0755) == 0);
    assert(mkdir("dirent_tmp/subdir", 0755) == 0);
    write_file("dirent_tmp/file1");

    errno = 0;
    assert(opendir("dirent_tmp/does-not-exist") == NULL);
    assert(errno == ENOENT);

    errno = 0;
    assert(opendir("dirent_tmp/file1") == NULL);
    assert(errno == ENOTDIR);

    DIR *dirp = opendir("dirent_tmp");
    assert(dirp != NULL);

    int saw_dot = 0;
    int saw_dotdot = 0;
    int saw_file = 0;
    int saw_subdir = 0;

    errno = 0;
    struct dirent *ent;
    while ((ent = readdir(dirp)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0) {
            saw_dot = 1;
        } else if (strcmp(ent->d_name, "..") == 0) {
            saw_dotdot = 1;
        } else if (strcmp(ent->d_name, "file1") == 0) {
            saw_file = 1;
        } else if (strcmp(ent->d_name, "subdir") == 0) {
            saw_subdir = 1;
        }
    }
    assert(errno == 0);
    assert(saw_dot && saw_dotdot && saw_file && saw_subdir);
    assert(closedir(dirp) == 0);

    errno = 0;
    assert(fdopendir(-1) == NULL);
    assert(errno == EBADF);

    int fd = open("dirent_tmp/file1", O_RDONLY);
    assert(fd >= 0);
    errno = 0;
    assert(fdopendir(fd) == NULL);
    assert(errno == ENOTDIR);
    assert(close(fd) == 0);

    int dir_fd;
#ifdef O_DIRECTORY
    dir_fd = open("dirent_tmp", O_RDONLY | O_DIRECTORY);
#else
    dir_fd = open("dirent_tmp", O_RDONLY);
#endif
    assert(dir_fd >= 0);
    DIR *fd_dir = fdopendir(dir_fd);
    assert(fd_dir != NULL);
    ent = readdir(fd_dir);
    assert(ent != NULL);
    assert(closedir(fd_dir) == 0);

    errno = 0;
    assert(close(dir_fd) == -1);
    assert(errno == EBADF);

    int closed_fd;
#ifdef O_DIRECTORY
    closed_fd = open("dirent_tmp", O_RDONLY | O_DIRECTORY);
#else
    closed_fd = open("dirent_tmp", O_RDONLY);
#endif
    assert(closed_fd >= 0);
    assert(close(closed_fd) == 0);
    errno = 0;
    assert(fdopendir(closed_fd) == NULL);
    assert(errno == EBADF);

    DIR *dirp2 = opendir("dirent_tmp");
    assert(dirp2 != NULL);
    int dirp2_fd = dirfd(dirp2);
    assert(dirp2_fd >= 0);
    int close_ok = close(dirp2_fd);
    if (close_ok == 0) {
        errno = 0;
        ent = readdir(dirp2);
        assert(ent == NULL);
        assert(errno == EBADF);
    }
    closedir(dirp2);

    ensure_clean();
    return 0;
}
