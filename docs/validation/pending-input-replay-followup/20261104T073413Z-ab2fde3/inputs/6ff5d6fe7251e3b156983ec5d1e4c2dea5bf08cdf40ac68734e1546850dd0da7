#ifndef PPCCODE_TEXT_COMPAT_H
#define PPCCODE_TEXT_COMPAT_H
#include <stddef.h>

/* The recovered callers intentionally accept truncation. These interfaces
 * preserve libc's return values, padding and termination rules. In particular,
 * app_copy_padded does NOT append a terminator when the source fills the field.
 * Their distinct names make that contract explicit at the affected call sites.
 * Source and destination must not overlap, as for strncpy/snprintf. */
char *app_copy_padded(char *destination, const char *source, size_t count);
int app_format_truncated(char *destination, size_t capacity, const char *format, ...)
    __attribute__((format(printf, 3, 4)));
#endif
