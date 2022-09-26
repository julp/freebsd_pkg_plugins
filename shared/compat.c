#include <assert.h>
#include <string.h>
#include <pkg.h>

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
        if (0 == strcmp(key, k)) { // TODO: keys were insensitive so strcasecmp would make more sense?
            match = v;
        }
    }

    return match;
}
#endif /* HAVE_PKG_OBJECT_FIND */

void pkg_get_string_or_null(struct pkg *pkg, pkg_attr attr, const char **value)
{
    assert(NULL != pkg);
    assert(NULL != value);

#ifdef pkg_get_string
    {
        /**
          * NOTE: it seems like pkg_get_string doesn't allow NULL only/strictly strings
          *
          * Even weirder since e->type = 0 when e->string is NULL and pkg_el_t (which doesn't have any explicit 0 value)
          * has anything in common with pkg_object_t
          **/
        struct pkg_el *e;

        e = pkg_get_element(pkg, attr);
        *value = e->string;
    }
#else
    pkg_get(pkg, attr, *value);
#endif /* pkg_get_string */
}
