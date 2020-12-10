#pragma once

#include <stdbool.h>

void argv_free(const char **);
char **argv_copy(const char **, char **);
bool argv_join(const char **, char *, const char * const, char **);
