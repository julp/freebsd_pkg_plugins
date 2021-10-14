#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <pkg.h>

typedef struct retention_t retention_t;

bool retention_parse(const pkg_object *, retention_t *, char **);
