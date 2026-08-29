#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "miniqom/error.h"
#include "miniqom/memory.h"
#include "miniqom/object.h"
#include "miniqom/property.h"
#include "miniqom/type.h"

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

static void lifecycle_events_rest(void)
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

int main(void)
{
    Error err;
    Object *root;
    Object *objects;
    Object *mem0;
    HostMemoryBackendMemfd *backend;

    type_system_init();
    host_memory_backend_register_types();
    test_lifecycle_register_types();

    error_clear(&err);
    root = object_new("object");
    objects = object_new("object");
    mem0 = object_new(TYPE_MEMORY_BACKEND_MEMFD);

    assert(object_is_instance_of(mem0, TYPE_MEMORY_BACKEND_MEMFD));
    assert(object_is_instance_of(mem0, TYPE_MEMORY_BACKEND));
    assert(object_is_instance_of(mem0, "object"));
    assert(!strcmp(object_get_type_name(mem0),
                   TYPE_MEMORY_BACKEND_MEMFD));

    assert(object_is_a(mem0, TYPE_MEMORY_BACKEND_MEMFD));
    assert(object_is_a(mem0, TYPE_MEMORY_BACKEND));
    assert(object_is_a(mem0, "object"));
    assert(!object_is_a(mem0, "unknown-type"));
    assert(object_get_type_name(NULL) == NULL);
    assert(!object_is_a(NULL, "object"));

    error_clear(&err);

    assert(!object_is_realized(mem0));

    assert(object_realize(mem0, &err));
    assert(object_is_realized(mem0));

    assert(!object_realize(mem0, &err));
    assert(!strcmp(err.message, "object is already realized"));
    assert(object_is_realized(mem0));

    object_unrealize(mem0);
    assert(!object_is_realized(mem0));

    object_unrealize(mem0);
    assert(!object_is_realized(mem0));

    assert(object_realize(mem0, &err));
    assert(object_is_realized(mem0));

    object_unrealize(mem0);
    assert(!object_is_realized(mem0));

    backend = (HostMemoryBackendMemfd *)mem0;
    assert(backend->seal);
    assert(backend->parent_obj.share);

    assert(object_add_child(root, "objects", objects, &err));
    assert(object_add_child(objects, "mem0", mem0, &err));
    assert(object_resolve_path(root, "/objects/mem0") == mem0);

    assert(!object_property_set_from_string(mem0, "swap-storage",
                                            "file:///swap", &err));
    assert(!strcmp(err.message, "swap-storage requires mmoc=on"));

    error_clear(&err);
    assert(object_property_set_from_string(mem0, "mmoc", "on", &err));
    assert(object_property_set_from_string(mem0, "swap-storage",
                                           "file:///swap", &err));
    assert(object_property_set_from_string(mem0, "size", "2G", &err));

    assert(backend->parent_obj.size == 2ULL * 1024 * 1024 * 1024);
    assert(!strcmp(backend->parent_obj.swap_storage, "file:///swap"));

    // ====== Freeze properties ======
    //
    Object *property_object;
    property_object = object_new(TYPE_MEMORY_BACKEND_MEMFD);
    assert(property_object != NULL);

    PropertyValue property_value;
    error_clear(&err);

    assert(object_property_set_from_string(property_object,
                                           "size",
                                           "2G",
                                           &err));

    assert(object_realize(property_object, &err));
    assert(object_is_realized(property_object));

    assert(!object_property_set_from_string(property_object,
                                            "size", "4G",
                                            &err));

    assert(!strcmp(err.message, "object is already realized"));

    assert(object_property_get(property_object,
                               "size",
                               &property_value,
                               &err));

    assert(property_value.u64 == 2ULL * 1024 * 1024 * 1024);
    // ====== Test property ======
    assert(object_property_get(mem0, "size",
                               &property_value, &err));
    assert(property_value.u64 == 2ULL * 1024 * 1024 * 1024);

    assert(object_property_get(mem0, "mmoc",
                               &property_value, &err));
    assert(property_value.b);

    assert(object_property_get(mem0, "share",
                               &property_value, &err));
    assert(property_value.b);

    assert(object_property_get(mem0, "swap-storage",
                               &property_value, &err));
    assert(!strcmp(property_value.str, "file:///swap"));

    error_clear(&err);

    assert(!object_property_get(mem0, "unknown-property",
                                &property_value, &err));
    assert(!strcmp(err.message, "property not found"));

    // ====== Test realize callbacks ======
    error_clear(&err);

    Object *lifecycle_object;

    lifecycle_object = object_new(TYPE_TEST_LIFECYCLE_CHILD);
    assert(lifecycle_object != NULL);

    lifecycle_events_rest();

    assert(object_realize(lifecycle_object, &err));
    assert(object_is_realized(lifecycle_object));
    assert(!strcmp(lifecycle_events, "PC"));

    lifecycle_events_rest();

    object_unrealize(lifecycle_object);
    assert(!object_is_realized(lifecycle_object));
    assert(!strcmp(lifecycle_events, "cp"));

    object_free(lifecycle_object);

    // ===== Test realize failed ======
    Object *failing_object;

    failing_object = object_new(TYPE_TEST_LIFECYCLE_FAILING_CHILD);
    assert(failing_object != NULL);

    lifecycle_events_rest();
    error_clear(&err);

    assert(!object_realize(failing_object, &err));
    assert(!strcmp(err.message, "child realize failed"));
    assert(!object_is_realized(failing_object));
    assert(!strcmp(lifecycle_events, "PFp"));

    object_free(failing_object);

    // ======= Release test resources ======
    object_free(root);
    return 0;
}
