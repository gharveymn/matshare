#include "headers/utils.h"

void printShmError(int err)
{
	case EACCES:
		readMXError("ShmAccessError", "The shared memory object exists and the permissions specified by oflag are denied, or the shared memory object does not exist and permission to create the shared memory object is denied, or O_TRUNC is specified and write permission is denied.");
	case EINTR:
		readMXError("ShmInterruptError", "The shm_open() operation was interrupted by a signal.");
	case EINVAL:
		readMXError("ShmNameError", "The shm_open() operation is not supported for the given name.");
	case EMFILE:
		readMXError("ShmTooManyFilesError", "Too many file descriptors are currently in use by this process.");
	case ENFILE:
		readMXError("ShmTooManySharedError", "Too many shared memory objects are currently open in the system.");
	case ENOSPC:
		readMXError("ShmSpaceError", "There is insufficient space for the creation of the new shared memory object.");
	default:
		readMXError("UnknownError", "An unknown error occurred (Error number: %i)", errno);
}

#undef readMXError
void readMXError(const char* file_name, int line, const char error_id[], const char error_message[], ...)
{
	
	char full_message[FULL_ERROR_MESSAGE_SIZE] = {0};
	char message_buffer[ERROR_MESSAGE_SIZE] = {0};
	char error_id_buffer[ERROR_ID_SIZE] = {0};
	
	va_list va;
	va_start(va, error_message);
	vsprintf(message_buffer, error_message, va);
	va_end(va);
	
	sprintf(error_id_buffer, MATLAB_ERROR_ID, error_id);
	sprintf(full_message, MATLAB_ERROR_MESSAGE_FMT, error_id, file_name, line, message_buffer, MATLAB_HELP_MESSAGE);

#ifdef NO_MEX
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
	fprintf(stderr, message_buffer);
#pragma GCC diagnostic pop
	exit(1);
#else
	mexErrMsgIdAndTxt(error_id_buffer, full_message);
#endif

}


void readMXWarn(const char warn_id[], const char warn_message[], ...)
{
	char full_message[FULL_WARNING_MESSAGE_SIZE] = {0};
	char message_buffer[WARNING_MESSAGE_SIZE] = {0};
	char warn_id_buffer[ERROR_ID_SIZE] = {0};
		
	va_list va;
	va_start(va, warn_message);
	vsprintf(message_buffer, warn_message, va);
	va_end(va);
	
	sprintf(warn_id_buffer, MATLAB_ERROR_ID, warn_id);
	sprintf(full_message, MATLAB_WARN_MESSAGE_FMT, warn_id, message_buffer, MATLAB_WARN_MESSAGE);

#ifdef NO_MEX
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
	fprintf(stderr, message_buffer);
#pragma GCC diagnostic pop
#else
	mexWarnMsgIdAndTxt(warn_id_buffer, full_message);
#endif
}