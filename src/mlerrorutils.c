#include "headers/mlerrorutils.h"
#include <stdarg.h>
#include <fcntl.h>

#ifndef _WIN32
#  include <string.h>
#endif

#define VALUE_AS_STRING(value) #value
#define EXPAND_AS_STRING(num) VALUE_AS_STRING(num)

#define MEU_SEVERITY_USER_STRING "[USER]"
#define MEU_SEVERITY_INTERNAL_STRING "[INTERNAL]"
#define MEU_SEVERITY_SYSTEM_STRING "[SYSTEM]"
#define MEU_SEVERITY_CORRUPTION_STRING "[CORRUPTION]"
#define MEU_SEVERITY_FATAL_STRING "[FATAL]"

#define MEU_LIBRARY_NAME_SIZE 31
#define MEU_ID_SIZE 95
#define MEU_ID_BUFFER_SIZE 128
#define MEU_ERROR_SEVERITY_SIZE 64
#define MEU_FILE_NAME_SIZE 260
#define MEU_ERROR_STRING_SIZE 2048
#define MEU_SYSTEM_ERROR_BUFFER_SIZE 1100
#define MEU_SYSTEM_ERROR_STRING_SIZE 1024 /* size of POSIX error buffer size */
#define MEU_MATLAB_HELP_MESSAGE_SIZE 512
#define MEU_FULL_MESSAGE_SIZE 4096 /* four times the size of max POSIX error buffer size */

#define MEU_ID_FORMAT "%."EXPAND_AS_STRING(MEU_LIBRARY_NAME_SIZE)"s:%."EXPAND_AS_STRING(MEU_ID_SIZE)"s"

#define MEU_ERROR_MESSAGE_FORMAT "%."EXPAND_AS_STRING(MEU_ID_BUFFER_SIZE)"s in %."EXPAND_AS_STRING(MEU_FILE_NAME_SIZE)"s, at line %d.\n%."EXPAND_AS_STRING(MEU_ERROR_SEVERITY_SIZE)"s\n\n%."EXPAND_AS_STRING(MEU_ERROR_STRING_SIZE)"s\n%."EXPAND_AS_STRING(MEU_SYSTEM_ERROR_BUFFER_SIZE)"s%."EXPAND_AS_STRING(MEU_MATLAB_HELP_MESSAGE_SIZE)"s"

#define MEU_WARN_MESSAGE_FORMAT "%."EXPAND_AS_STRING(MEU_ERROR_STRING_SIZE)"s%."EXPAND_AS_STRING(MEU_MATLAB_HELP_MESSAGE_SIZE)"s"

/**
 * Writes out the severity string.
 *
 * @param buffer A preallocated buffer. Should be size MEU_ERROR_SEVERITY_SIZE.
 * @param error_severity The error severity bitmask.
 */
static void meu_WriteSeverityString(char* buffer, unsigned int error_severity);

/**
 * Writes out the system error string.
 *
 * @param buffer A preallocated buffer. Should be size MEU_SYSTEM_ERROR_STRING_SIZE.
 * @param error_code The system error code returned from GetLastError() or errno.
 */
static void meu_WriteSystemErrorString(char* buffer, errcode_t error_code);

static char* meu_library_name = "";

static void (*meu_error_callback)(void) = NULL;
static void (*meu_warning_callback)(void) = NULL;

static char* meu_error_help_message = "";
static char* meu_warning_help_message = "";

void meu_PrintMexError(const char* file_name, int line, unsigned int error_severity, errcode_t error_code, const char* error_id, const char* error_message, ...)
{
	char full_message[MEU_FULL_MESSAGE_SIZE] = {0};
	char error_message_buffer[MEU_ERROR_STRING_SIZE] = {0};
	char error_severity_buffer[MEU_ERROR_SEVERITY_SIZE] = {0};
	char id_buffer[MEU_ID_BUFFER_SIZE] = {0};
	char system_error_string_buffer[MEU_SYSTEM_ERROR_STRING_SIZE] = {0};
	
	va_list va;
	va_start(va, error_message);
	vsprintf(error_message_buffer, error_message, va);
	va_end(va);
	
	meu_WriteSeverityString(error_severity_buffer, error_severity);

	if(error_severity & MEU_SEVERITY_SYSTEM)
	{
		meu_WriteSystemErrorString(system_error_string_buffer, error_code);
	}
	
	sprintf(id_buffer, MEU_ID_FORMAT, meu_library_name, error_id);
	sprintf(full_message, MEU_ERROR_MESSAGE_FORMAT, error_id, file_name, line, error_severity_buffer, error_message_buffer, system_error_string_buffer, meu_error_help_message);
	
	if(meu_error_callback != NULL)
	{
		meu_error_callback();
	}
	mexErrMsgIdAndTxt(id_buffer, full_message);
	
}


void meu_PrintMexWarning(const char* warn_id, const char* warn_message, ...)
{
	char full_message[MEU_FULL_MESSAGE_SIZE] = {0};
	char message_prebuffer[MEU_ERROR_STRING_SIZE] = {0};
	char id_buffer[MEU_ID_BUFFER_SIZE] = {0};
	
	va_list va;
	va_start(va, warn_message);
	vsprintf(message_prebuffer, warn_message, va);
	va_end(va);
	
	sprintf(id_buffer, MEU_ID_FORMAT, meu_library_name, warn_id);
	sprintf(full_message, MEU_WARN_MESSAGE_FORMAT, message_prebuffer, meu_warning_help_message);

	if(meu_warning_callback != NULL)
	{
		meu_warning_callback();
	}
	mexWarnMsgIdAndTxt(id_buffer, full_message);
}


void meu_SetLibraryName(char* library_name)
{
	meu_library_name = library_name;
}


void meu_SetErrorHelpMessage(char* help_message)
{
	meu_error_help_message = help_message;
}


void meu_SetWarningHelpMessage(char* help_message)
{
	meu_warning_help_message = help_message;
}


void meu_SetErrorCallback(void (* callback_function)(void))
{
	meu_error_callback = callback_function;
}


void meu_SetWarningCallback(void (* callback_function)(void))
{
	meu_error_callback = callback_function;
}


static void meu_WriteSeverityString(char* buffer, unsigned int error_severity)
{
	char* buffer_iter = buffer;
	
	strcpy(buffer_iter, "Severity level: ");
	buffer_iter += strlen(buffer_iter);
	
	if(error_severity & MEU_SEVERITY_USER)
	{
		strcpy(buffer_iter, MEU_SEVERITY_USER_STRING);
		buffer_iter += strlen(MEU_SEVERITY_USER_STRING);
	}
	
	if(error_severity & MEU_SEVERITY_INTERNAL)
	{
		strcpy(buffer_iter, MEU_SEVERITY_INTERNAL_STRING);
		buffer_iter += strlen(MEU_SEVERITY_INTERNAL_STRING);
	}
	
	if(error_severity & MEU_SEVERITY_SYSTEM)
	{
		strcpy(buffer_iter, MEU_SEVERITY_SYSTEM_STRING);
		buffer_iter += strlen(MEU_SEVERITY_SYSTEM_STRING);
	}
	
	if(error_severity & MEU_SEVERITY_CORRUPTION)
	{
		strcpy(buffer_iter, MEU_SEVERITY_CORRUPTION_STRING);
		buffer_iter += strlen(MEU_SEVERITY_CORRUPTION_STRING);
	}
	
	if(error_severity & MEU_SEVERITY_FATAL)
	{
		strcpy(buffer_iter, MEU_SEVERITY_FATAL_STRING);
	}
	
}


static void meu_WriteSystemErrorString(char* buffer, errcode_t error_code)
{
	char* inner_buffer = buffer;
	
#ifdef _WIN32
	sprintf(buffer, "System error code 0x%lX: ", error_code);
	inner_buffer += strlen(buffer);
	
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			    NULL,
			    error_code,
			    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			    inner_buffer,
			    MEU_SYSTEM_ERROR_STRING_SIZE, NULL);
#else
	sprintf(buffer, "System error code 0x%d: ", error_code);
	inner_buffer += strlen(buffer);
	
#  if(((_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && !_GNU_SOURCE) || defined(__APPLE__))
	/* XSI-compliant version */
	strerror_r(error_code, inner_buffer, MEU_SYSTEM_ERROR_STRING_SIZE);
#  else
	/* GNU-specific */
	strcpy(inner_buffer, strerror_r(error_code, inner_buffer, MEU_SYSTEM_ERROR_STRING_SIZE));
#  endif
#endif
	*(buffer + strlen(buffer)) = '\n';
}
