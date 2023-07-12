#include <stdlib.h> /* abort from pkg_get_string */
#include <pkg.h>

#include "common.h"
#include "error.h"
// #include "shared/compat.h"

static struct pkg_plugin *self;

static char DESCRIPTION[] = "Automated integrity checks";

static int real_handle_hooks(pkg_plugin_hook_t UNUSED(hook), void *data, struct pkgdb *db)
{
    bool ok;
    char *error;
    pkg_error_t status;

    ok = true;
    error = NULL;
    status = EPKG_FATAL;
    do {
        void *iter;
        int solved_type;
        // pkg_jobs_t job_type;
        struct pkg_jobs *jobs;
        struct pkg *new_pkg, *old_pkg;

        iter = NULL;
        jobs = (struct pkg_jobs *) data;
        // job_type = pkg_jobs_type(jobs);
        while (pkg_jobs_iter(jobs, &iter, &new_pkg, &old_pkg, &solved_type)) {
            if (PKG_SOLVED_DELETE == solved_type || PKG_SOLVED_UPGRADE_REMOVE == solved_type || PKG_SOLVED_UPGRADE == solved_type) {
                const char *name;
                struct pkgdb_it *it;

                pkg_get(old_pkg, PKG_ATTR_NAME, &name);
                if (NULL == (it = pkgdb_query(db, name, MATCH_EXACT))) {
                    set_generic_error(&error, "pkgdb_query failed");
                    break;
                }
                while (EPKG_OK == pkgdb_it_next(it, &old_pkg, PKG_LOAD_FILES)) {
                    if (ok &= EPKG_OK != pkg_test_filesum(old_pkg)) {
                        //status = EPKG_TODO;
                        pkg_printf("WARNING: checksum failed for package %s", name);
#ifdef DEBUG
                    } else {
                        debug("checksum OK for package %s", name);
#endif /* DEBUG */
                    }
                }
                pkgdb_it_free(it);
            }
        }
        if (NULL != error) {
            break;
        }
        status = EPKG_OK;
    } while (false);
    // TODO: if (!ok)
    if (EPKG_FATAL == status && NULL != error) {
        pkg_plugin_error(self, "%s", error);
        error_free(&error);
    }

    return status;
}

#define HOOK(value, event) \
    static int handle_ ## event ## _hook(void *data, struct pkgdb *db) \
    { \
        return real_handle_hooks(value, data, db); \
    }
#include "hooks.h"
#undef HOOK

static int handle_event(void *UNUSED(data), struct pkg_event *event)
{
    if (PKG_EVENT_INTEGRITYCHECK_CONFLICT == event->type) {
        // const char *name;

        // get_string(event->e_file_mismatch.pkg, PKG_NAME, &name);
        // ev->e_file_mismatch.file
        pkg_fprintf(stderr, "WARNING: checksum mismatch for %Fn (package %n-%v)\n", event->e_file_mismatch.file, event->e_file_mismatch.pkg);
    }

    return 0;
}

int pkg_plugin_init(struct pkg_plugin *p)
{
    char *error;
    pkg_error_t status;

    self = p;
    error = NULL;

    pkg_plugin_set(p, PKG_PLUGIN_NAME, NAME);
    pkg_plugin_set(p, PKG_PLUGIN_DESC, DESCRIPTION);
    pkg_plugin_set(p, PKG_PLUGIN_VERSION, INTEGRITY_VERSION_STRING);

    /**
     * Default configuration: none
     */
    pkg_plugin_parse(p);

    pkg_event_register(handle_event, NULL);
    do {
#define HOOK(value, event) \
    if (EPKG_OK != (status = pkg_plugin_hook_register(p, value, handle_ ## event ## _hook))) { \
        pkg_plugin_error(p, "failed to hook %s (%d) into the library", value, #value); \
        break; \
    }
#include "hooks.h"
#undef HOOK
    } while (false);
    if (EPKG_FATAL == status && NULL != error) {
        pkg_plugin_error(self, "%s", error);
        error_free(&error);
    }

    return status;
}
