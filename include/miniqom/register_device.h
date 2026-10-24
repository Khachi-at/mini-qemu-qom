#ifndef MINIQOM_REGISTER_DEVICE_H
#define MINIQOM_REGISTER_DEVICE_H

#include "miniqom/address_space.h"
#include "miniqom/error.h"
#include "miniqom/memory_region.h"
#include "miniqom/object.h"

#define TYPE_REGISTER_DEVICE "register-device"

typedef struct RegisterDevice
{
    Object parent_obj;

    uint32_t control;
    uint32_t status;

    MemoryRegion mmio;
    AddressSpace *space;
    uint64_t base;
} RegisterDevice;

void register_device_register_types(void);

bool register_device_configure(RegisterDevice *device,
                               AddressSpace *space,
                               uint64_t base,
                               Error *err);

#endif