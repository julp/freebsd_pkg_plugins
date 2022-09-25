#include <assert.h>
#include <string.h>
#include <pkg.h>

// #include "common.h"

/**
 * ugly workaround to the removal of the function pkg_object_find by pkg 1.18
 *
 * See: https://github.com/freebsd/pkg/commit/825ab213eeccf16b3b48fd4f73beb895f730e627
 **/
#ifndef HAVE_PKG_OBJECT_FIND
const pkg_object *pkg_object_find(const pkg_object *object, const char *key)
{
    pkg_iter it;
    const pkg_object *v, *match;

    assert(NULL != object);
    assert(NULL != key);

    it = NULL;
    match = NULL;
    while (NULL == match && NULL != (v = pkg_object_iterate(object, &it))) {
        const char *k;

        k = pkg_object_key(v);
        // debug(">%s< = >%s<", k, pkg_object_dump(v));
        if (0 == strcmp(key, k)) { // TODO: keys were insensitive so strcasecmp would make more sense?
            match = v;
        }
    }

    return match;
}
#endif /* HAVE_PKG_OBJECT_FIND */
