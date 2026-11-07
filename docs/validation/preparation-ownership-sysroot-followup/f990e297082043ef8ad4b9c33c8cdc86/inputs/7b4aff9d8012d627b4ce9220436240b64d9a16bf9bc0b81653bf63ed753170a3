#ifndef PPCCODE_APP_H
#define PPCCODE_APP_H
/* Tiger SDK declarations and the recovered application's internal interfaces. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <glob.h>
#include <pwd.h>
#include <fnmatch.h>
#include <locale.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <curses.h>
#include <curl/curl.h>
#include <sqlite3.h>
#include "recovered-types.h"
#include "observed-abi.h"
#include "text_compat.h"
typedef int BOOL;
typedef void code();
#define ABS(x) fabs(x)

/* Explicit operations corresponding to Ghidra's integer/float bit notation. */
static inline uint64_t app_double_bits(double x) { uint64_t n; memcpy(&n, &x, 8); return n; }
static inline double app_bits_double(uint64_t n) { double x; memcpy(&x, &n, 8); return x; }
static inline uint64_t app_float_bits(float x) { uint32_t n; memcpy(&n, &x, 4); return n; }
static inline uint64_t app_integer_bits(uint64_t x) { return x; }
#define APP_BITS(x) _Generic((x), double: app_double_bits, float: app_float_bits, default: app_integer_bits)(x)
#define CONCAT44(hi, lo) (((uint64_t)(uint32_t)(hi) << 32) | (uint32_t)(lo))
#define SUB84(x, offset) ((uint32_t)(APP_BITS(x) >> ((offset) * 8)))
#define APP_FIELD8(object, offset) (*(uint8_t *)((char *)&(object) + (offset)))
#define APP_FIELD16(object, offset) (*(uint16_t *)((char *)&(object) + (offset)))
#define APP_FIELD32(object, offset) (*(uint32_t *)((char *)&(object) + (offset)))
#define APP_FIELD64(object, offset) (*(uint64_t *)((char *)&(object) + (offset)))

#include "app-globals.h"
#include "app-functions.h"
#endif
