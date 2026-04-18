#include "filesystem.h"
#include "utils.h"

#include <stdlib.h>

int fs_create(container_fs_t *fs, uuid_t container_id)
{
    char uuid[37];
    uuid_unparse(container_id, uuid);

    if (CONTAINER_UPPER_FS_PATH(fs->upper_dir, uuid) < 0)
    {
        LOG(ERROR, "Failed to load upper directory path");
        return -1;
    }

    if (CONTAINER_WORK_FS_PATH(fs->work_dir, uuid) < 0)
    {
        LOG(ERROR, "Failed to load work directory path");
        free(fs->upper_dir);
        fs->upper_dir = NULL;
        return -1;
    }

    CONTAINER_FS_PATH(fs->root, uuid);

    return 0;
}

int fs_mount(container_fs_t *fs)
{
    // Create the upper directory for the container
    if (mkdir_p(fs->upper_dir, 0755) < 0)
        return -1;

    // Create the work directory for the container
    if (mkdir_p(fs->work_dir, 0755) < 0)
        return -1;

    char *overlay_config = NULL;
    if (asprintf(&overlay_config, "lowerdir=%s,upperdir=%s,workdir=%s",
                 fs->image_path, fs->upper_dir, fs->work_dir) < 0)
    {
        LOG(ERROR, "Failed to allocate memory for overlay configuration");
        return -1;
    }

    // Create the merged directory for the container
    mkdir_p(fs->root, 0755);

    // Mount the overlay filesystem
    int status = mount("overlay", fs->root, "overlay", 0, overlay_config);

    free(overlay_config);

    if (status < 0)
        return -1;

    return 0;
}
