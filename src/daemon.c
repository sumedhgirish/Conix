#include "commands.h"
#include "engine.h"
#include <stdlib.h>

int main(int argc, char *argv[])
{
    create_command_t create_cmd = {
        .image_path = "/home/crownedhog/Code/Conix/containers/alpine",
        .tag_len = 4,
        .tag = "test",
    };

    LOG(INFO, "Creating container with tag: %s and image path: %s",
        create_cmd.tag, create_cmd.image_path);

    create_container(&create_cmd);

    const char *cmd = "ls -la && exit";
    start_command_t *start_cmd =
        malloc(sizeof(start_command_t) + strlen(cmd) + 1);

    start_cmd->id.id_t = TAG;
    strncpy(start_cmd->id.data.tag, "test", TAG_MAX);
    strncpy(start_cmd->command, cmd, strlen(cmd) + 1);

    LOG(INFO, "Starting container with tag: %s and command: %s",
        start_cmd->id.data.tag, start_cmd->command);

    start_container(start_cmd);

    free(start_cmd);

    return 0;
}
