#include <limits.h>
#include <be.h>

#include "common.h"
#include "error.h"
#include "shared/os.h"
#include "kissc/stpcpy_sp.h"
#include "shared/path_join.h"
#include "backup_method.h"

#ifdef DEBUG
# define set_be_error(error, lbh, format, ...) \
    _error_set(error, "[%s:%d] " format ": %s", __func__, __LINE__, ## __VA_ARGS__, libbe_error_description(lbh))
#else
# define set_be_error(error, lbh, format, ...) \
    _error_set(error, format ": %s", ## __VA_ARGS__, libbe_error_description(lbh))
#endif /* DEBUG */

static bm_code_t be_suitable(paths_to_check_t *ptc, void **data, char **error)
{
    size_t i;
    bm_code_t retval;

    retval = BM_ERROR;
    do {
        bool all_on_zfs;
        libbe_handle_t *lbh;

        all_on_zfs = true;
        for (i = 0; i < _FS_COUNT; i++) {
            all_on_zfs &= NULL != ptc->paths[i].fs;
        }
        // /usr/local is not on ZFS so it is not suitable for recovery
//         if (NULL == ptc->localbase.fs) {
        // not really suitable
        if (!all_on_zfs || !uzfs_same_fs(ptc->root.fs, ptc->localbase.fs) || !uzfs_same_fs(ptc->root.fs, ptc->pkg_dbdir.fs)) {
            retval = BM_SKIP;
            break;
        }
        if (NULL == (lbh = libbe_init(NULL))) {
            set_generic_error(error, "libbe initialisation failed");
            break;
        }
#ifdef DEBUG
        libbe_print_on_error(lbh, true); // TODO: explicit false when !DEBUG ?
#endif /* DEBUG */
        *data = lbh;
        retval = BM_OK;
    } while (false);

    return retval;
}

// bectl create -r beName(@snapshot) (après chaque opération) ? (fonction be_snapshot si snap_name est NULL il génère un nom avec les date/heure courantes)
// bectl activate -t beName (à la fin) ? (fonction be_activate)

static bool be_take_snapshot(const char *snapshot, void *data, char **error)
{
    bool ok;
    libbe_handle_t *lbh;

    assert(NULL != data);

    ok = false;
    lbh = (libbe_handle_t *) data;
    do {
        time_t t;
        struct tm ltm = { 0 };
        char be[BE_MAXPATHLEN];

        if (((time_t) -1) == time(&t)) {
            set_generic_error(error, "time(3) failed");
            break;
        }
        if (NULL == localtime_r(&t, &ltm)) {
            set_generic_error(error, "localtime_r(3) failed");
            break;
        }
        if (0 == strftime(be, STR_SIZE(be), snapshot, &ltm)) {
            set_generic_error(error, "unsufficient buffer to strftime '%s' into %zu bytes", snapshot, STR_SIZE(be));
            break;
        }
        if (BE_ERR_SUCCESS != be_create(lbh, be)) {
            set_be_error(error, lbh, "failed to create bootenv %s", snapshot);
            break;
        }
        ok = true;
    } while (false);

    return ok;
}

bool str_starts_with(const char *string, const char *prefix)
{
    size_t prefix_len;

    assert(NULL != string);
    assert(NULL != prefix);

    prefix_len = strlen(prefix);

    return prefix_len <= strlen(string) && 0 == strncmp(string, prefix, prefix_len);
}

static bool be_rollback(void *data, char **error)
{
    bool ok;
    bool temporary = false; // to be passed as argument?
    nvlist_t *props;
    libbe_handle_t *lbh;

    ok = false;
    props = NULL;
    lbh = (libbe_handle_t *) data;
    do {
        nvpair_t *cur;
        const char *last_bootenv_name;
        unsigned long long last_bootenv_creation;

        last_bootenv_name = NULL;
        if (0 != be_prop_list_alloc(&props)) {
            set_be_error(error, lbh, "be_prop_list_alloc failed");
            break;
        }
        if (0 != be_get_bootenv_props(lbh, props)) {
            set_be_error(error, lbh, "be_get_bootenv_props failed");
            break;
        }
        for (cur = nvlist_next_nvpair(props, NULL); NULL != cur; cur = nvlist_next_nvpair(props, cur)) {
            char *endptr;
            nvlist_t *dsprops;
            const char *bootenv;
            char *creation_as_string;
            unsigned long long creation;

            dsprops = NULL;
            bootenv = nvpair_name(cur);
            if (!str_starts_with(bootenv, "pkg_pre_upgrade_") || strlen(bootenv) != STR_LEN("pkg_pre_upgrade_YYYY-mm-dd_HH:ii:ss")) {
                debug("skipping BE %s, not created by pkg zint", bootenv);
                continue;
            }
            nvpair_value_nvlist(cur, &dsprops);
            if (0 != nvlist_lookup_string(dsprops, "creation", &creation_as_string)) {
                debug("skipping BE %s, couldn't retrieve its creation time", bootenv);
            }
            errno = 0;
            creation = strtoull(creation_as_string, &endptr, 10);
            if ('\0' != *endptr || (errno == ERANGE && creation == ULLONG_MAX)) {
                debug("skipping BE %s, creation time couldn't be properly parsed", bootenv);
            }
            if (NULL == last_bootenv_name || creation > last_bootenv_creation) {
                last_bootenv_name = bootenv;
                last_bootenv_creation = creation;
            }
// debug("%s = %s", bootenv, ctime((time_t *) &creation));
        }
        if (NULL == last_bootenv_name) {
            set_generic_error(error, "no BE identified to rollback to");
            break;
        } else if (BE_ERR_SUCCESS != be_activate(lbh, last_bootenv_name, temporary)) {
// debug("%s = %s", last_bootenv_name, ctime((time_t *) &last_bootenv_creation));
            set_be_error(error, lbh, "failed to activate BE '%s'", last_bootenv_name);
            break;
        }
        ok = true;
    } while (false);
    if (NULL != props) {
        be_prop_list_free(props);
    }

    return ok;
}

static void be_fini(void *data)
{
    libbe_handle_t *lbh;

    assert(NULL != data);

    lbh = (libbe_handle_t *) data;
    libbe_close(lbh);
}

const backup_method_t be_method = {
    "be",
    be_suitable,
    be_fini,
    be_take_snapshot,
    be_rollback,
};
