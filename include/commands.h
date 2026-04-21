#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include "common.h"
#include "containers.h"

#include <stdint.h>

// ------------------------------- PROTOCOL -----------------------------------

#define IPC_SOCKET_PATH CONIX_GLOBAL_PREFIX "/conix.sock"
#define N_BACKLOG 5

typedef enum
{
    // request
    OP_CREATE = 1,
    OP_START,
    OP_STOP,

    // util
    QRY_LIST,

    // response
    STAT_OK = 0,
    STAT_ERR = -1
} ipc_opcode_t;

typedef struct
{
    ipc_opcode_t opcode;
    size_t payload_size;
} ipc_header_t;

// ------------------------------- BODY ---------------------------------------

typedef enum
{
    UUID = 0,
    TAG
} identifier_t;

typedef struct
{
    identifier_t id_t;
    union
    {
        uuid_t id;
        char tag[TAG_MAX];
    } data;
} id_wireframe_t;

typedef struct
{
    container_fs_path_t image_path;
    char tag[TAG_MAX];
} create_command_t;

typedef struct
{
    id_wireframe_t id;
    size_t cmd_len;
    char cmd[];
} start_command_t;

typedef struct
{
    id_wireframe_t label;
} stop_command_t;

// ------------------------------- UTIL ---------------------------------------

int ipc_listen(void);
int ipc_connect(FILE **server_file);
int ipc_accept(int server_fd, FILE **client_file);

int command_recv_reply(ipc_header_t *header, void **payload, FILE *server_file);
int command_send_create(FILE *server_file, create_command_t *cmd);
int command_send_start(FILE *server_file, start_command_t *cmd);
int command_send_stop(FILE *server_file, stop_command_t *cmd);

static inline int invalid_command(FILE *server_file, void *cmd)
{
    (void) server_file;
    (void) cmd;

    fprintf(stderr, "Invalid command type\n");

    return -1;
}

#define command_send(stream, cmd)                                              \
    _Generic((cmd),                                                            \
        create_command_t *: command_send_create,                               \
        start_command_t *: command_send_start,                                 \
        stop_command_t *: command_send_stop,                                   \
        default: invalid_command)(stream, cmd)

int command_recv(ipc_header_t *header, void **payload, FILE *client_file);
int command_reply_ok(FILE *client_file, void *data, size_t len);
int command_reply_err(FILE *client_file, const char *err_msg);

#endif
