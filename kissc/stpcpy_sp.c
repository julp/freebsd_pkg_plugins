#include <stddef.h>

/**
 * Safely copy a string into a buffer and get back a pointer to its trailing nul
 * byte to easily reuse it to append any other string.
 *
 * \code
 *  #define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
 *  #define STR_SIZE(str) (ARRAY_SIZE(str))
 *
 *  const char *home;
 *  char *w, buffer[MAXPATHLEN];
 *  const char * const buffer_end = buffer + STR_SIZE(buffer);
 *
 *  if (NULL == (home = getenv("HOME"))) {
 *      // oops
 *  }
 *  if (NULL == (w = stpcpy_sp(buffer, home, buffer_end))) {
 *      // buffer overflow
 *  }
 *  if (NULL == (w = stpcpy_sp(w, "/MyApp/config", buffer_end))) {
 *      // buffer overflow
 *  }
 * \endcode
 *
 * @param to the output buffer
 * @param from the string to (attempt to) copy
 * @param to_limit the limit of *to* buffer (this is *to* + its size)
 *
 * @return `NULL` if *to* capacity is insufficient else a pointer on the `\0` character
 * which terminates it.
 */
char *stpcpy_sp(char *to, const char *from, const char * const to_limit)
{
    const char * const zero = to_limit - 1;

    if (NULL == to || to >= zero) {
        return NULL;
    }
    if (NULL != from) {
        while (to < to_limit && '\0' != (*to++ = *from++))
            ;
    }
    if (to == to_limit && '\0' != *zero) {
        to[-1] = '\0';
        return NULL;
    } else {
        return to - 1;
    }
}
