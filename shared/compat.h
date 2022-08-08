#pragma once

// pkg < 1.18: pkg_get was replaced by subfunctions for type checking
#ifndef pkg_get_string
# define pkg_get_string(pkg, attr, var) \
  pkg_get(pkg, attr, &var)
#else
# include <stdlib.h>
#endif /* pkg_get_string */

// pkg >= 1.18: pkg_object_find was removed
#ifndef HAVE_PKG_OBJECT_FIND
const pkg_object *pkg_object_find(const pkg_object *object, const char *key);
#endif /* HAVE_PKG_OBJECT_FIND */
