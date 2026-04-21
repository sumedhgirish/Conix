#ifndef __ENGINE_H__
#define __ENGINE_H__

#include <stddef.h>

typedef int (*handler_t)(void *input, void **payload, size_t *payload_len);

int create_container(void *input, void **payload, size_t *payload_len);
int start_container(void *input, void **payload, size_t *payload_len);
int stop_container(void *input, void **payload, size_t *payload_len);

#endif
