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

void ReadMexError(const char* file_name, int line, const char* error_id, const char* error_message, ...);
void ReadMexErrorWithCode(const char* file_name, int line, errcode_t error_code, const char* error_id, const char* error_message, ...);
void ReadMexWarning(const char* warn_id, const char* warn_message, ...);
void SetMexErrorCallback(void (*callback_function)(void));
void NullCallback(void);

#ifdef MSH_UNIX
void ReadFtruncateError_(const char* file_name, int line, int err);
void ReadShmOpenError_(const char* file_name, int line, int err);
void ReadShmUnlinkError_(const char* file_name, int line, int err);
void ReadMmapError_(const char* file_name, int line, int err);
void ReadMunmapError_(const char* file_name, int line, int err);
void ReadMsyncError_(const char* file_name, int line, int err);
void ReadFchmodError_(const char* file_name, int line, int err);
#define ReadFtruncateError(err) ReadFtruncateError_(__FILE__ , __LINE__, err)
#define ReadShmOpenError(err) ReadShmOpenError_(__FILE__ , __LINE__, err)
#define ReadShmUnlinkError(err) ReadShmUnlinkError_(__FILE__ , __LINE__, err)
#define ReadMmapError(err) ReadMmapError_(__FILE__ , __LINE__, err)
#define ReadMunmapError(err) ReadMunmapError_(__FILE__ , __LINE__, err)
#define ReadMsyncError(err) ReadMsyncError_(__FILE__ , __LINE__, err)
#define ReadFchmodError(err) ReadFchmodError_(__FILE__ , __LINE__, err)
#endif

#endif /* MATSHARE_UTILS_H */
