#include <sysexits.h> /* EX_USAGE */
#include <getopt.h>
#include <time.h>
#include <pkg.h>

#include "common.h"
#include "error.h"
#include "shared/os.h"
#include "backup_method.h"

#ifdef HAVE_BE
extern const backup_method_t be_method;
#endif /* HAVE_BE */
extern const backup_method_t none_method;
extern const backup_method_t raw_zfs_method;

const backup_method_t *available_methods[] = {
#ifdef HAVE_BE
    &be_method,
#endif /* HAVE_BE */
    &raw_zfs_method,
    &none_method,
};

static void *method_data;
static paths_to_check_t *ptc;
static struct pkg_plugin *self;
static const backup_method_t *method;

static char DESCRIPTION[] = "ZFS/BE integration to provide recovery";

static pkg_error_t find_backup_method(char **error)
{
    size_t i;
    pkg_error_t pkg_status;

    pkg_status = EPKG_OK;
    for (i = 0; i < ARRAY_SIZE(available_methods); i++) {
        if (NULL != available_methods[i]->suitable) {
            bm_code_t bm_status;

            bm_status = available_methods[i]->suitable(ptc, &method_data, error);
            if (BM_OK == bm_status) {
                pkg_status = EPKG_OK;
                method = available_methods[i];
                break;
            } else if (BM_ERROR == bm_status) {
                pkg_status = EPKG_FATAL;
                break;
            }
        }
    }

    return pkg_status;
}

static bool take_snapshot(const char *scheme, char **error)
{
    bool ok;

    ok = false;
    do {
        time_t t;
        struct tm ltm = { 0 };
        char snapshot[ZFS_MAX_NAME_LEN];

        if (((time_t) -1) == time(&t)) {
            set_generic_error(error, "time(3) failed");
            break;
        }
        if (NULL == localtime_r(&t, &ltm)) {
            set_generic_error(error, "localtime_r(3) failed");
            break;
        }
        if (0 == strftime(snapshot, STR_SIZE(snapshot), scheme, &ltm)) {
            set_generic_error(error, "buffer for strftime(3) was unsufficient for '%s'", scheme);
            break;
        }
        if (!method->snapshot(scheme, method_data, error)) {
            break;
        }
        ok = true;
    } while (false);

    return ok;
}

static char pkg_zint_optstr[] = "t";

static struct option pkg_zint_long_options[] = {
    { "temporary", no_argument, NULL, 't' },
    { NULL,        no_argument, NULL, 0 },
};

static void pkg_zint_usage(void)
{
    fprintf(stderr, "usage: pkg %s rollback\n", NAME);
}

static int pkg_zint_main(int argc, char **argv)
{
    int ch;
    char *error;
    bool temporary;
    pkg_error_t status;

    error = NULL;
    temporary = false;
    status = EPKG_FATAL;
    while (-1 != (ch = getopt_long(argc, argv, pkg_zint_optstr, pkg_zint_long_options, NULL))) {
        switch (ch) {
            case 't':
                temporary = true;
                break;
            default:
                status = EX_USAGE;
                break;
        }
    }
    argc -= optind;
    argv += optind;

    if (1 != argc || 0 != strcmp("rollback", argv[0])) {
        status = EX_USAGE;
    }

    do {
        if (EX_USAGE == status) {
            pkg_zint_usage();
            break;
        }
        if (!method->rollback(/*temporary, */method_data, &error)) {
            break;
        }
        status = EPKG_OK;
    } while (false);
    if (/*EPKG_FATAL == status && */NULL != error) {
        pkg_plugin_error(self, "%s", error);
        error_free(&error);
    }

    return status;
}

static int handle_upgrade_hook(const char *scheme, void *data/*, struct pkgdb *UNUSED(db)*/)
{
    char *error;
    pkg_error_t status;
    struct pkg_jobs *jobs;

    error = NULL;
    status = EPKG_FATAL;
    jobs = (struct pkg_jobs *) data;
    do {
        if (0 == pkg_jobs_count(jobs)) {
            status = EPKG_OK;
            break;
        }
        if (!take_snapshot(scheme, &error)) {
            break;
        }
        status = EPKG_OK;
    } while (false);
    if (EPKG_FATAL == status && NULL != error) {
        pkg_plugin_error(self, "%s", error);
        error_free(&error);
    }

    return status;
}

static int handle_pre_upgrade_hook(void *data, struct pkgdb *UNUSED(db))
{
    return handle_upgrade_hook("pkg_pre_upgrade_%F_%T", data/*, db*/);
}

#if 0
static int handle_post_upgrade_hook(void *data, struct pkgdb *UNUSED(db))
{
    return handle_upgrade_hook("pkg_post_upgrade_%F_%T", data/*, db*/);
}
#endif

int pkg_plugin_init(struct pkg_plugin *p)
{
    char *error;
    pkg_error_t status;

    self = p;
    error = NULL;

    pkg_plugin_set(p, PKG_PLUGIN_NAME, NAME);
    pkg_plugin_set(p, PKG_PLUGIN_DESC, DESCRIPTION);
    pkg_plugin_set(p, PKG_PLUGIN_VERSION, ZINT_VERSION_STRING);

    do {
        status = EPKG_FATAL;
        if (NULL == (ptc = prober_create(&error))) {
            break;
        }
        status = EPKG_OK;
        if (EPKG_OK != (status = find_backup_method(&error))) {
            break;
        }
        if (NULL == method) {
            break;
        }
        if (EPKG_OK != (status = pkg_plugin_hook_register(p, PKG_PLUGIN_HOOK_PRE_UPGRADE, handle_pre_upgrade_hook))) {
            pkg_plugin_error(self, "failed to set pre-upgrade hook");
            break;
        }
#if 0
        if (EPKG_OK != (status = pkg_plugin_hook_register(p, PKG_PLUGIN_HOOK_POST_UPGRADE, handle_post_upgrade_hook))) {
            pkg_plugin_error(self, "failed to set post-upgrade hook");
            break;
        }
#endif
    } while (false);
    if (EPKG_FATAL == status && NULL != error) {
        pkg_plugin_error(self, "%s", error);
        error_free(&error);
    }

    return status;
}

int pkg_register_cmd_count(void)
{
    return 1;
}

int pkg_register_cmd(int i, const char **name, const char **desc, int (**exec)(int argc, char **argv))
{
    assert(0 == i);

    (void) i; // quiet compiler when assert are disabled
    *name = NAME;
    *desc = DESCRIPTION;
    *exec = pkg_zint_main;

    return EPKG_OK;
}

int pkg_plugin_shutdown(struct pkg_plugin *UNUSED(p))
{
    if (NULL != method && NULL != method->fini) {
        method->fini(method_data);
    }
    if (NULL != ptc) {
        prober_destroy(ptc);
    }

    return EPKG_OK;
}
