#pragma once

#include <stdbool.h>
#include <stdarg.h>
#include <pkg.h>

#include "iterator.h"

/* <services_db.c> */
typedef struct services_db_t services_db_t;
typedef struct rc_d_script_t rc_d_script_t;
/* </services_db.c> */

/* <services_selection.c> */
typedef struct services_selection_t services_selection_t;

typedef enum {
    SERVICE_ACTION_NONE = 0,
    SERVICE_ACTION_STOP = (1<<0),
    SERVICE_ACTION_RESTART = (1<<1),
} service_action_t;
/* </services_selection.c> */

/* <services_result.c> */
typedef struct services_result_t services_result_t;

typedef enum {
    // service X (stop|restart) returned something else than EXIT_SUCCESS
    SERVICES_RESULT_FAILED,
    // (stop|restart) forbidden by configuration (blocklist)
    SERVICES_RESULT_BLOCKED,
    // service X (stop|restart) returned EXIT_SUCCESS
    SERVICES_RESULT_SUCCESS,
    // service X enabled hang/timed out
    SERVICES_RESULT_PROBING_FAILED,
    SERVICES_RESULT_COUNT,
} service_status_t;
/* </services_result.c> */

/* <services_db.c> */
services_db_t *services_db_create(char **);
void services_db_close(services_db_t *);

bool services_db_scan_system(struct pkgdb *, services_db_t *, char **);
void package_to_services_iterator(Iterator *, services_db_t *, const char *);
void services_db_rshlib(Iterator *, services_db_t *, const char *);

#define script_get(script, ...) \
    script_get2(script, __VA_ARGS__, -1)

enum {
    SCRIPT_ATTR_NAME, // const char *
    SCRIPT_ATTR_PATH, // const char *
    SCRIPT_ATTR_PACKAGE, // const package_t *
    SCRIPT_ATTR_BEFORES, // Iterator *
    SCRIPT_ATTR_REQUIRES, // Iterator *
    SCRIPT_ATTR_KEYWORDS, // Iterator *
};

void script_get2(const rc_d_script_t *script, ...);

#include "rcorder.h"

void services_db_rcorder_iter(services_db_t *, rcorder_options_t *, void (*)(const rc_d_script_t *, void *), void *);

void services_db_add_services_from_package_to_services_selection(services_db_t *, services_selection_t *, const char *, service_action_t, bool);
/* </services_db.c> */

/* <services_selection.c> */
void services_selection_block(services_selection_t *, const char *);
void services_selection_add_rdep(services_selection_t *, const char *, service_action_t);
void services_selection_add_direct(services_selection_t *, const char *, service_action_t);
bool services_selection_contains(services_selection_t *, const char *);
void services_selection_destroy(services_selection_t *);
services_selection_t *services_selection_create(char **);
services_result_t *services_selection_handle(services_selection_t *, char **);
/* </services_selection.c> */

/* <services_result.c> */
services_result_t *services_result_create(char **);
void services_result_destroy(services_result_t *);
void services_result_add(services_result_t *, const char *, service_action_t, service_status_t);
void services_result_to_iterator(Iterator *, services_result_t *, service_action_t, service_status_t);
/* </services_result.c> */
