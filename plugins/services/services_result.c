#include <stdlib.h>

#include "common.h"
#include "error.h"
#include "kissc/dlist.h"
#include "services.h"

struct services_result_t {
//     union {
        DList lists[2][SERVICES_RESULT_COUNT];
//         struct {
//             DList stop_failed;
//             DList stop_blocked;
//             DList stop_success;
//             DList stop_probing_failed;
//             DList restart_failed;
//             DList restart_blocked;
//             DList restart_success;
//             DList restart_probing_failed;
//         };
//     };
};

void services_result_destroy(services_result_t *sr)
{
    size_t i;

    assert(NULL != sr);

    for (i = 0; i < SERVICES_RESULT_COUNT; i++) {
        dlist_clear(&sr->lists[SERVICE_ACTION_STOP - 1][i]);
        dlist_clear(&sr->lists[SERVICE_ACTION_RESTART - 1][i]);
    }
}

void services_result_add(services_result_t *sr, const char *name, service_action_t action, service_status_t status)
{
    assert(NULL != sr);
    assert(NULL != name);

    if (SERVICE_ACTION_NONE != action) {
        dlist_append(&sr->lists[action - 1][status], (void *) name);
    }
}

void services_result_to_iterator(Iterator *it, services_result_t *sr, service_action_t action, service_status_t status)
{
    assert(NULL != it);
    assert(NULL != sr);
    assert(SERVICE_ACTION_RESTART == action || SERVICE_ACTION_STOP == action);

    dlist_to_iterator(it, &sr->lists[action - 1][status]);
}

services_result_t *services_result_create(char **error)
{
    services_result_t *sr;

    if (NULL == (sr = malloc(sizeof(*sr)))) {
        set_malloc_error(error, sizeof(*sr));
    } else {
        size_t i;

        for (i = 0; i < SERVICES_RESULT_COUNT; i++) {
            dlist_init(&sr->lists[SERVICE_ACTION_STOP - 1][i], NULL);
            dlist_init(&sr->lists[SERVICE_ACTION_RESTART - 1][i], NULL);
        }
    }

    return sr;
}
