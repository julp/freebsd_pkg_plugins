#include <pkg.h>

#include "common.h"
#include "error.h"

static struct pkg_plugin *self;

#undef D
#define D(c) \
    [ c ] = #c

static const char *change_names[] = {
    D(PKG_DOWNGRADE),
    D(PKG_REINSTALL),
    D(PKG_UPGRADE),
    "(not applicable)",
};

static const char *job_names[] = {
    D(PKG_JOBS_INSTALL),
    D(PKG_JOBS_DEINSTALL),
    D(PKG_JOBS_FETCH),
    D(PKG_JOBS_AUTOREMOVE),
    D(PKG_JOBS_UPGRADE),
};

static const char *solve_names[] = {
    D(PKG_SOLVED_INSTALL),
    D(PKG_SOLVED_DELETE),
    D(PKG_SOLVED_UPGRADE),
    D(PKG_SOLVED_UPGRADE_REMOVE),
    D(PKG_SOLVED_FETCH),
    D(PKG_SOLVED_UPGRADE_INSTALL),
};

#undef D

static int handle_hooks(void *data, struct pkgdb *UNUSED(pkg_db))
{
    pkg_error_t status;
    struct pkg_jobs *jobs;

    status = EPKG_FATAL;
    jobs = (struct pkg_jobs *) data;
    do {
        void *iter;
        int solved_type;
        pkg_jobs_t job_type;
        struct pkg *new_pkg, *old_pkg;

        iter = NULL;
        job_type = pkg_jobs_type(jobs);
        while (pkg_jobs_iter(jobs, &iter, &new_pkg, &old_pkg, &solved_type)) {
            int change_type;
            char *pkg_name, *origin, *version, *old_version, *repo;

            change_type = ARRAY_SIZE(change_names) - 2;
            pkg_get(new_pkg,
                PKG_NAME, &pkg_name,
                PKG_ORIGIN, &origin,
                PKG_VERSION, &version,
                PKG_OLD_VERSION, &old_version,
                PKG_REPONAME, &repo
            );
            if (NULL != old_pkg) {
                change_type = pkg_version_change_between(new_pkg, old_pkg);
            }
            pkg_plugin_info(self, "%s/%s (from %s) %s => %s: job = %s, change = %s, solve = %s", origin, pkg_name, repo, NULL == old_version ? "-" : old_version, version, job_names[job_type], change_names[change_type], solve_names[solved_type]);
        }
        status = EPKG_OK;
    } while (false);

    return status;
}

#define H(value) \
    {value, #value}

static const struct {
    pkg_plugin_hook_t value;
    const char *name;
} hooks[] = {
//     H(PKG_PLUGIN_HOOK_PRE_INSTALL),
    H(PKG_PLUGIN_HOOK_POST_INSTALL),
//     H(PKG_PLUGIN_HOOK_PRE_DEINSTALL),
    H(PKG_PLUGIN_HOOK_POST_DEINSTALL),
//     H(PKG_PLUGIN_HOOK_PRE_FETCH),
//     H(PKG_PLUGIN_HOOK_POST_FETCH),
//     H(PKG_PLUGIN_HOOK_EVENT),
//     H(PKG_PLUGIN_HOOK_PRE_UPGRADE),
    H(PKG_PLUGIN_HOOK_POST_UPGRADE),
//     H(PKG_PLUGIN_HOOK_PRE_AUTOREMOVE),
    H(PKG_PLUGIN_HOOK_POST_AUTOREMOVE),
//     PKG_PLUGIN_HOOK_PKGDB_CLOSE_RW,
};

#undef H

int pkg_plugin_init(struct pkg_plugin *p)
{
    size_t i;

    self = p;

    pkg_plugin_set(p, PKG_PLUGIN_NAME, "verbose");
    pkg_plugin_set(p, PKG_PLUGIN_DESC, "a plugin for development and testing");
    pkg_plugin_set(p, PKG_PLUGIN_VERSION, "1.0.0");

    for (i = 0; i < ARRAY_SIZE(hooks); i++) {
        if (EPKG_OK != pkg_plugin_hook_register(p, hooks[i].value, handle_hooks)) {
            pkg_plugin_error(p, "failed to hook %s (%d) into the library", hooks[i].name, hooks[i].value);
            return EPKG_FATAL;
        }
    }

    return EPKG_OK;
}

int pkg_plugin_shutdown(struct pkg_plugin *UNUSED(p))
{
    return EPKG_OK;
}
