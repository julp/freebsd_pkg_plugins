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

// <TODO: removal when dropping support for pkg < 1.20>
// pkg >= 1.20: PKG_* constants were renamed to PKG_ATTR_* (commit d9c65f6), see: https://github.com/freebsd/pkg/commit/d9c65f6896cf264bcf9926d553c32d92ddfb1ffb
#ifndef HAVE_PKG_ATTR
enum {
    PKG_ORIGIN = PKG_ATTR_ORIGIN,
    PKG_NAME = PKG_ATTR_NAME,
    PKG_VERSION = PKG_ATTR_VERSION,
    PKG_COMMENT = PKG_ATTR_COMMENT,
    PKG_DESC = PKG_ATTR_DESC,
    PKG_MTREE = PKG_ATTR_MTREE,
    PKG_MESSAGE = PKG_ATTR_MESSAGE,
    PKG_ARCH = PKG_ATTR_ARCH,
    PKG_ABI = PKG_ATTR_ABI,
    PKG_MAINTAINER = PKG_ATTR_MAINTAINER,
    PKG_WWW = PKG_ATTR_WWW,
    PKG_PREFIX = PKG_ATTR_PREFIX,
    PKG_REPOPATH = PKG_ATTR_REPOPATH,
    PKG_CKSUM = PKG_ATTR_CKSUM,
    PKG_OLD_VERSION = PKG_ATTR_OLD_VERSION,
    PKG_REPONAME = PKG_ATTR_REPONAME,
    PKG_REPOURL = PKG_ATTR_REPOURL,
    PKG_DIGEST = PKG_ATTR_DIGEST,
    PKG_REASON = PKG_ATTR_REASON,
    PKG_FLATSIZE = PKG_ATTR_FLATSIZE,
    PKG_OLD_FLATSIZE = PKG_ATTR_OLD_FLATSIZE,
    PKG_PKGSIZE = PKG_ATTR_PKGSIZE,
    PKG_LICENSE_LOGIC = PKG_ATTR_LICENSE_LOGIC,
    PKG_AUTOMATIC = PKG_ATTR_AUTOMATIC,
    PKG_LOCKED = PKG_ATTR_LOCKED,
    PKG_ROWID = PKG_ATTR_ROWID,
    PKG_TIME = PKG_ATTR_TIME,
    PKG_ANNOTATIONS = PKG_ATTR_ANNOTATIONS,
    PKG_UNIQUEID = PKG_ATTR_UNIQUEID,
    PKG_OLD_DIGEST = PKG_ATTR_OLD_DIGEST,
    PKG_DEP_FORMULA = PKG_ATTR_DEP_FORMULA,
    PKG_VITAL = PKG_ATTR_VITAL,
    PKG_CATEGORIES = PKG_ATTR_CATEGORIES,
    PKG_LICENSES = PKG_ATTR_LICENSES,
    PKG_NUM_FIELDS = PKG_ATTR_NUM_FIELDS,
    // the following were external to pkg_attr
    // great they still are defined as pkg_list
    // PKG_GROUPS = PKG_ATTR_GROUPS,
    // PKG_USERS = PKG_ATTR_USERS,
    // PKG_SHLIBS_REQUIRED = PKG_ATTR_SHLIBS_REQUIRED,
    // PKG_SHLIBS_PROVIDED = PKG_ATTR_SHLIBS_PROVIDED,
    // PKG_PROVIDES = PKG_ATTR_PROVIDES,
    // PKG_REQUIRES = PKG_ATTR_REQUIRES,
    // PKG_CONFLICTS = PKG_ATTR_CONFLICTS,
};
#endif /* !HAVE_PKG_ATTR */
// </TODO: removal when dropping support for pkg < 1.20>

void get_string(struct pkg *, pkg_attr, const char **);
void get_stringlist(struct pkg *, pkg_attr, struct pkg_stringlist **);
