#ifndef __PROCESS_H__
#define __PROCESS_H__

#include <stdbool.h>
#include <time.h>

typedef struct
{
    int pid;
    int exit_code;
    char *command;
    bool killed;
    struct
    {
        time_t start;
        time_t end;
    } time;
} process_t;

#define CLONE_FLAGS                                                            \
    CLONE_NEWUSER |     /* User NS    : user/group IDs */                      \
        CLONE_NEWPID |  /* PID NS     : process IDs */                         \
        CLONE_NEWNS |   /* Mount NS   : filesystem mounts */                   \
        CLONE_NEWUTS |  /* UTS NS     : hostname and NIS domain name */        \
        CLONE_NEWIPC |  /* IPC NS     : System V IPC/POSIX message queues */   \
        CLONE_NEWNET |  /* Network NS : network interfaces, routes, ports */   \
        CLONE_NEWCGROUP /* cgroup NS  : cgroup root directory */

#endif
