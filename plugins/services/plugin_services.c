#include <sysexits.h> /* EX_USAGE */
#include <getopt.h>

#include "common.h"
#include "error.h"
#include "shared/os.h"
#include "kissc/hashtable.h"
#include "services.h"
#include "rcorder.h"
#include "kissc/stpcpy_sp.h"

static struct pkg_plugin *self;

static char NAME[] = "services";
static char VERSION[] = "0.6.0";
static char DESCRIPTION[] = "Management of services";

static char CFG_BLOCKLIST[] = "BLOCKLIST";

static char pkg_rcorder_optstr[] = "ork:s:";

static struct option pkg_rcorder_long_options[] = {
    { "reverse", no_argument,       NULL, 'r' },
    { "orphan",  no_argument,       NULL, 'o' },
    { "keep",    required_argument, NULL, 'k' },
    { "skip",    required_argument, NULL, 's' },
    { NULL,      no_argument,       NULL, 0   },
};

static void pkg_rcorder_usage(void)
{
    fputs("usage: pkg rcorder [-ro] [-k keep] [-s skip]\n", stderr);
    fputs("-r, --reverse\n", stderr);
    fputs("\tdisplay rc.d scripts in reverse order\n", stderr);
    fputs("-o, --orphan\n", stderr);
    fputs("\tinclude rc.d scripts which are not provided by any package (custom scripts and from base system)\n", stderr);
    fputs("-k, --keep\n", stderr);
    fputs("\tonly include rc.d scripts with *keep* as KEYWORD(S)\n", stderr);
    fputs("-s, --skip\n", stderr);
    fputs("\tignore rc.d scripts with *skip* as KEYWORD(S)\n", stderr);
}

static void rcorder_print_script(const rc_d_script_t *script, void *UNUSED(user_data))
{
    const char *path;

    script_get(script, SCRIPT_ATTR_PATH, &path);
    printf("%s\n", path);
}

static bool databases_open(struct pkgdb **pkg_db, services_db_t **services_db, char **error)
{
    bool ok;

    ok = false;
    do {
        if (EPKG_OK != pkgdb_open(pkg_db, PKGDB_DEFAULT)) {
            set_generic_error(error, "Cannot open database");
            break;
        }
        if (EPKG_OK != pkgdb_obtain_lock(*pkg_db, PKGDB_LOCK_READONLY)) {
            set_generic_error(error, "Cannot get a read lock on a database, it is locked by another process");
            break;
        }
        if (NULL == (*services_db = services_db_create(error))) {
            break;
        }
        if (!services_db_scan_system(*pkg_db, *services_db, error)) {
            break;
        }
        ok = true;
    } while (false);

    return ok;
}

static void databases_close(struct pkgdb *pkg_db, services_db_t *services_db)
{
    if (NULL != pkg_db) {
        pkgdb_release_lock(pkg_db, PKGDB_LOCK_READONLY);
        pkgdb_close(pkg_db);
    }
    if (NULL != services_db) {
        services_db_close(services_db);
    }
}

static int pkg_rcorder_main(int argc, char **argv)
{
    char *error;
    pkg_error_t status;
    struct pkgdb *pkg_db;
    rcorder_options_t *ro;
    services_db_t *services_db;

    ro = NULL;
    error = NULL;
    pkg_db = NULL;
    status = EPKG_FATAL;
    services_db = NULL;
    do {
        int ch;

        if (NULL == (ro = rcorder_options_create(&error))) {
            break;
        }
        while (-1 != (ch = getopt_long(argc, argv, pkg_rcorder_optstr, pkg_rcorder_long_options, NULL))) {
            switch (ch) {
                case 'o':
                    rcorder_options_set_include_orphans(ro, true);
                    break;
                case 'r':
                    rcorder_options_set_reverse(ro, true);
                    break;
                case 'k':
                    rcorder_options_add_ks(ro, optarg, RCORDER_ACTION_KEEP);
                    break;
                case 's':
                    rcorder_options_add_ks(ro, optarg, RCORDER_ACTION_SKIP);
                    break;
                default:
                    status = EX_USAGE;
                    break;
            }
        }
        argc -= optind;
        argv += optind;

        if (0 != argc) {
            status = EX_USAGE;
        }

        if (EX_USAGE == status) {
            pkg_rcorder_usage();
            break;
        }
        if (!databases_open(&pkg_db, &services_db, &error)) {
            break;
        }
        services_db_rcorder_iter(services_db, ro, rcorder_print_script, NULL);
        status = EPKG_OK;
    } while (false);
    if (NULL != ro) {
        rcorder_options_destroy(ro);
    }
    databases_close(pkg_db, services_db);
    if (NULL != error) {
        pkg_plugin_error(self, "%s", error);
        error_free(&error);
    }

    return status;
}

static char pkg_services_optstr[] = "r";

static struct option pkg_services_long_options[] = {
    { "required", no_argument,       NULL, 'r' },
    { NULL,       no_argument,       NULL, 0   },
};

static void pkg_services_usage(void)
{
    fputs("usage: pkg services [-r] package ...\n", stderr);
    fputs("-r, --required\n", stderr);
    fputs("\tdisplay all services which are required by *package*\n", stderr);
}

static int pkg_services_main(int argc, char **argv)
{
    int ch;
    char *error;
    bool required;
    pkg_error_t status;
    struct pkgdb *pkg_db;
    services_db_t *services_db;

    error = NULL;
    pkg_db = NULL;
    required = false;
    services_db = NULL;
    status = EPKG_FATAL;
    while (-1 != (ch = getopt_long(argc, argv, pkg_services_optstr, pkg_services_long_options, NULL))) {
        switch (ch) {
            case 'r':
                required = true;
                break;
            default:
                pkg_services_usage();
                return EX_USAGE;
        }
    }
    argc -= optind;
    argv += optind;

    do {
        int i;

        if (!databases_open(&pkg_db, &services_db, &error)) {
            break;
        }
        for (i = 0; i < argc; i++) {
            Iterator it;
            const rc_d_script_t *script;

            if (0 != i) {
                fputc('\n', stdout);
            }
            if (required) {
                services_db_rshlib(&it, services_db, argv[i]);
            } else {
                package_to_services_iterator(&it, services_db, argv[i]);
            }
            iterator_first(&it);
            if (iterator_is_valid(&it, NULL, &script)) {
                printf(required ? "The package %s is required by the following service(s):\n" : "The package %s provides the following service(s):\n", argv[i]);
                do {
                    const char *name, *path;

                    script_get(script, SCRIPT_ATTR_NAME, &name, SCRIPT_ATTR_PATH, &path);
                    printf("- %s (%s)\n", name, path);
                    iterator_next(&it);
                } while (iterator_is_valid(&it, NULL, &script));
            } else {
                printf(required ? "The package %s is not required by any service\n" : "The package %s does not provide any service\n", argv[i]);
            }
            iterator_close(&it);
        }
        status = EPKG_OK;
    } while (false);
    databases_close(pkg_db, services_db);
    if (NULL != error) {
        pkg_plugin_error(self, "%s", error);
        error_free(&error);
    }

    return status;
}

static const char *results_description[2][SERVICES_RESULT_COUNT] = {
    [ SERVICE_ACTION_STOP - 1 ] = {
        [ SERVICES_RESULT_FAILED ] = "services failing to stop",
        [ SERVICES_RESULT_BLOCKED ] = "services not stopped due to blocklist",
        [ SERVICES_RESULT_SUCCESS ] = "services successfully stopped",
        [ SERVICES_RESULT_PROBING_FAILED ] = "services failing to probe",
    },
    [ SERVICE_ACTION_RESTART - 1 ] = {
        [ SERVICES_RESULT_FAILED ] = "services failing to restart",
        [ SERVICES_RESULT_BLOCKED ] = "services not restarted due to blocklist",
        [ SERVICES_RESULT_SUCCESS ] = "services successfully restarted",
        [ SERVICES_RESULT_PROBING_FAILED ] = "services failing to probe",
    },
};

static void print_services_result_details(Iterator *it, service_action_t action, service_status_t status)
{
    const char *name;
    char *w, buffer[8192];
    const char * const buffer_end = buffer + STR_SIZE(buffer);

    w = buffer;
    *buffer = '\0';
    for (iterator_first(it); iterator_is_valid(it, NULL, &name); iterator_next(it)) {
        if (w != buffer) {
            if (NULL == (w = stpcpy_sp(w, ", ", buffer_end))) {
                break;
            }
        }
        if (NULL == (w = stpcpy_sp(w, name, buffer_end))) {
            break;
        }
    }
    iterator_close(it);
    if ('\0' != *buffer) {
        pkg_plugin_info(self, "%s: %s", results_description[action - 1][status], buffer); // TODO: pkg_plugin_info vs pkg_plugin_error
    }
}

static void print_services_result_helper(services_result_t *sr, service_action_t action)
{
    size_t i;

    for (i = 0; i < SERVICES_RESULT_COUNT; i++) {
        Iterator it;

        services_result_to_iterator(&it, sr, action, i);
        print_services_result_details(&it, action, i);
    }
}

static void print_services_result_summary(services_result_t *sr)
{
    print_services_result_helper(sr, SERVICE_ACTION_STOP);
    print_services_result_helper(sr, SERVICE_ACTION_RESTART);
}

static int handle_hooks(void *data, struct pkgdb *pkg_db)
{
    char *error;
    pkg_error_t status;
    struct pkg_jobs *jobs;
    services_result_t *sr;
    services_selection_t *ss;
    services_db_t *services_db;

    sr = NULL;
    ss = NULL;
    error = NULL;
    services_db = NULL;
    status = EPKG_FATAL;
    jobs = (struct pkg_jobs *) data;

//     assert(pkg_jobs_count(jobs) > 0);
    do {
        void *iter;
        int solved_type;
        pkg_jobs_t job_type;
        struct pkg *new_pkg, *old_pkg;

        iter = NULL;
        job_type = pkg_jobs_type(jobs);
        if (NULL == (services_db = services_db_create(&error))) {
            break;
        }
        if (NULL == (ss = services_selection_create(&error))) {
            break;
        }
        if (PKG_JOBS_UPGRADE == job_type) {
            pkg_iter it;
            const pkg_object *config, *blocked, *blocklist;

            it = NULL;
            config = blocklist = NULL;
            config = pkg_plugin_conf(self);
            blocklist = pkg_object_find(config, CFG_BLOCKLIST);
            while (NULL != (blocked = pkg_object_iterate(blocklist, &it))) {
                services_selection_block(ss, pkg_object_string(blocked));
            }
        }
        if (!services_db_scan_system(pkg_db, services_db, &error)) {
            break;
        }
        while (pkg_jobs_iter(jobs, &iter, &new_pkg, &old_pkg, &solved_type)) {
            char *pkg_name;

            pkg_get(new_pkg,
                PKG_NAME, &pkg_name
            );
            if (PKG_SOLVED_DELETE == solved_type) {
                services_db_add_services_from_package_to_services_selection(services_db, ss, pkg_name, SERVICE_ACTION_STOP, false);
            }
            if (PKG_SOLVED_UPGRADE == solved_type) {
                services_db_add_services_from_package_to_services_selection(services_db, ss, pkg_name, SERVICE_ACTION_RESTART, true);
            }
        }
        sr = services_selection_handle(ss, &error);
        print_services_result_summary(sr);
        status = EPKG_OK;
    } while (false);
    if (NULL != sr) {
        services_result_destroy(sr);
    }
    if (NULL != ss) {
        services_selection_destroy(ss);
    }
    if (NULL != services_db) {
        services_db_close(services_db);
    }
    if (/*EPKG_OK != status && */NULL != error) {
        pkg_plugin_error(self, "%s", error);
        error_free(&error);
    }

    return status;
}

#define H(value) \
    {value, #value}

static const struct {
    pkg_plugin_hook_t value;
    const char *name;
} hooks[] = {
    H(PKG_PLUGIN_HOOK_PRE_INSTALL),
//     H(PKG_PLUGIN_HOOK_POST_INSTALL),
    H(PKG_PLUGIN_HOOK_PRE_DEINSTALL),
//     H(PKG_PLUGIN_HOOK_POST_DEINSTALL),
//     H(PKG_PLUGIN_HOOK_PRE_FETCH),
//     H(PKG_PLUGIN_HOOK_POST_FETCH),
//     H(PKG_PLUGIN_HOOK_EVENT),
//     H(PKG_PLUGIN_HOOK_PRE_UPGRADE),
    H(PKG_PLUGIN_HOOK_POST_UPGRADE),
    H(PKG_PLUGIN_HOOK_PRE_AUTOREMOVE),
//     H(PKG_PLUGIN_HOOK_POST_AUTOREMOVE),
//     PKG_PLUGIN_HOOK_PKGDB_CLOSE_RW,
};

#undef H

int pkg_plugin_init(struct pkg_plugin *p)
{
    size_t i;

    self = p;

    pkg_plugin_set(p, PKG_PLUGIN_NAME, NAME);
    pkg_plugin_set(p, PKG_PLUGIN_DESC, DESCRIPTION);
    pkg_plugin_set(p, PKG_PLUGIN_VERSION, VERSION);

    pkg_plugin_conf_add(p, PKG_ARRAY, CFG_BLOCKLIST, "sddm, hald, dbus");
    pkg_plugin_parse(p);

    for (i = 0; i < ARRAY_SIZE(hooks); i++) {
        if (EPKG_OK != pkg_plugin_hook_register(p, hooks[i].value, handle_hooks)) {
            pkg_plugin_error(p, "failed to hook %s (%d) into the library", hooks[i].name, hooks[i].value);
            return EPKG_FATAL;
        }
    }

    return EPKG_OK;
}

int pkg_register_cmd_count(void)
{
    return 2;
}

int pkg_register_cmd(int i, const char **name, const char **desc, int (**exec)(int argc, char **argv))
{
    switch (i) {
        case 0:
            *name = NAME;
            *desc = DESCRIPTION;
            *exec = pkg_services_main;
            break;
        case 1:
            *name = "rcorder";
            *desc = "a reimplementation of rcorder as part of pkg";
            *exec = pkg_rcorder_main;
            break;
        default:
            assert(false);
            break;
    }

    return EPKG_OK;
}

int pkg_plugin_shutdown(struct pkg_plugin *UNUSED(p))
{
    return EPKG_OK;
}
