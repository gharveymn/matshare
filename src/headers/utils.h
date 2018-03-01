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
#  include "../extlib/mman-win32/sys/mman.h"
#  define MSH_UNIX
extern int shm_open(const char *name, int oflag, mode_t mode);
extern int shm_unlink(const char *name);
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
#define MATLAB_WARN_MESSAGE_FMT "%s: %s%s"
#define MATLAB_HELP_MESSAGE ""
#define MATLAB_WARN_MESSAGE ""

void readMXError_(const char* file_name, int line, const char error_id[], const char error_message[], ...);
void readMXWarn(const char warn_id[], const char warn_message[], ...);

/* defined so we don't have write __FILE__, __LINE__ every time */
#define readMXError(error_id, error_message, ...) readMXError_(__FILE__ , __LINE__, error_id, error_message , ##__VA_ARGS__)


#ifdef MSH_UNIX
void readFtruncateError_(const char* file_name, int line, int err);
void readShmError_(const char* file_name, int line, int err);
void readMmapError_(const char* file_name, int line, int err);
#define readShmError(err) readShmError_(__FILE__ , __LINE__, err)
#define readFtruncateError(err) readFtruncateError_(__FILE__ , __LINE__, err)
#define readMmapError(err) readMmapError_(__FILE__ , __LINE__, err)
#endif

#endif //MATSHARE_UTILS_H
