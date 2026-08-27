#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "miniqom/object.h"
#include "miniqom/type.h"
#include "miniqom/property.h"

static char *xstrdup(const char *text)
{
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);

    assert(copy != NULL);
    memcpy(copy, text, length);
    return copy;
}

static void object_instance_init(Object *obj, Type *type)
{
    Type *chain[16];
    size_t depth = 0;

    for (Type *current = type; current; current = type_get_parent(current))
    {
        assert(depth < sizeof(chain) / sizeof(chain[0]));
        chain[depth++] = current;
    }

    while (depth)
    {
        const TypeInfo *info = type_get_info(chain[--depth]);
        if (info->instance_init)
        {
            info->instance_init(obj);
        }
    }
}

static size_t object_type_chain(Object *obj, Type **chain,
                                size_t chain_capacity)
{
    size_t depth = 0;

    for (Type *type = obj->klass->type; type; type = type_get_parent(type))
    {
        assert(depth < chain_capacity);
        chain[depth++] = type;
    }

    return depth;
}

Object *object_new(const char *type_name)
{
    Type *type = type_get_by_name(type_name);
    Object *obj;

    if (!type)
    {
        return NULL;
    }

    obj = calloc(1, type_get_info(type)->instance_size);
    assert(obj != NULL);

    obj->klass = type_get_class(type);
    object_instance_init(obj, type);
    return obj;
}

bool object_is_realized(const Object *obj)
{
    return obj && obj->state == OBJECT_STATE_REALIZED;
}

bool object_realize(Object *obj, Error *err)
{
    Type *chain[16];
    size_t depth;

    if (!obj)
    {
        error_set(err, "cannot realize null object");
        return false;
    }

    if (obj->state == OBJECT_STATE_REALIZED)
    {
        error_set(err, "object is already realized");
        return false;
    }

    depth = object_type_chain(obj, chain, sizeof(chain) / sizeof(chain[0]));

    while (depth)
    {
        Type *type = chain[--depth];
        const TypeInfo *info = type_get_info(type);

        if (info->instance_realize &&
            !info->instance_realize(obj, err))
        {
            return false;
        }
    }

    obj->state = OBJECT_STATE_REALIZED;
    return true;
}

void object_unrealize(Object *obj)
{
    Type *type;

    if (!obj || obj->state != OBJECT_STATE_REALIZED)
    {
        return;
    }

    for (type = obj->klass->type; type; type = type_get_parent(type))
    {
        const TypeInfo *info = type_get_info(type);

        if (info->instance_unrealize)
        {
            info->instance_unrealize(obj);
        }
    }

    obj->state = OBJECT_STATE_NEW;
}

void object_free(Object *obj)
{
    Type *type;

    if (!obj)
    {
        return;
    }

    for (size_t i = 0; i < obj->child_count; i++)
    {
        object_free(obj->children[i]);
    }

    for (type = obj->klass->type; type; type = type_get_parent(type))
    {
        InstanceFinalize finalize = type_get_info(type)->instance_finalize;

        if (finalize)
        {
            finalize(obj);
        }
    }

    free(obj->name);
    free(obj);
}

const char *object_get_type_name(const Object *obj)
{
    if (!obj || !obj->klass || !obj->klass->type)
    {
        return NULL;
    }

    return type_get_info(obj->klass->type)->name;
}

bool object_is_a(const Object *obj, const char *type_name)
{
    Type *type;

    if (!obj || !type_name || !obj->klass || !obj->klass->type)
    {
        return false;
    }

    for (type = obj->klass->type; type; type = type_get_parent(type))
    {
        if (!strcmp(type_get_info(type)->name, type_name))
        {
            return true;
        }
    }

    return false;
}

bool object_is_instance_of(const Object *obj, const char *type_name)
{
    return object_is_a(obj, type_name);
}

bool object_add_child(Object *parent, const char *name,
                      Object *child, Error *err)
{
    if (child->parent || parent->child_count == MINIQOM_MAX_CHILDREN)
    {
        error_set(err, "invalid child insertion");
        return false;
    }

    for (size_t i = 0; i < parent->child_count; i++)
    {
        if (!strcmp(parent->children[i]->name, name))
        {
            error_set(err, "duplicate child name");
            return false;
        }
    }

    child->parent = parent;
    child->name = xstrdup(name);
    parent->children[parent->child_count++] = child;
    return true;
}

static Object *object_find_child(Object *obj, const char *name)
{
    for (size_t i = 0; i < obj->child_count; i++)
    {
        if (!strcmp(obj->children[i]->name, name))
        {
            return obj->children[i];
        }
    }
    return NULL;
}

Object *object_resolve_path(Object *root, const char *path)
{
    Object *current = root;
    const char *cursor = path;

    if (*cursor != '/')
    {
        return NULL;
    }

    while (*cursor == '/')
    {
        cursor++;
    }

    while (*cursor)
    {
        char component[128];
        size_t length = 0;

        while (cursor[length] && cursor[length] != '/')
        {
            length++;
        }

        if (!length || length >= sizeof(component))
        {
            return NULL;
        }

        memcpy(component, cursor, length);

        component[length] = '\0';

        current = object_find_child(current, component);
        if (!current)
        {
            return NULL;
        }

        cursor += length;
        while (*cursor == '/')
        {
            cursor++;
        }
    }

    return current;
}
