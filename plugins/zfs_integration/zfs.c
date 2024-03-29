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
# define zfs_iter_snapshots(zhp, callback, data, min_txg, max_txg) \
    zfs_iter_snapshots(zhp, callback, data)

# define zfs_iter_snapshots_sorted(zhp, callback, data, min_txg, max_txg) \
    zfs_iter_snapshots_sorted(zhp, callback, data)
#endif /* FreeBSD < 12.2 */

#if __FreeBSD_version < 1400000
# define zfs_is_shared(zhp, _where, _protocols) \
    zfs_is_shared(zhp)
# define zfs_unshareall(zhp, _protocols) \
    zfs_unshareall(zhp)
#endif /* FreeBSD < 14.0.0 */

// NOTE: functions are prefixed of a 'u' for "userland" to avoid clashes with actual (lib)zfs functions

typedef void (*uzfs_close_callback_t)(void *ptr);
typedef const char *(*uzfs_get_name_callback_t)(void *ptr);
typedef void *(*uzfs_from_name_callback_t)(libzfs_handle_t *lh, const char *name, int types);
typedef const char *(*uzfs_to_pool_name_callback_t)(void *ptr);
typedef libzfs_handle_t *(*uzfs_to_libzfs_handle_callback_t)(void *ptr);
typedef bool (*uzfs_get_prop_callback_t)(void *ptr, const char *name, char *value, size_t value_size);
typedef int (*uzfs_set_prop_callback_t)(void *ptr, const char *name, const char *value);

#if 0
uint64_t zpool_get_prop_int(zpool_handle_t *, zpool_prop_t, zprop_source_t *);
int zfs_prop_get_numeric(zfs_handle_t *, zfs_prop_t, uint64_t *, zprop_source_t *, char *, size_t);
#endif

typedef struct {
    zfs_type_t type;
    uzfs_close_callback_t close;
    uzfs_get_name_callback_t get_name;
    uzfs_from_name_callback_t from_name;
    uzfs_to_pool_name_callback_t to_pool_name;
    uzfs_to_libzfs_handle_callback_t to_libzfs_handle;
    uzfs_get_prop_callback_t get_prop;
    uzfs_set_prop_callback_t set_prop;
} zfs_klass_t;

struct uzfs_lib_t {
    libzfs_handle_t *lh;
};

struct uzfs_ptr_t {
    union {
        void *ptr;
        zfs_handle_t *fh;
        zpool_handle_t *ph;
    };
    const zfs_klass_t *klass;
};

/* ========== assertions ========== */

#define assert_valid_uzfs_type_t(/*uzfs_type_t*/ type) \
    do { \
        assert(type >= UZFS_TYPE_FIRST && type <= UZFS_TYPE_LAST); \
    } while (false);

#define assert_valid_uzfs_ptr_t(/*uzfs_ptr_t **/h) \
    do { \
        assert(NULL != (h)); \
        assert(NULL != (h)->ptr); \
        assert(NULL != (h)->klass); \
    } while (false);

#define assert_valid_uzfs_lib_t(/*uzfs_lib_t **/lib) \
    do { \
        assert(NULL != lib); \
        assert(NULL != lib->lh); \
    } while (false);

#define assert_uzfs_ptr_t_is(/*uzfs_ptr_t **/h, /*int*/ types) \
    do { \
        assert_valid_uzfs_ptr_t(h); \
        assert(0 == ((h)->klass->type & ~(types))); \
    } while (false);

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
    assert_valid_uzfs_lib_t(lib);

//     zpool_free_handles(lib->lh);
    libzfs_fini(lib->lh);
    free(lib);
}

/* ========== zfs_handle_t/zpool_handle_t abstraction stuffs ========== */

static void *uzfs_zpool_from_name(libzfs_handle_t *lh, const char *name, int UNUSED(types))
{
    return zpool_open_canfail(lh, name);
}

static bool uzfs_zpool_get_prop(void *ptr, const char *name, char *value, size_t value_size)
{
    bool ok;

    assert(NULL != ptr);
    assert(NULL != name);
    assert(NULL != value);

    ok = false;
    do {
        int prop;
        zpool_handle_t *zh;

        zh = (zpool_handle_t *) ptr;
        if (ZPROP_INVAL == (prop = zprop_name_to_prop(name, ZFS_TYPE_POOL))) {
            //set_generic_error(error, "property '%s' is not valid", name);
            break;
        }
        if (0 != zpool_get_prop(zh, prop, value, value_size, NULL, B_TRUE)) {
            break;
        }
        ok = true;
    } while (false);

    return ok;
}

static bool uzfs_dataset_get_prop(void *ptr, const char *name, char *value, size_t value_size)
{
    bool ok;

    assert(NULL != ptr);
    assert(NULL != name);
    assert(NULL != value);

    ok = false;
    do {
        zfs_handle_t *fh;

        fh = (zfs_handle_t *) ptr;
        if (NULL != strchr(name, ':')) {
            const char *v;
            nvlist_t *props, *propval;

            if (NULL == (props = zfs_get_user_props(fh))) {
                break;
            }
            if (0 != nvlist_lookup_nvlist(props, name, &propval)) {
                break;
            }
            if (0 != nvlist_lookup_string(propval, ZPROP_VALUE, &v)) {
                break;
            }
            if (strlcpy(value, v, value_size) >= value_size) {
                break;
            }
        } else {
            int prop;

            if (ZPROP_INVAL == (prop = zprop_name_to_prop(name, zfs_get_type(fh)))) {
                //set_generic_error(error, "property '%s' is not valid", name);
                break;
            }
            if (0 != zfs_prop_get(fh, prop, value, value_size, NULL, NULL, 0, B_TRUE)) {
                break;
            }
        }
        ok = true;
    } while (false);

    return ok;
}

#if 0
typedef enum {
	ZFS_TYPE_FILESYSTEM	= (1 << 0),
	ZFS_TYPE_SNAPSHOT	= (1 << 1),
	ZFS_TYPE_VOLUME		= (1 << 2),
	ZFS_TYPE_POOL		= (1 << 3),
	ZFS_TYPE_BOOKMARK	= (1 << 4)
} zfs_type_t;
#endif

#define K(type, close, get_name, from_name, to_pool_name, to_libzfs_handle, get_prop, set_prop) \
    { \
        type, \
        (uzfs_close_callback_t) close, \
        (uzfs_get_name_callback_t) get_name, \
        (uzfs_from_name_callback_t) from_name, \
        (uzfs_to_pool_name_callback_t) to_pool_name, \
        (uzfs_to_libzfs_handle_callback_t) to_libzfs_handle, \
        (uzfs_get_prop_callback_t) get_prop, \
        (uzfs_set_prop_callback_t) set_prop, \
    }

static const zfs_klass_t zpool_klass = K(ZFS_TYPE_POOL, zpool_close, zpool_get_name, uzfs_zpool_from_name, zpool_get_name, zpool_get_handle, uzfs_zpool_get_prop, zpool_set_prop);
static const zfs_klass_t filesystem_klass = K(ZFS_TYPE_FILESYSTEM, zfs_close, zfs_get_name, zfs_open, zfs_get_pool_name, zfs_get_handle, uzfs_dataset_get_prop, zfs_prop_set);
static const zfs_klass_t snapshot_klass = K(ZFS_TYPE_SNAPSHOT, zfs_close, zfs_get_name, zfs_open, zfs_get_pool_name, zfs_get_handle, uzfs_dataset_get_prop, zfs_prop_set);

#undef K

static const zfs_klass_t *klasses[] = {
    [ UZFS_TYPE_POOL ] = &zpool_klass,
    [ UZFS_TYPE_FILESYSTEM ] = &filesystem_klass,
    [ UZFS_TYPE_SNAPSHOT ] = &snapshot_klass,
};

/* ========== wrapping ========== */

static uzfs_ptr_t *uzfs_wrap(void *zh, uzfs_type_t type)
{
    uzfs_ptr_t *h;

    assert_valid_uzfs_type_t(type);

    h = NULL;
    if (NULL != zh && NULL != (h = malloc(sizeof(*h)))) {
        h->ptr = zh;
        h->klass = klasses[type];
    }

    return h;
}

/* ========== private helpers ========== */

static inline libzfs_handle_t *to_libzfs_handle(uzfs_ptr_t *h)
{
    assert_valid_uzfs_ptr_t(h);

    return h->klass->to_libzfs_handle(h->ptr);
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

uzfs_ptr_t *uzfs_from_name(uzfs_lib_t *lib, const char *name, uzfs_type_t type)
{
    assert(NULL != name);
    assert_valid_uzfs_lib_t(lib);
    assert_valid_uzfs_type_t(type);

    return uzfs_wrap(klasses[type]->from_name(lib->lh, name, type), type);
}

const char *uzfs_get_name(uzfs_ptr_t *h)
{
    assert_valid_uzfs_ptr_t(h);

    return h->klass->get_name(h->ptr);
}

void uzfs_close(uzfs_ptr_t **hp)
{
    uzfs_ptr_t *h;

    assert(NULL != hp);
    h = *hp;
    assert_valid_uzfs_ptr_t(h);

    h->klass->close(h->ptr);
    free(h);
    *hp = NULL;
}

bool uzfs_same_pool(uzfs_ptr_t *h1, uzfs_ptr_t *h2)
{
    const char *pool1_name, *pool2_name;

    assert_valid_uzfs_ptr_t(h1);
    assert_valid_uzfs_ptr_t(h2);

    pool1_name = h1->klass->to_pool_name(h1->ptr);
    pool2_name = h2->klass->to_pool_name(h2->ptr);

    return NULL != pool1_name && NULL != pool2_name && 0 == strcmp(pool1_name, pool2_name);
}

uzfs_type_t uzfs_get_type(uzfs_ptr_t *h)
{
    assert_valid_uzfs_ptr_t(h);

    return (uzfs_type_t) (h->klass - *klasses);
}

bool uzfs_get_prop(uzfs_ptr_t *h, const char *name, char *value, size_t value_size)
{
    assert_valid_uzfs_ptr_t(h);
    assert(NULL != name);
    assert(NULL != value);

    return h->klass->get_prop(h->ptr, name, value, value_size);
}

bool uzfs_get_prop_numeric(uzfs_ptr_t *h, const char *name, uint64_t *value)
{
    bool ok;

    assert_valid_uzfs_ptr_t(h);
    assert(NULL != name);
    assert(NULL != value);

    ok = false;
    do {
        char propstr[128], *endptr;
        unsigned long long propui;

        if (!h->klass->get_prop(h->ptr, name, propstr, STR_SIZE(propstr))) {
            break;
        }
        propui = strtoull(propstr, &endptr, 10);
        if ('\0' != *endptr || (ERANGE == errno && ULLONG_MAX == propui)) {
            break;
        }
        *value = propui;
        ok = true;
    } while (false);

    return ok;
}

bool uzfs_set_prop(uzfs_ptr_t *h, const char *name, const char *value, char **error)
{
    int ret;

    assert_valid_uzfs_ptr_t(h);
    assert(NULL != name);
    assert(NULL != value);

    if (-1 == (ret = h->klass->set_prop(h->ptr, name, value))) {
        set_zfs_error(error, to_libzfs_handle(h), "failed to set property '%s' to '%s'", name, value);
    }

    return -1 != ret;
}

bool uzfs_set_prop_numeric(uzfs_ptr_t *h, const char *name, uint64_t value, char **error)
{
    int ret;
    char buffer[128];

    assert_valid_uzfs_ptr_t(h);
    assert(NULL != name);

    ret = snprintf(buffer, STR_SIZE(buffer), "%" PRIu64, value);
    assert(ret > 0 && ((size_t) ret) <= STR_SIZE(buffer));

    return uzfs_set_prop(h, name, buffer, error);
}

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
bool uzfs_snapshot(uzfs_ptr_t *fs, const char *scheme, bool strftime_scheme, char *name, size_t name_size, bool recursive, char **error)
{
    bool ok;

    assert_uzfs_ptr_t_is(fs, ZFS_TYPE_FILESYSTEM);
    assert(NULL != scheme);
    assert(NULL != name);

    ok = false;
    do {
        char *w;
        libzfs_handle_t *lh;
        const char *filesystem;
        const char * const name_end = name + name_size;

        if (NULL == (lh = to_libzfs_handle(fs))) {
            set_generic_error(error, "can't acquire a valid libzfs_handle_t");
            break;
        }
        filesystem = uzfs_get_name(fs);
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
bool uzfs_filesystem_destroy(uzfs_ptr_t **fs, char **error)
{
    bool ok;
    zfs_handle_t *fh;

    assert(NULL != fs);
    assert_uzfs_ptr_t_is(*fs, ZFS_TYPE_FILESYSTEM | ZFS_TYPE_SNAPSHOT);

    ok = false;
    fh = (*fs)->fh;
    do {
        libzfs_handle_t *lh;

        if (NULL == (lh = to_libzfs_handle(*fs))) {
            set_generic_error(error, "can't acquire a valid libzfs_handle_t");
            break;
        }
        if (zfs_is_shared(fh, NULL, NULL)) {
            if (0 != zfs_unshareall(fh, NULL)) {
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
            uzfs_close(fs);
        } else {
            set_zfs_error(error, lh, "zfs_destroy %s failed", zfs_get_name(fh));
//             break;
        }
    } while (false);

    return ok;
}

/**
 * Get a pool descriptor from a child ZFS filesystem
 *
 * @param fs the descendant ZFS filesytem
 *
 * @return a pool descriptor to its pool
 */
uzfs_ptr_t *uzfs_pool_from_fs(uzfs_ptr_t *fs)
{
    assert_uzfs_ptr_t_is(fs, ZFS_TYPE_FILESYSTEM);

    return uzfs_wrap(zfs_get_pool_handle(fs->fh), UZFS_TYPE_POOL);
}

/**
 * Get a descriptor on a ZFS filesystem from where a file is located
 *
 * @param path the path of the file
 *
 * @return `NULL` if *path* is not located on a ZFS filesystem or a descriptor of the
 * ZFS filesystem where the file is located.
 */
uzfs_ptr_t *uzfs_fs_from_file(uzfs_lib_t *lib, const char *path)
{

    assert_valid_uzfs_lib_t(lib);
    assert(NULL != path);

    return uzfs_wrap(uzfs_get_fs_from_file(lib->lh, path), UZFS_TYPE_FILESYSTEM);
}

bool uzfs_same_fs(uzfs_ptr_t *fs1, uzfs_ptr_t *fs2)
{
    const char *fs1_name, *fs2_name;

    assert_uzfs_ptr_t_is(fs1, ZFS_TYPE_FILESYSTEM);
    assert_uzfs_ptr_t_is(fs2, ZFS_TYPE_FILESYSTEM);

    fs1_name = zfs_get_name(fs1->fh);
    fs2_name = zfs_get_name(fs2->fh);

    return NULL != fs1_name && NULL != fs2_name && 0 == strcmp(fs1_name, fs2_name);
}

static int filesystems_iter_callback_callback(zfs_handle_t *fh, void *data)
{
    bool ok;

    ok = false;
    do {
//         uzfs_ptr_t *child;

//         child = uzfs_wrap(fh, UZFS_TYPE_FILESYSTEM);
        debug("%s is a child of %s", zfs_get_name(fh), (const char *) data);
        zfs_close(fh);
        ok = true;
    } while (false);

    return ok ? 0 : 1;
}

/**
 * Determine the location of the filesystem *child* according to *parent*
 *
 * @param parent the reference ZFS filesystem
 * @param child the filesystem to check compared to *parent*
 *
 * @return `UZFS_LOCATION_SAME` if *parent* and *child* are the same filesystem,
 * `UZFS_LOCATION_CHILD` if *child* is a subfilesystem of *parent* (parent, grandparent, grandgrandparent and so on)
 * or `UZFS_LOCATION_NONE` if *child* is not affiliated to *parent*.
 *
 * @note this function doesn't look for the opposite relation (if *child* is an ancestor of *parent*, only if it [child] is a descendant)
 *
 * @todo need to take care of a child with mountpoint=none?
 */
#if 0
extern int zfs_parent_name(zfs_handle_t *, char *, size_t);
#endif
uzfs_location_t uzfs_depth(uzfs_ptr_t *parent, uzfs_ptr_t *child)
{
    assert_uzfs_ptr_t_is(parent, ZFS_TYPE_FILESYSTEM);
    assert_uzfs_ptr_t_is(child, ZFS_TYPE_FILESYSTEM);

    zfs_iter_filesystems(parent->fh, filesystems_iter_callback_callback, (void *) zfs_get_name(parent->fh));

    return -1;
}

bool uzfs_rollback(uzfs_ptr_t *fs, uzfs_ptr_t *snapshot, bool force, char **error)
{
    bool ok;

    assert_uzfs_ptr_t_is(fs, ZFS_TYPE_FILESYSTEM);
    assert_uzfs_ptr_t_is(snapshot, ZFS_TYPE_SNAPSHOT);

    ok = false;
    do {
        if (0 != zfs_rollback(fs->fh, snapshot->fh, force)) {
            set_zfs_error(error, to_libzfs_handle(fs), "failed to rollback '%s' to '%s'", zfs_get_name(fs->fh), zfs_get_name(snapshot->fh));
            break;
        }
        ok = true;
    } while (false);

    return ok;
}

struct internal_snaphosts_iter_callback_data {
    void *data;
    char **error;
    bool (*callback)(uzfs_ptr_t *, void *, char **);
};

static int snaphosts_iter_callback_callback(zfs_handle_t *fh, struct internal_snaphosts_iter_callback_data *isicdt)
{
    bool ok;
    uzfs_ptr_t *snapshot;

    assert(NULL != isicdt);

    snapshot = uzfs_wrap(fh, UZFS_TYPE_SNAPSHOT);
    ok = isicdt->callback(snapshot, isicdt->data, isicdt->error);

    return ok ? 0 : 1;
}

// NOTE: callback is responsible to call uzfs_close on its first argument at some point
bool uzfs_iter_snapshots(uzfs_ptr_t *fs, bool (*callback)(uzfs_ptr_t *, void *, char **), void *data, char **error)
{
    int ret;
    struct internal_snaphosts_iter_callback_data isicdt;

    assert_uzfs_ptr_t_is(fs, ZFS_TYPE_FILESYSTEM);

    isicdt.data = data;
    isicdt.error = error;
    isicdt.callback = callback;
    ret = zfs_iter_snapshots(fs->fh, false, (zfs_iter_f) snaphosts_iter_callback_callback, &isicdt, 0, 0);

    return !ret;
}

#if 0
typedef int (*zfs_iter_f)(zfs_handle_t *, void *);

extern int zfs_iter_snapshots(zfs_handle_t *, boolean_t, zfs_iter_f, void *, uint64_t, uint64_t);
extern int zfs_iter_snapshots_sorted(zfs_handle_t *, zfs_iter_f, void *, uint64_t, uint64_t);
extern int zfs_iter_snapspec(zfs_handle_t *, const char *, zfs_iter_f, void *);

extern int zfs_destroy(zfs_handle_t *, boolean_t);
extern int zfs_destroy_snaps(zfs_handle_t *, char *, boolean_t);
extern int zfs_destroy_snaps_nvl(libzfs_handle_t *, nvlist_t *, boolean_t);

extern int zfs_snapshot(libzfs_handle_t *, const char *, boolean_t, nvlist_t *);
extern int zfs_snapshot_nvl(libzfs_handle_t *hdl, nvlist_t *snaps, nvlist_t *props);
extern int zfs_rollback(zfs_handle_t *, zfs_handle_t *, boolean_t);
#endif
