#ifndef MATSHARE_UTILS_H
#define MATSHARE_UTILS_H

#include "mex.h"
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>

#if !defined MSH_WIN && !defined MSH_UNIX
	#if defined(MATLAB_UNIX)
#  include <sys/mman.h>
#  define MSH_UNIX
	#elif defined(MATLAB_WINDOWS)
#  define MSH_WIN
	#elif defined(DEBUG_UNIX)
#  define MSH_UNIX
#elif defined(DEBUG_UNIX_ON_WINDOWS)
#  include "../extlib/mman-win32/sys/mman.h"
extern int shm_open(const char *name, int oflag, mode_t mode);
extern int shm_unlink(const char *name);
#  include <sys/stat.h>
#  define MSH_UNIX
#elif defined(DEBUG_WINDOWS)
#  define MSH_WIN
#else
# error(No build type specified.)
	#endif
#endif

#define FULL_ERROR_MESSAGE_SIZE 0x1FFE
#define ERROR_MESSAGE_SIZE 0xFFF
#define ERROR_ID_SIZE 0xFF
#define FULL_WARNING_MESSAGE_SIZE 0x1FE
#define WARNING_MESSAGE_SIZE 0xFF
#define MATLAB_ERROR_ID "MATLAB:matshare_:%s"
#define MATLAB_ERROR_MESSAGE_FMT "%s in %s, line %d.\n\n%s%s"
#define MATLAB_WARN_MESSAGE_FMT "%s%s"
#define MATLAB_HELP_MESSAGE ""
#define MATLAB_WARN_MESSAGE ""


void ReadMexError_(const char* file_name, int line, const char* error_id, const char* error_message, ...);
void ReadMexWarning(const char* warn_id, const char* warn_message, ...);
void SetMexErrorCallback(void (*callback_function)(void));
void NullCallback(void);

/* defined so we don't have write __FILE__, __LINE__ every time */
#define ReadMexError(error_id, error_message, ...) ReadMexError_(__FILE__ , __LINE__, error_id, error_message , ##__VA_ARGS__)


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
