#ifndef MINIQOM_MEMORY_H
#define MINIQOM_MEMORY_H

#include <stdbool.h>
#include <stdint.h>

#include "miniqom/object.h"

#define TYPE_MEMORY_BACKEND "memory-backend"
#define TYPE_MEMORY_BACKEND_MEMFD "memory-backend-memfd"

typedef struct HostMemoryBackend
{
    Object parent_obj;

    uint64_t size;
    bool share;
    bool mmoc;
    char *swap_storage;
} HostMemoryBackend;

typedef struct HostMemoryBackendMemfd
{
    HostMemoryBackend parent_obj;

    bool seal;
    bool hugetlb;
} HostMemoryBackendMemfd;

void host_memory_backend_register_types(void);

#endif