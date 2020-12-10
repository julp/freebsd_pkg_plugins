#include <stddef.h>

#include "common.h"
#include "error.h"
#include "path_join.h"
#include "kissc/stpcpy_sp.h"

bool path_join(char *buffer, const char * const buffer_end, char **error, ...)
{
    char *w;
    va_list ap;
    const char *arg;

    assert(NULL != buffer);
    assert(NULL != buffer_end);

    w = buffer;
    va_start(ap, error);
    while (NULL != (arg = va_arg(ap, const char *))) {
        char *oldw;

        if (w != buffer) {
            oldw = w;
            if (NULL == (w = stpcpy_sp(w, "/", buffer_end))) {
                set_buffer_overflow_error(error, "/", w, buffer_end - oldw);
                break;
            }
        }
        oldw = w;
        if (NULL == (w = stpcpy_sp(w, arg, buffer_end))) {
            set_buffer_overflow_error(error, arg, w, buffer_end - oldw);
            break;
        }
    }
    va_end(ap);

    return NULL != w;
}
