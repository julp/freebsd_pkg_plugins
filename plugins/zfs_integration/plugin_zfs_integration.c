#include <sysexits.h> /* EX_USAGE */
#include <getopt.h>
#include <time.h>
#include <pkg.h>

#include "common.h"
#include "error.h"
#include "shared/os.h"
#include "backup_method.h"
#include "retention.h"

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

static bool force;
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

static bool take_snapshot(const char *scheme, const char *hook, char **error)
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
            set_generic_error(error, "unsufficient buffer to strftime '%s' into %zu bytes", scheme, STR_SIZE(snapshot));
            break;
        }
        if (!method->snapshot(ptc, snapshot, hook, method_data, error)) {
            break;
        }
        ok = true;
    } while (false);

    return ok;
}

static char pkg_zint_optstr[] = "nty";

static struct option pkg_zint_long_options[] = {
    { "dry-run",   no_argument, NULL, 'n' },
    { "temporary", no_argument, NULL, 't' },
    { "yes",       no_argument, NULL, 'y' },
    { NULL,        no_argument, NULL, 0 },
};

static void pkg_zint_usage(void)
{
    fprintf(stderr, "usage: pkg %s [-%s] rollback\n", NAME, pkg_zint_optstr);
}

static int pkg_zint_main(int argc, char **argv)
{
    int ch;
    char *error;
    pkg_error_t status;
    bool dry_run, temporary, yes;

    error = NULL;
    status = EPKG_FATAL;
    dry_run = temporary = yes = false;
    while (-1 != (ch = getopt_long(argc, argv, pkg_zint_optstr, pkg_zint_long_options, NULL))) {
        switch (ch) {
            case 'n':
                dry_run = true;
                break;
            case 't':
                temporary = true;
                break;
            case 'y':
                yes = true;
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
        if (!method->rollback(ptc, method_data, dry_run, temporary, &error)) {
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

static const char *hooks[] = {
#define HOOK(value, _event, name) \
    [ value ] = name,
#include "hooks.h"
};
#undef HOOK

int name_to_hook(const char *name)
{
    int hook;

    hook = -1;
    if (NULL != name) {
        size_t i;

        for (i = 0; i < ARRAY_SIZE(hooks); i++) {
            if (NULL != hooks[i] && 0 == strcmp(hooks[i], name)) {
                hook = i;
                break;
            }
        }
    }

    return hook;
}

static int real_handle_hooks(pkg_plugin_hook_t hook, const char *scheme, void *data)
{
    char *error;
    pkg_error_t status;
    struct pkg_jobs *jobs;

    error = NULL;
    status = EPKG_FATAL;
    jobs = (struct pkg_jobs *) data;
    do {
        if (0 == pkg_jobs_count(jobs) && !force) {
            status = EPKG_OK;
            break;
        }
        if (!take_snapshot(scheme, hooks[hook], &error)) {
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

// I use a xmacro because the callback doesn't know the pkg_plugin_hook_t/why it is called for
// It would be nice if it received the pkg_plugin_hook_t at its call in case you share a hook to handle several cases
#define HOOK(value, event, _name) \
    static int handle_ ## event ## _hook(void *data, struct pkgdb *UNUSED(db)) \
    { \
        return real_handle_hooks(value, "pkg_" #event "_%F_%T", data); \
    }
#include "hooks.h"
#undef HOOK

static const char *schemes[] = {
#define HOOK(value, event, _name) \
    [ value ] = "pkg_" #event "_%F_%T",
#include "hooks.h"
};
#undef HOOK

static char CFG_ON[] = "ON";
static char CFG_FORCE[] = "FORCE";

int pkg_plugin_init(struct pkg_plugin *p)
{
    char *error;
    pkg_error_t status;
    retention_t *retention;

    self = p;
    error = NULL;
    retention = NULL;

    pkg_plugin_set(p, PKG_PLUGIN_NAME, NAME);
    pkg_plugin_set(p, PKG_PLUGIN_DESC, DESCRIPTION);
    pkg_plugin_set(p, PKG_PLUGIN_VERSION, ZINT_VERSION_STRING);

    /**
     * Default configuration:
     *
     * RETENTION = "";
     * FORCE = false;
     * ON: [
     *     pre_upgrade,
     *     pre_deinstall,
     *     pre_autoremove,
     * ]
     */
    pkg_plugin_conf_add(p, PKG_BOOL, CFG_FORCE, "false");
    pkg_plugin_conf_add(p, PKG_ARRAY, CFG_ON, "pre_upgrade, pre_deinstall, pre_autoremove");
    pkg_plugin_conf_add(p, PKG_STRING, CFG_RETENTION, "");
    pkg_plugin_parse(p);

    do {
        uint64_t limit;
        pkg_object_t object_type;
        const pkg_object *config, *object;

        status = EPKG_FATAL;
        config = pkg_plugin_conf(p);
        debug("<config>\n%s\n</config>", pkg_object_dump(config));

        object = pkg_object_find(config, CFG_RETENTION);
        if (NULL == (retention = retention_parse(object, &limit, &error))) {
            break;
        }

        object = pkg_object_find(config, CFG_FORCE);
        force = pkg_object_bool(object);

        object = pkg_object_find(config, CFG_ON);
        object_type = pkg_object_type(object);
        if (PKG_ARRAY == object_type || PKG_OBJECT == object_type) {
            pkg_iter it;
            const pkg_object *item;

            it = NULL;
            debug("[ZINT] <%s>", CFG_ON);
            while (NULL != (item = pkg_object_iterate(object, &it))) {
                const char *k, *v;

                k = pkg_object_key(item);
                v = pkg_object_string(item);
                debug("[ZINT] %s = %s", k, v);
#define HOOK(value, event, _name) \
    if (0 == strcmp(#event, (NULL == k ? v : k))) { \
        if (NULL != k) { \
            schemes[value] = v; \
        } \
        if (EPKG_OK != (status = pkg_plugin_hook_register(p, value, handle_ ## event ## _hook))) { \
            pkg_plugin_error(p, "failed to hook %s (%d) into the library", value, #value); \
            break; \
        } \
        continue; \
    }
#include "hooks.h"
#undef HOOK
            }
            debug("[ZINT] </%s>", CFG_ON);
            if (EPKG_OK != status) {
                break;
            }
        } else {
            set_generic_error(&error, "configuration key '%s' is expected to be an array or an object but got: %s (%d)", CFG_ON, pkg_object_string(object), object_type);
            break;
        }
        if (NULL == (ptc = prober_create(&error))) {
            break;
        }
        if (EPKG_OK != (status = find_backup_method(&error))) {
            break;
        }
        assert(NULL != method);
    } while (false);
    if (NULL != retention) {
        retention_destroy(retention);
    }
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
