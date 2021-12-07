#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <pkg.h>

#define CFG_RETENTION "RETENTION"

char *retention_parse(const pkg_object *, uint64_t *, char **);
