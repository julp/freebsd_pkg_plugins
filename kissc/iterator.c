/**
 * @file iterator/iterator.c
 * @brief An iterator is an abstraction layer to easily traverse a collection of (read only) items.
 *
 * This is simple as this:
 * \code
 *   Iterator it;
 *   const MyItem *item;
 *
 *   // initialize the iterator
 *   my_collection_to_iterator(&it);
 *   // traverse it (forward order here)
 *   for (iterator_first(&it); iterator_is_valid(&it, NULL, &item); iterator_next(&it)) {
 *       // use item
 *   }
 *   // we're done, "close" the iterator
 *   iterator_close(&it);
 * \endcode
 *
 * You can even known if your collection is empty by writing:
 * \code
 *   Iterator it;
 *   const MyItem *item;
 *
 *   // initialize the iterator
 *   my_collection_to_iterator(&it);
 *   iterator_first(&it);
 *   if (iterator_is_valid(&it, NULL, &item)) {
 *       // collection is not empty, "normal" traversal
 *       do {
 *           // use item
 *           iterator_next(&it); // have to be the last instruction of the loop
 *       } while (iterator_is_valid(&it, NULL, &item));
 *   } else {
 *       // collection is empty
 *   }
 *   iterator_close(&it);
 * \endcode
 */

#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "attributes.h"
#include "iterator.h"

static const Iterator NULL_ITERATOR;

/**
 * Initialize an iterator
 *
 * @param collection the collection to iterate on
 * @param state a data passed to the callbacks to mark the current position in the collection
 * @param first a callback to (re)initialize the current position on the first item of the
 * collection (may be `NULL` if the collection is not intended to be traversed in forward order)
 * @param last a callback to (re)initialize the current position on the last item of the
 * collection (may be `NULL` if the collection is not intended to be traversed in reverse order)
 * @param current the callback to get the current element
 * @param next the callback to move the internal position to the next element
 * (may be `NULL` if the collection is not intended to be traversed in forward order)
 * @param previous the callback to move the internal position to the previous element
 * (may be `NULL` if the collection is not intended to be traversed in reverse order)
 * @param valid the callback to known if the internal position is still valid after
 * moving it to its potential next or previous element
 * @param close a callback to free internal state (`NULL` if you have nothing to free)
 * @param count if you can provide a callback to get the number of elements in *collection* without
 * traversing it set it there. If not, set it to `NULL` to use the default (and slow) algorithm.
 * @param member TODO
 * @param delete TODO
 */
void iterator_init(
    Iterator *it,
    const void *collection,
    void *state,
    iterator_first_t first,
    iterator_last_t last,
    iterator_current_t current,
    iterator_next_t next,
    iterator_previous_t previous,
    iterator_is_valid_t valid,
    iterator_close_t close,
    iterator_count_t count,
    iterator_member_t member,
    iterator_delete_current_t delete
) {
    it->state = state;
    it->collection = collection;
    it->first = first;
    it->last = last;
    it->current = current;
    it->next = next;
    it->previous = previous;
    it->valid = valid;
    it->close = close;
    it->count = count;
    it->member = member;
    it->delete = delete;
}

/**
 * End the iterator
 * (it shouldn't no longer be used)
 *
 * @param it the iterator
 */
void iterator_close(Iterator *it)
{
    if (NULL != it->close) {
        it->close(it->state);
    }
    *it = NULL_ITERATOR;
}


/**
 * (Re)set internal position to the first element (if supported)
 *
 * @param it the iterator to (re)set
 */
void iterator_first(Iterator *it)
{
    if (NULL != it->first) {
        it->first(it->collection, &it->state);
    }
}

/**
 * (Re)set internal position to the last element (if supported)
 *
 * @param it the iterator to (re)set
 */
void iterator_last(Iterator *it)
{
    if (NULL != it->last) {
        it->last(it->collection, &it->state);
    }
}

/**
 * Move internal position to the next element (if supported)
 *
 * @param it the iterator to move forward
 */
void iterator_next(Iterator *it)
{
    if (NULL != it->next) {
        it->next(it->collection, &it->state);
    }
}

/**
 * Move internal position to the previous element (if supported)
 *
 * @param it the iterator to move backward
 */
void iterator_previous(Iterator *it)
{
    if (NULL != it->previous) {
        it->previous(it->collection, &it->state);
    }
}

/**
 * Determine if the current position of the iterator is still valid
 * If not, the only valid operations are a reset of its internal
 * cursor to its first or last element (if any) or to close it if
 * you are done
 *
 * @param it the iterator
 * @param key the associated to the current element, if any
 * @param value the current element
 */
bool _iterator_is_valid(Iterator *it, void **key, void **value)
{
    bool valid;

    if ((valid = it->valid(it->collection, &it->state))) {
        it->current(it->collection, &it->state, key, value);
    }

    return valid;
}

/**
 * TODO
 */
bool iterator_empty(Iterator *it)
{
    bool empty;

    assert(NULL != it);

    if (NULL != it->count) {
        empty = 0 == it->count(it->collection);
    } else {
        iterator_first(it);
        empty = !iterator_is_valid(it, NULL, NULL);
    }

    return empty;
}

/**
 * TODO
 */
size_t iterator_count(Iterator *it)
{
    size_t count;

    assert(NULL != it);

    if (NULL != it->count) {
        count = it->count(it->collection);
    } else {
        count = 0;
        // TODO: use iterator_reduce ?
        for (iterator_first(it); iterator_is_valid(it, NULL, NULL); iterator_next(it)) {
            ++count;
        }
    }

    return count;
}

/**
 * TODO
 */
void iterator_delete_current(Iterator *it)
{
    assert(NULL != it);

    if (NULL != it->delete) {
        it->delete(it->collection, &it->state);
    }
}

#define CHAR_P(p) \
    ((const char *) (p))

/* ========== array ========== */

typedef struct {
    const char *ptr;
    size_t element_size;
    size_t element_count;
} as_t /*array_state*/;

static void array_iterator_first(const void *collection, void **state)
{
    as_t *s;

    assert(NULL != collection);
    assert(NULL != state);
    assert(NULL != *state);

    s = (as_t *) *state;
    s->ptr = collection;
}

static void array_iterator_last(const void *collection, void **state)
{
    as_t *s;

    assert(NULL != collection);
    assert(NULL != state);
    assert(NULL != *state);

    s = (as_t *) *state;
    s->ptr = collection + (s->element_count - 1) * s->element_size;
}

static bool array_iterator_is_valid(const void *collection, void **state)
{
    as_t *s;

    assert(NULL != state);
    assert(NULL != *state);

    s = (as_t *) *state;

    return s->ptr >= CHAR_P(collection) && s->ptr < CHAR_P(collection) + s->element_count * s->element_size;
}

static void array_iterator_current(const void *collection, void **state, void **key, void **value)
{
    as_t *s;

    assert(NULL != state);

    s = (as_t *) *state;
    if (NULL != value) {
      *value = (void *) s->ptr;
    }
    if (NULL != key) {
        *((uint64_t *) key) = (s->ptr - CHAR_P(collection)) / s->element_size;
    }
}

static void array_iterator_next(const void *UNUSED(collection), void **state)
{
    as_t *s;

    assert(NULL != state);
    assert(NULL != *state);

    s = (as_t *) *state;
    s->ptr += s->element_size;
}

static void array_iterator_prev(const void *UNUSED(collection), void **state)
{
    as_t *s;

    assert(NULL != state);
    assert(NULL != *state);

    s = (as_t *) *state;
    s->ptr -= s->element_size;
}

/**
 * Iterate on a "regular" C-array of any type.
 *
 * @param it the iterator to initialize
 * @param array the array to iterate on
 * @param element_size the size of an element of this array
 * @param element_count the number of elements in the array
 *
 * Example:
 * \code
 *   Iterator it;
 *   int i, numbers[] = {1, 2, 3, 4};
 *
 *   i = 0;
 *   // initialize the iterator
 *   array_to_iterator(&it, sizeof(numbers[0]), ARRAY_SIZE(numbers));
 *   // iterate on values from numbers
 *   for (iterator_first(&it); iterator_is_valid(&it, NULL, &value); iterator_next(&it)) {
 *     printf("%d: %d\n", i++, *value);
 *   }
 *   // or use whatever iterator_* function(s)
 *   printf("Their sum is: %" PRIi64 "\n", iterator_sum(&it));
 *   // but don't forget to call iterate_close when you're done
 *   iterator_close(&it);
 * \endcode
 */
void array_to_iterator(Iterator *it, void *array, size_t element_size, size_t element_count)
{
    as_t *s;

    s = malloc(sizeof(*s));
    s->ptr = CHAR_P(array);
    s->element_size = element_size;
    s->element_count = element_count;

    iterator_init(
        it, array, s,
        array_iterator_first, array_iterator_last,
        array_iterator_current,
        array_iterator_next, array_iterator_prev,
        array_iterator_is_valid,
        free,
        NULL, NULL, NULL
    );
}

/* ========== NULL terminated (pointers) array ========== */

static void null_terminated_ptr_array_iterator_first(const void *collection, void **state)
{
    assert(NULL != collection);
    assert(NULL != state);

    *(void ***) state = (void **) collection;
}

static bool null_terminated_ptr_array_iterator_is_valid(const void *UNUSED(collection), void **state)
{
    assert(NULL != state);

    return NULL != **((void ***) state);
}

static void null_terminated_ptr_array_iterator_current(const void *collection, void **state, void **key, void **value)
{
    assert(NULL != collection);
    assert(NULL != state);

    if (NULL != value) {
        *value = **(void ***) state;
    }
    if (NULL != key) {
        *((uint64_t *) key) = *state - collection;
    }
}

static void null_terminated_ptr_array_iterator_next(const void *UNUSED(collection), void **state)
{
    assert(NULL != state);

    ++*((void ***) state);
}

/**
 * Iterate on a NULL terminated array of pointers
 *
 * @note the avantage of this iterator is that it doesn't need (allocate)
 * any additionnal memory for its use. (but keep calling iterator_close for
 * one of this kind)
 *
 * @param it the iterator to initialize
 * @param array the array to iterate on
 */
void null_terminated_ptr_array_to_iterator(Iterator *it, void **array)
{
    iterator_init(
        it, array, NULL,
        null_terminated_ptr_array_iterator_first, NULL,
        null_terminated_ptr_array_iterator_current,
        null_terminated_ptr_array_iterator_next, NULL,
        null_terminated_ptr_array_iterator_is_valid,
        NULL,
        NULL, NULL, NULL
    );
}

/* ========== NULL sentineled field terminated array of struct/union ========== */

typedef struct {
    const char *ptr;
    size_t element_size;
    size_t field_offset;
} nsftas_t /*null_sentineled_field_terminated_array_state*/;

static void null_sentineled_field_terminated_array_iterator_first(const void *collection, void **state)
{
    nsftas_t *s;

    assert(NULL != collection);
    assert(NULL != state);
    assert(NULL != *state);

    s = (nsftas_t *) *state;
    s->ptr = collection;
}

static bool null_sentineled_field_terminated_array_iterator_is_valid(const void *UNUSED(collection), void **state)
{
    nsftas_t *s;

    assert(NULL != state);
    assert(NULL != *state);

    s = (nsftas_t *) *state;

    return NULL != *((char **) (s->ptr + s->field_offset));
}

static void null_sentineled_field_terminated_array_iterator_current(const void *collection, void **state, void **key, void **value)
{
    nsftas_t *s;

    assert(NULL != state);

    s = (nsftas_t *) *state;
    if (NULL != value) {
        *value = (void *) s->ptr;
    }
    if (NULL != key) {
        *((uint64_t *) key) = (s->ptr - CHAR_P(collection)) / s->element_size;
    }
}

static void null_sentineled_field_terminated_array_iterator_next(const void *UNUSED(collection), void **state)
{
    nsftas_t *s;

    assert(NULL != state);
    assert(NULL != *state);

    s = (nsftas_t *) *state;
    s->ptr += s->element_size;
}

/**
 * Iterate on an array of struct (or union) where one of its field is sentineled
 * by a NULL pointer.
 *
 * @param it the iterator to initialize
 * @param array the array to iterate on
 * @param element_size the size of an element of this array
 * @param field_offset the offset of the member to test against NULL
 * (use offsetof to define it)
 */
void null_sentineled_field_terminated_array_to_iterator(Iterator *it, void *array, size_t element_size, size_t field_offset)
{
    nsftas_t *s;

    s = malloc(sizeof(*s));
    s->ptr = CHAR_P(array);
    s->element_size = element_size;
    s->field_offset = field_offset;

    iterator_init(
        it, array, s,
        null_sentineled_field_terminated_array_iterator_first, NULL,
        null_sentineled_field_terminated_array_iterator_current,
        null_sentineled_field_terminated_array_iterator_next, NULL,
        null_sentineled_field_terminated_array_iterator_is_valid,
        free,
        NULL, NULL, NULL
    );
}

/* ========== utilities ========== */

/**
 * Check if a collection contains at least one value from which *callback* returns `true`
 *
 * @param it the iterator to traverse (until *callback* returns `true`)
 * @param callback the callback called on each element for comparaison or test
 * @param user_data a potential user data to transmit to each call to *callback* (if you
 * don't use it set it to `NULL` then ignore this parameter in your callback)
 *
 * @return `true` if at least one value satisfies *callback* else `false`
 *
 * An example which somewhat mimics the function iterator_member:
 * \code
 *   static bool is_same_string(const void *value, const void *data) {
 *       return 0 == strcmp((const char *) value, (const char *) data);
 *   }
 *
 *   bool found;
 *   Iterator it;
 *   const char *words[] = {"hello", "world"};
 *
 *   array_to_iterator(&it, sizeof(words[0]), ARRAY_SIZE(words));
 *   found = iterator_any(&it, is_same_string, "hello");
 *   printf("words %s contain%s anagram\n", found ? "" : "does not", found ? "" : "s");
 *   iterator_close(&it);
 * \endcode
 **/
bool iterator_any(Iterator *it, FilterFunc callback, const void *user_data)
{
    bool any;
    void *value;

    assert(NULL != it);

    for (any = false, iterator_first(it); !any && iterator_is_valid(it, NULL, &value); iterator_next(it)) {
        any = callback(value, user_data);
    }

    return any;
}

/**
 * TODO
 *
 * @param it TODO
 * @param callback TODO
 * @param user_data TODO
 *
 * @return TODO
 **/
bool iterator_all(Iterator *it, FilterFunc callback, const void *user_data)
{
    bool all;
    void *value;

    assert(NULL != it);

    for (all = true, iterator_first(it); all && iterator_is_valid(it, NULL, &value); iterator_next(it)) {
        all &= callback(value, user_data);
    }

    return all;
}

#if 0
typedef void (*ApplyFunc)(void *value, const void *data);
typedef bool (*ApplyWithErrorFunc)(void *value, const void *user_data, char **error);

/**
 * TODO
 **/
void iterator_each(Iterator *it, ApplyFunc callback, const void *user_data)
{
    void *value;

    assert(NULL != it);

    for (iterator_first(it); iterator_is_valid(it, NULL, &value); iterator_next(it)) {
        callback(value, user_data);
    }
}

bool iterator_each_with_error(Iterator *it, ApplyFunc callback, const void *user_data, char **error)
{
    bool ok;
    void *value;

    assert(NULL != it);

    for (ok = true, iterator_first(it); ok && iterator_is_valid(it, NULL, &value); iterator_next(it)) {
        ok &= callback(value, user_data, error);
    }

    return ok;
}
#endif

/**
 * TODO
 **/
bool iterator_at(Iterator *it, int index, void **data)
{
    int i;
    void *value;
    void (*next)(Iterator *);
    void (*first)(Iterator *);

    assert(NULL != it);

    if (index < 0) {
        index = -index;
        first = iterator_last;
        next = iterator_previous;
    } else {
        first = iterator_first;
        next = iterator_next;
    }
    for (i = 0, first(it); i < index && iterator_is_valid(it, NULL, &value); next(it))
        ;
    if (i == index && NULL != data) {
        *data = value;
    }

    return i == index;
}

/**
 * TODO
 **/
bool iterator_max(Iterator *it, CmpFunc cmp, void **max)
{
    bool any;
    void *current_max, *value;

    assert(NULL != it);

    iterator_first(it);
    any = iterator_is_valid(it, NULL, &current_max/*max*/);
    if (any) {
        iterator_next(it);
        while (iterator_is_valid(it, NULL, &value)) {
//         do {
            if (cmp(value, current_max/**max*/) > 0) {
                /**max*/current_max = value;
            }
            iterator_next(it);
        }
//         } while (iterator_is_valid(it, NULL, &value));
        *max = current_max;
    }

    return any;
}

/**
 * TODO
 **/
bool iterator_reduce(Iterator *it, void *acc, bool (*callback)(void *acc, void *value, char **error), char **error)
{
    bool ok;
    void *value;

    assert(NULL != it);

    for (ok = true, iterator_first(it); ok && iterator_is_valid(it, NULL, &value); iterator_next(it)) {
        ok &= callback(acc, value, error);
    }

    return ok;
}

static bool iterator_sum_callback(void *acc, void *value, char **UNUSED(error))
{
    *((int64_t *) acc) += *((int64_t *) value);

    return true;
}

/**
 * TODO
 **/
int64_t iterator_sum(Iterator *it)
{
    int64_t sum;

    assert(NULL != it);

    sum = 0;
    iterator_reduce(it, &sum, iterator_sum_callback, NULL);

    return sum;
}

static bool iterator_product_callback(void *acc, void *value, char **UNUSED(error))
{
    *((int64_t *) acc) *= *((int64_t *) value);

    return true;
}

/**
 * TODO
 **/
int64_t iterator_product(Iterator *it)
{
    int64_t product;

    assert(NULL != it);

    product = 1;
    iterator_reduce(it, &product, iterator_product_callback, NULL);

    return product;
}

/**
 * TODO
 **/
void iterator_filter(Iterator *it, FilterFunc callback, const void *data)
{
    void *value;

    assert(NULL != it);
    assert(NULL != callback);

    for (iterator_first(it); iterator_is_valid(it, NULL, &value); iterator_next(it)) {
        if (!callback(value, data)) {
            iterator_delete_current(it);
        }
    }
}

/**
 * TODO
 **/
void iterator_reject(Iterator *it, FilterFunc callback, const void *data)
{
    void *value;

    assert(NULL != it);
    assert(NULL != callback);

    for (iterator_first(it); iterator_is_valid(it, NULL, &value); iterator_next(it)) {
        if (callback(value, data)) {
            iterator_delete_current(it);
        }
    }
}

/* ========== collectable ========== */

/**
 * TODO
 **/
void collectable_init(Collectable *collectable, void *collection, collectable_into_t into)
{
    assert(NULL != collectable);
    assert(NULL != collection);
    assert(NULL != into);

    collectable->collection = collection;
    collectable->into = into;
}


static bool iterator_into_callback(void *acc, void *value, char **UNUSED(error))
{
    Collectable *collectable;

    collectable = (Collectable *) acc;
    collectable->into(collectable->collection, NULL/*TODO: key?*/, value);

    return true;
}

/**
 * TODO
 **/
bool iterator_into(Iterator *it, Collectable *collectable)
{
    return iterator_reduce(it, collectable, iterator_into_callback, NULL);
}
