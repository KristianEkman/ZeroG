#ifndef WIN_STRINGS_H
#define WIN_STRINGS_H

#include <string.h>

#ifdef _MSC_VER
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif
#endif

#endif /* WIN_STRINGS_H */
