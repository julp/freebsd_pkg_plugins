#include <stdlib.h>

#include "common.h"
#include "error.h"
#include "kissc/dlist.h"
#include "kissc/hashtable.h"
#include "services.h"
#include "process_monitor.h"

#define SERVICE_TIMEOUT 10 /* seconds */

// service was not referenced as a rdep
#define SERVICE_FLAG_DIRECT (1 << 0)
// service was referenced by the blocklist
#define SERVICE_FLAG_BLOCKED (1 << 1)

struct services_selection_t {
    HashTable list;
};

typedef struct {
    uint32_t flags;
    const char *name;
    service_action_t action;
} service_data_t;

static service_data_t *value_create(const char *name, service_action_t action, uint32_t flags)
{
    service_data_t *sd;

    sd = malloc(sizeof(*sd));
    assert(NULL != sd);
    sd->name = name;
    sd->flags = flags;
    sd->action = action;

    return sd;
}

static void value_destroy(service_data_t *sd)
{
    free(sd);
}

services_selection_t *services_selection_create(char **error)
{
    services_selection_t *ss;

    if (NULL == (ss = malloc(sizeof(*ss)))) {
        set_malloc_error(error, sizeof(*ss));
    } else {
        hashtable_ascii_cs_init(&ss->list, NULL, NULL, (DtorFunc) value_destroy);
    }

    return ss;
}

static void services_selection_add(services_selection_t *ss, const char *name, service_action_t action, uint32_t flags)
{
    ht_hash_t h;
    service_data_t *sd;

    assert(NULL != ss);

    h = hashtable_hash(&ss->list, name);
    if (!hashtable_quick_get(&ss->list, h, name, &sd)) {
        sd = value_create(name, action, flags);
        hashtable_quick_put(&ss->list, HT_PUT_ON_DUP_KEY_PRESERVE, h, /*sd->*/name, sd, NULL);
    } else {
        sd->flags |= flags;
        sd->action = action;
    }
}

void services_selection_block(services_selection_t *ss, const char *name)
{
    services_selection_add(ss, name, SERVICE_ACTION_NONE, SERVICE_FLAG_BLOCKED);
}

void services_selection_add_direct(services_selection_t *ss, const char *name, service_action_t action)
{
    services_selection_add(ss, name, action, SERVICE_FLAG_DIRECT);
}

void services_selection_add_rdep(services_selection_t *ss, const char *name, service_action_t action)
{
    services_selection_add(ss, name, action, 0);
}

bool services_selection_contains(services_selection_t *ss, const char *name)
{
    assert(NULL != ss);

    return hashtable_contains(&ss->list, name);
}

void services_selection_destroy(services_selection_t *ss)
{
    assert(NULL != ss);

    hashtable_destroy(&ss->list);
    free(ss);
}

static void handle_enabled_exited(pid_t UNUSED(pid), int status, void *acc, void *data)
{
    DList *enabled;
    service_data_t *sd;

    enabled = (DList *) acc;
    sd = (service_data_t *) data;
    if (EXIT_SUCCESS == status) {
        dlist_append(enabled, sd, NULL);
    }
}

static void handle_enabled_hanging(pid_t UNUSED(pid), void *acc, void *data)
{
    service_data_t *sd;
    services_result_t *sr;

    sd = (service_data_t *) data;
    sr = (services_result_t *) acc;
    services_result_add(sr, sd->name, sd->action, SERVICES_RESULT_PROBING_FAILED);
}

static void handle_restart_stop_exited(pid_t UNUSED(pid), int status, void *acc, void *data)
{
    service_data_t *sd;
    services_result_t *sr;

    sd = (service_data_t *) data;
    sr = (services_result_t *) acc;
    services_result_add(sr, sd->name, sd->action, EXIT_SUCCESS == status ? SERVICES_RESULT_SUCCESS : SERVICES_RESULT_FAILED);
}

static void handle_restart_stop_hanging(pid_t UNUSED(pid), void *acc, void *data)
{
    service_data_t *sd;
    services_result_t *sr;

    sd = (service_data_t *) data;
    sr = (services_result_t *) acc;
    services_result_add(sr, sd->name, sd->action, SERVICES_RESULT_FAILED);
}

services_result_t *services_selection_handle(services_selection_t *ss, char **error)
{
    bool ok;
    services_result_t *sr;
    process_monitor_t *pm;

    assert(NULL != ss);

    sr = NULL;
    pm = NULL;
    ok = false;
    do {
        Iterator it;
        DList enabled;
        const char *name;
        service_data_t *sd;
        const char *argv[] = {"/usr/sbin/service", NULL, "enabled", NULL};

        if (NULL == (sr = services_result_create(error))) {
            break;
        }
        if (NULL == (pm = process_monitor_create(error))) {
            break;
        }
        hashtable_to_iterator(&it, &ss->list);
        for (iterator_first(&it); iterator_is_valid(&it, &name, &sd); iterator_next(&it)) {
            if (HAS_FLAG(sd->flags, SERVICE_FLAG_BLOCKED)) {
                services_result_add(sr, name, sd->action, SERVICES_RESULT_BLOCKED);
                continue;
            }
            argv[1] = name;
            if (!process_monitor_exec(pm, argv, sd, NULL, error)) {
                break;
            }
        }
        iterator_close(&it);
        dlist_init(&enabled, NULL, NULL);
        process_monitor_await(pm, SERVICE_TIMEOUT, handle_enabled_exited, &enabled, handle_enabled_hanging, sr, error);
        process_monitor_clear(pm);
        // second step
#if 0
        argv[0] = "echo";
#endif
        dlist_to_iterator(&it, &enabled);
        for (iterator_first(&it); iterator_is_valid(&it, NULL, &sd); iterator_next(&it)) {
            assert(SERVICE_ACTION_RESTART == sd->action || SERVICE_ACTION_STOP == sd->action);
            argv[1] = sd->name;
            argv[2] = SERVICE_ACTION_STOP == sd->action ? "stop" : "restart";
            if (!process_monitor_exec(pm, argv, sd, NULL, error)) {
                break;
            }
        }
        iterator_close(&it);
        dlist_clear(&enabled);
        process_monitor_await(pm, SERVICE_TIMEOUT, handle_restart_stop_exited, sr, handle_restart_stop_hanging, sr, error);
        process_monitor_clear(pm);
        ok = true;
    } while (false);
    if (NULL != pm) {
//         process_monitor_clear(pm);
        process_monitor_destroy(pm);
    }
    if (!ok && NULL != sr) {
        services_result_destroy(sr);
        sr = NULL;
    }

    return sr;
}
