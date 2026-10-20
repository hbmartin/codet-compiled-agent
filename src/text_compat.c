#include "text_compat.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

char *app_copy_padded(char *destination, const char *source, size_t count) {
  size_t length = 0;
  while (length < count && source[length] != '\0')
    ++length;
  if (length != 0)
    memcpy(destination, source, length);
  if (length < count)
    memset(destination + length, 0, count - length);
  return destination;
}

int app_format_truncated(char *destination, size_t capacity, const char *format, ...) {
  va_list arguments;
  va_start(arguments, format);
  int result = vsnprintf(destination, capacity, format, arguments);
  va_end(arguments);
  return result;
}
