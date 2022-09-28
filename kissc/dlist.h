#pragma once

#include <stddef.h>
#include <stdbool.h>

#include "defs.h"

typedef struct DListElement
{
    struct DListElement *next;
    struct DListElement *prev;
    void *data;
} DListElement;

typedef struct
{
    size_t length;
    DupFunc dup;
    DtorFunc dtor;
    DListElement *head;
    DListElement *tail;
} DList;

void dlist_init(DList *, DupFunc, DtorFunc);
DList *dlist_new(DupFunc, DtorFunc, char **);

void dlist_clear(DList *);
void dlist_destroy(DList *);

bool dlist_append(DList *, void *, char **);
bool dlist_empty(DList *);
DListElement *dlist_find_first(DList *, CmpFunc, void *);
DListElement *dlist_find_last(DList *, CmpFunc, void *);
bool dlist_insert_after(DList *, DListElement *, void *, char **);
bool dlist_insert_before(DList *, DListElement *, void *, char **);
size_t dlist_length(DList *);
bool dlist_prepend(DList *, void *, char **);
void dlist_remove_head(DList *);
void dlist_remove_link(DList *, DListElement *);
void dlist_remove_tail(DList *);

bool dlist_at(DList *, int, void **);
void dlist_sort(DList *, CmpFunc);

#ifndef WITHOUT_ITERATOR
# include "iterator.h"

void dlist_to_iterator(Iterator *, DList *);
void dlist_to_collectable(Collectable *, DList *);
#endif /* !WITHOUT_ITERATOR */
