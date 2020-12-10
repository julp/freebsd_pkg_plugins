#pragma once

#include <stdbool.h>

const char *localbase(void);
const char *pkg_dbdir(void);

bool env_get_option(const char *, bool);
const char *system_get_env(const char *, const char *);

char **get_pkg_cmd_line(size_t, size_t *, char **);
