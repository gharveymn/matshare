#include "headers/utils.h"


void readMXError(const char error_id[], const char error_message[], ...)
{
	
	char message_buffer[ERROR_BUFFER_SIZE];
	
	va_list va;
	va_start(va, error_message);
	sprintf(message_buffer, error_message, va);
	strcat(message_buffer, MATLAB_HELP_MESSAGE);
	va_end(va);

#ifdef NO_MEX
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
	fprintf(stderr, message_buffer);
#pragma GCC diagnostic pop
	exit(1);
#else
	mexErrMsgIdAndTxt(error_id, message_buffer);
#endif

}


void readMXWarn(const char warn_id[], const char warn_message[], ...)
{
	char message_buffer[WARNING_BUFFER_SIZE];
	
	va_list va;
	va_start(va, warn_message);
	sprintf(message_buffer, warn_message, va);
	strcat(message_buffer, MATLAB_WARN_MESSAGE);
	va_end(va);

#ifdef NO_MEX
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
	fprintf(stderr, message_buffer);
#pragma GCC diagnostic pop
#else
	mexWarnMsgIdAndTxt(warn_id, message_buffer);
#endif
	
}