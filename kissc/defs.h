#pragma once

typedef void (*DtorFunc)(void *);
typedef void *(*DupFunc)(const void *);
typedef int (*CmpFunc)(const void *, const void *);

#include <sys/param.h>
#ifdef BSD
# define QSORT_R(base, nmemb, size, compar, thunk) \
    qsort_r(base, nmemb, size, thunk, compar)
# define QSORT_CB_ARGS(a, b, data) data, a, b
#else
# define QSORT_R(base, nmemb, size, compar, thunk) \
    qsort_r(base, nmemb, size, compar, thunk)
# define QSORT_CB_ARGS(a, b, data) a, b, data
#endif /* BSD */
typedef int (*CmpFuncArg)(QSORT_CB_ARGS(const void *, const void *, void *));
