#ifndef MATSHARE_UTILS_H
#define MATSHARE_UTILS_H

#include "mex.h"
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>

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

void printShmError(int err);
void readMXError(const char* file_name, int line, const char error_id[], const char error_message[], ...);
void readMXWarn(const char warn_id[], const char warn_message[], ...);

/* defined so we don't have write __FILE__, __LINE__ every time */
#define readMXError(...) readMXError(__FILE__ , __LINE__, __VA_ARGS__)

#endif //MATSHARE_UTILS_H
