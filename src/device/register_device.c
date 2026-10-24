#include "miniqom/address_space.h"
#include "miniqom/error.h"
#include "miniqom/memory_region.h"
#include "miniqom/register_device.h"
#include "miniqom/type.h"

bool register_device_configure(RegisterDevice *device,
                               AddressSpace *space,
                               uint64_t base,
                               Error *err)
{
    if (!device || !space)
    {
        error_set(err, "invalid device or space");
        return false;
    }

    if (object_is_realized(&device->parent_obj))
    {
        error_set(err, "object is already realized");
        return false;
    }

    device->space = space;
    device->base = base;
    return true;
}

static uint64_t register_read(void *opaque,
                              uint64_t offset,
                              unsigned size)
{
    RegisterDevice *device = opaque;

    switch (offset)
    {
    case 0x00:
        return device->control;

    case 0x04:
        return device->status;

    default:
        return 0xFF;
    }
}

static bool register_write(void *opaque,
                           uint64_t offset,
                           unsigned size,
                           uint64_t value,
                           Error *err)
{
    RegisterDevice *device = opaque;

    switch (offset)
    {
    case 0x00:
        device->control = (uint32_t)value;
        return true;
    case 0x04:
        error_set(err, "status register is read-only");
        return false;

    default:
        error_set(err, "invalid register offset");
        return false;
    }
}

static const MemoryRegionOps register_ops = {
    .read = register_read,
    .write = register_write,
};

static void register_device_instance_init(Object *obj)
{
    RegisterDevice *device = (RegisterDevice *)obj;
    MemoryRegion region;

    device->control = 0;
    device->status = 0x12345678;

    memory_region_init_io(&region, 8, &register_ops, device);
    device->mmio = region;
}

static bool register_device_instance_realize(Object *obj, Error *err)
{
    RegisterDevice *device = (RegisterDevice *)obj;
    return address_space_add_region(device->space, device->base, &device->mmio, err);
}

static void register_device_instance_unrealize(Object *obj)
{
    RegisterDevice *device = (RegisterDevice *)obj;

    Error err;
    address_space_remove_region(device->space, device->base, &err);
}

static const TypeInfo register_device_info = {
    .name = TYPE_REGISTER_DEVICE,
    .parent = "object",
    .instance_size = sizeof(RegisterDevice),
    .instance_init = register_device_instance_init,
    .instance_realize = register_device_instance_realize,
    .instance_unrealize = register_device_instance_unrealize,
};

void register_device_register_types(void)
{
    type_register_static(&register_device_info);
}
