#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <pthread.h>
#include <stdio.h>

#define LOG_BUFFER_SIZE (4 * 1024)

#define CONTAINER_LOG_PATH(buffer, id, filename)                               \
    asprintf(&buffer, CONIX_GLOBAL_PREFIX "/containers/%s/logs/" filename, id)

typedef struct
{
    struct
    {
        int stdin;
        int stdout;
        int stderr;
    } fd;

    struct
    {
        pthread_t output;
        pthread_t error;
    } thread;

    FILE *log_file;
} logger_t;

typedef struct
{
    int stdin;
    int stdout;
    int stderr;
} log_interface_t;

int logger_init(logger_t *logger, log_interface_t *interface);
int logger_destroy(logger_t *logger);
void logger_attach_interface(log_interface_t *interface);
void logger_close_interface(log_interface_t *interface);

void *logger(void *args);

#endif
