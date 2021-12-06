#include <inttypes.h>
#include <stdlib.h>
#include <strings.h>
#include <limits.h>
#include <time.h>

#include "common.h"
#include "error.h"
#include "retention.h"

#define R(name) \
    RETENTION_ ## name

typedef enum {
    R(DISABLED),
    R(BY_COUNT),
    //R(BY_SIZE),
    R(BY_CREATION),
} retention_type_t;

struct retention_t {
    uint64_t limit;
    retention_type_t type;
};

static bool retention_disabled_keep(uint64_t UNUSED(value), uint64_t UNUSED(limit), uint64_t *UNUSED(state))
{
    return true;
}

static bool retention_by_count_keep(uint64_t UNUSED(value), uint64_t limit, uint64_t *state)
{
    bool keep;

    keep = *state <= limit;
    if (keep) {
        ++*state;
    }

    return keep;
}

static bool retention_by_creation_keep(uint64_t value, uint64_t limit, uint64_t *UNUSED(state))
{
    return value >= limit;
}

struct {
    const char *name;
    bool (*callback)(uint64_t, uint64_t, uint64_t *);
} static const kinds[] = {
    [ R(DISABLED) ] = {"disabled: no deletion", retention_disabled_keep},
    [ R(BY_COUNT) ] = {"by count: keep the N most recent snapshots", retention_by_count_keep},
    //[ R(BY_SIZE) ] = {"", },
    [ R(BY_CREATION) ] = {"by creation: keep the snapshots over the last N period", retention_by_creation_keep},
};

#define MINUTE 60
#define HOUR (60 * MINUTE)
#define DAY (24 * HOUR)
#define WEEK (7 * DAY)
#define MONTH (30 * DAY)
#define YEAR (365 * DAY)

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

static retention_t *retention_create(void)
{
    retention_t *retention;

    if (NULL != (retention = malloc(sizeof(*retention)))) {
        retention->limit = 0;
        retention->type = R(DISABLED);
    }

    return retention;
}

void retention_destroy(retention_t *retention)
{
    free(retention);
}

// TODO: bool (*)(retention_t *, uint64_t) + uint64_t * ?
retention_t *retention_parse(const pkg_object *object, uint64_t *limit, char **error)
{
    retention_t *ret, *retention;

    assert(NULL != limit);

    ret = retention = NULL;
    do {
        pkg_object_t type;

        if (NULL == (retention = retention_create())) {
            break;
        }
        type = pkg_object_type(object);
        if (PKG_NULL == type) {
            // NOP: accepted as disabled
        } else if (PKG_BOOL == type && false == pkg_object_bool(object)) {
            // NOP: accepted as disabled
        } else if (PKG_INT == type) {
            if (pkg_object_int(object) > 0) {
                retention->limit = pkg_object_int(object);
                retention->type = R(BY_COUNT);
            } else {
                // NOP: accepted as disabled
            }
        } else if (PKG_STRING == type) {
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
                retention->limit = value;
                retention->type = value > 0 ? R(BY_COUNT) : R(DISABLED);
            } else if (value <= 0) {
                set_generic_error(error, "expected quantified %s value to be > 0, got: %lld", CFG_RETENTION, value);
                break;
            } else /*if (value > 0)*/ {
                char *p;
                size_t i;

                for (p = endptr; ' ' == *p; p++)
                    ;
                for (i = 0; i < ARRAY_SIZE(retention_units); i++) {
                    if (0 == /*ascii_*/strncasecmp(p, retention_units[i].name, retention_units[i].name_len) && '\0' == p[retention_units[i].name_len]) {
                        retention->limit = time(NULL) - value * retention_units[i].value;
                        retention->type = R(BY_CREATION);
                        break;
                    }
                }
                if (R(BY_CREATION) != retention->type) {
                    set_generic_error(error, "unable to parse '%s' for %s setting", CFG_RETENTION, string);
                    break;
                }
            }
        } else {
            set_generic_error(error, "expected %s to be either false, null, an integer or a string, got: %s (%d)", CFG_RETENTION, pkg_object_string(object), type);
            break;
        }
        ret = retention;
    } while (false);
    debug("retention : type = %d, limit = %" PRIu64, retention->type, retention->limit);
    if (ret != retention) {
        retention_destroy(retention);
    }

    return ret;
}

bool retention_disabled(const retention_t *retention)
{
    return retention->type == R(DISABLED);
}

bool retention_keep(retention_t *UNUSED(retention), uint64_t UNUSED(value))
{
    return true;
}
