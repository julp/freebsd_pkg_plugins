#include <stdlib.h>

#include "common.h"
#include "selection.h"

struct selection_t {
    CmpFunc cmp;
    DupFunc dup;
    DtorFunc dtor;
    size_t length;
    selection_element_t *head, *tail;
};

struct selection_element_t {
    void *data;
    selection_element_t *next, *prev;
};

selection_t *selection_new(CmpFunc cmp, DtorFunc dtor, DupFunc dup)
{
    selection_t *sel;

    assert(NULL != cmp);

    if (NULL != (sel = malloc(sizeof(*sel)))) {
        sel->cmp = cmp;
        sel->dtor = dtor;
        sel->length = 0;
        sel->dup = dup;
        sel->head = sel->tail = NULL;
    }

    return sel;
}

bool selection_is_empty(selection_t *sel)
{
    assert(NULL != sel);

    return NULL == sel->head;
}

size_t selection_length(selection_t *sel)
{
    assert(NULL != sel);

    return sel->length;
}

bool selection_add(selection_t *sel, void *data)
{
    selection_element_t *el;

    assert(NULL != sel);

    do {
        if (NULL == (el = malloc(sizeof(*el)))) {
            break;
        }
        if (NULL == (el->data = sel->dup ? sel->dup(data) : data)) {
            free(el);
            el = NULL;
            break;
        }
        if (NULL == sel->head) {
            el->prev = el->next = NULL;
            sel->head = sel->tail = el;
        } else if (sel->cmp(el->data, sel->head->data) < 0) {
            el->next = sel->head;
            el->next->prev = el;
            sel->head = el;
        } else {
            selection_element_t *cur;

            for (cur = sel->head; NULL != cur->next && sel->cmp(el->data, cur->next->data) > 0; cur = cur->next)
                ;
            el->next = cur->next;
            if (NULL != cur->next) {
                el->next->prev = el;
            }
            cur->next = el;
            el->prev = cur;
        }
        ++sel->length;
    } while (false);

    return NULL != el;
}

bool selection_apply(selection_t *sel, bool (*apply)(void *, void *, char **), void *data, char **error)
{
    bool ok;

    assert(NULL != sel);

    ok = true;
    if (NULL != apply) {
        selection_element_t *cur;

        for (cur = sel->head; ok && NULL != cur; cur = cur->next) {
            ok &= apply(cur->data, data, error);
        }
    }

    return ok;
}

#ifdef DEBUG
void selection_dump(selection_t *sel, void (*apply)(void *))
{
    selection_element_t *cur;

    assert(NULL != sel);
    assert(NULL != apply);

    fprintf(stderr, "<%s (%zu)>\n", __func__, sel->length);
    for (cur = sel->head; NULL != cur; cur = cur->next) {
        apply(cur->data);
    }
    fprintf(stderr, "</%s>\n", __func__);
}
#endif /* DEBUG */

static selection_element_t *resolve_position(selection_t *sel, int n)
{
    int c;
    selection_element_t *cur;

    c = n < 0 ? -n : n;
    cur = n < 0 ? sel->tail : sel->head;
    while (NULL != cur && 0 != c) {
        cur = n < 0 ? cur->prev : cur->next;
        --c;
    }

    return (0 == c && NULL != cur) ? cur : NULL;
}

bool selection_at(selection_t *sel, int n, void **data)
{
    selection_element_t *cur;

    assert(NULL != sel);
    assert(NULL != data);

    if (NULL != (cur = resolve_position(sel, n))) {
        *data = cur->data;
    }

    return NULL != cur;
}

static bool selection_internal_append_no_check(selection_t *sel, void *data)
{
    selection_element_t *el;

    if (NULL != (el = malloc(sizeof(*el)))) {
        el->data = data;
        el->next = NULL;
        el->prev = sel->tail;
        sel->tail = el;
    }

    return NULL != el;
}

selection_t *selection_slice(selection_t *sel, int from, int to)
{
    selection_t *slice;

    assert(NULL != sel);

    slice = NULL;
    do {
        bool ok;
        selection_element_t *a, *b;

        if (NULL == (a = resolve_position(sel, from))) {
            break;
        }
        if (NULL == (b = resolve_position(sel, to))) {
            break;
        }
        if (sel->cmp(a->data, b->data) > 0) {
            selection_element_t *tmp;

            tmp = a;
            a = b;
            b = tmp;
        }
        ok = true;
        slice = selection_new(sel->cmp, NULL, NULL);
        do {
            ok &= selection_internal_append_no_check(slice, a->data);
            a = a->next;
        } while (ok && a != b);
        if (!ok) {
            selection_destroy(slice);
            break;
        }
    } while (false);

    return slice;
}

bool selection_filter(selection_t *sel, bool (*filter)(void *, void *), void *data, selection_t **accepted, selection_t **discarded)
{
    bool ok;

    assert(NULL != sel);
    assert(NULL != accepted);
    assert(NULL != discarded);

    ok = true;
    if (NULL != filter) {
        selection_element_t *cur;

        *accepted = selection_new(sel->cmp, NULL, NULL);
        *discarded = selection_new(sel->cmp, NULL, NULL);
        for (cur = sel->head; ok && NULL != cur; cur = cur->next) {
            ok &= selection_internal_append_no_check(filter(cur->data, data) ? *accepted : *discarded, data);
        }
    }

    return ok;
}

void selection_destroy(selection_t *sel)
{
    selection_element_t *tmp, *last;

    assert(NULL != sel);

    tmp = sel->head;
    while (NULL != tmp) {
        last = tmp;
        tmp = tmp->next;
        if (NULL != sel->dtor) {
            sel->dtor(last->data);
        }
        free(last);
    }
    free(sel);
}
