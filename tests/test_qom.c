#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "miniqom/error.h"
#include "miniqom/memory.h"
#include "miniqom/object.h"
#include "miniqom/property.h"
#include "miniqom/type.h"
#include "miniqom/memory_region.h"
#include "miniqom/address_space.h"

#define TYPE_TEST_LIFECYCLE_PARENT "test-lifecycle-parent"
#define TYPE_TEST_LIFECYCLE_CHILD "test-lifecycle-child"
#define TYPE_TEST_LIFECYCLE_FAILING_CHILD \
    "test-lifecycle-failing-child"

typedef struct TestLifecycleObject
{
    Object parent_object;
} TestLifecycleObject;

static char lifecycle_events[16];
static size_t lifecycle_event_count;

static void lifecycle_events_reset(void)
{
    lifecycle_event_count = 0;
    lifecycle_events[0] = '\0';
}

static void lifecycle_event_add(char event)
{
    assert(lifecycle_event_count + 1 < sizeof(lifecycle_events));

    lifecycle_events[lifecycle_event_count++] = event;
    lifecycle_events[lifecycle_event_count] = '\0';
}

static bool test_parent_realize(Object *obj, Error *err)
{
    (void)obj;
    (void)err;

    lifecycle_event_add('P');
    return true;
}

static void test_parent_unrealize(Object *obj)
{
    (void)obj;

    lifecycle_event_add('p');
}

static bool test_child_realize(Object *obj, Error *err)
{
    (void)obj;
    (void)err;

    lifecycle_event_add('C');
    return true;
}

static void test_child_unrealize(Object *obj)
{
    (void)obj;

    lifecycle_event_add('c');
}

static bool test_failing_child_realize(Object *obj, Error *err)
{
    (void)obj;

    lifecycle_event_add('F');
    error_set(err, "child realize failed");
    return false;
}

static void test_failing_child_unrealize(Object *obj)
{
    (void)obj;

    lifecycle_event_add('x');
}

static const TypeInfo test_lifecycle_parent_info = {
    .name = TYPE_TEST_LIFECYCLE_PARENT,
    .parent = "object",
    .instance_size = sizeof(TestLifecycleObject),
    .instance_realize = test_parent_realize,
    .instance_unrealize = test_parent_unrealize,
};

static const TypeInfo test_lifecycle_child_info = {
    .name = TYPE_TEST_LIFECYCLE_CHILD,
    .parent = TYPE_TEST_LIFECYCLE_PARENT,
    .instance_size = sizeof(TestLifecycleObject),
    .instance_realize = test_child_realize,
    .instance_unrealize = test_child_unrealize,
};

static const TypeInfo test_lifecycle_failing_child_info = {
    .name = TYPE_TEST_LIFECYCLE_FAILING_CHILD,
    .parent = TYPE_TEST_LIFECYCLE_PARENT,
    .instance_size = sizeof(TestLifecycleObject),
    .instance_realize = test_failing_child_realize,
    .instance_unrealize = test_failing_child_unrealize,
};

static void test_lifecycle_register_types(void)
{
    type_register_static(&test_lifecycle_parent_info);
    type_register_static(&test_lifecycle_child_info);
    type_register_static(&test_lifecycle_failing_child_info);
}

static void test_type_queries(void)
{
    Object *obj = object_new(TYPE_MEMORY_BACKEND_MEMFD);

    assert(obj != NULL);
    assert(object_is_instance_of(obj, TYPE_MEMORY_BACKEND_MEMFD));
    assert(object_is_instance_of(obj, TYPE_MEMORY_BACKEND));
    assert(object_is_instance_of(obj, "object"));
    assert(!strcmp(object_get_type_name(obj),
                   TYPE_MEMORY_BACKEND_MEMFD));

    assert(object_is_a(obj, TYPE_MEMORY_BACKEND_MEMFD));
    assert(object_is_a(obj, TYPE_MEMORY_BACKEND));
    assert(object_is_a(obj, "object"));
    assert(!object_is_a(obj, "unknown-type"));
    assert(object_get_type_name(NULL) == NULL);
    assert(!object_is_a(NULL, "object"));

    object_free(obj);
}

static void test_memory_backend_defaults(void)
{
    HostMemoryBackendMemfd *backend;
    Object *obj = object_new(TYPE_MEMORY_BACKEND_MEMFD);

    assert(obj != NULL);

    backend = (HostMemoryBackendMemfd *)obj;
    assert(backend->seal);
    assert(backend->parent_obj.share);

    object_free(obj);
}

static void test_object_lifecycle(void)
{
    Error err;
    Object *obj = object_new(TYPE_MEMORY_BACKEND_MEMFD);

    assert(obj != NULL);
    error_clear(&err);

    assert(!object_is_realized(obj));

    assert(object_realize(obj, &err));
    assert(object_is_realized(obj));

    assert(!object_realize(obj, &err));
    assert(!strcmp(err.message, "object is already realized"));
    assert(object_is_realized(obj));

    object_unrealize(obj);
    assert(!object_is_realized(obj));

    object_unrealize(obj);
    assert(!object_is_realized(obj));

    assert(object_realize(obj, &err));
    assert(object_is_realized(obj));

    object_unrealize(obj);
    assert(!object_is_realized(obj));

    object_free(obj);
}

static void test_property_get(void)
{
    Error err;
    PropertyValue value;
    Object *obj = object_new(TYPE_MEMORY_BACKEND_MEMFD);

    assert(obj != NULL);
    error_clear(&err);

    assert(!object_property_set_from_string(obj, "swap-storage",
                                            "file:///swap", &err));
    assert(!strcmp(err.message, "swap-storage requires mmoc=on"));

    error_clear(&err);
    assert(object_property_set_from_string(obj, "mmoc", "on", &err));
    assert(object_property_set_from_string(obj, "swap-storage",
                                           "file:///swap", &err));
    assert(object_property_set_from_string(obj, "size", "2G", &err));

    assert(object_property_get(obj, "size", &value, &err));
    assert(value.u64 == 2ULL * 1024 * 1024 * 1024);

    assert(object_property_get(obj, "mmoc", &value, &err));
    assert(value.b);

    assert(object_property_get(obj, "share", &value, &err));
    assert(value.b);

    assert(object_property_get(obj, "swap-storage", &value, &err));
    assert(!strcmp(value.str, "file:///swap"));

    error_clear(&err);

    assert(!object_property_get(obj, "unknown-property", &value, &err));
    assert(!strcmp(err.message, "property not found"));

    object_free(obj);
}

static void test_property_freeze_after_realize(void)
{
    Error err;
    PropertyValue value;
    Object *obj = object_new(TYPE_MEMORY_BACKEND_MEMFD);

    assert(obj != NULL);
    error_clear(&err);

    assert(object_property_set_from_string(obj, "size", "2G", &err));

    assert(object_realize(obj, &err));
    assert(object_is_realized(obj));

    assert(!object_property_set_from_string(obj, "size", "4G", &err));
    assert(!strcmp(err.message, "object is already realized"));

    assert(object_property_get(obj, "size", &value, &err));
    assert(value.u64 == 2ULL * 1024 * 1024 * 1024);

    object_unrealize(obj);
    assert(!object_is_realized(obj));

    assert(object_property_set_from_string(obj, "size", "4G", &err));
    assert(object_property_get(obj, "size", &value, &err));
    assert(value.u64 == 4ULL * 1024 * 1024 * 1024);

    object_free(obj);
}

static void test_inherited_lifecycle_callbacks(void)
{
    Error err;
    Object *obj = object_new(TYPE_TEST_LIFECYCLE_CHILD);

    assert(obj != NULL);
    error_clear(&err);

    lifecycle_events_reset();

    assert(object_realize(obj, &err));
    assert(object_is_realized(obj));
    assert(!strcmp(lifecycle_events, "PC"));

    lifecycle_events_reset();

    object_unrealize(obj);
    assert(!object_is_realized(obj));
    assert(!strcmp(lifecycle_events, "cp"));

    object_free(obj);
}

static void test_realize_failure_rollback(void)
{
    Error err;
    Object *obj = object_new(TYPE_TEST_LIFECYCLE_FAILING_CHILD);

    assert(obj != NULL);

    lifecycle_events_reset();
    error_clear(&err);

    assert(!object_realize(obj, &err));
    assert(!strcmp(err.message, "child realize failed"));
    assert(!object_is_realized(obj));
    assert(!strcmp(lifecycle_events, "PFp"));

    object_free(obj);
}

static void test_object_tree(void)
{
    Error err;
    Object *root = object_new("object");
    Object *objects = object_new("object");
    Object *mem0 = object_new(TYPE_MEMORY_BACKEND_MEMFD);

    assert(root != NULL);
    assert(objects != NULL);
    assert(mem0 != NULL);

    error_clear(&err);

    assert(object_add_child(root, "objects", objects, &err));
    assert(object_add_child(objects, "mem0", mem0, &err));
    assert(object_resolve_path(root, "/objects/mem0") == mem0);

    assert(object_get_parent(objects) == root);
    assert(object_get_parent(mem0) == objects);

    assert(!strcmp(object_get_name(root), ""));
    assert(!strcmp(object_get_name(objects), "objects"));
    assert(!strcmp(object_get_name(mem0), "mem0"));

    assert(object_get_child_count(root) == 1);
    assert(object_get_child_count(objects) == 1);
    assert(object_get_child_count(mem0) == 0);

    assert(object_get_child(root, 0) == objects);
    assert(object_get_child(objects, 0) == mem0);
    assert(object_get_child(mem0, 0) == NULL);

    assert(object_find_child(root, "objects") == objects);
    assert(object_find_child(objects, "mem0") == mem0);
    assert(object_find_child(root, "missing") == NULL);

    assert(object_get_parent(NULL) == NULL);
    assert(!strcmp(object_get_name(NULL), ""));
    assert(object_get_child_count(NULL) == 0);
    assert(object_get_child(NULL, 0) == NULL);
    assert(object_find_child(NULL, "objects") == NULL);
    assert(object_find_child(root, NULL) == NULL);

    assert(object_get_child(root, 1) == NULL);
    assert(object_get_child(objects, 1) == NULL);

    object_free(root);
}

static void test_object_tree_boundaries(void)
{
    Error err;
    Object *root = object_new("object");
    Object *child = object_new("object");

    assert(root != NULL);
    assert(child != NULL);

    error_clear(&err);

    assert(!object_add_child(NULL, "child", child, &err));
    assert(!strcmp(err.message, "invalid child insertion"));

    error_clear(&err);

    assert(!object_add_child(root, NULL, child, &err));
    assert(!strcmp(err.message, "invalid child insertion"));

    error_clear(&err);

    assert(!object_add_child(root, "child", NULL, &err));
    assert(!strcmp(err.message, "invalid child insertion"));

    error_clear(&err);

    assert(!object_add_child(root, "", child, &err));
    assert(!strcmp(err.message, "invalid child name"));

    error_clear(&err);

    assert(!object_add_child(root, "objects/mem0", child, &err));
    assert(!strcmp(err.message, "invalid child name"));

    Object *other_parent = object_new("object");
    Object *duplicate = object_new("object");

    assert(other_parent != NULL);
    assert(duplicate != NULL);

    error_clear(&err);

    assert(object_add_child(root, "child", child, &err));

    assert(!object_add_child(other_parent, "child2", child, &err));
    assert(!strcmp(err.message, "invalid child insertion"));

    error_clear(&err);

    assert(!object_add_child(root, "child", duplicate, &err));
    assert(!strcmp(err.message, "duplicate child name"));

    object_free(duplicate);
    object_free(other_parent);

    error_clear(&err);

    assert(!object_add_child(root, "self", root, &err));
    assert(!strcmp(err.message, "invalid child insertion"));

    error_clear(&err);

    assert(!object_add_child(child, "root", root, &err));
    assert(!strcmp(err.message, "invalid child insertion"));

    object_free(root);
}

static void test_object_tree_capacity(void)
{
    Error err;
    Object *root = object_new("object");
    Object *extra;
    char name[32];

    assert(root != NULL);
    error_clear(&err);

    for (size_t i = 0; i < MINIQOM_MAX_CHILDREN; i++)
    {
        Object *child = object_new("object");

        assert(child != NULL);

        snprintf(name, sizeof(name), "child-%zu", i);
        assert(object_add_child(root, name, child, &err));
    }

    assert(object_get_child_count(root) == MINIQOM_MAX_CHILDREN);

    extra = object_new("object");
    assert(extra != NULL);

    assert(!object_add_child(root, "extra", extra, &err));
    assert(!strcmp(err.message, "invalid child insertion"));

    assert(object_resolve_path(NULL, "/") == NULL);
    assert(object_resolve_path(root, NULL) == NULL);
    assert(object_resolve_path(root, "objects/mem0") == NULL);

    object_free(extra);
    object_free(root);
}

typedef struct TestRegisterDevice
{
    uint32_t control;
    uint32_t status;
    unsigned reads;
    unsigned writes;
} TestRegisterDevice;

static uint64_t test_register_read(void *opaque,
                                   uint64_t offset,
                                   unsigned size)
{
    TestRegisterDevice *device = opaque;

    device->reads++;
    assert(size == 4);
    switch (offset)
    {
    case 0x00:
        return device->control;

    case 0x04:
        return device->status;

    default:
        assert(false);
    }
}

static bool test_register_write(void *opaque,
                                uint64_t offset,
                                unsigned size,
                                uint64_t value,
                                Error *err)
{
    TestRegisterDevice *device = opaque;

    device->writes++;
    assert(size == 4);

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

static const MemoryRegionOps test_register_ops = {
    .read = test_register_read,
    .write = test_register_write,
};

static void test_memory_region_io(void)
{
    Error err;
    MemoryRegion region;
    TestRegisterDevice device = {
        .control = 0,
        .status = 0x12345678,
    };
    uint64_t value;

    memory_region_init_io(&region, 0x100, &test_register_ops, &device);

    error_clear(&err);

    assert(memory_region_write(&region, 0x00, 4, 0xdeadbeef, &err));

    assert(memory_region_read(&region, 0x00, 4, &value, &err));

    assert(value == 0xdeadbeef);
    assert(device.control == 0xdeadbeef);

    assert(memory_region_read(&region,
                              0x04,
                              4,
                              &value,
                              &err));

    assert(value == 0x12345678);

    error_clear(&err);
    assert(!memory_region_write(&region, 0x04, 4, 0xffffffff, &err));
    assert(!strcmp(err.message, "status register is read-only"));
    assert(device.status == 0x12345678);
}

static void test_address_space_translation(void)
{
    Error err;
    MemoryRegion region;
    TestRegisterDevice device = {.status = 0x12345678};
    AddressSpace space;
    MemoryRegion *matched = NULL;
    uint64_t offset, value;

    error_clear(&err);
    memory_region_init_io(&region, 8, &test_register_ops, &device);
    address_space_init(&space);
    assert(address_space_add_region(&space, 0x1000, &region, &err));
    assert(address_space_translate(&space, 0x1004, 4,
                                   &matched, &offset, &err));
    assert(matched == &region);
    assert(offset == 4);
    assert(memory_region_read(matched, offset, 4, &value, &err));
    assert(value == 0x12345678);
}

static void test_address_space_boundaries(void)
{
    static const struct
    {
        uint64_t base;
        uint64_t length;
        uint64_t address;
        unsigned size;
        bool valid;
        uint64_t expected_offset;
        const char *error;
    } cases[] = {
        /* First byte, last byte, and whole region. */
        {0x1000, 8, 0x1000, 1, true, 0, NULL},
        {0x1000, 8, 0x1007, 1, true, 7, NULL},
        {0x1000, 8, 0x1000, 8, true, 0, NULL},

        /* Before, after, and crossing the region boundary. */
        {0x1000, 8, 0x0fff, 1, false, 0, "address is not mapped"},
        {0x1000, 8, 0x1008, 1, false, 0, "address is not mapped"},
        {0x1000, 8, 0x1004, 8, false, 0, "memory access out of bounds"},

        /* A region ending at UINT64_MAX is valid. */
        {UINT64_MAX - 7, 8, UINT64_MAX, 1, true, 7, NULL},
        {UINT64_MAX, 1, UINT64_MAX, 1, true, 0, NULL},

        /* An access must not wrap around. */
        {UINT64_MAX - 7, 8, UINT64_MAX, 2, false, 0,
         "memory access out of bounds"},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        Error err;
        MemoryRegion region = {.size = cases[i].length};
        AddressSpace space;
        MemoryRegion sentinel = {0};
        MemoryRegion *matched = &sentinel;
        uint64_t offset = UINT64_MAX;

        error_clear(&err);
        address_space_init(&space);
        assert(address_space_add_region(&space, cases[i].base, &region, &err));
        bool ok = address_space_translate(
            &space, cases[i].address, cases[i].size, &matched, &offset, &err);

        assert(ok == cases[i].valid);
        if (ok)
        {
            assert(matched == &region);
            assert(offset == cases[i].expected_offset);
        }
        else
        {
            assert(!strcmp(err.message, cases[i].error));
            assert(matched == &sentinel);
            assert(offset == UINT64_MAX);
        }
    }
}

static void test_address_space_read_and_write(void)
{
    Error err;
    MemoryRegion region;
    TestRegisterDevice device = {.status = 0x12345678};
    AddressSpace space;
    uint64_t value;

    error_clear(&err);
    memory_region_init_io(&region, 8, &test_register_ops, &device);
    address_space_init(&space);
    assert(address_space_add_region(&space, 0x1000, &region, &err));

    assert(address_space_read(&space, 0x1004, 4, &value, &err));
    assert(value == 0x12345678);

    assert(address_space_write(&space, 0x1000, 4, 0x1234, &err));
    assert(address_space_read(&space, 0x1000, 4, &value, &err));
    assert(value == 0x1234);

    error_clear(&err);
    assert(!address_space_write(&space, 0x1004, 4, 0x1234, &err));
    assert(!strcmp(err.message, "status register is read-only"));
    assert(device.status == 0x12345678);

    error_clear(&err);
    assert(!address_space_read(&space, 0x1008, 4, &value, &err));
    assert(!strcmp(err.message, "address is not mapped"));

    error_clear(&err);
    assert(!address_space_write(&space, 0x1008, 4, 0x1234, &err));
    assert(!strcmp(err.message, "address is not mapped"));
}

static void assert_address_unmapped(const AddressSpace *space, uint64_t address)
{
    Error err;
    MemoryRegion sentinel = {0};
    MemoryRegion *matched = &sentinel;
    uint64_t offset = UINT64_MAX;

    error_clear(&err);
    assert(!address_space_translate(space, address, 1, &matched, &offset, &err));
    assert(!strcmp(err.message, "address is not mapped"));
    assert(matched == &sentinel);
    assert(offset == UINT64_MAX);
}

static void test_address_space_multiple_regions(void)
{
    Error err;
    AddressSpace space;
    MemoryRegion regions[2];
    TestRegisterDevice devices[2] = {
        {.status = 0x11111111},
        {.status = 0x22222222},
    };
    uint64_t value;

    error_clear(&err);
    address_space_init(&space);
    assert_address_unmapped(&space, 0x1000);
    memory_region_init_io(&regions[0], 8, &test_register_ops, &devices[0]);
    memory_region_init_io(&regions[1], 8, &test_register_ops, &devices[1]);

    /* Lookup must not depend on ascending registration order. */
    assert(address_space_add_region(&space, 0x2000, &regions[1], &err));
    assert(address_space_add_region(&space, 0x1000, &regions[0], &err));
    assert(address_space_read(&space, 0x1004, 4, &value, &err));
    assert(value == devices[0].status);
    assert(address_space_read(&space, 0x2004, 4, &value, &err));
    assert(value == devices[1].status);

    assert(address_space_write(&space, 0x1000, 4, 0x1234, &err));
    assert(devices[0].control == 0x1234);
    assert(devices[1].control == 0);
    assert(address_space_write(&space, 0x2000, 4, 0x5678, &err));
    assert(devices[0].control == 0x1234);
    assert(devices[1].control == 0x5678);
    assert_address_unmapped(&space, 0x1800);

    value = UINT64_MAX;
    error_clear(&err);
    assert(!address_space_read(&space, 0x1800, 4, &value, &err));
    assert(!strcmp(err.message, "address is not mapped"));
    assert(value == UINT64_MAX);
    error_clear(&err);
    assert(!address_space_write(&space, 0x1800, 4, 0, &err));
    assert(!strcmp(err.message, "address is not mapped"));
    assert(devices[0].reads == 1 && devices[1].reads == 1);
    assert(devices[0].writes == 1 && devices[1].writes == 1);
}

static void test_address_space_overlapping_regions(void)
{
    static const struct
    {
        uint64_t base;
        uint64_t size;
    } cases[] = {
        {0x1000, 8},  /* Same range. */
        {0x0ffc, 8},  /* Overlap from below. */
        {0x1004, 8},  /* Overlap from above. */
        {0x1002, 2},  /* New mapping inside old mapping. */
        {0x0ffc, 16}, /* New mapping contains old mapping. */
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        Error err;
        AddressSpace space;
        MemoryRegion first = {.size = 8};
        MemoryRegion second = {.size = cases[i].size};
        MemoryRegion *matched = NULL;
        uint64_t offset;

        error_clear(&err);
        address_space_init(&space);
        assert(address_space_add_region(&space, 0x1000, &first, &err));
        assert(!address_space_add_region(&space, cases[i].base, &second, &err));
        assert(!strcmp(err.message, "memory region overlaps existing mapping"));
        assert(address_space_translate(&space, 0x1000, 8,
                                       &matched, &offset, &err));
        assert(matched == &first && offset == 0);
        assert_address_unmapped(&space, 0x0fff);
        assert_address_unmapped(&space, 0x1008);
    }
}

static void test_address_space_adjacent_regions(void)
{
    Error err;
    AddressSpace space;
    MemoryRegion regions[2];
    TestRegisterDevice devices[2] = {{.status = 1}, {.control = 2}};
    uint64_t value;

    error_clear(&err);
    address_space_init(&space);
    for (size_t i = 0; i < 2; i++)
    {
        memory_region_init_io(&regions[i], 8, &test_register_ops, &devices[i]);
        assert(address_space_add_region(&space, 0x1000 + i * 8,
                                        &regions[i], &err));
    }
    assert(address_space_read(&space, 0x1004, 4, &value, &err));
    assert(value == 1);
    assert(address_space_read(&space, 0x1008, 4, &value, &err));
    assert(value == 2);

    value = UINT64_MAX;
    assert(!address_space_read(&space, 0x1004, 8, &value, &err));
    assert(!strcmp(err.message, "memory access out of bounds"));
    assert(value == UINT64_MAX);
    error_clear(&err);
    assert(!address_space_write(&space, 0x1004, 8, 0, &err));
    assert(!strcmp(err.message, "memory access out of bounds"));
    assert(devices[0].reads == 1 && devices[1].reads == 1);
    assert(devices[0].writes == 0 && devices[1].writes == 0);
}

static void test_address_space_invalid_mapping(void)
{
    Error err;
    AddressSpace space;
    MemoryRegion empty = {.size = 0};
    MemoryRegion region = {.size = 8};

    error_clear(&err);
    address_space_init(&space);
    assert(!address_space_add_region(NULL, 0, &region, &err));
    assert(!strcmp(err.message, "invalid address space mapping"));
    error_clear(&err);
    assert(!address_space_add_region(&space, 0, NULL, &err));
    assert(!strcmp(err.message, "invalid address space mapping"));
    error_clear(&err);
    assert(!address_space_add_region(&space, 0, &empty, &err));
    assert(!strcmp(err.message, "invalid address space mapping"));
    error_clear(&err);
    assert(!address_space_add_region(&space, UINT64_MAX - 3, &region, &err));
    assert(!strcmp(err.message, "invalid address space mapping"));
    assert_address_unmapped(&space, 0);
    assert_address_unmapped(&space, UINT64_MAX);
    assert(address_space_add_region(&space, 0, &region, &err));
}

static void test_address_space_capacity(void)
{
    Error err;
    AddressSpace space;
    MemoryRegion regions[MINIQOM_MAX_ADDRESS_SPACE_REGIONS + 1];

    error_clear(&err);
    address_space_init(&space);
    for (size_t i = 0; i < MINIQOM_MAX_ADDRESS_SPACE_REGIONS; i++)
    {
        regions[i] = (MemoryRegion){.size = 8};
        assert(address_space_add_region(&space, i * 16, &regions[i], &err));
    }
    regions[MINIQOM_MAX_ADDRESS_SPACE_REGIONS] = (MemoryRegion){.size = 8};
    assert(!address_space_add_region(&space, MINIQOM_MAX_ADDRESS_SPACE_REGIONS * 16,
                                     &regions[MINIQOM_MAX_ADDRESS_SPACE_REGIONS], &err));
    assert(!strcmp(err.message, "address space is full"));
    assert_address_unmapped(&space, MINIQOM_MAX_ADDRESS_SPACE_REGIONS * 16);
    for (size_t i = 0; i < MINIQOM_MAX_ADDRESS_SPACE_REGIONS; i++)
    {
        MemoryRegion *matched = NULL;
        uint64_t offset = UINT64_MAX;
        assert(address_space_translate(&space, i * 16, 8,
                                       &matched, &offset, &err));
        assert(matched == &regions[i] && offset == 0);
    }
}

static void test_address_space_remove_region(void)
{
    Error err;
    AddressSpace space;
    MemoryRegion region = {.size = 16};

    error_clear(&err);
    address_space_init(&space);
    assert(address_space_add_region(&space, 0x1000,
                                    &region, &err));

    MemoryRegion *matched = NULL;
    uint64_t offset = UINT64_MAX;
    assert(address_space_translate(&space, 0x1000, 16,
                                   &matched, &offset, &err));
    assert(matched == &region && offset == 0);

    error_clear(&err);
    assert(address_space_remove_region(&space, 0x1000, &err));

    error_clear(&err);
    assert(!address_space_translate(&space, 0x1000, 16,
                                    &matched, &offset, &err));
    assert(!strcmp(err.message, "address is not mapped"));

    error_clear(&err);
    assert(!address_space_remove_region(&space, 0x1000, &err));
    assert(!strcmp(err.message, "mapping not found"));

    error_clear(&err);
    assert(!address_space_remove_region(NULL, 0x1000, &err));
    assert(!strcmp(err.message, "invalid address space arguments"));
}

static void test_address_space_remove_preserves_other_regions(void)
{
    for (size_t removed = 0; removed < 3; removed++)
    {
        Error err;
        AddressSpace space;
        MemoryRegion regions[3];
        TestRegisterDevice devices[3] = {
            {.status = 11}, {.status = 22}, {.status = 33},
        };
        uint64_t value = UINT64_MAX;
        uint64_t removed_base = 0x1000 + removed * 0x100;

        error_clear(&err);
        address_space_init(&space);
        for (size_t i = 0; i < 3; i++)
        {
            memory_region_init_io(&regions[i], 8, &test_register_ops, &devices[i]);
            assert(address_space_add_region(&space, 0x1000 + i * 0x100,
                                            &regions[i], &err));
        }

        assert(address_space_remove_region(&space, removed_base, &err));
        assert_address_unmapped(&space, removed_base);
        assert(!address_space_read(&space, removed_base + 4, 4, &value, &err));
        assert(!strcmp(err.message, "address is not mapped"));
        assert(value == UINT64_MAX);
        error_clear(&err);
        assert(!address_space_write(&space, removed_base, 4, 99, &err));
        assert(!strcmp(err.message, "address is not mapped"));
        assert(devices[removed].reads == 0);
        assert(devices[removed].writes == 0);
        assert(devices[removed].control == 0);

        for (size_t i = 0; i < 3; i++)
        {
            if (i == removed)
            {
                continue;
            }
            uint64_t base = 0x1000 + i * 0x100;
            error_clear(&err);
            assert(address_space_read(&space, base + 4, 4, &value, &err));
            assert(value == devices[i].status);
            assert(address_space_write(&space, base, 4, 100 + i, &err));
            assert(devices[i].control == 100 + i);
            assert(address_space_read(&space, base, 4, &value, &err));
            assert(value == 100 + i);
        }
    }
}

static void test_address_space_remove_requires_exact_base(void)
{
    Error err;
    AddressSpace space;
    MemoryRegion region;
    TestRegisterDevice device = {.status = 0x12345678};
    uint64_t value;
    const uint64_t invalid_bases[] = {0x1004, 0x1008, 0x2000};

    error_clear(&err);
    address_space_init(&space);
    memory_region_init_io(&region, 8, &test_register_ops, &device);
    assert(address_space_add_region(&space, 0x1000, &region, &err));

    for (size_t i = 0; i < sizeof(invalid_bases) / sizeof(invalid_bases[0]); i++)
    {
        error_clear(&err);
        assert(!address_space_remove_region(&space, invalid_bases[i], &err));
        assert(!strcmp(err.message, "mapping not found"));
        assert(address_space_read(&space, 0x1004, 4, &value, &err));
        assert(value == 0x12345678);
        assert(address_space_write(&space, 0x1000, 4, i + 1, &err));
        assert(device.control == i + 1);
    }
    assert(address_space_remove_region(&space, 0x1000, &err));
    assert_address_unmapped(&space, 0x1000);
}

static void test_address_space_remove_restores_capacity(void)
{
    Error err;
    AddressSpace space;
    MemoryRegion regions[MINIQOM_MAX_ADDRESS_SPACE_REGIONS];
    MemoryRegion replacement = {.size = 8};
    MemoryRegion extra = {.size = 8};
    size_t removed = MINIQOM_MAX_ADDRESS_SPACE_REGIONS / 2;

    error_clear(&err);
    address_space_init(&space);
    for (size_t i = 0; i < MINIQOM_MAX_ADDRESS_SPACE_REGIONS; i++)
    {
        regions[i] = (MemoryRegion){.size = 8};
        assert(address_space_add_region(&space, i * 16, &regions[i], &err));
    }

    assert(address_space_remove_region(&space, removed * 16, &err));
    assert_address_unmapped(&space, removed * 16);
    assert(address_space_add_region(&space, removed * 16, &replacement, &err));

    for (size_t i = 0; i < MINIQOM_MAX_ADDRESS_SPACE_REGIONS; i++)
    {
        MemoryRegion *matched = NULL;
        uint64_t offset = UINT64_MAX;
        assert(address_space_translate(&space, i * 16, 8,
                                       &matched, &offset, &err));
        assert(matched == (i == removed ? &replacement : &regions[i]));
        assert(offset == 0);
    }

    assert(!address_space_add_region(&space, MINIQOM_MAX_ADDRESS_SPACE_REGIONS * 16,
                                    &extra, &err));
    assert(!strcmp(err.message, "address space is full"));
    assert_address_unmapped(&space, MINIQOM_MAX_ADDRESS_SPACE_REGIONS * 16);
}

static void test_address_space_remove_one_alias(void)
{
    Error err;
    AddressSpace space;
    MemoryRegion region;
    TestRegisterDevice device = {.status = 0x12345678};
    uint64_t value;

    error_clear(&err);
    address_space_init(&space);
    memory_region_init_io(&region, 8, &test_register_ops, &device);
    assert(address_space_add_region(&space, 0x1000, &region, &err));
    assert(address_space_add_region(&space, 0x2000, &region, &err));
    assert(address_space_write(&space, 0x1000, 4, 42, &err));
    assert(address_space_read(&space, 0x2000, 4, &value, &err));
    assert(value == 42);

    assert(address_space_remove_region(&space, 0x1000, &err));
    assert_address_unmapped(&space, 0x1000);
    assert(address_space_read(&space, 0x2000, 4, &value, &err));
    assert(value == 42);
    assert(address_space_write(&space, 0x2000, 4, 84, &err));
    assert(device.control == 84);
    assert(address_space_read(&space, 0x2004, 4, &value, &err));
    assert(value == 0x12345678);

    error_clear(&err);
    assert(!address_space_remove_region(&space, 0x1000, &err));
    assert(!strcmp(err.message, "mapping not found"));
    assert(address_space_read(&space, 0x2000, 4, &value, &err));
    assert(value == 84);
}

int main(void)
{
    type_system_init();
    host_memory_backend_register_types();
    test_lifecycle_register_types();

    test_type_queries();
    test_memory_backend_defaults();
    test_object_lifecycle();
    test_property_get();
    test_property_freeze_after_realize();
    test_inherited_lifecycle_callbacks();
    test_realize_failure_rollback();
    test_object_tree();
    test_object_tree_boundaries();
    test_object_tree_capacity();
    test_memory_region_io();
    test_address_space_translation();
    test_address_space_boundaries();
    test_address_space_read_and_write();
    test_address_space_multiple_regions();
    test_address_space_overlapping_regions();
    test_address_space_adjacent_regions();
    test_address_space_invalid_mapping();
    test_address_space_capacity();
    test_address_space_remove_region();
    test_address_space_remove_preserves_other_regions();
    test_address_space_remove_requires_exact_base();
    test_address_space_remove_restores_capacity();
    test_address_space_remove_one_alias();

    return 0;
}
