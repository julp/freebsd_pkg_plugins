#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <pkg.h>

#include "selection.h"

#define CFG_RETENTION "RETENTION"

typedef struct retention_t retention_t;

const retention_t *retention_parse(const pkg_object *, uint64_t *, char **);
