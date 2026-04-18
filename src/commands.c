#include "commands.h"
#include "containers.h"

int generic_container_find(id_wireframe_t *id, container_record_t **record)
{
    switch (id->id_t)
    {
        case UUID:
            return find_container_by_id(id->data.id, record);
        case TAG:
            return find_container_by_tag(id->data.tag, record);
        default:
            return -1;
    }
}
