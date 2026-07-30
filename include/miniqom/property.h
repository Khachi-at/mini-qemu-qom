#ifndef MINIQOM_PROPERTY_H
#define MINIQOM_PROPERTY_H

#include <stdbool.h>
#include <stdint.h>

#include "miniqom/error.h"
#include "miniqom/object.h"

#define MINIQOM_MAX_PROPERTIES 16

typedef enum PropertyType
{
    PROP_BOOL,
    PROP_U64,
    PROP_STRING,
} PropertyType;

typedef union PropertyValue
{
    bool b;
    uint64_t u64;
    const char *str;
} PropertyValue;

typedef bool (*PropertySetter)(Object *obj, PropertyValue value, Error *err);

typedef struct Property
{
    const char *name;
    PropertyType type;
    PropertySetter set;
} Property;

struct ObjectClass
{
    Type *type;
    Property properties[MINIQOM_MAX_PROPERTIES];
    size_t property_count;
};

ObjectClass *object_class_new(Type *type);
void object_class_free(ObjectClass *klass);

void object_class_property_add(ObjectClass *klass, const char *name,
                               PropertyType type, PropertySetter set);

bool object_property_set_from_string(Object *obj, const char *name,
                                     const char *value, Error *err);

#endif