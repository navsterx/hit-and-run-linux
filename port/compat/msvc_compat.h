/* Force-included into every translation unit of the Linux build (see port/gen_cmake.py).
 * Maps MSVC compiler built-ins and CRT names onto GCC/POSIX equivalents so the original
 * source compiles unchanged. Must stay valid C and C++98. */
#ifndef SHAR_MSVC_COMPAT_H
#define SHAR_MSVC_COMPAT_H

#if defined(__GNUC__) && !defined(_MSC_VER)

#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

/* ---- compiler keywords ---- */
#define __int8  char
#define __int16 short
#define __int32 int
#define __int64 long long
#define _int64  long long
#define __forceinline inline
#define __cdecl
#define __stdcall
#define __fastcall
#define __declspec(x)
#define _inline inline

/* Symbols that must stay private to the executable. The game replaces the global operator new/delete;
   on Windows that only affects the .exe, but an ELF executable's definition would also replace them
   for every shared library (GPU drivers included). Hidden visibility restores the Windows behaviour. */
#define SHAR_LOCAL __attribute__((visibility("hidden")))

/* ---- CRT names ---- */
#define _isnan(x)   __builtin_isnan(x)
#define _finite(x)  __builtin_isfinite(x)
#define stricmp     strcasecmp
#define _stricmp    strcasecmp
#define strnicmp    strncasecmp
#define _strnicmp   strncasecmp
#define strcmpi     strcasecmp
#define _strcmpi    strcasecmp
#define _snprintf   snprintf
#define _vsnprintf  vsnprintf
#define _strdup     strdup

#include <ctype.h>
static inline char* shar_strupr(char* s) { char* p = s; for(; p && *p; ++p) *p = (char)toupper((unsigned char)*p); return s; }
static inline char* shar_strlwr(char* s) { char* p = s; for(; p && *p; ++p) *p = (char)tolower((unsigned char)*p); return s; }
#define strupr  shar_strupr
#define _strupr shar_strupr
#define strlwr  shar_strlwr
#define _strlwr shar_strlwr

#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#endif /* __GNUC__ */
#endif /* SHAR_MSVC_COMPAT_H */
