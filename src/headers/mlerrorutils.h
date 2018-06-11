#ifndef MATSHARE_UTILS_H
#define MATSHARE_UTILS_H

#include "mex.h"

#ifdef _WIN32
#  include <windows.h>
typedef DWORD errcode_t;
#else
typedef int errcode_t;
#  if(((_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && !_GNU_SOURCE) || defined(__APPLE__))
/* XSI-compliant version */
extern int strerror_r(int errnum, char *buf, size_t buflen);
#  elif defined(_GNU_SOURCE)
/* GNU-specific */
extern char *strerror_r(int errnum, char *buf, size_t buflen);
#  endif
#endif

#define MEU_SEVERITY_USER        1 << 0
#define MEU_SEVERITY_INTERNAL    1 << 1
#define MEU_SEVERITY_SYSTEM      1 << 2
#define MEU_SEVERITY_CORRUPTION  1 << 3
#define MEU_SEVERITY_FATAL       1 << 4

/**
 *
 *
 * @param file_name
 * @param line
 * @param error_severity
 * @param error_code
 * @param error_id
 * @param error_message
 * @param ...
 */
void meu_PrintMexError(const char* file_name, int line, unsigned int error_severity, errcode_t error_code, const char* error_id, const char* error_message, ...);
void meu_PrintMexWarning(const char* warn_id, const char* warn_message, ...);
void meu_SetLibraryName(char* library_name);
void meu_SetErrorHelpMessage(char* help_message);
void meu_SetWarningHelpMessage(char* help_message);
void meu_SetErrorCallback(void (* callback_function)(void));
void meu_SetWarningCallback(void (* callback_function)(void));

#endif /* MATSHARE_UTILS_H */
