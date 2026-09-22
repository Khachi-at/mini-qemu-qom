#ifndef MEMORY_REGION_H
#define MEMORY_REGION_H

#include <stdint.h>

#include "miniqom/error.h"

typedef uint64_t (*MemoryRegionRead)(void *opaque,
                                     uint64_t offset,
                                     unsigned size);

typedef bool (*MemoryRegionWrite)(void *opaque,
                                  uint64_t offset,
                                  unsigned size,
                                  uint64_t value,
                                  Error *err);

typedef struct MemoryRegionOps
{
    MemoryRegionRead read;
    MemoryRegionWrite write;
} MemoryRegionOps;

typedef struct MemoryRegion
{
    uint64_t size;
    const MemoryRegionOps *ops;
    void *opaque;
} MemoryRegion;

void memory_region_init_io(MemoryRegion *region,
                           uint64_t size,
                           const MemoryRegionOps *ops,
                           void *opaque);

bool memory_region_read(MemoryRegion *region,
                        uint64_t offset,
                        unsigned size,
                        uint64_t *value,
                        Error *err);

bool memory_region_write(MemoryRegion *region,
                         uint64_t offset,
                         unsigned size,
                         uint64_t value,
                         Error *err);

#endif