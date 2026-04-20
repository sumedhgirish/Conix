#include "commands.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

static void setup_ipc_addr(struct sockaddr_un *addr)
{
    memset(addr, 0, sizeof(struct sockaddr_un));
    addr->sun_family = AF_UNIX;
    strncpy(addr->sun_path, IPC_SOCKET_PATH, sizeof(addr->sun_path) - 1);
}

static FILE *wrap_socket(int fd, const char *mode)
{
    FILE *file = fdopen(fd, mode);
    if (!file)
    {
        LOG(ERROR, "Failed to wrap socket: %s", strerror(errno));
        close(fd);
        return NULL;
    }
    setvbuf(file, NULL, _IONBF, 0);
    return file;
}

int ipc_listen(void)
{
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        LOG(ERROR, "Failed to create socket: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_un addr;
    setup_ipc_addr(&addr);

    unlink(IPC_SOCKET_PATH);

    if (bind(server_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
        LOG(ERROR, "Failed to bind socket: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    chmod(IPC_SOCKET_PATH, 0600);

    if (listen(server_fd, N_BACKLOG) < 0)
    {
        LOG(ERROR, "Failed to listen on socket: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    LOG(INFO, "IPC server listening on %s", IPC_SOCKET_PATH);
    return server_fd;
}

int ipc_accept(int server_fd, FILE **client_file)
{
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0)
    {
        LOG(ERROR, "Failed to accept connection: %s", strerror(errno));
        return -1;
    }

    FILE *file = wrap_socket(client_fd, "r+");
    if (!file)
    {
        return -1;
    }
    *client_file = file;

    return 0;
}

int ipc_connect(FILE **server_file)
{
    int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd < 0)
    {
        LOG(ERROR, "Failed to create socket: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_un addr;
    setup_ipc_addr(&addr);
    if (connect(client_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
        LOG(ERROR, "Failed to connect to server: %s", strerror(errno));
        close(client_fd);
        return -1;
    }

    FILE *file = wrap_socket(client_fd, "r+");
    if (!file)
    {
        return -1;
    }
    *server_file = file;

    return 0;
}

// ------------------------------- RUNNER ------------------------------------

int command_send_create(FILE *server_file, create_command_t *cmd)
{
    ipc_header_t header = {.opcode = OP_CREATE,
                           .payload_size = sizeof(create_command_t)};

    if (fwrite(&header, sizeof(header), 1, server_file) != 1 ||
        fwrite(cmd, sizeof(create_command_t), 1, server_file) != 1)
    {
        LOG(ERROR, "Failed to send create command: %s", strerror(errno));
        return -1;
    }

    fflush(server_file);
    return 0;
}

int command_send_start(FILE *server_file, start_command_t *cmd)
{
    ipc_header_t header = {.opcode = OP_START,
                           .payload_size =
                               sizeof(start_command_t) + cmd->cmd_len};

    if (fwrite(&header, sizeof(header), 1, server_file) != 1 ||
        fwrite(cmd, sizeof(start_command_t), 1, server_file) != 1 ||
        fwrite(cmd->cmd, sizeof(char), cmd->cmd_len, server_file) !=
            cmd->cmd_len)
    {
        LOG(ERROR, "Failed to send start command: %s", strerror(errno));
        return -1;
    }

    fflush(server_file);
    return 0;
}

int command_recv_reply(ipc_header_t *header, void **payload, FILE *server_file)
{
    if (fread(header, sizeof(ipc_header_t), 1, server_file) != 1)
    {
        LOG(ERROR, "Failed to read reply header: %s", strerror(errno));
        return -1;
    }

    *payload = NULL;

    if (header->payload_size == 0)
        return 0;

    void *data = malloc(header->payload_size);
    if (!data)
    {
        LOG(ERROR, "Failed to allocate memory for reply payload");
        return -1;
    }

    if (fread(data, header->payload_size, 1, server_file) != 1)
    {
        LOG(ERROR, "Failed to read reply payload: %s", strerror(errno));
        free(data);
        return -1;
    }
    *payload = data;

    return 0;
}

// ------------------------------- DAEMON ------------------------------------

int command_recv(ipc_header_t *header, void **payload, FILE *client_file)
{
    if (fread(header, sizeof(ipc_header_t), 1, client_file) != 1)
    {
        LOG(ERROR, "Failed to read command header: %s", strerror(errno));
        return -1;
    }

    *payload = NULL;

    if (header->payload_size == 0)
        return 0;

    void *data = malloc(header->payload_size);
    if (!data)
    {
        LOG(ERROR, "Failed to allocate memory for command payload");
        return -1;
    }

    if (fread(data, header->payload_size, 1, client_file) != 1)
    {
        LOG(ERROR, "Failed to read command payload: %s", strerror(errno));
        free(data);
        return -1;
    }
    *payload = data;

    return 0;
}

int command_reply_ok(FILE *client_file, void *data, size_t len)
{
    ipc_header_t header = {.opcode = STAT_OK, .payload_size = len};

    if (fwrite(&header, sizeof(header), 1, client_file) != 1 ||
        fwrite(data, sizeof(char), len, client_file) != len)
    {
        LOG(ERROR, "Failed to send OK reply: %s", strerror(errno));
        return -1;
    }

    fflush(client_file);
    return 0;
}

int command_reply_err(FILE *client_file, const char *err_msg)
{
    ipc_header_t header = {.opcode = STAT_ERR, .payload_size = strlen(err_msg)};

    if (fwrite(&header, sizeof(header), 1, client_file) != 1 ||
        fwrite(err_msg, sizeof(char), strlen(err_msg), client_file) !=
            strlen(err_msg))
    {
        LOG(ERROR, "Failed to send ERROR reply: %s", strerror(errno));
        return -1;
    }

    fflush(client_file);
    return 0;
}
