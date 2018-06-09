#include "headers/matlabutils.h"
#include <stdarg.h>
#include <fcntl.h>

#ifdef MSH_UNIX
#  include <string.h>
#endif

#define VALUE_AS_STRING(value) #value
#define EXPAND_AS_STRING(num) VALUE_AS_STRING(num)

#define MATLAB_HELP_MESSAGE ""
#define MATLAB_WARN_MESSAGE ""

#define ERROR_SEVERITY_USER_STRING "[USER]"
#define ERROR_SEVERITY_INTERNAL_STRING "[INTERNAL]"
#define ERROR_SEVERITY_SYSTEM_STRING "[SYSTEM]"
#define ERROR_SEVERITY_CORRUPTION_STRING "[CORRUPTION]"
#define ERROR_SEVERITY_FATAL_STRING "[FATAL]"

#define MSH_ERROR_ID_SIZE 96
#define ID_BUFFER_SIZE 128
#define ERROR_SEVERITY_SIZE 64
#define FILE_NAME_SIZE 260
#define ERROR_STRING_SIZE 2048
#define SYSTEM_ERROR_BUFFER_SIZE 1100
#define SYSTEM_ERROR_STRING_SIZE 1024 /* size of POSIX error buffer size */
#define MATLAB_HELP_MESSAGE_SIZE 512
#define FULL_MESSAGE_SIZE 4096 /* four times the size of max POSIX error buffer size */
#define MATLAB_ERROR_ID "matshare:%."EXPAND_AS_STRING(MSH_ERROR_ID_SIZE)"s"
#define MATLAB_ERROR_MESSAGE_FORMAT "%."EXPAND_AS_STRING(ID_BUFFER_SIZE)"s in %."EXPAND_AS_STRING(FILE_NAME_SIZE)"s, at line %d.\n%."EXPAND_AS_STRING(ERROR_SEVERITY_SIZE)"s\n\n%."EXPAND_AS_STRING(ERROR_STRING_SIZE)"s\n%."EXPAND_AS_STRING(SYSTEM_ERROR_BUFFER_SIZE)"s%."EXPAND_AS_STRING(MATLAB_HELP_MESSAGE_SIZE)"s"

#define MATLAB_WARN_MESSAGE_FORMAT "%."EXPAND_AS_STRING(ERROR_STRING_SIZE)"s%."EXPAND_AS_STRING(MATLAB_HELP_MESSAGE_SIZE)"s"

static void NullCallback(void);
static void WriteErrorSeverityString(char* buffer, unsigned int error_severity);
static void WriteSystemErrorString(char* buffer, errcode_t error_code);

static void (*error_callback)(void) = NullCallback;


void ReadMexError(const char* file_name, int line, unsigned int error_severity, errcode_t error_code, const char* error_id, const char* error_message, ...)
{
	char full_message[FULL_MESSAGE_SIZE] = {0};
	char error_message_buffer[ERROR_STRING_SIZE] = {0};
	char error_severity_buffer[ERROR_SEVERITY_SIZE] = {0};
	char id_buffer[ID_BUFFER_SIZE] = {0};
	char system_error_string_buffer[SYSTEM_ERROR_STRING_SIZE] = {0};
	
	va_list va;
	va_start(va, error_message);
	vsprintf(error_message_buffer, error_message, va);
	va_end(va);
	
	WriteErrorSeverityString(error_severity_buffer, error_severity);

	if(error_severity & ERROR_SEVERITY_SYSTEM)
	{
		WriteSystemErrorString(system_error_string_buffer, error_code);
	}
	
	sprintf(id_buffer, MATLAB_ERROR_ID, error_id);
	sprintf(full_message, MATLAB_ERROR_MESSAGE_FORMAT, error_id, file_name, line, error_severity_buffer, error_message_buffer, system_error_string_buffer, MATLAB_HELP_MESSAGE);
	
	error_callback();
	mexErrMsgIdAndTxt(id_buffer, full_message);
	
}


static void WriteErrorSeverityString(char* buffer, unsigned int error_severity)
{
	char* buffer_iter = buffer;
	
	strcpy(buffer_iter, "Severity level: ");
	buffer_iter += strlen(buffer_iter);
	
	if(error_severity & ERROR_SEVERITY_USER)
	{
		strcpy(buffer_iter, ERROR_SEVERITY_USER_STRING);
		buffer_iter += strlen(ERROR_SEVERITY_USER_STRING);
	}
	
	if(error_severity & ERROR_SEVERITY_INTERNAL)
	{
		strcpy(buffer_iter, ERROR_SEVERITY_INTERNAL_STRING);
		buffer_iter += strlen(ERROR_SEVERITY_INTERNAL_STRING);
	}
	
	if(error_severity & ERROR_SEVERITY_SYSTEM)
	{
		strcpy(buffer_iter, ERROR_SEVERITY_SYSTEM_STRING);
		buffer_iter += strlen(ERROR_SEVERITY_SYSTEM_STRING);
	}
	
	if(error_severity & ERROR_SEVERITY_CORRUPTION)
	{
		strcpy(buffer_iter, ERROR_SEVERITY_CORRUPTION_STRING);
		buffer_iter += strlen(ERROR_SEVERITY_CORRUPTION_STRING);
	}
	
	if(error_severity & ERROR_SEVERITY_FATAL)
	{
		strcpy(buffer_iter, ERROR_SEVERITY_FATAL_STRING);
	}
	
}

static void WriteSystemErrorString(char* buffer, errcode_t error_code)
{
	char* inner_buffer = buffer;
	
#ifdef MSH_WIN
	sprintf(buffer, "System error code 0x%lX: ", error_code);
	inner_buffer += strlen(buffer);
	
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			    NULL,
			    error_code,
			    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			    inner_buffer,
			    SYSTEM_ERROR_STRING_SIZE, NULL);
#else
	sprintf(buffer, "System error code 0x%d: ", error_code);
	inner_buffer += strlen(buffer);
	
#  if(_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && !_GNU_SOURCE
	/* XSI-compliant version */
	strerror_r(error_code, inner_buffer, SYSTEM_ERROR_STRING_SIZE);
#  else
	/* GNU-specific */
	strcpy(inner_buffer, strerror_r(error_code, inner_buffer, SYSTEM_ERROR_STRING_SIZE));
#  endif
#endif
	*(buffer + strlen(buffer)) = '\n';
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
		error_callback = callback_function;
	}
	else
	{
		error_callback = NullCallback;
	}
}


static void NullCallback(void)
{
	/* does nothing */
}
