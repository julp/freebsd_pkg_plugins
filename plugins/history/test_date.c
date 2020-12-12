#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "error/error.h"
#include "date.h"

#define RED(str) "\33[1;31m" str "\33[0m"
#define GREEN(str) "\33[1;32m" str "\33[0m"
#define DATE_TIME_FORMAT "%02d-%02d-%04d %02d:%02d:%02d" /* d-m-Y */

#define CURRENT -1
#define COMPONENT(n) \
    (CURRENT == tests[i].expected_##n ? tm_now->tm_##n : tests[i].expected_##n)

struct {
    const char *string;
    bool expected_result;
    // same suffix as struct tm
    int expected_year; // - 1900
    int expected_mon; // -1
    int expected_mday;
    int expected_hour;
    int expected_min;
    int expected_sec;
} const tests[] = {
    { "truc", false, 0, 0, 0, 0, 0, 0, },
    { "Dec 2", true, CURRENT, 11, 2, 0, 0, 0, },
    { "2 Dec", true, CURRENT, 11, 2, 0, 0, 0, },
    { "2 Dec 1:23", true, CURRENT, 11, 2, 1, 23, 0, },
    { "2 Dec 1:23 PM", true, CURRENT, 11, 2, 13, 23, 0, },
    { "12/12", true, CURRENT, 11, 12, 0, 0, 0, },
    { "11/12", false, 0, 0, 0, 0, 0, 0, },
    { "16/12", true, CURRENT, 11, 16, 0, 0, 0, },
    { "16/12 01:23", true, CURRENT, 11, 16, 1, 23, 0, },
    { "16/12 01:23 PM", true, CURRENT, 11, 16, 13, 23, 0, },
    { "12/16", true, CURRENT, 11, 16, 0, 0, 0, },
    { "12/16 01:23", true, CURRENT, 11, 16, 1, 23, 0, },
    { "12/16 01:23 PM", true, CURRENT, 11, 16, 13, 23, 0, },
    { "1999 Jan 08", true, 99, 0, 8, 0, 0, 0, },
    { "1999 January 08", true, 99, 0, 8, 0, 0, 0, },
    { "1999-01-08", true, 99, 0, 8, 0, 0, 0, },
    { "1999-01-08truc", false, 0, 0, 0, 0, 0, 0, },
    { "01-08-1999", true, 99, 0, 8, 0, 0, 0, },
    { "23-01-1999", true, 99, 0, 23, 0, 0, 0, },
    { "01-23-1999", true, 99, 0, 23, 0, 0, 0, },
    { "01/01", true, CURRENT, 0, 1, 0, 0, 0, },
    { "01/01/01", true, 1, 0, 1, 0, 0, 0, },
    { "Jan 08", true, CURRENT, 0, 8, 0, 0, 0, },
    { "Jan 08 99", true, 99, 0, 8, 0, 0, 0, },
    { "Jan 08 1 PM", true, CURRENT, 0, 8, 13, 0, 0, },
    { "Jan 08 99 8AM", true, 99, 0, 8, 8, 0, 0, },
    { "Jan 08 99 8PM", true, 99, 0, 8, 20, 0, 0, },
    { "Jan 08 99 07:55PM", true, 99, 0, 8, 19, 55, 0, },
    { "Jan 08 99 07:61PM", false, 0, 0, 0, 0, 0, 0, },
    { "Feb 29 99 06:01:02 PM", false, 0, 0, 0, 0, 0, 0, },
    { "Feb 29 20 06:01:02 PM", true, 20, 1, 29, 18, 1, 2, },
    { "01-23-1999 machin", false, 0, 0, 0, 0, 0, 0, },
    { "ToDay", true, CURRENT, CURRENT, CURRENT, 0, 0, 0, },
};

static const char *true_false[] = {
    "false",
    "true",
};

int main(void)
{
    size_t i;
    time_t time_now;
    struct tm *tm_now;

    time_now = time(NULL);
    assert(((time_t) -1) != time_now);
    tm_now = gmtime(&time_now);
    assert(NULL != tm_now);
    for (i = 0; i < ARRAY_SIZE(tests); i++) {
        bool ret;
        char *error;
        time_t time;
        struct tm *tm;

        error = NULL;
        ret = parse_date(tests[i].string, &time, &error);
        if (ret != tests[i].expected_result) {
            printf(
                "[ " RED("FAILED") " ] parse_date(\"%s\"): expected %s, got %s (error = %s)\n",
                tests[i].string,
                true_false[!!tests[i].expected_result],
                true_false[!!ret], !ret ? error : "-"
            );
        } else {
            if (tests[i].expected_result) {
                bool match;

                tm = gmtime(&time);
                match =
                    tm->tm_year == COMPONENT(year)
                    && tm->tm_mon == COMPONENT(mon)
                    && tm->tm_mday == COMPONENT(mday)
                    && tm->tm_hour == tests[i].expected_hour
                    && tm->tm_min == tests[i].expected_min
                    && tm->tm_sec == tests[i].expected_sec
                ;
                printf(
                    "[ %s ] parse_date(\"%s\"): expected " DATE_TIME_FORMAT ", got " DATE_TIME_FORMAT "\n",
                    match ? GREEN("OK") : RED("FAILED"),
                    tests[i].string,
                    COMPONENT(mday), COMPONENT(mon) + 1, COMPONENT(year) + 1900, tests[i].expected_hour, tests[i].expected_min, tests[i].expected_sec,
                    tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900, tm->tm_hour, tm->tm_min, tm->tm_sec
                );
            } else {
                if (NULL == error) {
                    printf("[ " RED("FAILED") " ] parse_date(\"%s\"): expected error to be set, got none\n", tests[i].string);
                }
            }
        }
//         printf("parse_date(\"%s\") = %d (%s) => %s", ret ? GREEN : RED, dates[i], RESET, ret, NULL == error ? "ok" : error, ret ? ctime(&time) : "-\n");
        error_free(&error);
    }

    return EXIT_SUCCESS;
}
