#include <inttypes.h>
#include <stdlib.h>
#include <strings.h>
#include <limits.h>
#include <time.h>

#include "common.h"
#include "error.h"
#include "retention.h"
#include "snapshot.h"

#define R(name) \
    RETENTION_ ## name

typedef enum {
    R(DISABLED),
    R(BY_COUNT),
    //R(BY_SIZE),
    R(BY_CREATION),
} retention_type_t;

struct retention_t {
    const char *name;
    bool (*callback)(uint64_t, uint64_t, uint64_t *);
};

typedef struct {
    const retention_t *retention;
    uint64_t limit;
    uint64_t state;
} filter_callback_data_t;

bool retention_filter_callback(const void *data, const void *user_data/*, char **error*/)
{
    snapshot_t *snap;
    filter_callback_data_t *fcd;

    assert(NULL != data);
    assert(NULL != user_data);

    snap = (snapshot_t *) data;
    fcd = (filter_callback_data_t *) user_data;

    return fcd->retention->callback(snap->creation, fcd->limit, &fcd->state);
}

void *retention_filter_callback_data_create(const retention_t *retention, uint64_t limit, char **error)
{
    filter_callback_data_t *fcd;

    if (NULL == (fcd = malloc(sizeof(*fcd)))) {
        set_malloc_error(error, sizeof(*fcd));
    } else {
        fcd->retention = retention;
        fcd->limit = limit;
        fcd->state = 0;
    }

    return (void *) fcd;
}

void retention_filter_callback_data_reset(void *data)
{
    filter_callback_data_t *fcd;

    assert(NULL != data);

    fcd = (filter_callback_data_t *) data;
    fcd->state = 0;
}

void retention_filter_callback_data_destroy(void *data)
{
    // filter_callback_data_t *fcd;

    assert(NULL != data);
    // fcd = (filter_callback_data_t *) data;
    free(data);
}

static bool retention_disabled_keep(uint64_t UNUSED(value), uint64_t UNUSED(limit), uint64_t *UNUSED(state))
{
    return true;
}

static bool retention_by_count_keep(uint64_t UNUSED(value), uint64_t limit, uint64_t *state)
{
    bool keep;

    // NOTE: <, not <=, because limit start at 0, not 1
    keep = *state < limit;
    if (keep) {
        ++*state;
    }

    return keep;
}

static bool retention_by_creation_keep(uint64_t value, uint64_t limit, uint64_t *UNUSED(state))
{
    // debug("%" PRIu64 " >= %" PRIu64 " = %d", value, limit, value >= limit);
    return value >= limit;
}

static const retention_t kinds[] = {
    [ R(DISABLED) ] = {"disabled: no deletion", retention_disabled_keep},
    [ R(BY_COUNT) ] = {"by count: keep the N most recent snapshots", retention_by_count_keep},
    //[ R(BY_SIZE) ] = {"by size: keep the most recent snapshots while their cumulated size doesn't exceed N", retention_by_size_keep},
    [ R(BY_CREATION) ] = {"by creation: keep the snapshots over the last N period", retention_by_creation_keep},
};

#if 0
#define MINUTE 60
#define HOUR (60 * MINUTE)
#define DAY (24 * HOUR)
#define WEEK (7 * DAY)
#define MONTH (30 * DAY)
#define YEAR (365 * DAY)
#else
/*typedef*/ enum {
    MINUTE = 60,
    HOUR = 60 * MINUTE,
    DAY = 24 * HOUR,
    WEEK = 7 * DAY,
    MONTH = 30 * DAY,
    YEAR = 365 * DAY,
};
#endif

static const struct {
    const char *name;
    size_t name_len;
    size_t value; // in seconds
} retention_units[] = {
    { S("days"), DAY },
    { S("day"), DAY },
    { S("weeks"), WEEK },
    { S("week"), WEEK },
    { S("months"), MONTH },
    { S("month"), MONTH },
    { S("years"), YEAR },
    { S("year"), YEAR },
};

const retention_t *retention_parse(const pkg_object *object, uint64_t *limit, char **error)
{
    const retention_t *retention;

    assert(NULL != limit);

    retention = NULL;
    do {
        pkg_object_t object_type;
        retention_type_t retention_type;

        /**
         * TODO: it seems, by `pkg_plugin_conf_add(p, PKG_STRING, CFG_RETENTION, "");` (in plugin_zfs_integration.c), pkg, in configuration file, expect a string.
         * For example, `RETENTION = 10;` won't work, it has to be `RETENTION = "10";`.
         * So, I guess, type checking is totaly useless, only `} else if (PKG_STRING == object_type) {` will always be true/executed.
         */
        retention_type = R(DISABLED);
        object_type = pkg_object_type(object);
        if (PKG_NULL == object_type) {
            // NOP: accepted as disabled
        } else if (PKG_BOOL == object_type && false == pkg_object_bool(object)) {
            // NOP: accepted as disabled
        } else if (PKG_INT == object_type) {
            if (pkg_object_int(object) > 0) {
                *limit = pkg_object_int(object);
                retention_type = R(BY_COUNT);
            } else {
                // NOP: accepted as disabled
            }
        } else if (PKG_STRING == object_type) {
            char *endptr;
            long long value;
            const char *string;

            string = pkg_object_string(object);
            // NOTE: pkg_object_string seems to convert "" to NULL
            if (NULL == string) {
                string = "";
            }
            errno = 0;
            value = strtoll(string, &endptr, 10);
            if (errno == ERANGE && (LONG_MAX == value || LONG_MIN == value)) {
                set_generic_error(error, "value '%s' for %s setting is out of the range [%lld;%lld]", string, CFG_RETENTION, LONG_MIN, LONG_MAX);
                break;
            }
            if ('\0' == *endptr) {
                *limit = value;
                retention_type = value > 0 ? R(BY_COUNT) : R(DISABLED);
            } else if (value <= 0) {
                set_generic_error(error, "expected quantified '%s' value to be > 0, got: %lld", CFG_RETENTION, value);
                break;
            } else /*if (value > 0)*/ {
                char *p;
                size_t i;

                for (p = endptr; ' ' == *p; p++)
                    ;
                for (i = 0; i < ARRAY_SIZE(retention_units); i++) {
                    if (0 == /*ascii_*/strncasecmp(p, retention_units[i].name, retention_units[i].name_len) && '\0' == p[retention_units[i].name_len]) {
                        *limit = time(NULL) - value * retention_units[i].value;
                        retention_type = R(BY_CREATION);
                        break;
                    }
                }
                if (R(BY_CREATION) != retention_type) {
                    set_generic_error(error, "unable to parse '%s' for %s setting", CFG_RETENTION, string);
                    break;
                }
            }
        } else {
            set_generic_error(error, "expected '%s' to be either false, null, an integer or a string, got: '%s' (%d)", CFG_RETENTION, pkg_object_string(object), object_type);
            break;
        }
        retention = &kinds[retention_type];
        debug("retention : type = %d, limit = %" PRIu64, retention_type, *limit);
    } while (false);

    return retention;
}

/*
bool retention_apply(const retention_t *retention, uint64_t limit, selection_t *selection)
{
    uint64_t state;

    state = 0;
    selection_apply(selection, retention->callback, &state, limit);

    return false;
}
*/
