#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <search.h>

#include <stdbool.h>
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <libzfs.h>
#include <zfs_prop.h>

#ifdef DEBUG
# define set_zfs_error(error, lzh, format, ...) \
    _error_set(error, "[%s:%d] " format ": %s %s", __func__, __LINE__, ## __VA_ARGS__, libzfs_error_action(lzh), libzfs_error_description(lzh))
#else
# define set_zfs_error(error, lzh, format, ...) \
    _error_set(error, format ": %s %s", ## __VA_ARGS__, libzfs_error_action(lzh), libzfs_error_description(lzh))
#endif /* DEBUG */

#include "common.h"
#include "zfs.h"
// #include <zfeature_common.h>
#include "error.h"
#include "stpcpy_sp.h"

#if __FreeBSD_version < 1202000
# define zfs_iter_snapshots_sorted(zhp, callback, data, min_txg, max_txg) \
    zfs_iter_snapshots_sorted(zhp, callback, data)
#endif /* FreeBSD < 12.2 */

// NOTE: functions are prefixed of a 'u' for "userland" to avoid clashes with actual (lib)zfs functions

struct uzfs_lib_t {
    libzfs_handle_t *lh;
};

struct uzfs_pool_t {
    zpool_handle_t *ph;
};

struct uzfs_fs_t {
    zfs_handle_t *fh;
};

static inline libzfs_handle_t *lh_from_fs(uzfs_fs_t *fs)
{
    return zfs_get_handle(fs->fh);
}

#if 0 /* unused */
static inline libzfs_handle_t *lh_from_pool(uzfs_pool_t *pool)
{
    return zpool_get_handle(pool->ph);
}

static inline zpool_handle_t *ph_from_pool(uzfs_pool_t *pool)
{
    return pool->ph;
}

static inline zfs_handle_t *fh_from_fs(uzfs_fs_t *fs)
{
    return fs->fh;
}
#endif

/* ========== (un)initialization ========== */

uzfs_lib_t *uzfs_init(char **error)
{
    uzfs_lib_t *lib;

    if (NULL == (lib = malloc(sizeof(*lib)))) {
        set_malloc_error(error, sizeof(*lib));
    } else {
        if (NULL == (lib->lh = libzfs_init())) {
            free(lib);
            lib = NULL;
            set_generic_error(error, "libzfs_init failed");
        } else {
#ifndef DEBUG
            libzfs_print_on_error(lib->lh, false);
#endif /* !DEBUG */
        }
    }

    return lib;
}

void uzfs_fini(uzfs_lib_t *lib)
{
    assert(NULL != lib);
    assert(NULL != lib->lh);

//     zpool_free_handles(lib->lh);
    libzfs_fini(lib->lh);
    free(lib);
}

/* ========== File systems/mountpoints related helpers ========== */

static bool uzfs_statfs(const char *path, struct statfs *buf)
{
    assert(NULL != buf);

    return 0 == statfs(path, buf) && 0 == strcmp(buf->f_fstypename, "zfs");
}

static zfs_handle_t *uzfs_get_fs_from_file(libzfs_handle_t *lh, const char *path)
{
    zfs_handle_t *zh;
    struct statfs buf;

    zh = NULL;
    if (uzfs_statfs(path, &buf)) {
        zh = zfs_open(lh, buf.f_mntfromname, ZFS_TYPE_FILESYSTEM);
    }

    return zh;
}

/* ========== public ========== */

/**
 * Determine if the given directory is an **actual** mountpoint for a ZFS filesystem
 *
 * @param path the mountpoint/directory/file
 *
 * @return `false` if *path* is not a mountpoint to a ZFS filesystem
 */
bool uzfs_is_fs(const char *path)
{
    struct statfs buf;
    char normalized[PATH_MAX];

    return NULL != realpath(path, normalized) && uzfs_statfs(normalized, &buf) && 0 == strcmp(normalized, buf.f_mntonname);
}

static int snaphosts_iter_callback_keep_last(zfs_handle_t *fh, void *data)
{
    *((const char **) data) = zfs_get_name(fh);
    zfs_close(fh);

    return 0;
}

#if 0
static
#endif
bool uzfs_last_snapshot(uzfs_lib_t *lib, char *to, char *last_snapshot, size_t last_snapshot_len, char **error)
{
    bool ok;
    zfs_handle_t *fh;
    const char *last;
    char buffer[ZFS_MAX_DATASET_NAME_LEN];

    assert(NULL != last_snapshot);

    last = NULL;
    *buffer = *last_snapshot = '\0';

    ok = NULL != (fh = zfs_path_to_zhandle(lib->lh, to, ZFS_TYPE_FILESYSTEM));
    if (ok) {
        ok = 0 == zfs_iter_snapshots_sorted(fh, snaphosts_iter_callback_keep_last, &last, 0, 0);
        zfs_close(fh);
    } else {
        set_zfs_error(error, lib->lh, "%s is not a ZFS filesystem", to);
    }
    if (ok && NULL != last) {
        if (strlcpy(last_snapshot, last, last_snapshot_len) >= last_snapshot_len) {
            set_buffer_overflow_error(error, last, last_snapshot, last_snapshot_len);
        }
    }

    return ok;
}

static const char *snapshot_skip_filesystem(const char *snapshot)
{
    const char *retval, *at;

    if (NULL == (at = strchr(snapshot, '@'))) {
        retval = snapshot;
    } else {
        retval = at + 1;
    }

    return retval;
}

struct snapshot_t {
    zfs_handle_t *fh;
    TAILQ_ENTRY(snapshot_t) entries;
};

static int snaphosts_iter_callback_build_array(zfs_handle_t *fh, void *data)
{
    struct snapshot_t *e;
    TAILQ_HEAD(tailhead, snapshot_t) *head;

    head = (struct tailhead *) data;
    e = malloc(sizeof(*e));
    assert(NULL != e);
    e->fh = fh;
    TAILQ_INSERT_HEAD(head, e, entries);

    return 0;
}

/**
 * Keep the count lastest snapshots, (recursively - in descendants) deleting the others.
 *
 * @param fs the base ZFS filesystem to cleanup
 * @param count the number of the last snapshots to keep
 * @param deleted send back to the caller the number of snapshots which have been successfully destroyed, can be `NULL` to ignore
 * @param error
 *
 * @return `false` on failure
 */
bool uzfs_retain_snapshots(uzfs_fs_t *fs, size_t count, size_t *deleted, char **error)
{
    bool ok;
    size_t destroyed;
    struct snapshot_t *e1, *e2;
    TAILQ_HEAD(tailhead, snapshot_t) head;

    assert(NULL != fs);
    assert(0 != count);

    ok = true;
    destroyed = 0;
    TAILQ_INIT(&head);
    zfs_iter_snapshots_sorted(fs->fh, snaphosts_iter_callback_build_array, &head, 0, 0);
    if (!TAILQ_EMPTY(&head)) {
        size_t i;

        i = 0;
        e1 = TAILQ_FIRST(&head);
        while (ok && e1 != NULL) {
            e2 = TAILQ_NEXT(e1, entries);
            if (i >= count) {
                const char *name;

                name = zfs_get_name(e1->fh);
                if (0 != zfs_destroy_snaps(fs->fh, (char *) snapshot_skip_filesystem(name), true/*defer*/)) {
                    ok = false;
                    set_zfs_error(error, lh_from_fs(fs), "zfs_destroy_snaps failed to delete %s", name);
                } else {
                    ++destroyed;
                }
            }
            zfs_close(e1->fh); // should be done between zfs_get_name and zfs_destroy_snaps?
            free(e1);
            e1 = e2;
            ++i;
        }
    }
    if (NULL != deleted) {
        *deleted = destroyed;
    }

    return ok;
}

/**
 * Take a snapshot of a filesystem, equivalent to: zfs snapshot *filesystem*@$(date "+*scheme*")
 *
 * @param filesystem the name of the filesystem
 * @param scheme the name of the snapshot
 * @param strftime_scheme when set to `true`, substitute strftime(3) modifiers in *scheme*
 * @param name an output buffer where to put the name of the snapshot (recommanded size: ZFS_MAX_NAME_LEN)
 * @param name_size the size of *name*
 * @param recursive `true` to also snapshots children filesystems
 * @param error
 *
 * @return `false` on failure
 */
bool uzfs_snapshot(uzfs_fs_t *fs, const char *scheme, bool strftime_scheme, char *name, size_t name_size, bool recursive, char **error)
{
    bool ok;

    assert(NULL != fs);
    assert(NULL != scheme);
    assert(NULL != name);

    ok = false;
    do {
        char *w;
        libzfs_handle_t *lh;
        const char *filesystem;
        const char * const name_end = name + name_size;

        lh = lh_from_fs(fs);
        filesystem = uzfs_fs_get_name(fs);
        if (NULL == (w = stpcpy_sp(name, filesystem, name_end))) {
            set_buffer_overflow_error(error, filesystem, name, name_size);
            break;
        }
        // NOTE: no check, at least we just overwrite the '\0' and we recheck buffer size below
        *w++ = '@';
        // NOTE: this test is not really usefull, this is a just to create a subscope/compartimentize time stuffs
        if (strftime_scheme) {
            time_t t;
            struct tm ltm = { 0 };

            if (((time_t) -1) == time(&t)) {
                set_generic_error(error, "time(3) failed");
                break;
            }
            if (NULL == localtime_r(&t, &ltm)) {
                set_generic_error(error, "localtime_r(3) failed");
                break;
            }
            if (0 == strftime(w, name_end - w, scheme, &ltm)) {
                set_buffer_overflow_error(error, scheme, w, name_end - w);
                break;
            }
        } else {
            char *oldw;

            oldw = w;
            if (NULL == (w = stpcpy_sp(w, scheme, name_end))) {
                set_buffer_overflow_error(error, scheme, w, name_end - oldw);
                break;
            }
        }
        if (!zfs_name_valid(name, ZFS_TYPE_SNAPSHOT)) {
            set_generic_error(error, "'%s' is not a valid ZFS snapshot name", name);
            break;
        }
        ok = 0 == zfs_snapshot(lh, name, recursive, NULL);
        if (!ok) {
            set_zfs_error(error, lh, "zfs_snapshot failed to create snapshot '%s'", name);
        }
    } while (false);

    return ok;
}

/**
 * Destroy a filesystem, equivalent to the command: zfs destroy *filesystem*
 *
 * @param filesystem the name of the filesystem to destroy, closed and reset to `NULL` if successfull
 * @param error
 *
 * @return `true` if successfull
 *
 * @note this operation is recursive, children filesystems/snapshots/bookmarks will
 * also be deleted!!!
 */
bool uzfs_filesystem_destroy(uzfs_fs_t **fs, char **error)
{
    bool ok;
    zfs_handle_t *fh;
    libzfs_handle_t *lh;

    assert(NULL != fs);
    assert(NULL != *fs);

    ok = false;
    fh = (*fs)->fh;
    lh = lh_from_fs(*fs);
    do {
        if (zfs_is_shared(fh)) {
            if (0 != zfs_unshareall(fh)) {
                set_zfs_error(error, lh, "zfs_unshareall %s failed", zfs_get_name(fh));
                break;
            }
        }
        if (zfs_is_mounted(fh, NULL)) {
            if (0 != zfs_unmountall(fh, 0)) {
                set_zfs_error(error, lh, "zfs_unmountall %s failed", zfs_get_name(fh));
                break;
            }
        }
        ok = 0 == zfs_destroy(fh, false/*defer*/);
        if (ok) {
            uzfs_fs_close(*fs);
            *fs = NULL;
        } else {
            set_zfs_error(error, lh, "zfs_destroy %s failed", zfs_get_name(fh));
//             break;
        }
    } while (false);

    return ok;
}

static uzfs_fs_t *fs_wrap(zfs_handle_t *);

static uzfs_pool_t *pool_wrap(zpool_handle_t *ph)
{
    uzfs_pool_t *pool;

    assert(NULL != ph);
    pool = malloc(sizeof(*pool));
    assert(NULL != pool);
    pool->ph = ph;

    return pool;
}

/**
 * Get a ZFS pool from its name
 *
 * @param name the name of the targeted pool
 *
 * @return the corresponding pool, `NULL` if there is no such pool
 */
uzfs_pool_t *uzfs_pool_from_name(uzfs_lib_t *lib, const char *name)
{
    uzfs_pool_t *pool;
    zpool_handle_t *ph;

    assert(NULL != name);

    pool = NULL;
    if (NULL != (ph = zpool_open_canfail(lib->lh, name))) {
        pool = pool_wrap(ph);
    }

    return pool;
}

/**
 * Get a pool descriptor from a child ZFS filesystem
 *
 * @param fs the descendant ZFS filesytem
 *
 * @return a pool descriptor to its pool
 */
uzfs_pool_t *uzfs_pool_from_fs(uzfs_fs_t *fs)
{
    assert(NULL != fs);

    return pool_wrap(zfs_get_pool_handle(fs->fh));
}

/**
 * Free memory associated to a pool descriptor
 * (it does not affect the pool itself)
 *
 * @param pool the pool pointer to free
 */
void uzfs_pool_close(uzfs_pool_t *pool)
{
    assert(NULL != pool);

    zpool_close(pool->ph);
    free(pool);
}

/* FileSystem */

static uzfs_fs_t *fs_wrap(zfs_handle_t *fh)
{
    uzfs_fs_t *fs;

    assert(NULL != fh);
    fs = malloc(sizeof(*fs));
    assert(NULL != fs);
    fs->fh = fh;

    return fs;
}

/**
 * Get the name of the pool
 *
 * @param pool a descriptor to the pool
 *
 * @return the name of the pool
 */
const char *uzfs_pool_get_name(uzfs_pool_t *pool)
{
    assert(NULL != pool);

    return zpool_get_name(pool->ph);
}

/**
 * Get a descriptor on a ZFS filesystem from where a file is located
 *
 * @param path the path of the file
 *
 * @return `NULL` if *path* is not located on a ZFS filesystem or a descriptor of the
 * ZFS filesystem where the file is located.
 */
uzfs_fs_t *uzfs_fs_from_file(uzfs_lib_t *lib, const char *path)
{
    uzfs_fs_t *fs;
    zfs_handle_t *fh;

    assert(NULL != path);

    fs = NULL;
    if (NULL != (fh = uzfs_get_fs_from_file(lib->lh, path))) {
        fs = fs_wrap(fh);
    }

    return fs;
}

bool uzfs_same_pool(uzfs_fs_t *fs1, uzfs_fs_t *fs2)
{
    const char *pool1_name, *pool2_name;

    assert(NULL != fs1);
    assert(NULL != fs2);

    pool1_name = zfs_get_pool_name(fs1->fh);
    pool2_name = zfs_get_pool_name(fs2->fh);

    return 0 == strcmp(pool1_name, pool2_name);
}

bool uzfs_same_fs(uzfs_fs_t *fs1, uzfs_fs_t *fs2)
{
    const char *fs1_name, *fs2_name;

    assert(NULL != fs1);
    assert(NULL != fs2);

    fs1_name = zfs_get_name(fs1->fh);
    fs2_name = zfs_get_name(fs2->fh);

    return 0 == strcmp(fs1_name, fs2_name);
}

/**
 * Get a descriptor to a ZFS filesystem from its complete name
 *
 * @param name the full name of the targeted ZFS filesystem
 *
 * @return `NULL` if there is no such filesystem or a descriptor to it
 */
uzfs_fs_t *uzfs_fs_from_name(uzfs_lib_t *lib, const char *name)
{
    uzfs_fs_t *fs;
    zfs_handle_t *fh;

    fs = NULL;
    if (NULL != (fh = zfs_open(lib->lh, name, ZFS_TYPE_FILESYSTEM))) {
        fs = fs_wrap(fh);
    }

    return fs;
}

/**
 * Get the full name (including the prefix "<pool's name>/" of a ZFS filesystem
 *
 * @param fs a descriptor the concerned ZFS filesystem
 *
 * @return its complete name
 */
const char *uzfs_fs_get_name(uzfs_fs_t *fs)
{
    assert(NULL != fs);

    return zfs_get_name(fs->fh);
}

/**
 * Free memory associated to a ZFS filesystem descriptor
 * (this does not affect the ZFS filesystem itself)
 *
 * @param fs the descriptor to a ZFS filesystem to close
 */
void uzfs_fs_close(uzfs_fs_t *fs)
{
    assert(NULL != fs);

    zfs_close(fs->fh);
    free(fs);
}

bool uzfs_fs_prop_get(uzfs_fs_t *fs, const char *name, char *value, size_t value_size)
{
    bool ok;

    assert(NULL != fs);
    assert(NULL != name);
    assert(NULL != value);

    ok = false;
    do {
        if (NULL != strchr(name, ':')) {
            nvlist_t *props, *propval;

            if (NULL == (props = zfs_get_user_props(fs->fh))) {
                break;
            }
            if (0 != nvlist_lookup_nvlist(props, name, &propval)) {
                break;
            }
            if (0 != nvlist_lookup_string(propval, ZPROP_VALUE, &value)) {
                break;
            }
        } else {
            int prop;

            if (ZPROP_INVAL == (prop = zprop_name_to_prop(name, zfs_get_type(fs->fh)))) {
                //set_generic_error(error, "property '%s' is not valid", name);
                break;
            }
            if (0 != zfs_prop_get(fs->fh, prop, value, value_size, NULL, NULL, 0, B_TRUE)) {
                break;
            }
        }
        ok = true;
    } while (false);

    return ok;
}

bool uzfs_fs_prop_get_numeric(uzfs_fs_t *fs, const char *name, uint64_t *value)
{
    bool ok;

    assert(NULL != fs);
    assert(NULL != name);
    assert(NULL != value);

    ok = false;
    do {
        if (NULL != strchr(name, ':')) {
            char *propstr, *endptr;
            nvlist_t *props, *propval;
            unsigned long long propui;

            if (NULL == (props = zfs_get_user_props(fs->fh))) {
                break;
            }
            if (0 != nvlist_lookup_nvlist(props, name, &propval)) {
                break;
            }
            if (0 != nvlist_lookup_string(propval, ZPROP_VALUE, &propstr)) {
                break;
            }
            propui = strtoull(propstr, &endptr, 10);
            if ('\0' != *endptr || (errno == ERANGE && propui == ULLONG_MAX)) {
                break;
            }
            *value = propui;
        } else {
            int prop;

            if (ZPROP_INVAL == (prop = zprop_name_to_prop(name, zfs_get_type(fs->fh)))) {
                //set_generic_error(error, "property '%s' is not valid", name);
                break;
            }
            // uint64_t zfs_prop_get_int(zfs_handle_t *, zfs_prop_t);
            if (0 != zfs_prop_get_numeric(fs->fh, prop, value, NULL, NULL, 0)) {
                break;
            }
        }
        ok = true;
    } while (false);

    return ok;
}

bool uzfs_fs_prop_set(uzfs_fs_t *fs, const char *name, const char *value, char **error)
{
    int ret;

    assert(NULL != fs);
    assert(NULL != name);
    assert(NULL != value);

    if (-1 == (ret = zfs_prop_set(fs->fh, name, value))) {
        set_zfs_error(error, lh_from_fs(fs), "failed to set property '%s' to '%s'", name, value);
    }

    return -1 != ret;
}

bool uzfs_fs_prop_set_numeric(uzfs_fs_t *fs, const char *name, uint64_t value, char **error)
{
    int ret;
    char buffer[128];

    assert(NULL != fs);
    assert(NULL != name);

    ret = snprintf(buffer, STR_SIZE(buffer), "%" PRIu64, value);
    assert(ret > 0 && ((size_t) ret) <= STR_SIZE(buffer));
    if (-1 == (ret = zfs_prop_set(fs->fh, name, buffer))) {
        set_zfs_error(error, lh_from_fs(fs), "failed to set property '%s' to '%" PRIu64 "'", name, value);
    }

    return -1 != ret;
}
