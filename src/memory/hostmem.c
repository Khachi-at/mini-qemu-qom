#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "miniqom/memory.h"
#include "miniqom/property.h"
#include "miniqom/type.h"

void host_memory_backend_memfd_register_type(void);

static char *xstrdup(const char *text)
{
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);

    assert(copy != NULL);
    memcpy(copy, text, length);
    return copy;
}

static bool backend_set_size(Object *obj, PropertyValue value, Error *err)
{
    HostMemoryBackend *backend = (HostMemoryBackend *)obj;
    if (!value.u64)
    {
        error_set(err, "size must be non-zero");
        return false;
    }

    backend->size = value.u64;
    return true;
}

static bool backend_set_share(Object *obj, PropertyValue value, Error *err)
{
    (void)err;
    ((HostMemoryBackend *)obj)->share = value.b;
    return true;
}

static bool backend_set_mmoc(Object *obj, PropertyValue value, Error *err)
{
    (void)err;
    ((HostMemoryBackend *)obj)->mmoc = value.b;
    return true;
}

static bool backend_set_swap_storage(Object *obj, PropertyValue value,
                                     Error *err)
{
    HostMemoryBackend *backend = (HostMemoryBackend *)obj;

    if (!backend->mmoc)
    {
        error_set(err, "swap-storage requires mmoc=on");
        return false;
    }

    free(backend->swap_storage);
    backend->swap_storage = xstrdup(value.str);
    return true;
}

static bool backend_get_size(Object *obj, PropertyValue *value, Error *err)
{
    (void)err;

    value->u64 = ((HostMemoryBackend *)obj)->size;
    return true;
}

static bool backend_get_share(Object *obj, PropertyValue *value, Error *err)
{
    (void)err;

    value->b = ((HostMemoryBackend *)obj)->share;
    return true;
}

static bool backend_get_mmoc(Object *obj, PropertyValue *value, Error *err)
{
    (void)err;

    value->b = ((HostMemoryBackend *)obj)->mmoc;
    return true;
}

static bool backend_get_swap_storage(Object *obj,
                                     PropertyValue *value,
                                     Error *err)
{
    (void)err;

    value->str = ((HostMemoryBackend *)obj)->swap_storage;
    return true;
}

static void memory_backend_class_init(ObjectClass *klass)
{
    object_class_property_add(klass,
                              "size",
                              PROP_U64,
                              backend_set_size,
                              backend_get_size);
    object_class_property_add(klass,
                              "share",
                              PROP_BOOL,
                              backend_set_share,
                              backend_get_share);
    object_class_property_add(klass,
                              "mmoc",
                              PROP_BOOL,
                              backend_set_mmoc,
                              backend_get_mmoc);
    object_class_property_add(klass,
                              "swap-storage",
                              PROP_STRING,
                              backend_set_swap_storage,
                              backend_get_swap_storage);
}

static void memory_backend_instance_init(Object *obj)
{
    HostMemoryBackend *backend = (HostMemoryBackend *)obj;

    backend->share = false;
    backend->mmoc = false;
}

static void memory_backend_instance_finalize(Object *obj)
{
    HostMemoryBackend *backend = (HostMemoryBackend *)obj;

    free(backend->swap_storage);
}

static const TypeInfo memory_backend_info = {
    .name = TYPE_MEMORY_BACKEND,
    .parent = "object",
    .instance_size = sizeof(HostMemoryBackend),
    .class_init = memory_backend_class_init,
    .instance_init = memory_backend_instance_init,
    .instance_finalize = memory_backend_instance_finalize,
};

void host_memory_backend_register_types(void)
{
    type_register_static(&memory_backend_info);
    host_memory_backend_memfd_register_type();
}