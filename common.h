#pragma once

#include "attributes.h"
#include "utils.h"

#define S(s) \
    s, STR_LEN(s)

#ifdef DEBUG
# undef NDEBUG
# include <stdio.h>
# define debug(format, ...) \
    fprintf(stderr, format "\n", ## __VA_ARGS__)
#else
# ifndef NDEBUG
#  define NDEBUG
# endif /* !NDEBUG */
# define debug(format, ...) \
    /* NOP */
#endif /* DEBUG */
#include <assert.h>
