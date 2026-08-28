#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "miniqom/property.h"
#include "miniqom/type.h"

ObjectClass *object_class_new(Type *type)
{
    ObjectClass *klass = calloc(1, sizeof(*klass));

    assert(klass != NULL);
    klass->type = type;
    return klass;
}

void object_class_free(ObjectClass *klass)
{
    free(klass);
}

void object_class_property_add(ObjectClass *klass,
                               const char *name,
                               PropertyType type,
                               PropertySetter set,
                               PropertyGetter get)
{
    assert(klass->property_count < MINIQOM_MAX_PROPERTIES);

    klass->properties[klass->property_count++] = (Property){
        .name = name,
        .type = type,
        .set = set,
        .get = get,
    };
}

static const Property *object_property_find(Object *obj, const char *name)
{
    for (Type *type = obj->klass->type; type; type = type_get_parent(type))
    {
        ObjectClass *klass = type_get_class(type);

        for (size_t i = 0; i < klass->property_count; i++)
        {
            if (!strcmp(klass->properties[i].name, name))
            {
                return &klass->properties[i];
            }
        }
    }
    return NULL;
}

static bool parse_bool(const char *text, bool *value)
{
    if (!strcmp(text, "on") || !strcmp(text, "true") || !strcmp(text, "1"))
    {
        *value = true;
        return true;
    }

    if (!strcmp(text, "off") || !strcmp(text, "false") || !strcmp(text, "0"))
    {
        *value = false;
        return true;
    }
    return false;
}

static bool parse_u64(const char *text, uint64_t *value)
{
    char *end = NULL;
    uint64_t multiplier = 1;
    unsigned long long number;

    errno = 0;
    number = strtoull(text, &end, 0);
    if (errno || end == text)
    {
        return false;
    }

    if (*end)
    {
        switch (toupper((unsigned char)*end++))
        {
        case 'K':
            multiplier = 1024ULL;
            break;

        case 'M':
            multiplier = 1024ULL * 1024ULL;
            break;

        case 'G':
            multiplier = 1024ULL * 1024ULL * 1024ULL;
            break;

        default:
            return false;
        }
    }

    if (*end || number > UINT64_MAX / multiplier)
    {
        return false;
    }

    *value = number * multiplier;
    return true;
}

bool object_property_set_from_string(Object *obj, const char *name,
                                     const char *text, Error *err)
{
    const Property *property = object_property_find(obj, name);
    PropertyValue value = {0};

    if (!property)
    {
        error_set(err, "property not found");
        return false;
    }

    switch (property->type)
    {
    case PROP_BOOL:
        if (!parse_bool(text, &value.b))
        {
            error_set(err, "invalid boolean");
            return false;
        }
        break;

    case PROP_U64:
        if (!parse_u64(text, &value.u64))
        {
            error_set(err, "invalid unsigned integer");
            return false;
        }
        break;

    case PROP_STRING:
        value.str = text;
        break;
    }

    return property->set(obj, value, err);
}

bool object_property_get(Object *obj,
                         const char *name,
                         PropertyValue *value,
                         Error *err)
{
    const Property *property;

    if (!obj || !name || !value)
    {
        error_set(err, "invalid property get arguments");
        return false;
    }

    property = object_property_find(obj, name);

    if (!property)
    {
        error_set(err, "property not found");
        return false;
    }

    if (!property->get)
    {
        error_set(err, "property is not readable");
        return false;
    }
    return property->get(obj, value, err);
}