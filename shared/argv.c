#include <stddef.h>
#include <stdlib.h>

#include "common.h"
#include "error.h"
#include "argv.h"
#include "kissc/stpcpy_sp.h"

char **argv_copy(const char **args, char **error)
{
    char **copy;
    const char **p;

    for (p = args; NULL != *p; p++) {
        // NOP
    }
    if (NULL == (copy = calloc(p - args + 1, sizeof(*copy)))) {
        set_calloc_error(error, p - args + 1, sizeof(*copy));
    } else {
        for (p = args; NULL != *p; p++) {
            copy[p - args] = strdup(*p); // TODO: check strdup
        }
        copy[p - args] = NULL;
    }

    return copy;
}

void argv_free(const char **args)
{
    const char **p;

    for (p = args; NULL != *p; p++) {
        free((void *) *p);
    }
    free((void *) args);
}

bool argv_join(const char **args, char *buffer, const char * const buffer_end, char **error)
{
    char *w;
    const char **p;

    for (w = buffer, p = args; NULL != *p; p++) {
        char *oldw;

        if (w != buffer) {
            if (NULL == (w = stpcpy_sp(oldw = w, " ", buffer_end))) {
                set_buffer_overflow_error(error, " ", w, buffer_end - oldw);
                break;
            }
        }
        if (NULL == (w = stpcpy_sp(oldw = w, *p, buffer_end))) {
            set_buffer_overflow_error(error, *p, w, buffer_end - oldw);
            break;
        }
    }

    return NULL != w;
}
