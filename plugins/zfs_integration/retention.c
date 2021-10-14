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
    R(BY_CREATION),
} retention_type_t;

struct retention_t {
    uint64_t value;
    retention_type_t type;
};

static bool disabled_retention_keep(retention_t *UNUSED(retention), uint64_t UNUSED(value))
{
    return true;
}

static bool total_retention_keep(retention_t *retention, uint64_t value)
{
    bool keep;

    keep = retention->value >= value;
    if (keep) {
        --retention->value;
    }

    return keep;
}

static bool creation_retention_keep(retention_t *retention, uint64_t value)
{
    return value >= retention->value;
}

static bool (*callbacks[])(retention_t *, uint64_t) = {
    [ R(DISABLED) ] = disabled_retention_keep,
    [ R(BY_COUNT) ] = total_retention_keep,
    [ R(BY_CREATION) ] = creation_retention_keep,
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

// static
void retention_init(retention_t *retention)
{
    assert(NULL != retention);

    retention->value = 0;
    retention->type = R(DISABLED);
}

// TODO: bool (*)(retention_t *, uint64_t) + uint64_t * ?
bool retention_parse(const pkg_object *object, retention_t *retention, char **error)
{
    bool ok;
    pkg_object_t type;

    assert(NULL != retention);

    ok = false;
    retention->type = R(DISABLED);
    type = pkg_object_type(object);
    do {
        if (PKG_NULL == type) {
            // NOP: accepted as disabled
        } else if (PKG_BOOL == type && false == pkg_object_bool(object)) {
            // NOP: accepted as disabled
        } else if (PKG_INT == type) {
            if (pkg_object_int(object) > 0) {
                retention->value = pkg_object_int(object);
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
                set_generic_error(error, "value '%s' for RETENTION setting is out of the range [%lld;%lld]", string, LONG_MIN, LONG_MAX);
                break;
            }
            if ('\0' == *endptr) {
                retention->value = value;
                retention->type = value > 0 ? R(BY_COUNT) : R(DISABLED);
            } else if (value <= 0) {
                set_generic_error(error, "expected quantified RETENTION value to be > 0, got: %lld", value);
                break;
            } else /*if (value > 0)*/ {
                char *p;
                size_t i;

                for (p = endptr; ' ' == *p; p++)
                    ;
                for (i = 0; i < ARRAY_SIZE(retention_units); i++) {
                    if (0 == /*ascii_*/strncasecmp(p, retention_units[i].name, retention_units[i].name_len) && '\0' == p[retention_units[i].name_len]) {
                        retention->value = time(NULL) - value * retention_units[i].value;
                        retention->type = R(BY_CREATION);
                        break;
                    }
                }
                if (R(BY_CREATION) != retention->type) {
                    set_generic_error(error, "unable to parse '%s' for RETENTION setting", string);
                    break;
                }
            }
        } else {
            set_generic_error(error, "expected RETENTION to be either false, null, an integer or a string, got: %s (%d)", pkg_object_string(object), type);
            break;
        }
        ok = true;
    } while (false);
    debug("retention : type = %d, value = %" PRIu64, retention->type, retention->value);

    return ok;
}

bool retention_disabled(const retention_t *retention)
{
    return retention->type == R(DISABLED);
}

bool retention_keep(retention_t *UNUSED(retention), uint64_t UNUSED(value))
{
    return true;
}
