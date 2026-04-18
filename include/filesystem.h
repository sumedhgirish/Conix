#ifndef __FILESYSTEM_H__
#define __FILESYSTEM_H__

#include "common.h"

#include <limits.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <uuid/uuid.h>

#define CONTAINER_FS_PATH(buffer, id)                                          \
    snprintf(buffer, PATH_MAX, CONIX_GLOBAL_PREFIX "/containers/%s/fs", id)

#define CONTAINER_UPPER_FS_PATH(buffer, id)                                    \
    asprintf(&buffer, CONIX_GLOBAL_PREFIX "/cache/%s/upper", id)
#define CONTAINER_WORK_FS_PATH(buffer, id)                                     \
    asprintf(&buffer, CONIX_GLOBAL_PREFIX "/cache/%s/work", id)

typedef char container_fs_path_t[PATH_MAX];

typedef struct
{
    container_fs_path_t root;
    char *image_path;
    char *upper_dir;
    char *work_dir;
} container_fs_t;

static inline int fs_isolate_root()
{
    return mount("none", "/", NULL, MS_REC | MS_PRIVATE, NULL);
}

static inline void fs_start_virtfs()
{
    // Mount virtual filesystems
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("tmpfs", "/tmp", "tmpfs", 0, NULL);

    mount("tmpfs", "/dev", "tmpfs", 0, "mode=755");
    mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3));
    mknod("/dev/zero", S_IFCHR | 0666, makedev(1, 5));
    mknod("/dev/urandom", S_IFCHR | 0666, makedev(1, 9));
    mknod("/dev/tty", S_IFCHR | 0666, makedev(5, 0));
}

static inline int pivot_root(const char *new_root)
{
    if (chdir(new_root) < 0)
        return -1;

    if (syscall(SYS_pivot_root, ".", ".") < 0)
        return -1;

    // Old root is now mounted over new root — detach it
    if (umount2(".", MNT_DETACH) < 0)
        return -1;

    if (chdir("/") < 0)
        return -1;

    return 0;
}

int fs_create(container_fs_t *fs, uuid_t container_id);
int fs_mount(container_fs_t *fs);

#endif
