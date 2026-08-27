#ifndef MINIQOM_TYPE_H
#define MINIQOM_TYPE_H

#include <stddef.h>

typedef struct Error Error;
typedef struct Object Object;
typedef struct ObjectClass ObjectClass;
typedef struct Type Type;

typedef void (*ClassInit)(ObjectClass *klass);
typedef void (*InstanceInit)(Object *obj);
typedef void (*InstanceFinalize)(Object *obj);
typedef bool (*InstanceRealize)(Object *obj, Error *err);
typedef void (*InstanceUnrealize)(Object *obj);

typedef struct TypeInfo
{
    const char *name;
    const char *parent;
    size_t instance_size;
    ClassInit class_init;
    InstanceInit instance_init;
    InstanceFinalize instance_finalize;
    InstanceRealize instance_realize;
    InstanceUnrealize instance_unrealize;
} TypeInfo;

void type_system_init(void);
void type_register_static(const TypeInfo *info);

Type *type_get_by_name(const char *name);
Type *type_get_parent(Type *type);
const TypeInfo *type_get_info(Type *type);
ObjectClass *type_get_class(Type *type);

#endif
