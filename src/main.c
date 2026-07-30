#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

#include "miniqom/memory.h"
#include "miniqom/object.h"
#include "miniqom/type.h"
#include "miniqom/property.h"

static void dump_tree(const Object *obj, unsigned depth)
{
    for (size_t i = 0; i < depth; i++)
    {
        printf("  ");
    }

    printf("%s (%s)\n",
           obj->name ? obj->name : "<root>",
           type_get_info(obj->klass->type)->name);

    for (size_t i = 0; i < obj->child_count; i++)
    {
        dump_tree(obj->children[i], depth + 1);
    }
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

    error_clear(&err);
    root = object_new("object");
    objects = object_new("object");
    mem0 = object_new(TYPE_MEMORY_BACKEND_MEMFD);
    assert(root && objects && mem0);

    assert(object_add_child(root, "objects", objects, &err));
    assert(object_add_child(objects, "mem0", mem0, &err));

    assert(object_property_set_from_string(mem0, "size", "1G", &err));
    assert(object_property_set_from_string(mem0, "mmoc", "on", &err));
    assert(object_property_set_from_string(mem0, "swap-storage",
                                           "file:///tmp/mmoc.swap", &err));
    assert(object_property_set_from_string(mem0, "seal", "off", &err));

    backend = (HostMemoryBackendMemfd *)mem0;

    dump_tree(root, 0);
    printf("\nmem0: size=%" PRIu64 ", share=%d, mmoc=%d, seal=%d\n",
           backend->parent_obj.size,
           backend->parent_obj.share,
           backend->parent_obj.mmoc,
           backend->seal);

    object_free(root);
    return 0;
}
