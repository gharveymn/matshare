#ifndef MATSHARE_UTILS_H
#define MATSHARE_UTILS_H

#include "mex.h"

#if !defined MSH_WIN && !defined MSH_UNIX
#if defined(MATLAB_UNIX)
#  include <sys/mman.h>
#  define MSH_UNIX
#elif defined(MATLAB_WINDOWS)
#  define MSH_WIN
#elif defined(DEBUG_UNIX)
#  define MSH_UNIX
#elif defined(DEBUG_UNIX_ON_WINDOWS)
#  include <sys/stat.h>
#  define MSH_UNIX
#  ifdef __GNUC__
extern char *strerror_r(int errnum, char *buf, size_t buflen);
#else
extern int strerror_r(int errnum, char *buf, size_t buflen);
#endif
#elif defined(DEBUG_WINDOWS)
#  define MSH_WIN
#else
# error(No build type specified.)
#endif
#endif

#ifdef MSH_WIN
#include <windows.h>
typedef DWORD errcode_t;
#else
typedef int errcode_t;
#endif

#define ERROR_SEVERITY_USER        1 << 0
#define ERROR_SEVERITY_INTERNAL    1 << 1
#define ERROR_SEVERITY_SYSTEM      1 << 2
#define ERROR_SEVERITY_CORRUPTION  1 << 3
#define ERROR_SEVERITY_FATAL       1 << 4

void ReadMexError(const char* file_name, int line, unsigned int error_severity, errcode_t error_code, const char* error_id, const char* error_message, ...);
void ReadMexWarning(const char* warn_id, const char* warn_message, ...);
void SetMexErrorCallback(void (*callback_function)(void));

#endif /* MATSHARE_UTILS_H */
