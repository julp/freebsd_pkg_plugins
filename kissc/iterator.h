#pragma once

#include <stdbool.h>

typedef void (*iterator_first_t)(const void *, void **);
typedef void (*iterator_last_t)(const void *, void **);
typedef void (*iterator_current_t)(const void *, void **, void **, void **);
typedef void (*iterator_next_t)(const void *, void **);
typedef void (*iterator_previous_t)(const void *, void **);
typedef bool (*iterator_is_valid_t)(const void *, void **);
typedef void (*iterator_close_t)(void *);

typedef struct _Iterator Iterator;

struct _Iterator {
    void *state;
    const void *collection;
    iterator_first_t first;
    iterator_last_t last;
    iterator_current_t current;
    iterator_next_t next;
    iterator_previous_t previous;
    iterator_is_valid_t valid;
    iterator_close_t close;
};

#define iterator_is_valid(it, k, v) \
    _iterator_is_valid(it, (void **) k, (void **) v)

void iterator_init(
    Iterator *,
    const void *,
    void *,
    iterator_first_t,
    iterator_last_t,
    iterator_current_t,
    iterator_next_t,
    iterator_previous_t,
    iterator_is_valid_t,
    iterator_close_t
);
void iterator_first(Iterator *);
void iterator_last(Iterator *);
void iterator_next(Iterator *);
void iterator_previous(Iterator *);
bool _iterator_is_valid(Iterator *, void **, void **);
void iterator_close(Iterator *);

void array_to_iterator(Iterator *, void *, size_t, size_t);
void null_terminated_ptr_array_to_iterator(Iterator *, void **);
void null_sentineled_field_terminated_array_to_iterator(Iterator *, void *, size_t, size_t);
