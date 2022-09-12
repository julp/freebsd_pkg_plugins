/**
 * @file lib/dlist.c
 * @brief double linked list
 */

#include <stdlib.h>
#include <assert.h>

#include "attributes.h"
#include "dlist.h"

#define ALLOC_ELEMENT(el, ptr) \
    do { \
        if (NULL == (el = malloc(sizeof(*el)))) { \
            return false; \
        } \
        el->data = ptr; \
    } while (0);

void dlist_init(DList *list, DtorFunc dtor)
{
    assert(NULL != list);

    list->length = 0;
    list->head = list->tail = NULL;
    list->dtor = dtor;
}

/**
 * Get the length of double linked list
 * This information is maintained into the list, its
 * elements are not traversed at each call.
 *
 * @param list the list
 *
 * @return the number of elements in the list
 */
size_t dlist_length(DList *list)
{
    assert(NULL != list);

    return list->length;
}

/**
 * Destroy the elements of a double linked list
 *
 * @param list the list to clear
 */
void dlist_clear(DList *list)
{
    DListElement *tmp, *last;

    assert(NULL != list);

    tmp = list->head;
    while (NULL != tmp) {
        last = tmp;
        tmp = tmp->next;
        if (NULL != list->dtor) {
            list->dtor(last->data);
        }
        free(last);
    }
    list->length = 0;
    list->head = list->tail = NULL;
}

bool dlist_append(DList *list, void *data)
{
    DListElement *tmp;

    assert(NULL != list);

    ALLOC_ELEMENT(tmp, data);
    tmp->next = NULL;
    if (NULL != list->tail) {
        list->tail->next = tmp;
        tmp->prev = list->tail;
        list->tail = tmp;
    } else {
        list->head = list->tail = tmp;
        tmp->prev = NULL;
    }
    ++list->length;

    return true;
}

DListElement *dlist_find_first(DList *list, CmpFunc cmp, void *data)
{
    DListElement *el;

    assert(NULL != list);
    assert(NULL != cmp);

    for (el = list->head; NULL != el; el = el->next) {
        if (0 == cmp(el->data, data)) {
            return el;
        }
    }

    return NULL;
}

DListElement *dlist_find_last(DList *list, CmpFunc cmp, void *data)
{
    DListElement *el;

    assert(NULL != list);
    assert(NULL != cmp);

    for (el = list->tail; NULL != el; el = el->prev) {
        if (0 == cmp(el->data, data)) {
            return el;
        }
    }

    return NULL;
}

bool dlist_insert_before(DList *list, DListElement *sibling, void *data)
{
    assert(NULL != list);
    assert(NULL != sibling);

    if (sibling == list->head) {
        return dlist_prepend(list, data);
    } else {
        DListElement *tmp;

        ALLOC_ELEMENT(tmp, data);
        tmp->next = sibling;
        tmp->prev = sibling->prev;
        sibling->prev->next = tmp;
        sibling->prev = tmp;
    }

    return true;
}

bool dlist_insert_after(DList *list, DListElement *sibling, void *data)
{
    assert(NULL != list);
    assert(NULL != sibling);

    if (sibling == list->tail) {
        return dlist_append(list, data);
    } else {
        DListElement *tmp;

        ALLOC_ELEMENT(tmp, data);
        sibling->next->prev = tmp;
        tmp->next = sibling->next;
        tmp->prev = sibling;
        sibling->next = tmp;
    }

    return true;
}

DListElement *dlist_link_at(DList *list, int n)
{
    size_t offset;
    DListElement *el;

    assert(NULL != list);

    if (n < 0) {
        n = -n;
        el = list->tail;
        offset = offsetof(DListElement, prev);
    } else {
        el = list->head;
        offset = offsetof(DListElement, next);
    }
    if (n <= (int) (list->length - (el == list->head))) {
        for (; NULL != el && n >= 0; n--) {
            el = (DListElement *) (((char *) el) + offset);
        }
    }

    return el;
}

bool dlist_insert_at(DList *list, int n, void *data)
{
    DListElement *el;

    assert(NULL != list);

    if (NULL == (el = dlist_link_at(list, n))) {
        return false;
    } else {
        return dlist_insert_before(list, el, data);
    }
}

bool dlist_remove_at(DList *list, int n)
{
    DListElement *el;

    assert(NULL != list);

    if (NULL == (el = dlist_link_at(list, n))) {
        return false;
    } else {
        dlist_remove_link(list, el);
        return true;
    }
}

bool dlist_empty(DList *list)
{
    assert(NULL != list);

    return NULL == list->head;
}

bool dlist_prepend(DList *list, void *data)
{
    DListElement *tmp;

    assert(NULL != list);

    ALLOC_ELEMENT(tmp, data);
    tmp->prev = NULL;
    if (NULL != list->head) {
        tmp->next = list->head;
    } else {
        tmp->next = NULL;
        list->tail = tmp;
    }
    list->head = tmp;
    ++list->length;

    return true;
}

void dlist_remove_head(DList *list)
{
    DListElement *tmp;

    assert(NULL != list);

    if (NULL != list->head) {
        tmp = list->head;
        list->head = list->head->next;
        if (NULL != list->head) {
            list->head->prev = NULL;
        } else {
            list->head = list->tail = NULL;
        }
        if (NULL != list->dtor) {
            list->dtor(tmp->data);
        }
        free(tmp);
        --list->length;
    }
}

void dlist_remove_link(DList *list, DListElement *element)
{
    assert(NULL != list);
    assert(NULL != element);

    if (NULL != element->prev) {
        element->prev->next = element->next;
    }
    if (NULL != element->next) {
        element->next->prev = element->prev;
    }
    if (element == list->head) {
        list->head = list->head->next;
        if (NULL != list->head) {
            list->head->prev= NULL;
        }
    }
    if (element == list->tail) {
        list->tail = list->tail->prev;
        if (NULL != list->tail) {
            list->tail->next = NULL;
        }
    }
    free(element);
    --list->length;
}

void dlist_remove_tail(DList *list)
{
    DListElement *tmp;

    assert(NULL != list);

    if (list->tail) {
        tmp = list->tail;
        list->tail = list->tail->prev;
        if (NULL != list->tail) {
            list->tail->next = NULL;
        } else {
            list->head = list->tail = NULL;
        }
        if (NULL != list->dtor) {
            list->dtor(tmp->data);
        }
        free(tmp);
        --list->length;
    }
}

static DListElement *resolve_position(DList *list, int n)
{
    int c;
    DListElement *cur;

    assert(NULL != list);

    c = n < 0 ? -n : n;
    cur = n < 0 ? list->tail : list->head;
    while (NULL != cur && 0 != c) {
        cur = n < 0 ? cur->prev : cur->next;
        --c;
    }

    return (0 == c && NULL != cur) ? cur : NULL;
}

bool dlist_at(DList *list, int n, void **data)
{
    DListElement *cur;

    assert(NULL != list);
    assert(NULL != data);

    if (NULL != (cur = resolve_position(list, n))) {
        *data = cur->data;
    }

    return NULL != cur;
}

void dlist_sort(DList *list, CmpFunc cmp)
{
    assert(NULL != list);
    assert(NULL != cmp);

    if (NULL != list->head) {
        bool swapped;
        DListElement *cur, *tmp;

        tmp = NULL;
        do {
            swapped = false;
            cur = list->head;
            while (cur->next != tmp) {
                if (cmp(cur->data, cur->next->data) > 0) {
                    void *data;

                    data = cur->data;
                    cur->data = cur->next->data;
                    cur->next->data = data;
                    swapped = true;
                }
                cur = cur->next;
            }
            tmp = cur;
        } while (swapped);
    }
}

/*
void dlist_apply(DList *list, void (*x)(int))
{
    //
}
*/

#ifndef WITHOUT_ITERATOR
static void dlist_iterator_first(const void *collection, void **state)
{
    assert(NULL != collection);
    assert(NULL != state);

    *state = ((const DList *) collection)->head;
}

static void dlist_iterator_last(const void *collection, void **state)
{
    assert(NULL != collection);
    assert(NULL != state);

    *state = ((const DList *) collection)->tail;
}

static bool dlist_iterator_is_valid(const void *UNUSED(collection), void **state)
{
    assert(NULL != state);

    return NULL != *state;
}

static void dlist_iterator_current(const void *UNUSED(collection), void **state, void **UNUSED(key), void **value)
{
    assert(NULL != state);

    if (NULL != value) {
        *value = ((DListElement *) *state)->data;
    }
}

static void dlist_iterator_next(const void *UNUSED(collection), void **state)
{
    DListElement *el;

    assert(NULL != state);

    el = (DListElement *) *state;
    *state = el->next;
}

static void dlist_iterator_previous(const void *UNUSED(collection), void **state)
{
    DListElement *el;

    assert(NULL != state);

    el = (DListElement *) *state;
    *state = el->prev;
}

/**
 * Initialize an *Iterator* to loop, in both directions, on the elements
 * of a double linked list.
 *
 * @param it the iterator to initialize
 * @param da the double linked list to traverse
 *
 * @note iterator directions: forward and backward
 * @note there is no key
 **/
void dlist_to_iterator(Iterator *it, DList *list)
{
    iterator_init(
        it, list, NULL,
        dlist_iterator_first, dlist_iterator_last,
        dlist_iterator_current,
        dlist_iterator_next, dlist_iterator_previous,
        dlist_iterator_is_valid,
        NULL
    );
}
#endif /* !WITHOUT_ITERATOR */
