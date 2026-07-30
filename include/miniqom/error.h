#ifndef MINIQOM_ERROR_H
#define MINIQOM_ERROR_H

#include <stdio.h>

typedef struct Error
{
    char message[256];
} Error;

static inline void error_clear(Error *err)
{
    err->message[0] = '\0';
}

static inline void error_set(Error *err, const char *message)
{
    snprintf(err->message, sizeof(err->message), "%s", message);
}

#endif