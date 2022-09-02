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
#ifdef DEBUG
        {
# define FMT "%*s %*s"
            char none[] = "-";
//             char buffer[4096] = {0};
            const char *columns[] = {
                "DIRECTORY",
                "ZFS",
            };
            int sizes[ARRAY_SIZE(columns)];

            for (i = 0; i < ARRAY_SIZE(columns); i++) {
                sizes[i] = strlen(columns[i]);
            }
            for (i = 0; i < _FS_COUNT; i++) {
                int l;

                if ((l = strlen(ptc->paths[i].path)) > sizes[0]) {
                    sizes[0] = l;
                }
                if ((l = NULL == ptc->paths[i].fs ? STR_LEN(none) : strlen(uzfs_get_name(ptc->paths[i].fs))) > sizes[1]) {
                    sizes[1] = l;
                }
            }
#if 0
            memset(buffer, '-', STR_LEN(buffer));
            for (i = 0; i < ARRAY_SIZE(columns); i++) {
                if (i == 0) {
                    fputc('+', stderr);
                }
                fprintf(stderr, "%.*s+", sizes[i] + 2, buffer);
            }
            fputc('\n', stderr);
            for (i = 0; i < ARRAY_SIZE(columns); i++) {
                if (i == 0) {
                    fputs("| ", stderr);
                }
                fprintf(stderr, "%*s | ", -sizes[i], columns[i]);
            }
            fputc('\n', stderr);
#else
            debug("<ZFS probing result>");
            debug(FMT, -sizes[0], columns[0], -sizes[1], columns[1]);
#endif
            for (i = 0; i < _FS_COUNT; i++) {
                debug(FMT, -sizes[0], ptc->paths[i].path, -sizes[1], NULL == ptc->paths[i].fs ? none : uzfs_get_name(ptc->paths[i].fs));
            }
            debug("</ZFS probing result>");
        }
#endif /* DEBUG */
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
            uzfs_close(&ptc->paths[i].fs);
        }
    }
    uzfs_fini(ptc->lh);
    free(ptc);
}
