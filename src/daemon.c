#include "commands.h"
#include "common.h"
#include "engine.h"
#include "utils.h"

#include <pthread.h>
#include <signal.h>
#include <stdlib.h>

typedef struct
{
    ipc_opcode_t opcode;
    handler_t handler;
    size_t min_payload;
} dispatch_entry_t;

static const dispatch_entry_t dispatch_table[] = {
    // Operations
    {OP_CREATE, create_container, sizeof(create_command_t)},
    {OP_START, start_container, sizeof(start_command_t)},
    {OP_STOP, stop_container, sizeof(stop_command_t)},

    // Queries
    {QRY_LIST, list_containers, 0},
};

#define DISPATCH_TABLE_LEN (sizeof(dispatch_table) / sizeof(dispatch_entry_t))

typedef struct
{
    FILE *stream;
} conn_args_t;

static void *handle_connection(void *arg)
{
    conn_args_t *conn = (conn_args_t *) arg;
    FILE *stream = conn->stream;
    free(conn);

    ipc_header_t header;
    void *payload = NULL;

    if (command_recv(&header, &payload, stream) < 0)
        goto done;

    const dispatch_entry_t *entry = NULL;
    for (size_t i = 0; i < DISPATCH_TABLE_LEN; i++)
    {
        if (dispatch_table[i].opcode == header.opcode)
        {
            entry = &dispatch_table[i];
            break;
        }
    }

    if (entry == NULL)
    {
        LOG(WARN, "Unknown opcode %d", header.opcode);
        command_reply_err(stream, "unknown opcode");
        goto done;
    }

    if (header.payload_size < entry->min_payload)
    {
        LOG(ERROR, "opcode %d: payload too short (%zu < %zu)", header.opcode,
            header.payload_size, entry->min_payload);
        command_reply_err(stream, "malformed payload");
        goto done;
    }

    void *reply = NULL;
    size_t reply_len = 0;

    (entry->handler(payload, &reply, &reply_len) == 0)
        ? command_reply_ok(stream, reply, reply_len)
        : command_reply_err(stream, "command failed");

    free(reply);

done:
    free(payload);
    fclose(stream);
    return NULL;
}

static int g_server_fd = -1;

static void on_signal(int sig)
{
    (void) sig;

    LOG(INFO, "Shutting down conixd");

    if (g_server_fd >= 0)
        close(g_server_fd);

    unlink(IPC_SOCKET_PATH);

    _exit(0);
}

int main(void)
{
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    signal(SIGPIPE, SIG_IGN);

    mkdir_p(CONIX_GLOBAL_PREFIX, 0755);

    g_server_fd = ipc_listen();
    if (g_server_fd < 0)
        return 1;

    LOG(INFO, "Conixd ready");

    for (;;)
    {
        FILE *stream = NULL;
        if (ipc_accept(g_server_fd, &stream) < 0)
            continue;

        conn_args_t *args = malloc(sizeof(conn_args_t));
        if (args == NULL)
        {
            LOG(ERROR, "Out of memory allocating conn_args");
            fclose(stream);
            continue;
        }

        args->stream = stream;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_connection, args) < 0)
        {
            LOG(ERROR, "pthread_create failed");
            free(args);
            fclose(stream);
            continue;
        }

        pthread_detach(tid);
    }

    return 0;
}
