#include "miniqom/memory.h"
#include "miniqom/property.h"
#include "miniqom/type.h"

static bool memfd_set_seal(Object *obj, PropertyValue value, Error *err)
{
    (void)err;
    ((HostMemoryBackendMemfd *)obj)->seal = value.b;
    return true;
}

static bool memfd_set_hugetlb(Object *obj, PropertyValue value, Error *err)
{
    (void)err;
    ((HostMemoryBackendMemfd *)obj)->hugetlb = value.b;
    return true;
}

static void memory_backend_memfd_class_init(ObjectClass *klass)
{
    object_class_property_add(klass, "seal", PROP_BOOL, memfd_set_seal);
    object_class_property_add(klass, "hugetlb", PROP_BOOL, memfd_set_hugetlb);
}

static void memory_backend_memfd_instance_init(Object *obj)
{
    HostMemoryBackendMemfd *memfd = (HostMemoryBackendMemfd *)obj;

    memfd->parent_obj.share = true;
    memfd->seal = true;
    memfd->hugetlb = true;
}

static const TypeInfo memory_backend_memfd_info = {
    .name = TYPE_MEMORY_BACKEND_MEMFD,
    .parent = TYPE_MEMORY_BACKEND,
    .instance_size = sizeof(HostMemoryBackendMemfd),
    .class_init = memory_backend_memfd_class_init,
    .instance_init = memory_backend_memfd_instance_init,
};

void host_memory_backend_memfd_register_type(void)
{
    type_register_static(&memory_backend_memfd_info);
}
