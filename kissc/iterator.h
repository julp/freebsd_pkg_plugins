#pragma once

#include <stdbool.h>
#include "defs.h"

typedef void (*iterator_first_t)(const void *, void **);
typedef void (*iterator_last_t)(const void *, void **);
typedef void (*iterator_current_t)(const void *, void **, void **, void **);
typedef void (*iterator_next_t)(const void *, void **);
typedef void (*iterator_previous_t)(const void *, void **);
typedef bool (*iterator_is_valid_t)(const void *, void **);
typedef void (*iterator_close_t)(void *);

typedef size_t (*iterator_count_t)(const void *);
typedef bool (*iterator_member_t)(const void *, void *);
typedef void (*iterator_delete_current_t)(const void *, void **);

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
    iterator_count_t count;
    iterator_member_t member;
    iterator_delete_current_t delete;
};

/* <TEST> */
typedef struct _Collectable Collectable;

typedef void (*collectable_into_t)(void *, void *, void *); // TODO: return bool + char **error ?
// typedef void (*collectable_close_t)(void *);

struct _Collectable {
    void *collection;
    collectable_into_t into;
};

void collectable_init(Collectable *, void *, collectable_into_t);
bool iterator_into(Iterator *, Collectable *);
/* </TEST> */

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
    iterator_close_t,
    iterator_count_t,
    iterator_member_t,
    iterator_delete_current_t
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

bool iterator_empty(Iterator *);
size_t iterator_count(Iterator *);

typedef bool (*FilterFunc)(const void *, const void *);

bool iterator_any(Iterator *, FilterFunc, const void *);
bool iterator_all(Iterator *, FilterFunc, const void *);
bool iterator_at(Iterator *, int, void **);
bool iterator_max(Iterator *, CmpFunc, void **);
bool iterator_reduce(Iterator *, void *, bool (*)(void *, void *, char **), char **);
int64_t iterator_sum(Iterator *);
int64_t iterator_product(Iterator *);
void iterator_filter(Iterator *, FilterFunc, const void *);
void iterator_reject(Iterator *, FilterFunc, const void *);
