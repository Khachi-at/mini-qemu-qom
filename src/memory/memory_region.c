#include <stdbool.h>
#include "miniqom/memory_region.h"

void memory_region_init_io(MemoryRegion *region,
                           uint64_t size,
                           const MemoryRegionOps *ops,
                           void *opaque)
{
    region->size = size;
    region->ops = ops;
    region->opaque = opaque;
}

bool memory_region_read(MemoryRegion *region,
                        uint64_t offset,
                        unsigned size,
                        uint64_t *value,
                        Error *err)
{
    if (offset + size > region->size)
    {
        error_set(err, "memory access out of bounds");
        return false;
    }

    if (region->ops == NULL || region->ops->read == NULL)
    {
        error_set(err, "memory region is not readable");
        return false;
    }

    if (value == NULL)
    {
        error_set(err, "invalid memory read arguments");
        return false;
    }

    *value = region->ops->read(region->opaque, offset, size);
    return true;
}

bool memory_region_write(MemoryRegion *region,
                         uint64_t offset,
                         unsigned size,
                         uint64_t value,
                         Error *err)
{
    if (offset + size > region->size)
    {
        error_set(err, "memory access out of bounds");
        return false;
    }

    if (region->ops == NULL || region->ops->write == NULL)
    {
        error_set(err, "memory region is not writable");
        return false;
    }

    return region->ops->write(region->opaque, offset, size, value, err);
}