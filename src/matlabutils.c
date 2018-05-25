#include "headers/matlabutils.h"
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>

#define VALUE_AS_STRING(value) #value
#define EXPAND_AS_STRING(num) VALUE_AS_STRING(num)

#define MATLAB_HELP_MESSAGE ""
#define MATLAB_WARN_MESSAGE ""

#define MSH_ERROR_ID_SIZE 96
#define ID_BUFFER_SIZE 128
#define FILE_NAME_SIZE 260
#define ERROR_STRING_SIZE 2048
#define SYSTEM_ERROR_STRING_SIZE 1024 /* size of POSIX error buffer size */
#define MATLAB_HELP_MESSAGE_SIZE 512
#define FULL_MESSAGE_SIZE 4096 /* four times the size of max POSIX error buffer size */
#define MATLAB_ERROR_ID "matshare:%."EXPAND_AS_STRING(MSH_ERROR_ID_SIZE)"s"
#define MATLAB_ERROR_MESSAGE_FORMAT "%."EXPAND_AS_STRING(ID_BUFFER_SIZE)"s in %."EXPAND_AS_STRING(FILE_NAME_SIZE)"s, at line %d.\n\n%."EXPAND_AS_STRING(ERROR_STRING_SIZE)"s%."EXPAND_AS_STRING(MATLAB_HELP_MESSAGE_SIZE)"s"

#define MATLAB_WARN_MESSAGE_FORMAT "%."EXPAND_AS_STRING(ERROR_STRING_SIZE)"s%."EXPAND_AS_STRING(MATLAB_HELP_MESSAGE_SIZE)"s"

#ifdef MSH_WIN
#define MATLAB_ERROR_MESSAGE_WITH_CODE_FORMAT "%."EXPAND_AS_STRING(ID_BUFFER_SIZE)"s in %."EXPAND_AS_STRING(FILE_NAME_SIZE)"s, at line %d.\n\n%."EXPAND_AS_STRING(ERROR_STRING_SIZE)"s\nSystem error code 0x%lX: %."EXPAND_AS_STRING(SYSTEM_ERROR_STRING_SIZE)"s%."EXPAND_AS_STRING(MATLAB_HELP_MESSAGE_SIZE)"s"
#else
#include <errno.h>
#define MATLAB_ERROR_MESSAGE_WITH_CODE_FORMAT "%."EXPAND_AS_STRING(ID_BUFFER_SIZE)"s in %."EXPAND_AS_STRING(FILE_NAME_SIZE)"s, at line %d.\n\n%."EXPAND_AS_STRING(ERROR_STRING_SIZE)"s\nSystem error code %d: %."EXPAND_AS_STRING(SYSTEM_ERROR_STRING_SIZE)"s%."EXPAND_AS_STRING(MATLAB_HELP_MESSAGE_SIZE)"s"
#endif

static void (*matlab_error_callback)(void) = NullCallback;


void ReadMexError(const char* file_name, int line, const char* error_id, const char* error_message, ...)
{
	
	char full_message[FULL_MESSAGE_SIZE] = {0};
	char message_prebuffer[ERROR_STRING_SIZE] = {0};
	char id_buffer[ID_BUFFER_SIZE] = {0};
	
	va_list va;
	va_start(va, error_message);
	vsprintf(message_prebuffer, error_message, va);
	va_end(va);
	
	sprintf(id_buffer, MATLAB_ERROR_ID, error_id);
	sprintf(full_message, MATLAB_ERROR_MESSAGE_FORMAT, error_id, file_name, line, message_prebuffer, MATLAB_HELP_MESSAGE);
	
	matlab_error_callback();
	mexErrMsgIdAndTxt(id_buffer, full_message);

}


void ReadMexErrorWithCode(const char* file_name, int line, errcode_t error_code, const char* error_id, const char* error_message, ...)
{
	char full_message[FULL_MESSAGE_SIZE] = {0};
	char message_prebuffer[ERROR_STRING_SIZE] = {0};
	char id_buffer[ID_BUFFER_SIZE] = {0};
	char errstr_buffer[SYSTEM_ERROR_STRING_SIZE] = {0};
	char* sys_errstr;
	
	va_list va;
	va_start(va, error_message);
	vsprintf(message_prebuffer, error_message, va);
	va_end(va);

#ifdef MSH_WIN
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			    NULL,
			    error_code,
			    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			    errstr_buffer,
			    SYSTEM_ERROR_STRING_SIZE, NULL);
	sys_errstr = errstr_buffer;
#else
#  if(_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE
	/* XSI-compliant version */
	strerror_r(error_code, errstr_buffer, SYSTEM_ERROR_STRING_SIZE);
	sys_errstr = errstr_buffer;
#  else
	/* GNU-specific (stupid) */
	sys_errstr = strerror_r(error_code, errstr_buffer, SYSTEM_ERROR_STRING_SIZE);
#  endif
#endif
	
	sprintf(id_buffer, MATLAB_ERROR_ID, error_id);
	sprintf(full_message, MATLAB_ERROR_MESSAGE_WITH_CODE_FORMAT, error_id, file_name, line, message_prebuffer, error_code, sys_errstr, MATLAB_HELP_MESSAGE);
	
	matlab_error_callback();
	mexErrMsgIdAndTxt(id_buffer, full_message);
	
}


void ReadMexWarning(const char* warn_id, const char* warn_message, ...)
{
	char full_message[FULL_MESSAGE_SIZE] = {0};
	char message_prebuffer[FULL_MESSAGE_SIZE] = {0};
	char id_buffer[ID_BUFFER_SIZE] = {0};
	
	va_list va;
	va_start(va, warn_message);
	vsprintf(message_prebuffer, warn_message, va);
	va_end(va);
	
	sprintf(id_buffer, MATLAB_ERROR_ID, warn_id);
	sprintf(full_message, MATLAB_WARN_MESSAGE_FORMAT, message_prebuffer, MATLAB_WARN_MESSAGE);
	
	mexWarnMsgIdAndTxt(id_buffer, full_message);
}


void SetMexErrorCallback(void (*callback_function)(void))
{
	if(callback_function != NULL)
	{
		matlab_error_callback = callback_function;
	}
	else
	{
		matlab_error_callback = NullCallback;
	}
}


void NullCallback(void)
{
	/* does nothing */
}
