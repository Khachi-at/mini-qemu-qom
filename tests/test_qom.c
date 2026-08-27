#include <assert.h>
#include <string.h>

#include "miniqom/memory.h"
#include "miniqom/object.h"
#include "miniqom/property.h"
#include "miniqom/type.h"

int main(void)
{
    Error err;
    Object *root;
    Object *objects;
    Object *mem0;
    HostMemoryBackendMemfd *backend;

    type_system_init();
    host_memory_backend_register_types();

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

    object_free(root);
    return 0;
}