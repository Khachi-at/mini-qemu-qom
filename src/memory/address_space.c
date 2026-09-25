#include "miniqom/address_space.h"

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