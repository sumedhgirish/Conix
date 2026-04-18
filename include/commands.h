#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include "common.h"
#include "containers.h"

#include <stdint.h>

#define TAG_MAX 128

typedef struct
{
    container_fs_path_t image_path;
    size_t tag_len;
    char tag[TAG_MAX];
} create_command_t;

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
    id_wireframe_t id;
    char command[];
} start_command_t;

int generic_container_find(id_wireframe_t *id, container_record_t **record);

#endif
