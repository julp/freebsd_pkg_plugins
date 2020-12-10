#include "common.h"
#include "error.h"
#include "shared/os.h"
#include "probe.h"

paths_to_check_t *prober_create(char **error)
{
    bool ok;
    paths_to_check_t *ptc;

    ok = false;
    ptc = NULL;
    do {
        size_t i;

        if (NULL == (ptc = malloc(sizeof(*ptc)))) {
            set_malloc_error(error, sizeof(*ptc));
            break;
        }
        if (NULL == (ptc->lh = uzfs_init(error))) {
            break;
        }
        ptc->root.path = "/";
        for (i = 0; i < _FS_COUNT; i++) {
            ptc->paths[i].fs = NULL;
        }
        if (NULL == (ptc->localbase.path = localbase())) {
            set_generic_error(error, "unable to determine LOCALBASE");
            break;
        }
//         assert(NULL != ptc->localbase.path);
        if (NULL == (ptc->pkg_dbdir.path = pkg_dbdir())) {
            set_generic_error(error, "unable to determine PKG_DBDIR");
            break;
        }
//         assert(NULL != ptc->pkg_dbdir.path);
        for (i = 0; i < _FS_COUNT; i++) {
            ptc->paths[i].fs = uzfs_fs_from_file(ptc->lh, ptc->paths[i].path);
//             debug("%s is %s on ZFS", ptc->paths[i].path, NULL == ptc->paths[i].fs ? "not" : "");
        }
        ok = true;
    } while (false);

    if (!ok) {
        if (NULL != ptc) {
            uzfs_fini(ptc->lh);
            free(ptc);
            ptc = NULL;
        }
    }

    return ptc;
}

void prober_destroy(paths_to_check_t *ptc)
{
    size_t i;

    assert(NULL != ptc);

    for (i = 0; i < _FS_COUNT; i++) {
        if (NULL != ptc->paths[i].fs) {
            uzfs_fs_close(ptc->paths[i].fs);
        }
    }
    uzfs_fini(ptc->lh);
    free(ptc);
}
