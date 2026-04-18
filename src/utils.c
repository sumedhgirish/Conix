#include "utils.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/random.h>

int mkdir_p(const char *path, mode_t mode)
{
    if (!path || *path == '\0')
    {
        errno = EINVAL;
        return -1;
    }

    char buf[4096];
    if (snprintf(buf, sizeof(buf), "%s", path) >= (int) sizeof(buf))
    {
        errno = ENAMETOOLONG;
        return -1;
    }

    for (char *chr = buf + (buf[0] == '/'); *chr; chr++)
    {
        switch (*chr)
        {
            case '/':
                *chr = '\0';
                if (mkdir(buf, mode) != 0 && errno != EEXIST)
                    return -1;
                *chr = '/';
            default:
                continue;
        }
    }

    if (mkdir(buf, mode) != 0 && errno != EEXIST)
        return -1;

    return 0;
}
