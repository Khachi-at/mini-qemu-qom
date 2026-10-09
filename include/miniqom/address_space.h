#ifndef MINIQOM_ADDRESS_SPACE_H
#define MINIQOM_ADDRESS_SPACE_H

#include <stdbool.h>
#include <miniqom/memory_region.h>

#define MINIQOM_MAX_ADDRESS_SPACE_REGIONS 16

typedef struct AddressSpaceMapping
{
    uint64_t base;
    MemoryRegion *region;
} AddressSpaceMapping;

typedef struct AddressSpace
{
    AddressSpaceMapping mappings[MINIQOM_MAX_ADDRESS_SPACE_REGIONS];
    unsigned count;
} AddressSpace;

void address_space_init(AddressSpace *space);
bool address_space_add_region(AddressSpace *space,
                              uint64_t base,
                              MemoryRegion *region,
                              Error *err);

bool address_space_translate(const AddressSpace *space,
                             uint64_t address, unsigned size,
                             MemoryRegion **region,
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

bool address_space_remove_region(AddressSpace *space,
                                 uint64_t base,
                                 Error *err);

#endif