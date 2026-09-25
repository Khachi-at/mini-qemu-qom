#ifndef MINIQOM_ADDRESS_SPACE_H
#define MINIQOM_ADDRESS_SPACE_H

#include <stdbool.h>
#include <miniqom/memory_region.h>

typedef struct AddressSpace
{
    uint64_t base;
    MemoryRegion *region;
} AddressSpace;

bool address_space_translate(const AddressSpace *space,
                             uint64_t address, unsigned size,
                             uint64_t *offset, Error *err);

bool address_space_read(const AddressSpace *space,
                        uint64_t address,
                        unsigned size,
                        uint64_t *value,
                        Error *err);

bool address_space_write(const AddressSpace *space,
                         uint64_t address,
                         unsigned size,
                         uint64_t value,
                         Error *err);

#endif