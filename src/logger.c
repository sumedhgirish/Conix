#include "logger.h"
#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define UnwrapStatus(result)                                                   \
    if (result < 0)                                                            \
    {                                                                          \
        LOG(ERROR, "Failed to create pipe: %s", strerror(errno));              \
        return -1;                                                             \
    }

int logger_init(logger_t *logger, log_interface_t *interface)
{
    int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];

    UnwrapStatus(pipe(stdin_pipe));
    UnwrapStatus(pipe(stdout_pipe));
    UnwrapStatus(pipe(stderr_pipe));

    interface->stdin = stdin_pipe[0];   // Read end of stdin
    interface->stdout = stdout_pipe[1]; // Write end of stdout
    interface->stderr = stderr_pipe[1]; // Write end of stderr

    logger->fd.stdin = stdin_pipe[1];   // Write end of stdin
    logger->fd.stdout = stdout_pipe[0]; // Read end of stdout
    logger->fd.stderr = stderr_pipe[0]; // Read end of stderr

    return 0;
}

int logger_destroy(logger_t *logger)
{
    close(logger->fd.stdin);
    close(logger->fd.stdout);
    close(logger->fd.stderr);
    return 0;
}

void logger_attach_interface(log_interface_t *interface)
{
    dup2(interface->stdin, STDIN_FILENO);
    dup2(interface->stdout, STDOUT_FILENO);
    dup2(interface->stderr, STDERR_FILENO);

    close(interface->stdin);
    close(interface->stdout);
    close(interface->stderr);
}

void logger_close_interface(log_interface_t *interface)
{
    close(interface->stdin);
    close(interface->stdout);
    close(interface->stderr);
}

typedef struct
{
    FILE *input_file;
    FILE *log_file;
} drain_t;

static void *drain(void *args)
{
    drain_t *drain = (drain_t *) args;

    char buffer[LOG_BUFFER_SIZE], ts[32];
    while (fgets(buffer, LOG_BUFFER_SIZE, drain->input_file))
    {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        strftime(ts, sizeof(ts), "%d/%m/%Y %H:%M:%SZ", tm_info);

        flockfile(drain->log_file);

        fprintf(drain->log_file, "[%32s] %s", ts, buffer);
        fflush(drain->log_file);

        funlockfile(drain->log_file);
    }

    return NULL;
}

void *logger(void *args)
{
    logger_t *logger = (logger_t *) args;

    drain_t output_drain;
    output_drain.log_file = logger->log_file;
    output_drain.input_file = fdopen(logger->fd.stdout, "r");

    drain_t error_drain;
    error_drain.log_file = logger->log_file;
    error_drain.input_file = fdopen(logger->fd.stderr, "r");

    pthread_create(&logger->thread.output, NULL, drain, &output_drain);
    pthread_create(&logger->thread.error, NULL, drain, &error_drain);

    pthread_join(logger->thread.output, NULL);
    pthread_join(logger->thread.error, NULL);

    fclose(output_drain.input_file);
    fclose(error_drain.input_file);

    return NULL;
}
