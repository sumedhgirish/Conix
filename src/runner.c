#include "commands.h"

#include <argtable3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>

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

    create_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    strncpy(cmd.image_path, image->filename[0], sizeof(cmd.image_path) - 1);
    strncpy(cmd.tag, tag->sval[0], TAG_MAX - 1);

    FILE *stream = NULL;
    if (ipc_connect(&stream) < 0)
        goto done;

    if (command_send(stream, &cmd) < 0)
    {
        fclose(stream);
        goto done;
    }

    ipc_header_t reply_hdr;
    void *reply_payload = NULL;
    if (command_recv_reply(&reply_hdr, &reply_payload, stream) < 0)
    {
        fclose(stream);
        goto done;
    }

    if (reply_hdr.opcode == STAT_OK)
    {
        printf("Container '%s' created.\n", cmd.tag);
        rc = 0;
    }
    else
    {
        fprintf(stderr, "error: %.*s\n", (int) reply_hdr.payload_size,
                (char *) reply_payload);
    }

    free(reply_payload);
    fclose(stream);

done:
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return rc;
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

    /* Resolve identifier — try UUID first, fall back to tag */
    id_wireframe_t id;
    memset(&id, 0, sizeof(id));

    if (strlen(identifier->sval[0]) == 36 &&
        uuid_parse(identifier->sval[0], id.data.id) == 0)
    {
        id.id_t = UUID;
    }
    else
    {
        if (strlen(identifier->sval[0]) >= TAG_MAX)
        {
            fprintf(stderr, "error: tag exceeds maximum length %d\n", TAG_MAX);
            goto done;
        }
        id.id_t = TAG;
        strncpy(id.data.tag, identifier->sval[0], TAG_MAX - 1);
    }

    size_t cmd_len = strlen(command->sval[0]) + 1;
    start_command_t *cmd = malloc(sizeof(start_command_t) + cmd_len);
    if (cmd == NULL)
    {
        fprintf(stderr, "error: out of memory\n");
        goto done;
    }

    cmd->id = id;
    cmd->cmd_len = cmd_len;
    memcpy(cmd->cmd, command->sval[0], cmd_len);

    FILE *stream = NULL;
    if (ipc_connect(&stream) < 0)
    {
        free(cmd);
        goto done;
    }

    if (command_send(stream, cmd) < 0)
    {
        free(cmd);
        fclose(stream);
        goto done;
    }

    free(cmd);

    ipc_header_t reply_hdr;
    void *reply_payload = NULL;
    if (command_recv_reply(&reply_hdr, &reply_payload, stream) < 0)
    {
        fclose(stream);
        goto done;
    }

    if (reply_hdr.opcode == STAT_OK)
        rc = 0;
    else
        fprintf(stderr, "error: %.*s\n", (int) reply_hdr.payload_size,
                (char *) reply_payload);

    free(reply_payload);
    fclose(stream);

done:
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return rc;
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s <command> [options]\n"
            "\n"
            "Commands:\n"
            "  create   Create a new container\n"
            "  start    Start a container\n"
            "\n"
            "Run '%s <command> --help' for command-specific help.\n",
            prog, prog);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        print_usage(argv[0]);
        return 1;
    }

    const char *subcmd = argv[1];

    if (strcmp(subcmd, "create") == 0)
        return run_create(argc - 1, argv + 1);
    if (strcmp(subcmd, "start") == 0)
        return run_start(argc - 1, argv + 1);

    fprintf(stderr, "conix: unknown command '%s'\n\n", subcmd);
    print_usage(argv[0]);
    return 1;
}
