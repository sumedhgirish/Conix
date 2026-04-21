#include "commands.h"
#include "common.h"
#include "containers.h"
#include "process.h"

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <uuid/uuid.h>

static int generic_container_find(id_wireframe_t *id,
                                  container_record_t **record)
{
    switch (id->id_t)
    {
        case UUID:
            return find_container_by_id(id->data.id, record);
        case TAG:
            return find_container_by_tag(id->data.tag, record);
        default:
            return -1;
    }
}

int create_container(void *input, void **payload, size_t *payload_len)
{
    create_command_t *cmd = (create_command_t *) input;

    container_record_t *record = NULL;
    create_container_record(&record, cmd->tag, cmd->image_path);

    char uuid_str[37];
    uuid_unparse(record->container.label.id, uuid_str);

    LOG(INFO, "Created container '%s' (%s)", cmd->tag, uuid_str);

    *payload = NULL;
    *payload_len = 0;

    return 0;
}

int start_container(void *input, void **payload, size_t *payload_len)
{
    start_command_t *cmd = (start_command_t *) input;

    container_record_t *record = NULL;
    int exists = generic_container_find(&cmd->id, &record);

    if (exists < 0)
    {
        LOG(ERROR, "Could not find container with given tag/uuid");
        return -1;
    }

    char uuid_buf[37];
    uuid_unparse(record->container.label.id, uuid_buf);

    // Open log file
    char *log_file;
    CONTAINER_LOG_PATH(log_file, uuid_buf, "start.log");
    record->container.logger.log_file = fopen(log_file, "w");

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
            return -1;
    }

    // Reset the process status
    memset(&record->container.process, 0, sizeof(process_t));

    // Update the container record with the command to be executed
    record->container.process.command = strdup(cmd->cmd);

    // Initialize the logger interface for the container
    log_interface_t child_interface;
    logger_init(&record->container.logger, &child_interface);

    char uuid_str[37];
    uuid_unparse(record->container.label.id, uuid_str);

    LOG(INFO, "Starting container '%s' (%s) with command: %s",
        record->container.label.tag, uuid_str, cmd->cmd);

    record->container.status = CONTAINER_RUNNING;
    run_process(record, child_interface);

    // Clean up logger interface
    fclose(record->container.logger.log_file);

    *payload = NULL;
    *payload_len = 0;

    return 0;
}

int stop_container(void *input, void **payload, size_t *payload_len)
{
    stop_command_t *cmd = input;

    container_record_t *record = NULL;
    if (generic_container_find(&cmd->label, &record) < 0)
    {
        LOG(ERROR, "Could not find container with given tag/uuid");
        return -1;
    }

    switch (record->container.status)
    {
        case CONTAINER_RUNNING:
        case CONTAINER_PAUSED:
            break;
        default:
            LOG(WARN, "Container is not running");
            return 0;
    }

    if (kill(record->container.process.pid, SIGKILL) < 0)
    {
        LOG(ERROR, "Failed to send SIGTERM: %s", strerror(errno));
        return -1;
    }

    char uuid_str[37];
    uuid_unparse(record->container.label.id, uuid_str);

    LOG(INFO, "Sent SIGKILL to container '%s' (%s)",
        record->container.label.tag, uuid_str);

    *payload = NULL;
    *payload_len = 0;

    return 0;
}
