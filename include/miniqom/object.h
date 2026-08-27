#ifndef MINIQOM_OBJECT_H
#define MINIQOM_OBJECT_H

#include <stdbool.h>
#include <stddef.h>

#include "miniqom/error.h"
#include "miniqom/type.h"

#define MINIQOM_MAX_CHILDREN 32

typedef enum ObjectState
{
    OBJECT_STATE_NEW,
    OBJECT_STATE_REALIZED,
} ObjectState;

struct Object
{
    ObjectClass *klass;
    Object *parent;
    char *name;
    Object *children[MINIQOM_MAX_CHILDREN];
    size_t child_count;
    ObjectState state;
};

Object *object_new(const char *type_name);
void object_free(Object *obj);

bool object_is_instance_of(const Object *obj, const char *type_name);

const char *object_get_type_name(const Object *obj);
bool object_is_a(const Object *obj, const char *type_name);

bool object_realize(Object *obj, Error *err);
void object_unrealize(Object *obj);
bool object_is_realized(const Object *obj);

bool object_add_child(Object *parent, const char *name,
                      Object *child, Error *err);

Object *object_resolve_path(Object *root, const char *path);

#endif