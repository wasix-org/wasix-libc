# WASI/WASIX-backed libc functions by category

Total functions: 215

## Control Flow/Setjmp (4)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| longjmp | setjmp.h | wasix-libc/libc-top-half/musl/src/setjmp/setjmplongjmp.c | __wasi_stack_restore |
| setjmp | setjmp.h | wasix-libc/libc-top-half/musl/src/setjmp/setjmplongjmp.c | __wasi_stack_checkpoint |
| siglongjmp | setjmp.h | wasix-libc/libc-top-half/musl/src/signal/siglongjmp.c | __wasi_stack_restore |
| sigsetjmp | setjmp.h | wasix-libc/libc-top-half/musl/src/setjmp/setjmplongjmp.c | __wasi_stack_checkpoint |

### Progress

- Ported tests: NetBSD `t_setjmp.c`, `t_threadjmp.c` + bionic `setjmp_test.cpp` (subset)
- New integration test: `wasix-libc/test/wasix/c_setjmp` (covers `setjmp`, `longjmp`, `sigsetjmp`, `siglongjmp`)
- Status (2026-01-30): passing after implementing `sigsetjmp(..., 1)` mask save/restore and tracking masks in `pthread_sigmask` (requires rebuilding libc in the sysroot)
- Notes: `_setjmp`/`_longjmp` not exported in wasm32-wasi symbols, so they were not covered

## Dynamic Loading (3)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| dlclose | dlfcn.h | wasix-libc/libc-top-half/musl/src/ldso/dlclose.c | __wasi_dl_invalid_handle |
| dlopen | dlfcn.h | wasix-libc/libc-top-half/musl/src/ldso/dlopen.c | __wasi_dlopen |
| dlsym | dlfcn.h | wasix-libc/libc-top-half/musl/src/ldso/dlsym.c | __wasi_dlsym |

### Progress

- Ported tests: Wasmer `tests/wasix/dlopen`, `tests/wasix/dl-cache`, `tests/wasix/dl-needed`, Wasmer `tests/c-wasi-tests/dlopen`, FreeBSD `lib/libc/tests/gen/dlopen_empty_test.c`, FreeBSD `libexec/rtld-elf/tests/dlopen_test.c` (dlsym after dlclose), bionic `tests/dlfcn_test.cpp` (RTLD_NOLOAD, RTLD_LOCAL/GLOBAL + RTLD_DEFAULT, empty-symbol dlsym, main-handle global lookup)
- New integration test: `wasix-libc/test/wasix/dynamic_loading` (cases: basic, cache, data-export, needed)
- Status (2026-01-31): passing
- Fixes applied: libc now tracks dlopen handles/flags, enforces `RTLD_NOLOAD` (no implicit load), resolves `RTLD_DEFAULT` via main + RTLD_GLOBAL handles, and rejects `dlsym` on closed/invalid handles. This aligns WASI host calls with POSIX semantics expected by plugin/extension code.

## Filesystem/Dirent (3)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| fdopendir | dirent.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/dirent/fdopendir.c | __wasi_fd_readdir |
| opendir | dirent.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_fd_close, __wasi_fd_fdstat_get, __wasi_fd_readdir, __wasi_getcwd, __wasi_path_open2 |
| readdir | dirent.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/dirent/readdir.c | __wasi_fd_readdir, __wasi_getcwd, __wasi_path_filestat_get |

### Progress

- Ported tests: FreeBSD `lib/libc/tests/gen/opendir_test.c` (basic dir listing, ENOENT/ENOTDIR, fdopendir), glibc `opendir-tst1.c` (non-dir check), glibc `tst-fdopendir2.c` (fdopendir on regular file), glibc `bug-readdir1.c` (readdir after closing dirfd), bionic `dirent_test.cpp` (subset: fdopendir invalid/ownership, opendir invalid, readdir end-of-dir errno behavior)
- New integration test: `wasix-libc/test/wasix/c_dirent`
- Status (2026-01-31): passing (with non‑EH sysroot)
- Notes: test focuses on `opendir`, `fdopendir`, `readdir` behavior and error codes using a synthetic directory; no OS-specific `/proc` or FUSE dependencies

## Filesystem/File I/O (50)

### Group 1: Process control & snapshot (5)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| __vfork_internal | unistd.h | wasix-libc/libc-top-half/musl/src/process/vfork.c | __wasi_proc_fork_env |
| _fork_internal | unistd.h | wasix-libc/libc-top-half/musl/src/process/fork.c | __wasi_callback_signal, __wasi_futex_wait, __wasi_proc_exit2, __wasi_proc_fork, __wasi_stack_restore, __wasi_thread_id, __wasi_thread_signal |
| fexecve | unistd.h | wasix-libc/libc-top-half/musl/src/process/fexecve.c | __wasi_proc_exec3, __wasi_stack_restore |
| setgroups | grp.h, unistd.h | wasix-libc/libc-top-half/musl/src/linux/setgroups.c | __wasi_callback_signal, __wasi_futex_wait |
| wasix_proc_snapshot | unistd.h | wasix-libc/libc-top-half/musl/src/process/wasix_proc_snapshot.c | __wasi_proc_snapshot |

### Progress

- Ported tests: LTP `fork01.c` (basic fork + pid/wait), LTP `fork04.c` (env inheritance), LTP `fork10.c` (shared fd offset), LTP `vfork01.c` (attribute parity), LTP `setgroups01.c` (basic call), glibc `posix/tst-fexecve.c`, NetBSD `tests/lib/libc/c063/t_fexecve.c`, bionic `tests/unistd_test.cpp` (fexecve failure + args/env)
- New integration test: `wasix-libc/test/wasix/c_process_control` (subtests: fork_basic, fork_env_inheritance, fork_fd_offset_shared, fexecve_errors, fexecve_success, wasix_proc_snapshot, vfork_attributes, setgroups_basic)
- Status (2026-01-31): all subtests passing (fork_basic/env/offset, fexecve_errors/success, wasix_proc_snapshot, vfork_attributes, setgroups_basic)
- Notes: getgroups is not exported in wasix-libc, so the setgroups test uses setgroups(0, NULL) as the basic check; vfork_attributes keeps shared state in globals to avoid stack clobber after vfork

### Group 2: Working directory & access (4)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| getcwd | unistd.h | wasix-libc/libc-bottom-half/sources/getcwd.c | __wasi_getcwd |
| chdir | unistd.h | wasix-libc/libc-bottom-half/sources/chdir.c | __wasi_chdir, __wasi_getcwd, __wasi_path_filestat_get |
| access | unistd.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_fd_fdstat_get, __wasi_getcwd, __wasi_path_filestat_get |
| faccessat | unistd.h | wasix-libc/libc-bottom-half/sources/at_fdcwd.c | __wasi_fd_fdstat_get, __wasi_getcwd, __wasi_path_filestat_get |

### Group 3: Terminal/TTY (3)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| getpass | unistd.h | wasix-libc/libc-top-half/musl/src/legacy/getpass.c | __wasi_fd_close, __wasi_fd_fdstat_get, __wasi_fd_fdstat_set_flags, __wasi_fd_read, __wasi_getcwd, __wasi_path_open2, __wasi_poll_oneoff, __wasi_tty_get, __wasi_tty_set |
| tcgetpgrp | unistd.h | wasix-libc/libc-top-half/musl/src/unistd/tcgetpgrp.c | __wasi_fd_fdstat_get, __wasi_fd_fdstat_set_flags, __wasi_poll_oneoff, __wasi_tty_get, __wasi_tty_set |
| tcsetpgrp | unistd.h | wasix-libc/libc-top-half/musl/src/unistd/tcsetpgrp.c | __wasi_fd_fdstat_get, __wasi_fd_fdstat_set_flags, __wasi_poll_oneoff, __wasi_tty_get, __wasi_tty_set |

### Group 4: FD duplication & close (4)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| close | unistd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/unistd/close.c | __wasi_fd_close |
| dup | unistd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/unistd/dup.c | __wasi_fd_dup |
| dup2 | unistd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/unistd/dup2.c | __wasi_fd_renumber |
| dup3 | unistd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/unistd/dup3.c | __wasi_fd_renumber |

### Group 5: FD control & pipes (4)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| fcntl | fcntl.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/fcntl/fcntl.c | __wasi_fd_dup2, __wasi_fd_fdflags_get, __wasi_fd_fdflags_set, __wasi_fd_fdstat_get, __wasi_fd_fdstat_set_flags |
| pipe | unistd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/unistd/pipe.c | __wasi_fd_pipe |
| pipe2 | unistd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/unistd/pipe2.c | __wasi_fd_pipe |
| __wasilibc_tell | unistd.h, wasi/libc.h | wasix-libc/libc-bottom-half/sources/__wasilibc_tell.c | __wasi_fd_tell |

### Group 6: Open & create directories (4)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| open | fcntl.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_fd_fdstat_get, __wasi_getcwd, __wasi_path_open2 |
| openat | fcntl.h | wasix-libc/libc-bottom-half/sources/at_fdcwd.c | __wasi_fd_fdstat_get, __wasi_getcwd, __wasi_path_open2 |
| mkdir | sys/stat.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_getcwd, __wasi_path_create_directory |
| mkdirat | sys/stat.h | wasix-libc/libc-bottom-half/sources/at_fdcwd.c | __wasi_getcwd, __wasi_path_create_directory |

### Group 7: Remove/unlink & readlink (5)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| rmdir | unistd.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_getcwd, __wasi_path_remove_directory |
| unlink | unistd.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_getcwd, __wasi_path_unlink_file |
| unlinkat | unistd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/unistd/unlinkat.c | __wasi_getcwd, __wasi_path_remove_directory, __wasi_path_unlink_file |
| readlink | unistd.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_getcwd, __wasi_path_readlink |
| readlinkat | unistd.h | wasix-libc/libc-bottom-half/sources/at_fdcwd.c | __wasi_getcwd, __wasi_path_readlink |

### Group 8: Links & symlinks (4)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| link | unistd.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_getcwd, __wasi_path_link |
| linkat | unistd.h | wasix-libc/libc-bottom-half/sources/at_fdcwd.c | __wasi_getcwd, __wasi_path_link |
| symlink | unistd.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_getcwd, __wasi_path_symlink |
| symlinkat | unistd.h | wasix-libc/libc-bottom-half/sources/at_fdcwd.c | __wasi_getcwd, __wasi_path_symlink |

### Group 9: Stat & metadata (4)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| stat | sys/stat.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_getcwd, __wasi_path_filestat_get |
| lstat | sys/stat.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_getcwd, __wasi_path_filestat_get |
| fstat | sys/stat.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/stat/fstat.c | __wasi_fd_filestat_get |
| fstatat | sys/stat.h | wasix-libc/libc-bottom-half/sources/at_fdcwd.c | __wasi_getcwd, __wasi_path_filestat_get |

### Group 10: Timestamps & sync (4)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| futimens | sys/stat.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/stat/futimens.c | __wasi_fd_filestat_set_times |
| utimensat | sys/stat.h | wasix-libc/libc-bottom-half/sources/at_fdcwd.c | __wasi_getcwd, __wasi_path_filestat_set_times |
| fsync | unistd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/unistd/fsync.c | __wasi_fd_sync |
| fdatasync | unistd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/unistd/fdatasync.c | __wasi_fd_datasync |

### Group 11: Allocation & size (4)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| ftruncate | unistd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/unistd/ftruncate.c | __wasi_fd_filestat_set_size |
| truncate | unistd.h | wasix-libc/libc-bottom-half/sources/truncate.c | __wasi_fd_close, __wasi_fd_fdstat_get, __wasi_fd_filestat_set_size, __wasi_getcwd, __wasi_path_open2 |
| posix_fallocate | fcntl.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/fcntl/posix_fallocate.c | __wasi_fd_allocate |
| posix_fadvise | fcntl.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/fcntl/posix_fadvise.c | __wasi_fd_advise |

### Group 12: Data I/O (5)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| read | unistd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/unistd/read.c | __wasi_fd_read |
| write | unistd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/unistd/write.c | __wasi_fd_write |
| pread | unistd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/unistd/pread.c | __wasi_fd_fdstat_get, __wasi_fd_pread |
| pwrite | unistd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/unistd/pwrite.c | __wasi_fd_fdstat_get, __wasi_fd_pwrite |
| sendfile | sys/sendfile.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/socket/sendfile.c | __wasi_fd_tell, __wasi_sock_send_file |

## I/O Multiplexing (10)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| epoll_create | sys/epoll.h | wasix-libc/libc-top-half/musl/src/linux/epoll.c | __wasi_epoll_create |
| epoll_create1 | sys/epoll.h | wasix-libc/libc-top-half/musl/src/linux/epoll.c | __wasi_epoll_create |
| epoll_ctl | sys/epoll.h | wasix-libc/libc-top-half/musl/src/linux/epoll.c | __wasi_epoll_ctl |
| epoll_pwait | sys/epoll.h | wasix-libc/libc-top-half/musl/src/linux/epoll.c | __wasi_epoll_wait |
| epoll_wait | sys/epoll.h | wasix-libc/libc-top-half/musl/src/linux/epoll.c | __wasi_epoll_wait |
| eventfd | sys/eventfd.h | wasix-libc/libc-top-half/musl/src/linux/eventfd.c | __wasi_fd_event |
| eventfd_read | sys/eventfd.h | wasix-libc/libc-top-half/musl/src/linux/eventfd.c | __wasi_fd_read |
| eventfd_write | sys/eventfd.h | wasix-libc/libc-top-half/musl/src/linux/eventfd.c | __wasi_fd_write |
| poll | poll.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/poll/poll.c | __wasi_poll_oneoff |
| pselect | sys/select.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/select/pselect.c | __wasi_poll_oneoff |

## Locale/Messages (3)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| catclose | nl_types.h | wasix-libc/libc-top-half/musl/src/locale/catclose.c | __wasi_fd_close, __wasi_fd_fdstat_get, __wasi_fd_pwrite |
| catopen | nl_types.h | wasix-libc/libc-top-half/musl/src/locale/catopen.c | __wasi_fd_close, __wasi_fd_fdstat_get, __wasi_fd_pwrite |
| fmtmsg | fmtmsg.h | wasix-libc/libc-top-half/musl/src/misc/fmtmsg.c | __wasi_fd_close, __wasi_fd_fdstat_get, __wasi_getcwd, __wasi_path_open2 |

## Misc (13)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| closelog | syslog.h | wasix-libc/libc-top-half/musl/src/misc/syslog.c | __wasi_fd_close |
| mmap | sys/mman.h | wasix-libc/libc-bottom-half/mman/mman.c | __wasi_fd_dup, __wasi_fd_fdstat_get, __wasi_fd_pread |
| msync | sys/mman.h | wasix-libc/libc-bottom-half/mman/mman.c | __wasi_fd_fdstat_get, __wasi_fd_pwrite |
| munmap | sys/mman.h | wasix-libc/libc-bottom-half/mman/mman.c | __wasi_fd_close, __wasi_fd_fdstat_get, __wasi_fd_pwrite |
| openlog | syslog.h | wasix-libc/libc-top-half/musl/src/misc/syslog.c | __wasi_sock_connect, __wasi_sock_open |
| preadv | sys/uio.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/uio/preadv.c | __wasi_fd_pread |
| pwritev | sys/uio.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/uio/pwritev.c | __wasi_fd_pwrite |
| readv | sys/uio.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/uio/readv.c | __wasi_fd_read |
| syslog | syslog.h | wasix-libc/libc-top-half/musl/src/misc/syslog.c | __wasi_clock_time_get, __wasi_fd_close, __wasi_fd_fdstat_get, __wasi_fd_write, __wasi_getcwd, __wasi_path_open2, __wasi_proc_id, __wasi_sock_connect, __wasi_sock_open, __wasi_sock_send |
| thrd_yield | threads.h | wasix-libc/libc-top-half/musl/src/thread/thrd_yield.c | __wasi_sched_yield |
| times | sys/times.h | wasix-libc/libc-top-half/musl/src/time/times.c | __wasi_clock_time_get |
| utime | utime.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_getcwd, __wasi_path_filestat_set_times |
| writev | sys/uio.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/uio/writev.c | __wasi_fd_write |

## Process/Exit (1)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| abort | stdlib.h | wasix-libc/libc-top-half/musl/src/exit/abort.c | __wasi_callback_signal, __wasi_proc_exit2, __wasi_stack_restore, __wasi_thread_signal |

## Process/Spawn (19)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| _Exit | stdlib.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/stdlib/_Exit.c | __wasi_proc_exit2, __wasi_stack_restore |
| _Exit | stdlib.h | wasix-libc/libc-top-half/musl/src/exit/_Exit.c | __wasi_proc_exit2, __wasi_stack_restore |
| _Fork | unistd.h | wasix-libc/libc-top-half/musl/src/process/_Fork.c | __wasi_callback_signal, __wasi_proc_exit2, __wasi_proc_fork, __wasi_stack_restore, __wasi_thread_id, __wasi_thread_signal |
| execv | unistd.h | wasix-libc/libc-top-half/musl/src/process/execv.c | __wasi_proc_exec3, __wasi_stack_restore |
| execve | unistd.h | wasix-libc/libc-top-half/musl/src/process/execve.c | __wasi_proc_exec3, __wasi_stack_restore |
| exit | stdlib.h | wasix-libc/libc-top-half/musl/src/exit/exit.c | __wasi_proc_exit2, __wasi_stack_restore |
| exit | stdlib.h | wasix-libc/libc-top-half/musl/src/exit/exit.c | __wasi_proc_exit2, __wasi_stack_restore |
| fork | unistd.h | wasix-libc/libc-top-half/musl/src/process/fork.c | __wasi_callback_signal, __wasi_futex_wait, __wasi_proc_exit2, __wasi_proc_fork, __wasi_stack_restore, __wasi_thread_id, __wasi_thread_signal |
| getpid | unistd.h | wasix-libc/libc-bottom-half/sources/getpid.c | __wasi_proc_id |
| getppid | unistd.h | wasix-libc/libc-bottom-half/sources/getppid.c | __wasi_proc_id, __wasi_proc_parent |
| posix_spawn | spawn.h | wasix-libc/libc-top-half/musl/src/process/posix_spawn.c | __wasi_callback_signal, __wasi_fd_close, __wasi_fd_pipe, __wasi_fd_read, __wasi_futex_wait, __wasi_proc_exit2, __wasi_proc_fork, __wasi_proc_join, __wasi_stack_restore, __wasi_thread_id, __wasi_thread_signal |
| posix_spawn | spawn.h | wasix-libc/libc-top-half/musl/src/process/posix_spawn.c | __wasi_proc_spawn2 |
| posix_spawnp | spawn.h | wasix-libc/libc-top-half/musl/src/process/posix_spawnp.c | __wasi_callback_signal, __wasi_fd_close, __wasi_fd_pipe, __wasi_fd_read, __wasi_futex_wait, __wasi_proc_exit2, __wasi_proc_fork, __wasi_proc_join, __wasi_proc_spawn2, __wasi_stack_restore, __wasi_thread_id, __wasi_thread_signal |
| system | stdlib.h | wasix-libc/libc-top-half/musl/src/process/system.c | __wasi_callback_signal, __wasi_fd_close, __wasi_fd_pipe, __wasi_fd_read, __wasi_futex_wait, __wasi_proc_exit2, __wasi_proc_fork, __wasi_proc_join, __wasi_proc_spawn2, __wasi_stack_restore, __wasi_thread_id, __wasi_thread_signal |
| vfork | unistd.h | wasix-libc/libc-top-half/musl/src/process/vfork.c | __wasi_callback_signal, __wasi_futex_wait, __wasi_proc_exit2, __wasi_proc_fork, __wasi_stack_restore, __wasi_thread_id, __wasi_thread_signal |
| wait | sys/wait.h | wasix-libc/libc-top-half/musl/src/process/wait.c | __wasi_proc_join |
| wait3 | sys/wait.h | wasix-libc/libc-top-half/musl/src/linux/wait3.c | __wasi_proc_join |
| wait4 | sys/wait.h | wasix-libc/libc-top-half/musl/src/linux/wait4.c | __wasi_proc_join |
| waitpid | sys/wait.h | wasix-libc/libc-top-half/musl/src/process/waitpid.c | __wasi_proc_join |

## Random (1)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| getrandom | sys/random.h | wasix-libc/libc-bottom-half/sources/getrandom.c | __wasi_random_get |

## Signals (3)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| kill | signal.h | wasix-libc/libc-top-half/musl/src/signal/kill.c | __wasi_proc_signal |
| raise | signal.h | wasix-libc/libc-top-half/musl/src/signal/raise.c | __wasi_callback_signal, __wasi_thread_signal |
| sigqueue | signal.h | wasix-libc/libc-top-half/musl/src/signal/sigqueue.c | __wasi_callback_signal, __wasi_proc_id |

## Sockets/Networking (19)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| accept | sys/socket.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/socket/accept.c | __wasi_sock_accept_v2 |
| accept4 | sys/socket.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/socket/accept.c | __wasi_sock_accept_v2 |
| bind | sys/socket.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/socket/bind.c | __wasi_sock_bind |
| connect | sys/socket.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/socket/connect.c | __wasi_sock_connect |
| getnameinfo | netdb.h | wasix-libc/libc-top-half/musl/src/network/getnameinfo.c | __wasi_fd_dup2, __wasi_fd_fdflags_get, __wasi_fd_fdflags_set, __wasi_fd_fdstat_get, __wasi_fd_fdstat_set_flags, __wasi_getcwd, __wasi_path_open2 |
| getpeername | sys/socket.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/socket/getpeername.c | __wasi_sock_addr_peer |
| getsockname | sys/socket.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/socket/getsockname.c | __wasi_sock_addr_local |
| getsockopt | sys/socket.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/socket/getsockopt.c | __wasi_fd_fdstat_get, __wasi_sock_get_opt_flag, __wasi_sock_get_opt_size, __wasi_sock_get_opt_time |
| listen | sys/socket.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/socket/listen.c | __wasi_sock_listen |
| recv | sys/socket.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/socket/recv.c | __wasi_sock_recv |
| recvfrom | sys/socket.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/socket/recvfrom.c | __wasi_sock_recv_from |
| recvmsg | sys/socket.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/socket/recvmsg.c | __wasi_sock_recv, __wasi_sock_recv_from |
| send | sys/socket.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/socket/send.c | __wasi_sock_send |
| sendmsg | sys/socket.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/socket/sendmsg.c | __wasi_sock_send, __wasi_sock_send_to |
| sendto | sys/socket.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/socket/sendto.c | __wasi_sock_send_to |
| setsockopt | sys/socket.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/socket/setsockopt.c | __wasi_sock_set_opt_flag, __wasi_sock_set_opt_size, __wasi_sock_set_opt_time |
| shutdown | sys/socket.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/socket/shutdown.c | __wasi_sock_shutdown |
| socket | sys/socket.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/socket/socket.c | __wasi_sock_open |
| socketpair | sys/socket.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/socket/socketpair.c | __wasi_sock_pair |

## StdIO (13)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| fclose | stdio.h | wasix-libc/libc-top-half/musl/src/stdio/fclose.c | __wasi_fd_close |
| fopen | stdio.h | wasix-libc/libc-top-half/musl/src/stdio/fopen.c | __wasi_fd_close, __wasi_fd_dup2, __wasi_fd_fdflags_get, __wasi_fd_fdflags_set, __wasi_fd_fdstat_get, __wasi_fd_fdstat_set_flags, __wasi_getcwd, __wasi_path_open2 |
| freopen | stdio.h | wasix-libc/libc-top-half/musl/src/stdio/freopen.c | __wasi_fd_close, __wasi_fd_dup2, __wasi_fd_fdflags_get, __wasi_fd_fdflags_set, __wasi_fd_fdstat_get, __wasi_fd_fdstat_set_flags, __wasi_fd_renumber, __wasi_getcwd, __wasi_path_open2 |
| pclose | stdio.h | wasix-libc/libc-top-half/musl/src/stdio/pclose.c | __wasi_fd_close, __wasi_proc_join |
| popen | stdio.h | wasix-libc/libc-top-half/musl/src/stdio/popen.c | __wasi_callback_signal, __wasi_environ_get, __wasi_environ_sizes_get, __wasi_fd_close, __wasi_fd_dup2, __wasi_fd_fdflags_get, __wasi_fd_fdflags_set, __wasi_fd_fdstat_get, __wasi_fd_fdstat_set_flags, __wasi_fd_pipe, __wasi_fd_read, __wasi_futex_wait, __wasi_proc_exit2, __wasi_proc_fork, __wasi_proc_join, __wasi_proc_spawn2, __wasi_stack_restore, __wasi_thread_id, __wasi_thread_signal |
| printf | stdio.h | wasix-libc/libc-top-half/musl/src/stdio/printf.c | __wasi_fd_write |
| remove | stdio.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_getcwd, __wasi_path_remove_directory, __wasi_path_unlink_file |
| rename | stdio.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_getcwd, __wasi_path_rename |
| renameat | stdio.h | wasix-libc/libc-bottom-half/sources/at_fdcwd.c | __wasi_getcwd, __wasi_path_rename |
| tmpfile | stdio.h | wasix-libc/libc-top-half/musl/src/stdio/tmpfile.c | __wasi_fd_close, __wasi_fd_dup2, __wasi_fd_fdflags_get, __wasi_fd_fdflags_set, __wasi_fd_fdstat_get, __wasi_fd_fdstat_set_flags, __wasi_getcwd, __wasi_path_open2 |
| vdprintf | stdio.h | wasix-libc/libc-top-half/musl/src/stdio/vdprintf.c | __wasi_fd_write |
| vfprintf | stdio.h | wasix-libc/libc-top-half/musl/src/stdio/vfprintf.c | __wasi_fd_write |
| vsnprintf | stdio.h | wasix-libc/libc-top-half/musl/src/stdio/vsnprintf.c | __wasi_fd_write |

## System/Config (1)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| sysconf | unistd.h | wasix-libc/libc-top-half/musl/src/conf/sysconf.c | __wasi_thread_parallelism |

## Terminal/TTY (11)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| ioctl | stropts.h, sys/ioctl.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/ioctl/ioctl.c | __wasi_fd_fdstat_get, __wasi_fd_fdstat_set_flags, __wasi_poll_oneoff, __wasi_tty_get, __wasi_tty_set |
| isatty | unistd.h | wasix-libc/libc-top-half/musl/src/unistd/isatty.c | __wasi_tty_get |
| tcflow | termios.h | wasix-libc/libc-top-half/musl/src/termios/tcflow.c | __wasi_fd_fdstat_get, __wasi_fd_fdstat_set_flags, __wasi_poll_oneoff, __wasi_tty_get, __wasi_tty_set |
| tcflush | termios.h | wasix-libc/libc-top-half/musl/src/termios/tcflush.c | __wasi_fd_fdstat_get, __wasi_fd_fdstat_set_flags, __wasi_poll_oneoff, __wasi_tty_get, __wasi_tty_set |
| tcgetattr | termios.h | wasix-libc/libc-top-half/musl/src/termios/tcgetattr.c | __wasi_fd_fdstat_get, __wasi_fd_fdstat_set_flags, __wasi_poll_oneoff, __wasi_tty_get, __wasi_tty_set |
| tcgetsid | termios.h | wasix-libc/libc-top-half/musl/src/termios/tcgetsid.c | __wasi_fd_fdstat_get, __wasi_fd_fdstat_set_flags, __wasi_poll_oneoff, __wasi_tty_get, __wasi_tty_set |
| tcgetwinsize | termios.h | wasix-libc/libc-top-half/musl/src/termios/tcgetwinsize.c | __wasi_tty_get |
| tcsendbreak | termios.h | wasix-libc/libc-top-half/musl/src/termios/tcsendbreak.c | __wasi_fd_fdstat_get, __wasi_fd_fdstat_set_flags, __wasi_poll_oneoff, __wasi_tty_get, __wasi_tty_set |
| tcsetattr | termios.h | wasix-libc/libc-top-half/musl/src/termios/tcsetattr.c | __wasi_fd_fdstat_get, __wasi_fd_fdstat_set_flags, __wasi_poll_oneoff, __wasi_tty_get, __wasi_tty_set |
| tcsetwinsize | termios.h | wasix-libc/libc-top-half/musl/src/termios/tcsetwinsize.c | __wasi_tty_get, __wasi_tty_set |
| ttyname_r | unistd.h | wasix-libc/libc-top-half/musl/src/unistd/ttyname_r.c | __wasi_fd_filestat_get, __wasi_getcwd, __wasi_path_filestat_get, __wasi_path_readlink, __wasi_tty_get |

## Threads (9)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| pthread_barrier_destroy | pthread.h | wasix-libc/libc-top-half/musl/src/thread/pthread_barrier_destroy.c | __wasi_futex_wait |
| pthread_barrier_wait | pthread.h | wasix-libc/libc-top-half/musl/src/thread/pthread_barrier_wait.c | __wasi_futex_wait |
| pthread_cancel | pthread.h | wasix-libc/libc-top-half/musl/src/thread/pthread_cancel.c | __wasi_callback_signal, __wasi_thread_signal |
| pthread_getschedparam | pthread.h | wasix-libc/libc-top-half/musl/src/thread/pthread_getschedparam.c | __wasi_callback_signal |
| pthread_kill | signal.h | wasix-libc/libc-top-half/musl/src/thread/pthread_kill.c | __wasi_callback_signal |
| pthread_kill | signal.h | wasix-libc/libc-top-half/musl/src/thread/pthread_kill.c | __wasi_callback_signal, __wasi_thread_signal |
| pthread_setname_np | pthread.h | wasix-libc/libc-top-half/musl/src/thread/pthread_setname_np.c | __wasi_fd_close, __wasi_fd_fdstat_get, __wasi_fd_write, __wasi_getcwd, __wasi_path_open2 |
| pthread_setschedparam | pthread.h | wasix-libc/libc-top-half/musl/src/thread/pthread_setschedparam.c | __wasi_callback_signal |
| pthread_setschedprio | pthread.h | wasix-libc/libc-top-half/musl/src/thread/pthread_setschedprio.c | __wasi_callback_signal |

## Threads/Scheduling (1)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| sched_yield | sched.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sched/sched_yield.c | __wasi_sched_yield |

## Time/Clocks (10)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| clock_getres | time.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/time/clock_getres.c | __wasi_clock_res_get |
| clock_nanosleep | time.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/time/clock_nanosleep.c | __wasi_poll_oneoff |
| getdate | time.h | wasix-libc/libc-top-half/musl/src/time/getdate.c | __wasi_fd_close, __wasi_fd_dup2, __wasi_fd_fdflags_get, __wasi_fd_fdflags_set, __wasi_fd_fdstat_get, __wasi_fd_fdstat_set_flags, __wasi_getcwd, __wasi_path_open2 |
| getrusage | sys/resource.h | wasix-libc/libc-top-half/musl/src/misc/getrusage.c | __wasi_clock_time_get |
| gettimeofday | sys/time.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/time/gettimeofday.c | __wasi_clock_time_get |
| setitimer | sys/time.h | wasix-libc/libc-top-half/musl/src/signal/setitimer.c | __wasi_proc_raise_interval |
| setrlimit | sys/resource.h | wasix-libc/libc-top-half/musl/src/misc/setrlimit.c | __wasi_callback_signal, __wasi_futex_wait |
| time | time.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/time/time.c | __wasi_clock_time_get |
| timer_create | time.h | wasix-libc/libc-top-half/musl/src/time/timer_create.c | __wasi_callback_signal, __wasi_futex_wait |
| utimes | sys/time.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_getcwd, __wasi_path_filestat_set_times |

## WASIX/WASI Internals (41)

| Function | Header(s) | Source | WASI/WASIX syscalls |
|---|---|---|---|
| __wasilibc_access | wasi/libc.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_fd_fdstat_get, __wasi_getcwd, __wasi_path_filestat_get |
| __wasilibc_ensure_environ | wasi/libc-environ.h | wasix-libc/libc-bottom-half/sources/__wasilibc_initialize_environ.c | __wasi_environ_get, __wasi_environ_sizes_get, __wasi_proc_exit2, __wasi_stack_restore |
| __wasilibc_fd_renumber | wasi/libc.h | wasix-libc/libc-bottom-half/sources/__wasilibc_fd_renumber.c | __wasi_fd_renumber |
| __wasilibc_find_relpath | wasi/libc-find-relpath.h | wasix-libc/libc-bottom-half/sources/preopens.c | __wasi_getcwd |
| __wasilibc_find_relpath_alloc | wasi/libc-find-relpath.h | wasix-libc/libc-bottom-half/sources/chdir.c | __wasi_getcwd |
| __wasilibc_futex_wait_wasix | wasi/libc.h | wasix-libc/libc-bottom-half/sources/__wasilibc_futex.c | __wasi_futex_wait |
| __wasilibc_futex_wake_wasix | wasi/libc.h | wasix-libc/libc-bottom-half/sources/__wasilibc_futex.c | __wasi_futex_wake, __wasi_futex_wake_all |
| __wasilibc_get_environ | wasi/libc-environ.h | wasix-libc/libc-bottom-half/sources/__wasilibc_environ.c | __wasi_environ_get, __wasi_environ_sizes_get, __wasi_proc_exit2, __wasi_stack_restore |
| __wasilibc_initialize_environ | wasi/libc-environ.h, wasi/libc.h | wasix-libc/libc-bottom-half/sources/__wasilibc_initialize_environ.c | __wasi_environ_get, __wasi_environ_sizes_get, __wasi_proc_exit2, __wasi_stack_restore |
| __wasilibc_link | wasi/libc.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_getcwd, __wasi_path_link |
| __wasilibc_link_newat | wasi/libc.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_getcwd, __wasi_path_link |
| __wasilibc_link_oldat | wasi/libc.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_getcwd, __wasi_path_link |
| __wasilibc_longjmp | wasi/libc.h | wasix-libc/libc-bottom-half/sources/__wasilibc_stack.c | __wasi_stack_restore |
| __wasilibc_maybe_reinitialize_environ_eagerly | wasi/libc-environ.h | wasix-libc/libc-bottom-half/sources/environ.c | __wasi_environ_get, __wasi_environ_sizes_get, __wasi_proc_exit2, __wasi_stack_restore |
| __wasilibc_nocwd___wasilibc_rmdirat | wasi/libc-nocwd.h | wasix-libc/libc-bottom-half/sources/__wasilibc_rmdirat.c | __wasi_path_remove_directory |
| __wasilibc_nocwd___wasilibc_unlinkat | wasi/libc-nocwd.h | wasix-libc/libc-bottom-half/sources/__wasilibc_unlinkat.c | __wasi_path_unlink_file |
| __wasilibc_nocwd_faccessat | wasi/libc-nocwd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/unistd/faccessat.c | __wasi_fd_fdstat_get, __wasi_path_filestat_get |
| __wasilibc_nocwd_fstatat | wasi/libc-nocwd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/stat/fstatat.c | __wasi_path_filestat_get |
| __wasilibc_nocwd_linkat | wasi/libc-nocwd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/unistd/linkat.c | __wasi_path_link |
| __wasilibc_nocwd_mkdirat_nomode | wasi/libc-nocwd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/stat/mkdirat.c | __wasi_path_create_directory |
| __wasilibc_nocwd_openat_nomode | wasi/libc-nocwd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/fcntl/openat.c | __wasi_fd_fdstat_get, __wasi_path_open2 |
| __wasilibc_nocwd_opendirat | wasi/libc-nocwd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/dirent/opendirat.c | __wasi_fd_close, __wasi_fd_fdstat_get, __wasi_fd_readdir, __wasi_path_open2 |
| __wasilibc_nocwd_readlinkat | wasi/libc-nocwd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/unistd/readlinkat.c | __wasi_path_readlink |
| __wasilibc_nocwd_renameat | wasi/libc-nocwd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/stdio/renameat.c | __wasi_path_rename |
| __wasilibc_nocwd_symlinkat | wasi/libc-nocwd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/unistd/symlinkat.c | __wasi_path_symlink |
| __wasilibc_nocwd_utimensat | wasi/libc-nocwd.h | wasix-libc/libc-bottom-half/cloudlibc/src/libc/sys/stat/utimensat.c | __wasi_path_filestat_set_times |
| __wasilibc_open_nomode | wasi/libc.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_fd_fdstat_get, __wasi_getcwd, __wasi_path_open2 |
| __wasilibc_rename_newat | wasi/libc.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_getcwd, __wasi_path_rename |
| __wasilibc_rename_oldat | wasi/libc.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_getcwd, __wasi_path_rename |
| __wasilibc_rmdirat | wasi/libc.h | wasix-libc/libc-bottom-half/sources/at_fdcwd.c | __wasi_getcwd, __wasi_path_remove_directory |
| __wasilibc_setjmp | wasi/libc.h | wasix-libc/libc-bottom-half/sources/__wasilibc_stack.c | __wasi_stack_checkpoint |
| __wasilibc_stat | wasi/libc.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_getcwd, __wasi_path_filestat_get |
| __wasilibc_unlinkat | wasi/libc.h | wasix-libc/libc-bottom-half/sources/at_fdcwd.c | __wasi_getcwd, __wasi_path_unlink_file |
| __wasilibc_utimens | wasi/libc.h | wasix-libc/libc-bottom-half/sources/posix.c | __wasi_getcwd, __wasi_path_filestat_set_times |
| wasix_call_dynamic | wasix/call_dynamic.h | wasix-libc/libc-top-half/musl/src/wasix/call_dynamic.c | __wasi_call_dynamic |
| wasix_closure_allocate | wasix/closure.h | wasix-libc/libc-top-half/musl/src/wasix/closure_allocate.c | __wasi_closure_allocate |
| wasix_closure_free | wasix/closure.h | wasix-libc/libc-top-half/musl/src/wasix/closure_free.c | __wasi_closure_free |
| wasix_closure_prepare | wasix/closure.h | wasix-libc/libc-top-half/musl/src/wasix/closure_prepare.c | __wasi_closure_prepare |
| wasix_context_destroy | wasix/context.h | wasix-libc/libc-top-half/musl/src/wasix/context.c | __wasi_context_destroy |
| wasix_context_switch | wasix/context.h | wasix-libc/libc-top-half/musl/src/wasix/context.c | __wasi_context_switch |
| wasix_reflect_signature | wasix/reflection.h | wasix-libc/libc-top-half/musl/src/wasix/reflection.c | __wasi_reflect_signature |

## PROMPT



Great! Now let's find port all the tests for the Control Flow/Setjmp category from /Users/fessguid/Projects/wasix-libc-tests/WASIX-libc-wasi-usage-categories.md document. 


Reminding again: 
- Find all the Functions in that specific category that are listed in a document
- across all the projects (EVERYTHING inside /Users/fessguid/Projects/wasix-libc-tests - all folders, excluding wasix-libc itself) find ALL the tests for those functions and cateogry. 
- Then analyse which logic each test tests, remove the duplicates, then port all of those into C integration test similar to what we have already
- Then run all those tests
- Then give me the overview of what you've added and which are working and which are not. 

The task is to verify the quality of our libc implementation  - so don't exclude the test if that is not working and not try to bend the test for the WASIX, we are validating it and if we've found an issue in WASIX then it's great. Test IS NOT CONSIDERED A TEST IF IT HAS NO ASSERTS, PORT ALL THE CUSTOM CHECK MACROSES TO assert. 

In the end update the document with covered category with the progress about ported test and short summary
