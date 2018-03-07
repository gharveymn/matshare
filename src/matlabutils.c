#include "headers/matlabutils.h"

#ifdef MSH_UNIX

void readSemError_(const char* file_name, int line, int err)
{
	switch(err)
	{
		case EACCES:
			readMXError_(file_name, line, "SemOpenAccessError", "The named semaphore exists and the permissions specified by oflag are denied, or the named semaphore does not exist and permission to create the named semaphore is denied.");
		case EINTR:
			readMXError_(file_name, line, "SemOpenInterruptError", "The sem_open() operation was interrupted by a signal.");
		case EINVAL:
			readMXError_(file_name, line, "SemOpenInvalidError", "The sem_open() operation is not supported for the given name.");
		case EMFILE:
			readMXError_(file_name, line, "SemOpenTooManyFilesError", "Too many semaphore descriptors or file descriptors are currently in use by this process.");
		case ENFILE:
			readMXError_(file_name, line, "SemOpenTooManySemsError", "Too many semaphores are currently open in the system.");
		case ENOSPC:
			readMXError_(file_name, line, "SemOpenNoSpaceError", "There is insufficient space for the creation of the new named semaphore.");
		case ENOSYS:
			readMXError_(file_name, line, "SemOpenNotSupportedError", "The function sem_open() is not supported by this implementation.");
		default:
			readMXError_(file_name, line, "SemOpenUnknownError", "An unknown error occurred.");
	}
}

void readMmapError_(const char* file_name, int line, int err)
{
	switch(err)
	{
		case EACCES:
			readMXError_(file_name, line, "MmapAccessError", "The fildes argument is not open for read, regardless of the protection specified, or fildes is not open for write and PROT_WRITE was specified for a MAP_SHARED type mapping.");
		case EBADF:
			readMXError_(file_name, line, "MmapFileClosedError", "The fildes argument is not a file descriptor open for writing.");
		case EMFILE:
			readMXError_(file_name, line, "MmapNumMapsError", "The number of mapped regions would exceed an implementation-defined limit (per process or per system).");
		case ENOMEM:
			readMXError_(file_name, line, "MmapMemoryError", "Not enough unallocated memory resources remain in the typed memory object designated by fildes to allocate len bytes.");
		case EFBIG:
			readMXError_(file_name, line, "MmapBigError", "The file is a regular file and length is greater than the offset maximum established in the open file description associated with fildes.");
		case EROFS:
			readMXError_(file_name, line, "MmapReadOnlyError", "The named file resides on a read-only file system.");
		default:
			readMXError_(file_name, line, "MmapUnknownError", "An unknown error occurred (Error number: %i)", errno);
	}
}


void readFtruncateError_(const char* file_name, int line, int err)
{
	switch(err)
	{
		case EINTR:
			readMXError_(file_name, line, "FtrInterruptError", "A signal was caught during execution.");
		case EIO:
			readMXError_(file_name, line, "FtrIOError", "An I/O error occurred while reading from or writing to a file system.");
		case EINVAL:
			readMXError_(file_name, line, "FtrInvalidError", "There was an invalid argument passed to ftruncate.");
		case EBADF:
			readMXError_(file_name, line, "FtrFileClosedError", "The fildes argument is not a file descriptor open for writing.");
		case EFBIG:
			readMXError_(file_name, line, "FtrBigError", "The file is a regular file and length is greater than the offset maximum established in the open file description associated with fildes.");
		case EROFS:
			readMXError_(file_name, line, "FtrReadOnlyError", "The named file resides on a read-only file system.");
		default:
			readMXError_(file_name, line, "FtrUnknownError", "An unknown error occurred (Error number: %i)", errno);
	}
}

void readShmError_(const char* file_name, int line, int err)
{
	switch(err)
	{
		case EACCES:
			readMXError_(file_name, line, "ShmAccessError",
					  "The shared memory object exists and the permissions specified by oflag are denied, or the shared memory object does not exist and permission to create the shared memory object is denied, or O_TRUNC is specified and write permission is denied.");
		case EEXIST:
			readMXError_(file_name, line, "ShmExistError", "O_CREAT and O_EXCL are set and the named shared memory object already exists.");
		case EINTR:
			readMXError_(file_name, line, "ShmInterruptError", "The shm_open() operation was interrupted by a signal.");
		case EINVAL:
			readMXError_(file_name, line, "ShmNameError", "The shm_open() operation is not supported for the given name.");
		case EMFILE:
			readMXError_(file_name, line, "ShmTooManyFilesError", "Too many file descriptors are currently in use by this process.");
		case ENAMETOOLONG:
			readMXError_(file_name, line, "ShmNameTooLongError", "The length of the name argument exceeds {PATH_MAX} or a pathname component is longer than {NAME_MAX}.");
		case ENFILE:
			readMXError_(file_name, line, "ShmTooManySharedError", "Too many shared memory objects are currently open in the system.");
		case ENOENT:
			readMXError_(file_name, line, "ShmNoCreateNotExistError", "O_CREAT is not set and the named shared memory object does not exist.");
		case ENOSPC:
			readMXError_(file_name, line, "ShmSpaceError", "There is insufficient space for the creation of the new shared memory object.");
		default:
			readMXError_(file_name, line, "ShmUnknownError", "An unknown error occurred (Error number: %i)", errno);
	}
}
#endif

void readMXError_(const char* file_name, int line, const char error_id[], const char error_message[], ...)
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