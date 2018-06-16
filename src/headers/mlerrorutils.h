/** mlerrorutils.h
 * Declares functions and macros for mex error utility functions.
 *
 * Copyright (c) 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
 
#ifndef MATSHARE_UTILS_H
#define MATSHARE_UTILS_H

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
   typedef DWORD errcode_t;
#else
   typedef int errcode_t;
#  if(((_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && !_GNU_SOURCE) || defined(__APPLE__))
     /* XSI-compliant version */
     extern int strerror_r(int errnum, char *buf, size_t buflen);
#  else
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
 * Prints the specified error in MATLAB. Takes various parameters.
 *
 * @param file_name The file name. Only pass __FILE__.
 * @param line The line number. Only pass __LINE__.
 * @param error_severity The error severity. pass a bitmask of the MEU_SEVERITY macros.
 * @param error_code The system error code fetched by either GetLastError() or errno.
 * @param error_id The identifier for this error which will be appended to "[library name]:".
 * @param error_message The printf message format associated to the error.
 * @param ... The error message params in printf style.
 */
void meu_PrintMexError(const char* file_name, int line, unsigned int error_severity, errcode_t error_code, const char* error_id, const char* error_message, ...);

/**
 * Prints the specified warning message in MATLAB.
 *
 * @param warn_id The identifier for this warning which will be appended to "[library name]:".
 * @param warn_message The printf message format associated to the error.
 * @param ... The error message params in printf style.
 */
void meu_PrintMexWarning(const char* warn_id, const char* warn_message, ...);

/**
 * Sets the pointer to a library name which will print with the identifier.
 *
 * @param library_name The name of the library.
 */
void meu_SetLibraryName(char* library_name);

/**
 * Sets the pointer to an error help message.
 *
 * @param help_message The error help message.
 */
void meu_SetErrorHelpMessage(char* help_message);

/**
 * Sets the pointer to an warning help message.
 *
 * @param help_message The warning help message.
 */
void meu_SetWarningHelpMessage(char* help_message);

/**
 * Sets the error callback function.
 *
 * @param callback_function The callback function. Does nothing if set to NULL.
 */
void meu_SetErrorCallback(void (* callback_function)(void));

/**
 * Sets the warning callback function.
 *
 * @param callback_function The callback function. Does nothing if set to NULL.
 */
void meu_SetWarningCallback(void (* callback_function)(void));

#endif /* MATSHARE_UTILS_H */
