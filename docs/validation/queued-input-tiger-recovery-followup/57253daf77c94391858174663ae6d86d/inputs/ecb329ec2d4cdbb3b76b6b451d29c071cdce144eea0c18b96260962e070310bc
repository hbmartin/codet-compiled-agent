/* Compile-time checks against the actual Tiger SDK; never executed. */
#include <stddef.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <dirent.h>
#include "recovered-types.h"
_Static_assert(sizeof(void *) == 4, "PPC32");
_Static_assert(sizeof(int) == 4 && sizeof(long) == 4 && sizeof(size_t) == 4,
               "Recovered register words and formatted integer arguments are 32 bits");
_Static_assert(sizeof(RecoveredStreamCallback) == sizeof(void *),
               "Recovered callback addresses use one PPC32 register word");
_Static_assert(__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__, "big endian");
_Static_assert(sizeof(struct stat) == sizeof(RecoveredTigerStat), "stat size");
_Static_assert(offsetof(struct stat, st_size) == 48, "stat size offset");
_Static_assert(offsetof(struct stat, st_mtimespec) == 32, "stat mtime offset");
_Static_assert(sizeof(struct dirent) == sizeof(RecoveredTigerDirent), "dirent size");
_Static_assert(offsetof(struct dirent, d_name) == 8, "dirent name offset");
_Static_assert(sizeof(struct utsname) == sizeof(RecoveredTigerUtsname), "utsname size");
_Static_assert(offsetof(struct utsname, machine) == 1024, "utsname machine offset");
int recovered_layout_probe(void) { return 0; }
