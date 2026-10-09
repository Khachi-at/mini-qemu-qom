#include <assert.h>

#include "miniqom/address_space.h"
#include "miniqom/memory_region.h"

void address_space_init(AddressSpace *space)
{
    assert(space != NULL);
    space->count = 0;
}

bool address_space_add_region(AddressSpace *space,
                              uint64_t base,
                              MemoryRegion *region,
                              Error *err)
{
    if (!space || !region || !region->size ||
        region->size - 1 > UINT64_MAX - base)
    {
        error_set(err, "invalid address space mapping");
        return false;
    }

    if (space->count >= MINIQOM_MAX_ADDRESS_SPACE_REGIONS)
    {
        error_set(err, "address space is full");
        return false;
    }

    for (size_t i = 0; i < space->count; i++)
    {
        const AddressSpaceMapping *mapping = &space->mappings[i];
        bool overlaps;

        /* Compare distances to avoid overflowing an end address. */
        if (base > mapping->base)
        {
            overlaps = base - mapping->base < mapping->region->size;
        }
        else
        {
            overlaps = mapping->base - base < region->size;
        }

        if (overlaps)
        {
            error_set(err, "memory region overlaps existing mapping");
            return false;
        }
    }

    space->mappings[space->count++] = (AddressSpaceMapping){
        .base = base,
        .region = region,
    };
    return true;
}

bool address_space_remove_region(AddressSpace *space,
                                 uint64_t base,
                                 Error *err)
{
    if (!space)
    {
        error_set(err, "invalid address space arguments");
        return false;
    }

    int matched_index = -1;

    for (size_t i = 0; i < space->count; i++)
    {
        if (space->mappings[i].base == base)
        {
            matched_index = i;
            break;
        }
    }

    if (matched_index == -1)
    {
        error_set(err, "mapping not found");
        return false;
    }

    for (size_t i = matched_index + 1; i < space->count; i++)
    {
        space->mappings[i - 1] = space->mappings[i];
    }
    space->count--;

    return true;
}

bool address_space_translate(const AddressSpace *space,
                             uint64_t address, unsigned size,
                             MemoryRegion **region,
                             uint64_t *offset, Error *err)
{
    if (!space || !region || !offset || !size)
    {
        error_set(err, "invalid address translation arguments");
        return false;
    }

    for (size_t i = 0; i < space->count; i++)
    {
        const AddressSpaceMapping *mapping = &space->mappings[i];

        if (address < mapping->base)
        {
            continue;
        }

        uint64_t local_offset = address - mapping->base;
        uint64_t length = mapping->region->size;

        if (local_offset >= length)
        {
            continue;
        }

        if (size > length - local_offset)
        {
            error_set(err, "memory access out of bounds");
            return false;
        }

        *region = mapping->region;
        *offset = local_offset;
        return true;
    }

    error_set(err, "address is not mapped");
    return false;
}

bool address_space_read(const AddressSpace *space,
                        uint64_t address,
                        unsigned size,
                        uint64_t *value,
                        Error *err)
{
    MemoryRegion *region;
    uint64_t offset;

    if (!address_space_translate(space, address, size, &region, &offset, err))
    {
        return false;
    }

    return memory_region_read(region, offset, size, value, err);
}

bool address_space_write(const AddressSpace *space,
                         uint64_t address,
                         unsigned size,
                         uint64_t value,
                         Error *err)
{
    MemoryRegion *region;
    uint64_t offset;

    if (!address_space_translate(space, address, size, &region, &offset, err))
    {
        return false;
    }

    return memory_region_write(region, offset, size, value, err);
}