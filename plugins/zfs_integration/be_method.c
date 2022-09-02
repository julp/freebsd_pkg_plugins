#include <limits.h>
#include <be.h>

#include "common.h"
#include "error.h"
#include "shared/os.h"
#include "kissc/stpcpy_sp.h"
#include "shared/path_join.h"
#include "backup_method.h"
#include "selection.h"

#ifdef DEBUG
# define set_be_error(error, lbh, format, ...) \
    _error_set(error, "[%s:%d] " format ": %s", __func__, __LINE__, ## __VA_ARGS__, libbe_error_description(lbh))
#else
# define set_be_error(error, lbh, format, ...) \
    _error_set(error, format ": %s", ## __VA_ARGS__, libbe_error_description(lbh))
#endif /* DEBUG */

// <defined by raw_zfs_method.c>
extern bool has_zfs_properties(uzfs_ptr_t *);
extern bool set_zfs_properties(uzfs_ptr_t *, const char *, char **);
// </defined by raw_zfs_method.c>

static const char *extract_name_from_be(nvpair_t *be)
{
    assert(NULL != be);

    return nvpair_name(be); // <=> nvlist_lookup_string(cur, "name", &name);
}

static bool extract_creation_from_be(nvpair_t *be, uint64_t *creation)
{
    bool ok;
    char property[] = "creation";

    assert(NULL != be);
    assert(NULL != creation);

    ok = false;
    *creation = 0;
    do {
        char *endptr, *v;
        nvlist_t *dsprops;
        unsigned long long value;

        dsprops = NULL;
        nvpair_value_nvlist(be, &dsprops);
        if (0 != nvlist_lookup_string(dsprops, property, &v)) {
            debug("skipping BE '%s', couldn't retrieve property '%s'", extract_name_from_be(be), property); // TODO
            break;
        }
        errno = 0;
        value = strtoull(v, &endptr, 10);
        if ('\0' != *endptr || (errno == ERANGE && value == ULLONG_MAX)) {
            debug("skipping BE '%s', value of property '%s' couldn't be properly parsed", extract_name_from_be(be), property); // TODO
            break;
        }
        *creation = (uint64_t) value;
        ok = true;
    } while (false);

    return ok;
}

static uzfs_ptr_t *be_to_fs(paths_to_check_t *ptc, libbe_handle_t *lbh, const char *be, char **error)
{
    uzfs_ptr_t *fs;

    fs = NULL;
    do {
        char dataset[ZFS_MAX_DATASET_NAME_LEN];

        if (!path_join(dataset, dataset + STR_SIZE(dataset), error, be_root_path(lbh), be, NULL)) {
            break;
        }
        if (NULL == (fs = uzfs_from_name(ptc->lh, dataset, UZFS_TYPE_FILESYSTEM))) {
            set_generic_error(error, "couldn't acquire a ZFS descriptor for BE '%s'", be);
            break;
        }
    } while (false);

    return fs;
}

typedef struct {
    char name[BE_MAXPATHLEN];
    uint64_t creation;
//     uzfs_ptr_t *fs;
} be_t;

static int compare_be_by_creation_date_desc(be_t *a, be_t *b)
{
    assert(NULL != a);
    assert(NULL != b);

    return (b->creation >= a->creation ? b->creation - a->creation : -1);
}

#if 0
static int compare_be_by_creation_date_asc(be_t *a, be_t *b)
{
    assert(NULL != a);
    assert(NULL != b);

    return (a->creation >= b->creation ? a->creation - b->creation : -1);
}
#endif

static void destroy_be(be_t *be)
{
    assert(NULL != be);

//     if (NULL != be->fs) {
//         uzfs_close(&be->fs);
//     }
    free(be);
}

// used as DupFunc
static be_t *copy_be(nvpair_t *bepair)
{
    be_t *be, *ret;

    be = ret = NULL;
    do {
        if (NULL == (be = malloc(sizeof(*be)))) {
            break;
        }
        if (strlcpy(be->name, extract_name_from_be(bepair), STR_SIZE(be->name)) >= STR_SIZE(be->name)) {
            break;
        }
        if (!extract_creation_from_be(bepair, &be->creation)) {
            break;
        }
        ret = be;
    } while (false);
    if (ret != be) {
        free(be);
    }

    return ret;
}

#ifdef DEBUG
# include <time.h>
// selection_dump(bes, (void (*)(void *)) print_be);
static void print_be(be_t *be)
{
    time_t time;
    struct tm tm;
    size_t written;
    char buffer[128];

    assert(NULL != be);

    time = (time_t) be->creation;
    localtime_r(&time, &tm);
    written = strftime(buffer, STR_SIZE(buffer), "%F %T", &tm);
    assert(written < STR_LEN(buffer));
    fprintf(stderr, "(%s) %s = %s (%" PRIu64 ")\n", __func__, be->name, buffer, be->creation);
}
#endif /* DEBUG */

static selection_t *fetch_sorted_zint_be(paths_to_check_t *ptc, libbe_handle_t *lbh, CmpFunc cmp, char **error)
{
    nvlist_t *props;
    selection_t *ret, *bes;

    assert(NULL != ptc);
    assert(NULL != lbh);
    assert(NULL != cmp);

    bes = ret = NULL;
    props = NULL;
    do {
        nvpair_t *cur;

        if (0 != be_prop_list_alloc(&props)) {
            set_be_error(error, lbh, "be_prop_list_alloc failed");
            break;
        }
        if (0 != be_get_bootenv_props(lbh, props)) {
            set_be_error(error, lbh, "be_get_bootenv_props failed");
            break;
        }
        if (NULL == (bes = selection_new(cmp, (DtorFunc) destroy_be, (DupFunc) copy_be))) {
            set_generic_error(error, "TODO");
            break;
        }
        for (cur = nvlist_next_nvpair(props, NULL); NULL != cur; cur = nvlist_next_nvpair(props, cur)) {
            uzfs_ptr_t *fs;
            const char *name;

            name = extract_name_from_be(cur);
            if (NULL == (fs = be_to_fs(ptc, lbh, name, error))) {
                // TODO: real error handling
                continue;
            }
            if (has_zfs_properties(fs)) {
                if (!selection_add(bes, cur)) {
                    // TODO: error handling
                }
            }
            if (NULL != fs) {
                uzfs_close(&fs);
            }
        }
        ret = bes;
    } while (false);
    if (NULL != props) {
        be_prop_list_free(props);
    }
    if (ret != bes) {
        selection_destroy(bes);
    }

    return ret;
}

static bool apply_retention(be_t *UNUSED(be), void *UNUSED(data), char **UNUSED(error))
{
    return true;
}

static bm_code_t be_suitable(paths_to_check_t *ptc, void **data, char **error)
{
    size_t i;
    bm_code_t retval;

    assert(NULL != ptc);
    assert(NULL != data);

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
#if 1 // TODO: TEST
        {
            bool ok;
            selection_t *bes;

            ok = false;
            bes = NULL;
            do {
                if (NULL == (bes = fetch_sorted_zint_be(ptc, lbh, (CmpFunc) compare_be_by_creation_date_desc, error))) {
                    break;
                }
                selection_dump(bes, (void (*)(void *)) print_be);
                if (!selection_apply(bes, (bool (*)(void *, void *, char **)) apply_retention, NULL /* TODO: data */, error)) {
                    break;
                }
                ok = true;
            } while (false);
            if (NULL != bes) {
                selection_destroy(bes);
            }
        }
#endif
        *data = lbh;
        retval = BM_OK;
    } while (false);

    return retval;
}

static bool be_take_snapshot(paths_to_check_t *ptc, const char *snapshot, const char *hook, void *data, char **error)
{
    bool ok;
    uzfs_ptr_t *fs;

    assert(NULL != ptc);
    assert(NULL != snapshot);
    assert(NULL != hook);
    assert(NULL != data);

    fs = NULL;
    ok = false;
    do {
        libbe_handle_t *lbh;

        lbh = (libbe_handle_t *) data;
        if (BE_ERR_SUCCESS != be_create(lbh, snapshot)) {
            set_be_error(error, lbh, "failed to create bootenv %s", snapshot);
            break;
        }
        if (NULL == (fs = be_to_fs(ptc, lbh, snapshot, error))) {
            break;
        }
        if (!set_zfs_properties(fs, hook, error)) {
            break;
        }
        ok = true;
    } while (false);
    if (NULL != fs) {
        uzfs_close(&fs);
    }

    return ok;
}

static bool be_rollback(paths_to_check_t *ptc, void *data, bool dry_run, bool temporary, char **error)
{
    bool ok;
    selection_t *bes;

    assert(NULL != ptc);
    assert(NULL != data);

    ok = false;
    do {
        be_t *last;
        libbe_handle_t *lbh;

        lbh = (libbe_handle_t *) data;
        if (NULL == (bes = fetch_sorted_zint_be(ptc, lbh, (CmpFunc) compare_be_by_creation_date_desc, error))) {
            break;
        }
        if (!selection_at(bes, 0, (void **) &last)) {
            set_generic_error(error, "no identified BE to rollback to");
            break;
        }
        if (!dry_run && BE_ERR_SUCCESS != be_activate(lbh, last->name, temporary)) {
            set_be_error(error, lbh, "failed to activate BE '%s'", last->name);
            break;
        }
        fprintf(stderr, "system %s rollbacked on BE '%s'\n", dry_run ? "would be" : "was", last->name);
        ok = true;
    } while (false);
    if (NULL != bes) {
        selection_destroy(bes);
    }

    return ok;
}

static bool be_list(paths_to_check_t *ptc, void *data, char **error)
{
    nvlist_t *props;
    selection_t *ret, *bes;

    assert(NULL != ptc);
    assert(NULL != data);

    bes = ret = NULL;
    props = NULL;
    do {
        nvpair_t *cur;
        libbe_handle_t *lbh;

        lbh = (libbe_handle_t *) data;
        if (0 != be_prop_list_alloc(&props)) {
            set_be_error(error, lbh, "be_prop_list_alloc failed");
            break;
        }
        if (0 != be_get_bootenv_props(lbh, props)) {
            set_be_error(error, lbh, "be_get_bootenv_props failed");
            break;
        }
        if (NULL == (bes = selection_new((CmpFunc) compare_be_by_creation_date_desc, (DtorFunc) destroy_be, (DupFunc) copy_be))) {
            set_generic_error(error, "TODO");
            break;
        }
        for (cur = nvlist_next_nvpair(props, NULL); NULL != cur; cur = nvlist_next_nvpair(props, cur)) {
            uzfs_ptr_t *fs;
            const char *name;

            name = extract_name_from_be(cur);
            if (NULL == (fs = be_to_fs(ptc, lbh, name, error))) {
                // TODO: real error handling
                continue;
            }
            if (has_zfs_properties(fs)) {
                if (!selection_add(bes, cur)) {
                    // TODO: error handling
                }
            }
            if (NULL != fs) {
                uzfs_close(&fs);
            }
        }
        ret = bes;
    } while (false);
    if (NULL != props) {
        be_prop_list_free(props);
    }
    if (ret != bes) {
        selection_destroy(bes);
    }

    return ret;
}

static bool be_rollback_to(const char *name, void *data, bool temporary, char **error)
{
    bool ok;

    assert(NULL != name);
    assert(NULL != data);

    ok = false;
    do {
        libbe_handle_t *lbh;

        lbh = (libbe_handle_t *) data;
        if (BE_ERR_SUCCESS != be_activate(lbh, name, temporary)) {
            set_be_error(error, lbh, "failed to activate BE '%s'", name);
            break;
        }
        ok = true;
    } while (false);

    return ok;
}

static bool be_destroy_by_name(const char *name, void *data, bool UNUSED(recursive), char **error)
{
    bool ok;

    assert(NULL != name);
    assert(NULL != data);

    ok = false;
    do {
        libbe_handle_t *lbh;

        lbh = (libbe_handle_t *) data;
        if (BE_ERR_SUCCESS != be_destroy(lbh, name, BE_DESTROY_ORIGIN)) {
            set_be_error(error, lbh, "failed to destroy BE '%s'", name);
            break;
        }
        ok = true;
    } while (false);

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
    be_list,
    be_rollback_to,
    be_destroy_by_name,
};
