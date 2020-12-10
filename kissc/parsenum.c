#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

#include "parsenum.h"

/**
 * - PARSE_NUM_NO_ERR: all characters of the string were consumed to convert it into an integer
 *
 * - PARSE_NUM_ERR_INVALID_BASE: base parameter is invalid
 *
 * - PARSE_NUM_ERR_NO_DIGIT_FOUND: the given string contains no digit at all, meaning we reach the end of the string without finding a first digit for
 *   the conversion into integer. Empty strings ("") as empty prefixed binary (0b) and hexadecimal (0x) numbers ("0x" as "0b") will throw this error.
 *   There is an exception: "0" with base auto-detection (base = 0) will be recognized as a (octal) zero without throwing this error.
 *
 * - PARSE_NUM_ERR_NON_DIGIT_FOUND: conversion failed on a character which can't be a digit (according to the actual base)
 *
 * - PARSE_NUM_ERR_TOO_SMALL:
 * - PARSE_NUM_ERR_TOO_LARGE:
 *   the number is valid but is out of physical limits
 *
 * - PARSE_NUM_ERR_LESS_THAN_MIN:
 * - PARSE_NUM_ERR_GREATER_THAN_MAX:
 *   the number is valid but is out of user limits
 **/

#define parse_signed(type, unsigned_type, value_type_min, value_type_max) \
    ParseNumError strto## type(const char *nptr, char **endptr, int base, type *min, type *max, type *ret) { \
        return strnto## type(nptr, nptr + strlen(nptr), endptr, base, min, max, ret); \
    } \
 \
    ParseNumError strnto## type(const char *nptr, const char * const end, char **endptr, int base, type *min, type *max, type *ret) { \
        char c; \
        char ***spp; \
        bool negative; \
        int any, cutlim; \
        ParseNumError err; \
        unsigned_type cutoff, acc; \
 \
        acc = any = 0; \
        negative = false; \
        err = PARSE_NUM_NO_ERR; \
        if (NULL == endptr) { \
            char **sp; \
 \
            sp = (char **) &nptr; \
            spp = &sp; \
        } else { \
            spp = &endptr; \
            *endptr = (char *) nptr; \
        } \
        if (**spp < end) { \
            if ('-' == ***spp) { \
                ++**spp; \
                negative = true; \
            } else { \
                negative = false; \
                if ('+' == ***spp) { \
                    ++**spp; \
                } \
            } \
            if ((0 == base || 2 == base) && '0' == ***spp && (end - **spp) > 1 && ('b' == (**spp)[1] || 'B' == (**spp)[1])) { \
                **spp += 2; \
                base = 2; \
            } \
            if ((0 == base || 16 == base) && '0' == ***spp && (end - **spp) > 1 && ('x' == (**spp)[1] || 'X' == (**spp)[1])) { \
                **spp += 2; \
                base = 16; \
            } \
            if (0 == base) { \
                base = '0' == ***spp ? 8 : 10; \
            } \
            if (base < 2 || base > 36) { \
                return PARSE_NUM_ERR_INVALID_BASE; \
            } \
            cutoff = negative ? (unsigned_type) - (value_type_min + value_type_max) + value_type_max : value_type_max; \
            cutlim = cutoff % base; \
            cutoff /= base; \
            while (**spp < end) { \
                if (***spp >= '0' && ***spp <= '9') { \
                    c = ***spp - '0'; \
                } else if (base > 10 && ***spp >= 'A' && ***spp <= 'Z') { \
                    c = ***spp - 'A' - 10; \
                } else if (base > 10 && ***spp >= 'a' && ***spp <= 'z') { \
                    c = ***spp - 'a' - 10; \
                } else { \
                    err = PARSE_NUM_ERR_NON_DIGIT_FOUND; \
                    break; \
                } \
                if (c >= base) { \
                    err = PARSE_NUM_ERR_NON_DIGIT_FOUND; \
                    break; \
                } \
                if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim)) { \
                    any = -1; \
                } else { \
                    any = 1; \
                    acc *= base; \
                    acc += c; \
                } \
                ++**spp; \
            } \
        } \
        if (any < 0) { \
            if (negative) { \
                *ret = value_type_min; \
                return PARSE_NUM_ERR_TOO_SMALL; \
            } else { \
                *ret = value_type_max; \
                return PARSE_NUM_ERR_TOO_LARGE; \
            } \
        } else if (!any && PARSE_NUM_NO_ERR == err) { \
            err = PARSE_NUM_ERR_NO_DIGIT_FOUND; \
        } else if (negative) { \
            *ret = -acc; \
        } else { \
            *ret = acc; \
        } \
        if (PARSE_NUM_NO_ERR == err) { \
            if (NULL != min && *ret < *min) { \
                err = PARSE_NUM_ERR_LESS_THAN_MIN; \
            } \
            if (NULL != max && *ret > *max) { \
                err = PARSE_NUM_ERR_GREATER_THAN_MAX; \
            } \
        } \
 \
        return err; \
    }

parse_signed(int8_t, uint8_t, INT8_MIN, INT8_MAX);
parse_signed(int16_t, uint16_t, INT16_MIN, INT16_MAX);
parse_signed(int32_t, uint32_t, INT32_MIN, INT32_MAX);
parse_signed(int64_t, uint64_t, INT64_MIN, INT64_MAX);

#undef parse_signed

#define parse_unsigned(type, value_type_max) \
    ParseNumError strto## type(const char *nptr, char **endptr, int base, type *min, type *max, type *ret) { \
        return strnto## type(nptr, nptr + strlen(nptr), endptr, base, min, max, ret); \
    } \
 \
    ParseNumError strnto## type(const char *nptr, const char * const end, char **endptr, int base, type *min, type *max, type *ret) { \
        char c; \
        char ***spp; \
        bool negative; \
        int any, cutlim; \
        type cutoff, acc; \
        ParseNumError err; \
 \
        acc = any = 0; \
        negative = false; \
        err = PARSE_NUM_NO_ERR; \
        if (NULL == endptr) { \
            char **sp; \
 \
            sp = (char **) &nptr; \
            spp = &sp; \
        } else { \
            spp = &endptr; \
            *endptr = (char *) nptr; \
        } \
        if (**spp < end) { \
            if ('-' == ***spp) { \
                ++**spp; \
                negative = true; \
            } else { \
                negative = false; \
                if ('+' == ***spp) { \
                    ++**spp; \
                } \
            } \
            if ((0 == base || 2 == base) && '0' == ***spp && (end - **spp) > 1 && ('b' == (**spp)[1] || 'B' == (**spp)[1])) { \
                **spp += 2; \
                base = 2; \
            } \
            if ((0 == base || 16 == base) && '0' == ***spp && (end - **spp) > 1 && ('x' == (**spp)[1] || 'X' == (**spp)[1])) { \
                **spp += 2; \
                base = 16; \
            } \
            if (0 == base) { \
                base = '0' == ***spp ? 8 : 10; \
            } \
            if (base < 2 || base > 36) { \
                return PARSE_NUM_ERR_INVALID_BASE; \
            } \
            cutoff = value_type_max / base; \
            cutlim = value_type_max % base; \
            while (**spp < end) { \
                if (***spp >= '0' && ***spp <= '9') { \
                    c = ***spp - '0'; \
                } else if (base > 10 && ***spp >= 'A' && ***spp <= 'Z') { \
                    c = ***spp - 'A' - 10; \
                } else if (base > 10 && ***spp >= 'a' && ***spp <= 'z') { \
                    c = ***spp - 'a' - 10; \
                } else { \
                    err = PARSE_NUM_ERR_NON_DIGIT_FOUND; \
                    break; \
                } \
                if (c >= base) { \
                    err = PARSE_NUM_ERR_NON_DIGIT_FOUND; \
                    break; \
                } \
                if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim)) { \
                    any = -1; \
                } else { \
                    any = 1; \
                    acc *= base; \
                    acc += c; \
                } \
                ++**spp; \
            } \
        } \
        if (any < 0) { \
            *ret = value_type_max; \
            return PARSE_NUM_ERR_TOO_LARGE; \
        } else if (!any && PARSE_NUM_NO_ERR == err) { \
            err = PARSE_NUM_ERR_NO_DIGIT_FOUND; \
        } else if (negative) { \
            *ret = -acc; \
        } else { \
            *ret = acc; \
        } \
        if (PARSE_NUM_NO_ERR == err) { \
            if (NULL != min && *ret < *min) { \
                err = PARSE_NUM_ERR_LESS_THAN_MIN; \
            } \
            if (NULL != max && *ret > *max) { \
                err = PARSE_NUM_ERR_GREATER_THAN_MAX; \
            } \
        } \
 \
        return err; \
    }

parse_unsigned(uint8_t, UINT8_MAX);
parse_unsigned(uint16_t, UINT16_MAX);
parse_unsigned(uint32_t, UINT32_MAX);
parse_unsigned(uint64_t, UINT64_MAX);

#undef parse_unsigned
