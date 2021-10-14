#pragma once

#include <stdbool.h>
#include "kissc/defs.h"

typedef struct selection_t selection_t;
typedef struct selection_element_t selection_element_t;

selection_t *selection_new(CmpFunc, DtorFunc, DupFunc);
void selection_destroy(selection_t *);

bool selection_is_empty(selection_t *);
size_t selection_length(selection_t *);

bool selection_add(selection_t *, void *);
bool selection_apply(selection_t *, bool (*)(void *, void *, char **), void *, char **);
bool selection_filter(selection_t *, bool (*)(void *, void *), void *, selection_t **, selection_t **);

bool selection_at(selection_t *, int, void **);

#ifdef DEBUG
void selection_dump(selection_t *, void (*)(void *));
#endif /* DEBUG */
