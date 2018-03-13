#ifndef MATSHARE_UTILS_H
#define MATSHARE_UTILS_H

#include "mex.h"
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

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


void readErrorMex_(const char* file_name, int line, const char* error_id, const char* error_message, ...);
void readWarnMex(const char* warn_id, const char* warn_message, ...);

/* defined so we don't have write __FILE__, __LINE__ every time */
#define readErrorMex(error_id, error_message, ...) readErrorMex_(__FILE__ , __LINE__, error_id, error_message , ##__VA_ARGS__)


#ifdef MSH_UNIX
void readFtruncateError_(const char* file_name, int line, int err);
void readShmOpenError_(const char* file_name, int line, int err);
void readShmUnlinkError_(const char* file_name, int line, int err);
void readMmapError_(const char* file_name, int line, int err);
void readMunmapError_(const char* file_name, int line, int err);
void readMsyncError_(const char* file_name, int line, int err);
void readFchmodError_(const char* file_name, int line, int err);
#define readFtruncateError(err) readFtruncateError_(__FILE__ , __LINE__, err)
#define readShmOpenError(err) readShmOpenError_(__FILE__ , __LINE__, err)
#define readShmUnlinkError(err) readShmUnlinkError_(__FILE__ , __LINE__, err)
#define readMmapError(err) readMmapError_(__FILE__ , __LINE__, err)
#define readMunmapError(err) readMunmapError_(__FILE__ , __LINE__, err)
#define readMsyncError(err) readMsyncError_(__FILE__ , __LINE__, err)
#define readFchmodError(err) readFchmodError_(__FILE__ , __LINE__, err)
#endif

#endif //MATSHARE_UTILS_H
