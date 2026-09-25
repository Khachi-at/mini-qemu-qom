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
} TestRegisterDevice;

static uint64_t test_register_read(void *opaque,
                                   uint64_t offset,
                                   unsigned size)
{
    TestRegisterDevice *device = opaque;

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
    AddressSpace space = {.base = 0x1000, .region = &region};
    uint64_t offset, value;

    error_clear(&err);
    memory_region_init_io(&region, 8, &test_register_ops, &device);
    assert(address_space_translate(&space, 0x1004, 4, &offset, &err));
    assert(offset == 4);
    assert(memory_region_read(space.region, offset, 4, &value, &err));
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
    } cases[] = {
        /* First byte, last byte, and whole region. */
        {0x1000, 8, 0x1000, 1, true, 0},
        {0x1000, 8, 0x1007, 1, true, 7},
        {0x1000, 8, 0x1000, 8, true, 0},

        /* Before, after, and crossing the region boundary. */
        {0x1000, 8, 0x0fff, 1, false, 0},
        {0x1000, 8, 0x1008, 1, false, 0},
        {0x1000, 8, 0x1004, 8, false, 0},
        {0x1000, 0, 0x1000, 1, false, 0},

        /* A region ending at UINT64_MAX is valid. */
        {UINT64_MAX - 7, 8, UINT64_MAX, 1, true, 7},

        /* The access or mapping must not wrap around. */
        {UINT64_MAX - 7, 8, UINT64_MAX, 2, false, 0},
        {UINT64_MAX - 3, 8, UINT64_MAX - 3, 1, false, 0},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        Error err;
        MemoryRegion region = {.size = cases[i].length};
        AddressSpace space = {.base = cases[i].base, .region = &region};
        uint64_t offset = UINT64_MAX;

        error_clear(&err);
        bool ok = address_space_translate(
            &space, cases[i].address, cases[i].size, &offset, &err);

        assert(ok == cases[i].valid);
        if (ok)
        {
            assert(offset == cases[i].expected_offset);
        }
        else
        {
            assert(!strcmp(err.message, "memory access out of bounds"));
            assert(offset == UINT64_MAX);
        }
    }
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

    return 0;
}
