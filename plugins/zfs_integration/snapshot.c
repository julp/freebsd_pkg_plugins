#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "common.h"
#include "snapshot.h"

int snapshot_compare_by_creation_date_desc(const void *a, const void *b)
{
    const snapshot_t *sa, *sb;

    assert(NULL != a);
    assert(NULL != b);

    sa = (const snapshot_t *) a;
    sb = (const snapshot_t *) b;

    return (sb->creation >= sa->creation ? sb->creation - sa->creation : -1);
}

#if 0
int snapshot_compare_by_creation_date_asc(snapshot_t *a, snapshot_t *b)
{
    assert(NULL != a);
    assert(NULL != b);

    return (a->creation >= b->creation ? a->creation - b->creation : -1);
}
#endif

void *snapshot_copy(void *snap) {
    snapshot_t *copy;

    if (NULL != (copy = malloc(sizeof(*copy)))) {
        memcpy(copy, snap, sizeof(*copy));
    }

    return copy;
}

void snapshot_destroy(void *data)
{
    snapshot_t *snap;

    assert(NULL != data);

    snap = (snapshot_t *) data;
    if (NULL != snap->fs) {
        uzfs_close(&snap->fs);
    }
//     free(snap);
}

#ifdef DEBUG
# include <time.h>
// selection_dump(bes, (void (*)(void *)) snapshot_print);
void snapshot_print(snapshot_t *snap)
{
    time_t time;
    struct tm tm;
    size_t written;
    char buffer[SNAPSHOT_MAX_NAME_LEN * 2];

    assert(NULL != snap);

    time = (time_t) snap->creation;
    localtime_r(&time, &tm);
    written = strftime(buffer, STR_SIZE(buffer), "%F %T", &tm);
    assert(written < STR_LEN(buffer));
    fprintf(stderr, "(%s) %s = %s (%" PRIu64 ")\n", __func__, snap->name, buffer, snap->creation);
}
#endif /* DEBUG */
