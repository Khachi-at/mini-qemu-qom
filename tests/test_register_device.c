#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "miniqom/register_device.h"

static RegisterDevice *new_device(void)
{
    Object *obj = object_new(TYPE_REGISTER_DEVICE);

    assert(obj != NULL);
    assert(object_is_a(obj, TYPE_REGISTER_DEVICE));
    assert(object_is_a(obj, "object"));
    return (RegisterDevice *)obj;
}

static void assert_register_value(AddressSpace *space, uint64_t address,
                                  uint64_t expected)
{
    Error err = {0};
    uint64_t value = UINT64_MAX;

    assert(address_space_read(space, address, 4, &value, &err));
    assert(value == expected);
}

static void assert_device_unmapped(AddressSpace *space, uint64_t base)
{
    Error err = {0};
    uint64_t value = UINT64_MAX;

    assert(!address_space_read(space, base + 4, 4, &value, &err));
    assert(!strcmp(err.message, "address is not mapped"));
    assert(value == UINT64_MAX);
    error_clear(&err);
    assert(!address_space_write(space, base, 4, 99, &err));
    assert(!strcmp(err.message, "address is not mapped"));
}

static void test_lifecycle(void)
{
    Error err = {0};
    AddressSpace space;
    RegisterDevice *device = new_device();
    Object *obj = &device->parent_obj;

    address_space_init(&space);
    assert(!object_is_realized(obj));
    assert(register_device_configure(device, &space, 0x1000, &err));
    assert_device_unmapped(&space, 0x1000);

    assert(object_realize(obj, &err));
    assert(object_is_realized(obj));
    assert_register_value(&space, 0x1000, 0);
    assert_register_value(&space, 0x1004, 0x12345678);
    assert(address_space_write(&space, 0x1000, 4, 0xdeadbeef, &err));
    assert_register_value(&space, 0x1000, 0xdeadbeef);

    object_unrealize(obj);
    assert(!object_is_realized(obj));
    assert_device_unmapped(&space, 0x1000);
    object_unrealize(obj);
    assert_device_unmapped(&space, 0x1000);

    assert(object_realize(obj, &err));
    assert(object_is_realized(obj));
    assert_register_value(&space, 0x1004, 0x12345678);
    assert(address_space_write(&space, 0x1000, 4, 42, &err));
    assert_register_value(&space, 0x1000, 42);
    object_unrealize(obj);
    object_free(obj);
    assert_device_unmapped(&space, 0x1000);
}

static void test_read_only_status(void)
{
    Error err = {0};
    AddressSpace space;
    RegisterDevice *device = new_device();
    Object *obj = &device->parent_obj;

    address_space_init(&space);
    assert(register_device_configure(device, &space, 0x1000, &err));
    assert(object_realize(obj, &err));
    assert(address_space_write(&space, 0x1000, 4, 42, &err));
    assert(!address_space_write(&space, 0x1004, 4, 0xffffffff, &err));
    assert(!strcmp(err.message, "status register is read-only"));
    assert_register_value(&space, 0x1004, 0x12345678);
    assert_register_value(&space, 0x1000, 42);
    object_unrealize(obj);
    object_free(obj);
}

static void test_configuration_validation(void)
{
    Error err = {0};
    AddressSpace space;
    RegisterDevice *device = new_device();
    Object *obj = &device->parent_obj;

    address_space_init(&space);
    assert(!object_realize(obj, &err));
    assert(err.message[0] != '\0');
    assert(!object_is_realized(obj));

    error_clear(&err);
    assert(!register_device_configure(NULL, &space, 0x1000, &err));
    assert(err.message[0] != '\0');
    assert(register_device_configure(device, &space, 0x1000, &err));
    error_clear(&err);
    assert(!register_device_configure(device, NULL, 0x2000, &err));
    assert(err.message[0] != '\0');

    /* A rejected configuration must preserve the last valid configuration. */
    assert(object_realize(obj, &err));
    assert_register_value(&space, 0x1004, 0x12345678);
    assert_device_unmapped(&space, 0x2000);
    object_unrealize(obj);
    object_free(obj);
}

static void test_reconfiguration(void)
{
    Error err = {0};
    AddressSpace first_space;
    AddressSpace second_space;
    RegisterDevice *device = new_device();
    Object *obj = &device->parent_obj;

    address_space_init(&first_space);
    address_space_init(&second_space);
    assert(register_device_configure(device, &first_space, 0x1000, &err));
    assert(register_device_configure(device, &first_space, 0x2000, &err));
    assert_device_unmapped(&first_space, 0x1000);
    assert_device_unmapped(&first_space, 0x2000);
    assert(object_realize(obj, &err));
    assert_device_unmapped(&first_space, 0x1000);
    assert_register_value(&first_space, 0x2004, 0x12345678);

    error_clear(&err);
    assert(!register_device_configure(device, &second_space, 0x3000, &err));
    assert(err.message[0] != '\0');
    assert(object_is_realized(obj));
    assert_register_value(&first_space, 0x2004, 0x12345678);
    assert_device_unmapped(&second_space, 0x3000);
    object_unrealize(obj);
    assert_device_unmapped(&first_space, 0x2000);

    assert(register_device_configure(device, &second_space, 0x3000, &err));
    assert(object_realize(obj, &err));
    assert_device_unmapped(&first_space, 0x2000);
    assert_register_value(&second_space, 0x3004, 0x12345678);
    object_unrealize(obj);
    assert_device_unmapped(&second_space, 0x3000);
    object_free(obj);
}

static void test_zero_base_configuration_freeze(void)
{
    Error err = {0};
    AddressSpace first_space;
    AddressSpace second_space;
    RegisterDevice *device = new_device();
    Object *obj = &device->parent_obj;

    address_space_init(&first_space);
    address_space_init(&second_space);
    assert(register_device_configure(device, &first_space, 0, &err));
    assert(object_realize(obj, &err));
    assert(address_space_write(&first_space, 0, 4, 42, &err));

    assert(!register_device_configure(device, &second_space, 0x1000, &err));
    assert(!strcmp(err.message, "object is already realized"));
    error_clear(&err);
    assert(!register_device_configure(device, &first_space, 0, &err));
    assert(!strcmp(err.message, "object is already realized"));
    assert(object_is_realized(obj));
    assert_register_value(&first_space, 0, 42);
    assert_device_unmapped(&second_space, 0x1000);

    object_unrealize(obj);
    assert_device_unmapped(&first_space, 0);
    assert(register_device_configure(device, &second_space, 0x1000, &err));
    assert(object_realize(obj, &err));
    assert_register_value(&second_space, 0x1004, 0x12345678);
    object_unrealize(obj);
    object_free(obj);
    assert_device_unmapped(&first_space, 0);
    assert_device_unmapped(&second_space, 0x1000);
}

static void test_mapping_conflict(void)
{
    Error err = {0};
    AddressSpace space;
    RegisterDevice *first = new_device();
    RegisterDevice *second = new_device();
    Object *first_obj = &first->parent_obj;
    Object *second_obj = &second->parent_obj;

    address_space_init(&space);
    assert(register_device_configure(first, &space, 0x1000, &err));
    assert(object_realize(first_obj, &err));
    assert(address_space_write(&space, 0x1000, 4, 11, &err));

    assert(register_device_configure(second, &space, 0x1000, &err));
    error_clear(&err);
    assert(!object_realize(second_obj, &err));
    assert(!strcmp(err.message, "memory region overlaps existing mapping"));
    assert(!object_is_realized(second_obj));
    assert(object_is_realized(first_obj));
    assert_register_value(&space, 0x1000, 11);
    object_unrealize(second_obj);
    assert_register_value(&space, 0x1000, 11);

    /* The failed object can be configured again and used independently. */
    assert(register_device_configure(second, &space, 0x2000, &err));
    assert(object_realize(second_obj, &err));
    assert_register_value(&space, 0x2000, 0);
    assert(address_space_write(&space, 0x2000, 4, 22, &err));
    assert_register_value(&space, 0x1000, 11);
    assert_register_value(&space, 0x2000, 22);
    object_unrealize(second_obj);
    object_free(second_obj);
    assert_device_unmapped(&space, 0x2000);
    assert_register_value(&space, 0x1000, 11);
    object_unrealize(first_obj);
    object_free(first_obj);
    assert_device_unmapped(&space, 0x1000);
}

int main(int argc, char **argv)
{
    static const struct
    {
        const char *name;
        void (*run)(void);
    } cases[] = {
        {"lifecycle", test_lifecycle},
        {"read-only-status", test_read_only_status},
        {"configuration-validation", test_configuration_validation},
        {"reconfiguration", test_reconfiguration},
        {"zero-base-configuration-freeze", test_zero_base_configuration_freeze},
        {"mapping-conflict", test_mapping_conflict},
    };

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <test-name>\n", argv[0]);
        return 1;
    }

    type_system_init();
    register_device_register_types();
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        if (!strcmp(argv[1], cases[i].name))
        {
            cases[i].run();
            return 0;
        }
    }
    fprintf(stderr, "unknown test: %s\n", argv[1]);
    return 1;
}
