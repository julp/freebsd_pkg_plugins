#include <stddef.h>
#include <stdlib.h>
#include <inttypes.h>

#include "common.h"
#include "error.h"
#include "kissc/ascii.h"
#include "kissc/dlist.h"
#include "kissc/parsenum.h"
#include "date.h"

#define YYCTYPE uint8_t

static inline bool is_digit(YYCTYPE c)
{
    return c >= ((YYCTYPE) '0') && c <= ((YYCTYPE) '9');
}

static bool yylex(const char *, struct tm *, char **);

static int my_atoi(const YYCTYPE *string, const YYCTYPE * const string_end)
{
    int32_t val;
    YYCTYPE *end;
    ParseNumError status;

    status = strntoint32_t((const char *) string, (const char * const) string_end, (char **) &end, 10, NULL, NULL, &val);
    assert(PARSE_NUM_NO_ERR == status && end == string_end);

    return (int) val;
}

static const struct {
    int value;
    const char *name;
    size_t name_len;
    const char *abbreviated;
    int days_regular_year;
    int days_leap_year;
} months[] = {
    { 1, S("January"), "Jan", 31, 31 },
    { 2, S("February"), "Feb", 28, 29 },
    { 3, S("March"), "Mar", 31, 31 },
    { 4, S("April"), "Apr", 30, 30 },
    { 5, S("May"), "May", 31, 31 },
    { 6, S("June"), "Jun", 30, 30 },
    { 7, S("July"), "Jul", 31, 31 },
    { 8, S("August"), "Aug", 31, 31 },
    { 9, S("September"), "Sep", 30, 30 },
    { 10, S("October"), "Oct", 31, 31 },
    { 11, S("November"), "Nov", 30, 30 },
    { 12, S("December"), "Dec", 31, 31 },
};

static bool is_leap_year(int y)
{
    return (0 == (y % 4) && 0 != (y % 100)) || 0 == (y % 400);
}

static int days_in_month(struct tm *tm)
{
    return is_leap_year(tm->tm_year + 1900) ? months[tm->tm_mon].days_leap_year : months[tm->tm_mon].days_regular_year;
}

static struct {
    const char *name;
    size_t offset;
    int min;
    int max;
} components[] = {
    { "seconds", offsetof(struct tm, tm_sec), 0, 60 },
    { "minutes", offsetof(struct tm, tm_min), 0, 60 },
    { "hours", offsetof(struct tm, tm_hour), 0, 23 },
//     { "month", offsetof(struct tm, tm_mon), 0, 11 },
};

static bool is_date_valid(struct tm *tm, char **error)
{
    size_t i;
    bool valid;

    valid = true;
    do {
        for (i = 0; i < ARRAY_SIZE(components) && valid; i++) {
            int *v;

            v = (int *) (((char *) tm) + components[i].offset);
            if (!(valid &= *v >= components[i].min && *v <= components[i].max)) {
                set_generic_error(error, "value '%d' as %s is invalid", v, components[i].name);
            }
        }
        if (!valid) {
            break;
        }
        valid = false;
        if (tm->tm_mon < 0 || tm->tm_mon > 11) {
            set_generic_error(error, "value '%d' as month is invalid", tm->tm_mon + 1);
            break;
        }
        if (tm->tm_mday < 1 || tm->tm_mday > days_in_month(tm)) {
            set_generic_error(error, "value '%d' as day is invalid for month '%s'", tm->tm_mday, months[tm->tm_mon].name);
            break;
        }
        valid = true;
    } while (false);

    return valid;
}

bool parse_date(const char *date, time_t *t, char **error)
{
    bool ok;

    ok = false;
    do {
        struct tm tm;
        time_t time_now;
        struct tm *tm_now;

        if (((time_t) -1) == (time_now = time(NULL))) {
            set_generic_error(error, "time(3) failed");
            break;
        }
        if (NULL == (tm_now = gmtime(&time_now))) {
            set_generic_error(error, "gmtime(3) failed");
            break;
        }
        memcpy(&tm, tm_now, sizeof(tm));
        tm.tm_sec = tm.tm_min = tm.tm_hour = 0;
        if (!yylex(date, &tm, error)) {
            break;
        }
        if (!is_date_valid(&tm, error)) {
            break;
        }
        *t = timegm(&tm);
        ok = true;
    } while (false);

    return ok;
}

static int month_value_from_name(const char *name, size_t name_len)
{
    size_t i;
    int value;

    value = -1;
    for (i = 0; i < ARRAY_SIZE(months) && -1 == value; i++) {
        if (0 == ascii_strcasecmp_l(name, name_len, months[i].name, months[i].name_len) || 0 == ascii_strcasecmp_l(name, name_len, months[i].name, 3)) {
            value = months[i].value;
        }
    }

    return value;
}

static bool is_year(int y)
{
    return y < 1 || y > 31; // -1 = current year, 0 = 1900
}

static int *guess_year(DList *unidentified)
{
    int *match_data_ptr;
    size_t match_count;
    DListElement *match;

    match = NULL;
    match_count = 0;
    match_data_ptr = NULL;
    // year is first or last of the three
    if (
        0 != dlist_length(unidentified) // NULL != unidentified->head
        && is_year(*(int *) unidentified->head->data)
    ) {
        ++match_count;
        match = unidentified->head;
    }
    if (
        unidentified->head != unidentified->tail // dlist_length(unidentified) > 1
        && (
            *((int *) unidentified->head->data) == *((int *) unidentified->tail->data) // month == year
            || is_year(*((int *) unidentified->tail->data))
        )
    ) {
        ++match_count;
        match = unidentified->tail;
    }

    if (1 == match_count) {
        match_data_ptr = (int *) match->data;
        dlist_remove_link(unidentified, match);
    }

    return match_data_ptr;
}

static bool is_month(int m)
{
    return m >= 1 && m <= 12;
}

static int *guess_month(DList *unidentified)
{
    int *match_data_ptr;
    size_t match_count;
    DListElement *match, *el;

    match = NULL;
    match_count = 0;
    match_data_ptr = NULL;
    for (el = unidentified->head; NULL != el; el = el->next) {
        if (
            (NULL == match || *((int *) match->data) != *((int *) el->data))
            && is_month(*((int *) el->data))
        ) {
            ++match_count;
            match = el;
        }
    }

    if (1 == match_count) {
        match_data_ptr = (int *) match->data;
        dlist_remove_link(unidentified, match);
    }

    return match_data_ptr;
}

static bool set_date(struct tm *tm, int *y, int *m, int *d, int *n1, int *n2, int *n3, char **error)
{
    bool ok;
    DList unidentified;

    ok = false;
    dlist_init(&unidentified, NULL);
    do {
        if (NULL != n1) {
            dlist_append(&unidentified, n1);
        }
        if (NULL != n2) {
            dlist_append(&unidentified, n2);
        }
        if (NULL != n3) {
            dlist_append(&unidentified, n3);
        }
        if (NULL == y) {
            if (NULL == (y = guess_year(&unidentified))) {
                set_generic_error(error, "can't identify year");
                break;
            }
        }
        if (NULL == m) {
            if (NULL == (m = guess_month(&unidentified))) {
                set_generic_error(error, "can't identify month");
                break;
            }
        }
        if (NULL == d) {
            assert(1 == dlist_length(&unidentified));
            d = (int *) unidentified.head->data;
        }

        if (-1 != *y) {
            tm->tm_year = *y;
            if (*y >= 1900) {
                tm->tm_year -= 1900;
            }
        }
        tm->tm_mon = *m - 1;
        tm->tm_mday = *d;
        ok = true;
    } while (false);
    dlist_clear(&unidentified);

    return ok;
}

static const YYCTYPE *skip(const YYCTYPE *string, const YYCTYPE * const string_end, const char *skiplist)
{
    const YYCTYPE *p;
    uint8_t map[256] = {0};

    for (p = (const YYCTYPE *) skiplist; '\0' != *p; p++) {
        map[(uint8_t) *p] = 1;
    }
    for (p = string; p < string_end && 0 != map[(uint8_t) *p]; p++) {
        // NOP
    }

    return p;
}

static void parse_am_pm(struct tm *tm, const YYCTYPE *string, const YYCTYPE * const string_end)
{
    if (tm->tm_hour < 12 && string < string_end) {
        const YYCTYPE *p;

        p = skip(string, string_end, " ");
        if ('P' == *p || 'p' == *p) {
            tm->tm_hour += 12;
        }
    }
}

static ParseNumError parse_time_component(const YYCTYPE *string, const YYCTYPE * const string_end, const char *skiplist, int *component, YYCTYPE **endptr)
{
    int32_t val;
    const YYCTYPE *p;
    ParseNumError status;

    assert(NULL != string);
    assert(NULL != string_end);
    assert(NULL != component);
    assert(NULL != endptr);

    if (NULL != skiplist) {
        p = skip(string, string_end, skiplist);
    } else {
        p = string;
    }
    val = 0;
    status = strntoint32_t((const char *) p, (const char * const) string_end, (char **) endptr, 10, NULL, NULL, &val);
    if (PARSE_NUM_NO_ERR == status || PARSE_NUM_ERR_NON_DIGIT_FOUND == status) {
        *component = (int) val;
    }

    return status;
}

static void set_time(struct tm *tm, const YYCTYPE *string, const YYCTYPE * const string_end)
{
    assert(NULL != tm);
    assert(NULL != string_end);

    do {
        YYCTYPE *end;
        ParseNumError status;

        if (NULL == string) {
            break;
        }
        // hours
        status = parse_time_component(string, string_end, " "/*date_time_separator*/, &tm->tm_hour, &end);
        if (PARSE_NUM_NO_ERR == status) {
            // nothing after hours, parsing done
            break;
        }
        if (PARSE_NUM_ERR_NON_DIGIT_FOUND == status && ':' != *end) {
            parse_am_pm(tm, end, string_end);
            break;
        }
        // minutes
        status = parse_time_component(end + 1, string_end, ":"/*time_separator*/, &tm->tm_min, &end);
        if (PARSE_NUM_NO_ERR == status) {
            // nothing after minutes, parsing done
            break;
        }
        if (PARSE_NUM_ERR_NON_DIGIT_FOUND == status && ':' != *end) {
            parse_am_pm(tm, end, string_end);
            break;
        }
        // seconds
        status = parse_time_component(end + 1, string_end, ":"/*time_separator*/, &tm->tm_sec, &end);
        if (PARSE_NUM_NO_ERR == status) {
            // nothing after seconds, parsing done
            break;
        } else {
            parse_am_pm(tm, end, string_end);
            break;
        }
    } while (false);
}

#if 0
# define YYDEBUG(s, c) fprintf(stderr, "state: %d char: >%c< (0x%02X)\n", s, c, c)
#else
# define YYDEBUG(s, c)
#endif

static bool yylex(const char *string, struct tm *tm, char **error)
{
    bool ok;
    int y, m, d;
    const YYCTYPE
        *YYCURSOR, *YYLIMIT, *YYMARKER,
        *ys, *ye, *ms, *me, *ds, *de, *ts
    ;
    /*!stags:re2c format = 'const uint8_t *@@;'; */

    ok = false;
    y = m = d = -1;
    ys = ye = ms = me = ds = de = ts = NULL;
    YYMARKER = YYCURSOR = (const YYCTYPE *) string;
    YYLIMIT = YYCURSOR + strlen(string);
/*!re2c
re2c:flags:tags = 1;
re2c:yyfill:enable = 0;
re2c:api:style = free-form;
re2c:define:YYCTYPE = uint8_t;
re2c:define:YYPEEK = "YYCURSOR < YYLIMIT ? *YYCURSOR : 0";

digit = [0-9];
number = digit+;

eos = [\x00];
whitespace = [ ];
whitespaces = whitespace+;
maybe_whitespaces = whitespace*;

time_separator = [:];
date_separator = maybe_whitespaces [-/.] maybe_whitespaces | whitespaces;
date_time_separator = whitespaces;

am_pm = 'AM' | 'PM';
// maybe_am_pm = (maybe_whitespaces am_pm)?;
h12 = (("0"? digit) | ("1" [01]));
h24 = ("0"? digit) | "1" digit | "2" [0-3];
minute = ("0"? digit) | ([1-5][0-9]);
second = ("0"? digit) | ([1-5][0-9]);
time12 = h12 ((time_separator minute)? (time_separator second)?)? maybe_whitespaces am_pm;
time24 = h24 ((time_separator minute)? (time_separator second)?)?;
time = time12 | time24;
maybe_with_time = (date_time_separator @ts time)?;

ordinal = 'st' | 'nd' | 'rd' | 'th';

month_full = 'January' | 'February' | 'March' | 'April' | 'May' | 'June' | 'July' | 'August' | 'September' | 'October' | 'November' | 'December';
month_abbr = 'Jan' | 'Feb' | 'Apr' | 'May' | 'Jul' | 'Jun' | 'Jul' | 'Aug' | 'Sep' | 'Oct' | 'Nov' | 'Dec';
monthtext = month_full | month_abbr;

yy = digit{2};
yyyy = digit{4};
year = (yyyy | yy);
maybe_year = (date_separator @ys year @ye)?;

day = (("0"? | [1-2]) digit) | ([3][01]);
month = ("0"? digit) | ("1" [0-2]);

'today' {
    return true;
}

@ys yyyy @ye whitespaces @ms monthtext @me whitespaces @ds day @de maybe_with_time eos {
    goto done;
}

@ys yyyy @ye date_separator @ms month @me date_separator @ds day @de maybe_with_time eos {
    goto done;
}

// mm-dd(-(yy)yy) (hh:ii:ss(am|pm))
//@ms month @me date_separator @ds day @de (date_separator @ys yyyy @ye)? maybe_with_time eos {
@ms month @me date_separator @ds day @de maybe_year maybe_with_time eos {
    int *mp, *dp, *u1, *u2;

    m = my_atoi(ms, me);
    d = my_atoi(ds, de);
    if (ys != ye) {
        // if year is present format is mm-dd-yyy
        y = my_atoi(ys, ye);
        mp = &m;
        dp = &d;
        u1 = u2 = NULL;
        //if (!set_date(tm, &y, &m, &d, NULL, NULL, NULL, error)) {
            //break;
        //}
    } else {
        // if year is absent format is either mm-dd or dd-mm
        // month is unidentified
        mp = NULL;
        u1 = &m;
        // day is unidentified
        dp = NULL;
        u2 = &d;
        //if (!set_date(tm, &y, NULL, NULL, &m, &d, NULL, error)) {
            //break;
        //}
    }
    do {
        //if (!set_date(tm, &y, &m, &d, NULL, NULL, NULL, error)) {
        if (!set_date(tm, &y, mp, dp, u1, u2, NULL, error)) {
            break;
        }
        set_time(tm, ts, YYLIMIT);
        ok = true;
    } while (false);

    return ok;
}

@ms monthtext @me whitespaces @ds day @de maybe_year maybe_with_time eos {
    goto done;
}

@ds day @de maybe_whitespaces @ms monthtext @me maybe_year maybe_with_time eos {
    goto done;
}

// ambiguous formats, eg: dd-mm-yy vs yy-mm-dd
// NOTE: I reuse the variables y/ys/ye, d/ds/de, but they might designate something else than year/day
@ys number @ye date_separator @ms month @me date_separator @ds number @de maybe_with_time eos {
    y = my_atoi(ys, ye);
    m = my_atoi(ms, me);
    d = my_atoi(ds, de);
    do {
        if (!set_date(tm, NULL, &m, NULL, &y, &d, NULL, error)) {
            break;
        }
        set_time(tm, ts, YYLIMIT);
        ok = true;
    } while (false);

    return ok;
}

// ambiguous formats, eg: dd-mm vs mm-dd
@ms number{1,2} @me date_separator @ds number{1,2} @de maybe_with_time eos {
    m = my_atoi(ms, me);
    d = my_atoi(ds, de);
    do {
        if (!set_date(tm, &y, NULL, NULL, &m, &d, NULL, error)) {
            break;
        }
        set_time(tm, ts, YYLIMIT);
        ok = true;
    } while (false);

    return ok;
}

[^] {
    set_generic_error(error, "invalid date");

    return false;
}
*/
done:
    {
        if (ys != ye) {
            y = my_atoi(ys, ye);
        }
        if (is_digit(*ms)) {
            m = my_atoi(ms, me);
        } else {
            m = month_value_from_name((const char *) ms, me - ms);
        }
        d = my_atoi(ds, de);
        do {
            if (!set_date(tm, &y, &m, &d, NULL, NULL, NULL, error)) {
                break;
            }
            set_time(tm, ts, YYLIMIT);
            ok = true;
        } while (false);
    }

    return ok;
}
