#include <assert.h>
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

    object_free(root);
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

    return 0;
}
