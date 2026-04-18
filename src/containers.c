#include "containers.h"
#include "logger.h"
#include "utils.h"

#include <stdlib.h>

container_buffer_t containers = {
    .head = NULL,           // Initialize the head of the linked list to NULL
    .num_containers = 0,    // Initialize the number of containers to 0
    .sync.reader_count = 0, // Initialize the reader count to 0
};

int create_container_record(container_record_t **record, char *tag,
                            container_fs_path_t image_path)
{
    container_record_t *new_record = malloc(sizeof(container_record_t));
    if (new_record == NULL)
    {
        *record = NULL;
        LOG(ERROR, "Failed to allocate memory for container record");
        return -1;
    }

    // Initialize the container record
    memset(new_record, 0, sizeof(container_record_t));

    // Generate a unique ID for the container
    uuid_generate(new_record->container.label.id);

    char uuid_buf[37];
    uuid_unparse(new_record->container.label.id, uuid_buf);

    // Populate the tag
    new_record->container.label.tag = strdup(tag);

    // Set the initial status of the container
    new_record->container.status = CONTAINER_CREATED;

    // Create log directory
    char *log_dir;
    CONTAINER_LOG_PATH(log_dir, uuid_buf, "");
    mkdir_p(log_dir, 0755);
    free(log_dir);

    // Open log file
    char *log_file;
    CONTAINER_LOG_PATH(log_file, uuid_buf, "output.log");
    new_record->container.logger.log_file = fopen(log_file, "w");

    // Init fs paths
    new_record->container.fs.image_path = strdup(image_path);
    fs_create(&new_record->container.fs, new_record->container.label.id);

    write_lock();

    new_record->next = containers.head;
    containers.head = new_record;
    containers.num_containers += 1;

    *record = new_record;

    write_unlock();

    return 0;
}

int find_container_by_id(const uuid_t id, container_record_t **record)
{
    read_lock();
    container_record_t *current = containers.head;
    while (current != NULL)
    {
        if (uuid_compare(current->container.label.id, id) == 0)
        {
            *record = current;
            read_unlock();
            return 0;
        }
        current = current->next;
    }
    *record = NULL;
    read_unlock();
    return -1; // Container not found
}

int find_container_by_tag(const char *tag, container_record_t **record)
{
    read_lock();
    container_record_t *current = containers.head;
    while (current != NULL)
    {
        if (strcmp(current->container.label.tag, tag) == 0)
        {
            *record = current;
            read_unlock();
            return 0;
        }
        current = current->next;
    }
    *record = NULL;
    read_unlock();
    return -1; // Container not found
}
