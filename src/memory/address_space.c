#include "miniqom/address_space.h"
#include "miniqom/memory_region.h"

bool address_space_translate(const AddressSpace *space,
                             uint64_t address, unsigned size,
                             uint64_t *offset, Error *err)
{
    if (!space || !space->region || !offset || !size)
    {
        error_set(err, "invalid address translation arguments");
        return false;
    }

    uint64_t length = space->region->size;
    if (!length || length - 1 > UINT64_MAX - space->base ||
        address < space->base || address - space->base >= length ||
        size > length - (address - space->base))
    {
        error_set(err, "memory access out of bounds");
        return false;
    }

    *offset = address - space->base;
    return true;
}

bool address_space_read(const AddressSpace *space,
                        uint64_t address,
                        unsigned size,
                        uint64_t *value,
                        Error *err)
{
    uint64_t offset;

    if (!address_space_translate(space, address, size, &offset, err))
    {
        return false;
    }

    return memory_region_read(space->region, offset, size, value, err);
}

bool address_space_write(const AddressSpace *space,
                         uint64_t address,
                         unsigned size,
                         uint64_t value,
                         Error *err)
{
    uint64_t offset;

    if (!address_space_translate(space, address, size, &offset, err))
    {
        return false;
    }

    return memory_region_write(space->region, offset, size, value, err);
}