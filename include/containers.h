#ifndef __CONTAINERS_H__
#define __CONTAINERS_H__

#include "filesystem.h"
#include "logger.h"
#include "process.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <uuid/uuid.h>

typedef enum
{
    CONTAINER_CREATED = 0,
    CONTAINER_RUNNING,
    CONTAINER_PAUSED,
    CONTAINER_RESTARTING,
    CONTAINER_EXITED,
    CONTAINER_REMOVING,
    CONTAINER_DEAD
} container_status_t;

typedef struct
{
    uuid_t id;
    char tag[TAG_MAX];
} container_label_t;

typedef struct
{
    container_status_t status;
    container_label_t label;
    container_fs_t fs;
    logger_t logger;
    process_t process;
} container_t;

typedef struct container_record
{
    container_t container;
    struct container_record *next;
} container_record_t;

typedef struct
{
    pthread_mutex_t reader_mutex;
    pthread_mutex_t writer_mutex;
    int reader_count;
} rw_sync_t;

typedef struct
{
    container_record_t *head;
    size_t num_containers;
    rw_sync_t sync;
} container_buffer_t;

extern container_buffer_t containers;

typedef struct
{
    container_status_t status;
    container_label_t label;
} container_entry_t;

static inline void read_lock(void)
{
    pthread_mutex_lock(&containers.sync.reader_mutex);

    if (++containers.sync.reader_count == 1)
        pthread_mutex_lock(&containers.sync.writer_mutex);

    pthread_mutex_unlock(&containers.sync.reader_mutex);
}

static inline void read_unlock(void)
{
    pthread_mutex_lock(&containers.sync.reader_mutex);

    if (--containers.sync.reader_count == 0)
        pthread_mutex_unlock(&containers.sync.writer_mutex);

    pthread_mutex_unlock(&containers.sync.reader_mutex);
}

static inline void write_lock(void)
{
    pthread_mutex_lock(&containers.sync.writer_mutex);
}

static inline void write_unlock(void)
{
    pthread_mutex_unlock(&containers.sync.writer_mutex);
}

int create_container_record(container_record_t **record, char *tag,
                            container_fs_path_t image_path);
int find_container_by_id(const uuid_t id, container_record_t **record);
int find_container_by_tag(const char *tag, container_record_t **record);

int list_containers(void *null, void **payload, size_t *payload_len);

int run_process(container_record_t *record, log_interface_t interface);

#endif
