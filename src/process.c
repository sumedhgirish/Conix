#include "process.h"
#include "containers.h"
#include "filesystem.h"
#include "logger.h"

#include <fcntl.h>
#include <linux/sched.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

extern int close_range(unsigned int start_fd, unsigned int end_fd, int flags);

static void write_file(const char *path, const char *data)
{
    int fd = open(path, O_WRONLY);
    write(fd, data, strlen(data));
    close(fd);
}

static int start_child(void (*child_fn)(void *args), void *args)
{
    struct clone_args opts = {
        .flags = CLONE_FLAGS,
        .exit_signal = SIGCHLD,
        .stack = (uintptr_t) NULL,
        .stack_size = 0,
    };

    int pid = syscall(SYS_clone3, &opts, sizeof(opts));

    if (pid < 0)
        return -1;

    if (pid == 0)
    {
        child_fn(args);
        _exit(127);
    }

    return pid;
}

void write_user_maps(int pid)
{
    char path[64], buf[64];

    snprintf(path, sizeof(path), "/proc/%d/setgroups", pid);
    write_file(path, "deny");

    snprintf(buf, sizeof(buf), "0 %d 1", getuid());
    snprintf(path, sizeof(path), "/proc/%d/uid_map", pid);
    write_file(path, buf);

    snprintf(buf, sizeof(buf), "0 %d 1", getgid());
    snprintf(path, sizeof(path), "/proc/%d/gid_map", pid);
    write_file(path, buf);
}

typedef struct
{
    container_record_t *record;
    log_interface_t interface;
    int syncpipe;
} child_args_t;

static void child_fn(void *args)
{
    child_args_t *child_args = (child_args_t *) args;

    // Wait for parent signal
    char buf;
    read(child_args->syncpipe, &buf, 1);
    close(child_args->syncpipe);

    // Mount the container's filesystem
    fs_mount(&child_args->record->container.fs);

    // Connect the logger interface
    logger_attach_interface(&child_args->interface);

    // Close all unused file descriptors
    close_range(3, UINT32_MAX, 0);

    // Set the hostname to the container tag
    sethostname(child_args->record->container.label.tag,
                strlen(child_args->record->container.label.tag));

    // Make mounts private
    fs_isolate_root();

    // Pivot root to the container's filesystem
    pivot_root(child_args->record->container.fs.root);

    // Mount virtual filesystems
    fs_start_virtfs();

    // Exec the container entrypoint
    char *argv[] = {"/bin/sh", "-c",
                    child_args->record->container.process.command, NULL};
    char *envp[] = {"PATH=/bin:/usr/bin", "HOME=/root", NULL};
    execve("/bin/sh", argv, envp);
    perror("execve");

    return;
}

int run_process(container_record_t *record, log_interface_t interface)
{
    record->container.process.time.start = time(NULL);
    int sync_pipe[2];
    pipe(sync_pipe);

    child_args_t args = {0};
    args.record = record;
    args.interface = interface;
    args.syncpipe = sync_pipe[0];

    if ((record->container.process.pid = start_child(child_fn, &args)) <= 0)
        return -1;

    close(sync_pipe[0]); // Close the read end of the pipe in the parent
    logger_close_interface(&interface);

    write_user_maps(record->container.process.pid);

    pthread_t log_thread;
    pthread_create(&log_thread, NULL, logger, &record->container.logger);

    write(sync_pipe[1], "c", 1); // Signal the child to continue
    close(sync_pipe[1]);

    int status;
    waitpid(record->container.process.pid, &status, 0);
    record->container.process.time.end = time(NULL);

    pthread_join(log_thread, NULL);

    if (WIFEXITED(status))
    {
        record->container.process.killed = false;
        record->container.process.exit_code = WEXITSTATUS(status);
    }
    else if (WIFSIGNALED(status))
    {
        record->container.process.killed = true;
        record->container.process.exit_code = WTERMSIG(status);
    }

    record->container.status = CONTAINER_EXITED;

    return -1;
}
