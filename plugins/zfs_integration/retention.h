#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <pkg.h>

#define CFG_RETENTION "RETENTION"

typedef struct retention_t retention_t;

const retention_t *retention_parse(const pkg_object *, uint64_t *, char **);

void *retention_filter_callback_data_create(const retention_t *, uint64_t, char **);
void retention_filter_callback_data_reset(void *);
void retention_filter_callback_data_destroy(void *);
bool retention_filter_callback(const void *, const void */*, char **error*/);
