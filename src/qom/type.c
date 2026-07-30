#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "miniqom/object.h"
#include "miniqom/property.h"
#include "miniqom/type.h"

#define MINIQOM_MAX_TYPES 16

struct Type
{
    const TypeInfo *info;
    Type *parent;
    ObjectClass *klass;
};

static Type types[MINIQOM_MAX_TYPES];
static size_t type_count;
static bool initialized;

static const TypeInfo object_info = {
    .name = "object",
    .instance_size = sizeof(Object),
};

Type *type_get_by_name(const char *name)
{
    for (size_t i = 0; i < type_count; i++)
    {
        if (!strcmp(types[i].info->name, name))
        {
            return &types[i];
        }
    }
    return NULL;
}

Type *type_get_parent(Type *type)
{
    return type->parent;
}

const TypeInfo *type_get_info(Type *type)
{
    return type->info;
}

ObjectClass *type_get_class(Type *type)
{
    return type->klass;
}

void type_register_static(const TypeInfo *info)
{
    Type *type;
    Type *parent = NULL;

    assert(type_count < MINIQOM_MAX_TYPES);
    assert(type_get_by_name(info->name) == NULL);

    if (info->parent)
    {
        parent = type_get_by_name(info->parent);
        assert(parent != NULL);
    }

    type = &types[type_count++];
    type->info = info;
    type->parent = parent;
    type->klass = object_class_new(type);

    if (info->class_init)
    {
        info->class_init(type->klass);
    }
}

void type_system_init(void)
{
    if (!initialized)
    {
        type_register_static(&object_info);
        initialized = true;
    }
}
