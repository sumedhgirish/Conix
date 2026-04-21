#include "commands.h"
#include "containers.h"

#include <argtable3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>

#define ipc_send_recv(cmd, reply_hdr, reply_payload)                           \
    ({                                                                         \
        FILE *_stream = NULL;                                                  \
        int _rc = -1;                                                          \
        if (ipc_connect(&_stream) == 0)                                        \
        {                                                                      \
            if (command_send(_stream, (cmd)) < 0)                              \
                fclose(_stream);                                               \
            else if (command_recv_reply((reply_hdr), (reply_payload),          \
                                        _stream) < 0)                          \
                fclose(_stream);                                               \
            else                                                               \
            {                                                                  \
                fclose(_stream);                                               \
                _rc = 0;                                                       \
            }                                                                  \
        }                                                                      \
        _rc;                                                                   \
    })

static int handle_reply(ipc_header_t *hdr, void *payload,
                        const char *success_msg)
{
    int rc;
    if (hdr->opcode == STAT_OK)
    {
        if (success_msg)
            printf("%s\n", success_msg);
        rc = 0;
    }
    else
    {
        fprintf(stderr, "error: %.*s\n", (int) hdr->payload_size,
                (char *) payload);
        rc = 1;
    }
    free(payload);
    return rc;
}

static int run_create(int argc, char *argv[])
{
    struct arg_file *image =
        arg_file1(NULL, NULL, "<image_path>", "path to the container image");

    struct arg_str *tag =
        arg_str1(NULL, NULL, "<tag>", "human-readable container tag");

    struct arg_lit *help = arg_lit0("h", "help", "print this help");

    struct arg_end *end = arg_end(20);

    void *argtable[] = {image, tag, help, end};

    int rc = 1;
    int errors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        printf("Usage: conix create");
        arg_print_syntax(stdout, argtable, "\n");
        arg_print_glossary(stdout, argtable, "  %-24s %s\n");
        rc = 0;
        goto done;
    }

    if (errors > 0)
    {
        arg_print_errors(stderr, end, "conix create");
        fprintf(stderr, "Usage: conix create");
        arg_print_syntax(stderr, argtable, "\n");
        goto done;
    }

    create_command_t cmd = {0};
    strncpy(cmd.image_path, image->filename[0], sizeof(cmd.image_path) - 1);
    strncpy(cmd.tag, tag->sval[0], TAG_MAX - 1);

    ipc_header_t hdr;
    void *payload = NULL;
    if (ipc_send_recv(&cmd, &hdr, &payload) < 0)
        goto done;

    char msg[TAG_MAX + 32];
    snprintf(msg, sizeof(msg), "Container '%s' created.", cmd.tag);
    rc = handle_reply(&hdr, payload, msg);

done:
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return rc;
}

static int parse_identifier(const char *str, id_wireframe_t *id)
{
    memset(id, 0, sizeof(*id));

    if (strlen(str) == 36 && uuid_parse(str, id->data.id) == 0)
    {
        id->id_t = UUID;
        return 0;
    }

    if (strlen(str) >= TAG_MAX)
    {
        fprintf(stderr, "error: tag exceeds maximum length %d\n", TAG_MAX);
        return -1;
    }

    id->id_t = TAG;
    strncpy(id->data.tag, str, TAG_MAX - 1);
    return 0;
}

static int run_start(int argc, char *argv[])
{
    struct arg_str *identifier =
        arg_str1(NULL, NULL, "<tag|uuid>", "container tag or UUID");
    struct arg_str *command = arg_str1(NULL, NULL, "<command>",
                                       "command to run inside the container");
    struct arg_lit *help = arg_lit0("h", "help", "print this help");
    struct arg_end *end = arg_end(20);

    void *argtable[] = {identifier, command, help, end};

    int rc = 1;
    int errors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        printf("Usage: conix start");
        arg_print_syntax(stdout, argtable, "\n");
        arg_print_glossary(stdout, argtable, "  %-24s %s\n");
        rc = 0;
        goto done;
    }

    if (errors > 0)
    {
        arg_print_errors(stderr, end, "conix start");
        fprintf(stderr, "Usage: conix start");
        arg_print_syntax(stderr, argtable, "\n");
        goto done;
    }

    id_wireframe_t id;
    if (parse_identifier(identifier->sval[0], &id) < 0)
        goto done;

    size_t cmd_len = strlen(command->sval[0]) + 1;
    start_command_t *cmd = malloc(sizeof(start_command_t) + cmd_len);

    if (!cmd)
    {
        fprintf(stderr, "error: out of memory\n");
        goto done;
    }

    cmd->id = id;
    cmd->cmd_len = cmd_len;
    memcpy(cmd->cmd, command->sval[0], cmd_len);

    ipc_header_t hdr;
    void *payload = NULL;
    int send_rc = ipc_send_recv(cmd, &hdr, &payload);
    free(cmd);

    if (send_rc < 0)
        goto done;

    rc = handle_reply(&hdr, payload, NULL);

done:
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return rc;
}

static int run_stop(int argc, char *argv[])
{
    struct arg_str *identifier =
        arg_str1(NULL, NULL, "<tag|uuid>", "container tag or UUID");
    struct arg_lit *help = arg_lit0("h", "help", "print this help");
    struct arg_end *end = arg_end(20);

    void *argtable[] = {identifier, help, end};

    int rc = 1;
    int errors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        printf("Usage: conix stop");
        arg_print_syntax(stdout, argtable, "\n");
        arg_print_glossary(stdout, argtable, "  %-24s %s\n");
        rc = 0;
        goto done;
    }

    if (errors > 0)
    {
        arg_print_errors(stderr, end, "conix stop");
        fprintf(stderr, "Usage: conix stop");
        arg_print_syntax(stderr, argtable, "\n");
        goto done;
    }

    stop_command_t cmd;
    if (parse_identifier(identifier->sval[0], &cmd.label) < 0)
        goto done;

    ipc_header_t hdr;
    void *payload = NULL;
    int send_rc = ipc_send_recv(&cmd, &hdr, &payload);

    if (send_rc < 0)
        goto done;

    rc = handle_reply(&hdr, payload, "Container stopped.");

done:
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return rc;
}

static const char *status_str(container_status_t s)
{
    switch (s)
    {
        case CONTAINER_CREATED:
            return "created";
        case CONTAINER_RUNNING:
            return "running";
        case CONTAINER_PAUSED:
            return "paused";
        case CONTAINER_RESTARTING:
            return "restarting";
        case CONTAINER_EXITED:
            return "exited";
        case CONTAINER_REMOVING:
            return "removing";
        case CONTAINER_DEAD:
            return "dead";
        default:
            return "unknown";
    }
}

static int run_list(int argc, char *argv[])
{
    struct arg_lit *help = arg_lit0("h", "help", "print this help");
    struct arg_end *end = arg_end(20);
    void *argtable[] = {help, end};

    int rc = 1;
    arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        printf("Usage: conix list");
        arg_print_syntax(stdout, argtable, "\n");
        arg_print_glossary(stdout, argtable, "  %-24s %s\n");
        rc = 0;
        goto done;
    }

    FILE *stream = NULL;
    if (ipc_connect(&stream) < 0)
        goto done;

    ipc_header_t qry = {.opcode = QRY_LIST, .payload_size = 0};
    if (fwrite(&qry, sizeof(qry), 1, stream) != 1)
    {
        LOG(ERROR, "Failed to send QRY_LIST");
        fclose(stream);
        goto done;
    }
    fflush(stream);

    ipc_header_t hdr;
    void *payload = NULL;
    if (command_recv_reply(&hdr, &payload, stream) < 0)
    {
        fclose(stream);
        goto done;
    }
    fclose(stream);

    if (hdr.opcode != STAT_OK)
    {
        fprintf(stderr, "error: %.*s\n", (int) hdr.payload_size,
                (char *) payload);
        free(payload);
        goto done;
    }

    size_t n = hdr.payload_size / sizeof(container_entry_t);
    container_entry_t *entries = (container_entry_t *) payload;

    if (n == 0)
        printf("No containers.\n");
    else
    {
        printf("%-36s  %-*s  %s\n", "UUID", 16, "TAG", "STATUS");
        for (size_t i = 0; i < n; i++)
        {
            char uuid_buf[37];
            uuid_unparse(entries[i].label.id, uuid_buf);
            printf("%-36s  %-*s  %s\n", uuid_buf, 16, entries[i].label.tag,
                   status_str(entries[i].status));
        }
    }

    free(payload);
    rc = 0;

done:
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return rc;
}

typedef struct
{
    const char *name;
    const char *description;
    int (*fn)(int argc, char *argv[]);
} subcommand_t;

static const subcommand_t subcommands[] = {
    {"create", "Create a new container", run_create},
    {"start", "Start a container", run_start},
    {"stop", "Stop a container", run_stop},
    {"list", "List all containers", run_list},
    {NULL, NULL, NULL}};

// ------------------------------ MAIN ----------------------------------------

static void print_usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <command> [options]\n\nCommands:\n", prog);

    for (const subcommand_t *sc = subcommands; sc->name != NULL; sc++)
        fprintf(stderr, "  %-10s %s\n", sc->name, sc->description);

    fprintf(stderr, "\nRun '%s <command> --help' for command-specific help.\n",
            prog);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        print_usage(argv[0]);
        return 1;
    }

    for (const subcommand_t *sc = subcommands; sc->name != NULL; sc++)
    {
        if (strcmp(argv[1], sc->name) == 0)
            return sc->fn(argc - 1, argv + 1);
    }

    fprintf(stderr, "conix: unknown command '%s'\n\n", argv[1]);
    print_usage(argv[0]);
    return 1;
}
