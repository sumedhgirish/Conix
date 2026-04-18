#include "commands.h"
#include "common.h"
#include "containers.h"
#include "process.h"

#include <stdlib.h>
#include <uuid/uuid.h>

void *create_container(void *arg)
{
    create_command_t *cmd = (create_command_t *) arg;

    container_record_t *record = NULL;
    create_container_record(&record, cmd->tag, cmd->image_path);

    char uuid_str[37];
    uuid_unparse(record->container.label.id, uuid_str);

    LOG(INFO, "Created container '%s' (%s)", cmd->tag, uuid_str);

    return NULL;
}

void *start_container(void *arg)
{
    start_command_t *cmd = (start_command_t *) arg;

    container_record_t *record = NULL;
    int exists = generic_container_find(&cmd->id, &record);

    if (exists < 0)
    {
        LOG(ERROR, "Container with specified ID or Tag not found");
        return NULL;
    }

    switch (record->container.status)
    {
        case CONTAINER_EXITED:
        {
            free(record->container.process.command);
            logger_destroy(&record->container.logger);
        }
        case CONTAINER_CREATED:
            break;
        default:
            LOG(ERROR, "Container is not in a state that can be started");
            return NULL;
    }

    // Reset the process status
    memset(&record->container.process, 0, sizeof(process_t));

    // Update the container record with the command to be executed
    record->container.process.command = strdup(cmd->command);

    // Initialize the logger interface for the container
    log_interface_t child_interface;
    logger_init(&record->container.logger, &child_interface);

    char uuid_str[37];
    uuid_unparse(record->container.label.id, uuid_str);

    LOG(INFO, "Starting container %s (%s) with command: %s", uuid_str,
        record->container.label.tag, cmd->command);

    record->container.status = CONTAINER_RUNNING;
    run_process(record, child_interface);

    return NULL;
}
