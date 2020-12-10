#pragma once

int ascii_isupper(int c);
int ascii_toupper(int c);
int ascii_islower(int c);
int ascii_tolower(int c);
int ascii_memcasecmp(const char *str1, const char *str2, size_t n);
int ascii_strcasecmp(const char *str1, const char *str2);
int ascii_strcasecmp_l(const char *str1, size_t str1_len, const char *str2, size_t str2_len);
int ascii_strncasecmp(const char *str1, const char *str2, size_t n);
int ascii_strncasecmp_l(const char *str1, size_t str1_len, const char *str2, size_t str2_len, size_t n);
char *ascii_memcasechr(const char *str, int c, size_t n);
