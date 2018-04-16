#include "headers/matlabutils.h"

#ifdef MSH_UNIX


void readFchmodError_(const char* file_name, int line, int err)
{
	switch(err)
	{
		case EBADF:
			readErrorMex_(file_name, line, "FchmodBadFileError", "The fildes argument is not an open file descriptor.");
		case EPERM:
			readErrorMex_(file_name, line, "FchmodPermissionError", "The effective user ID does not match the owner of the file and the process does not have appropriate privilege.");
		case EROFS:
			readErrorMex_(file_name, line, "FchmodReadOnlyError", "The file referred to by fildes resides on a read-only file system.");
		case EINTR:
			readErrorMex_(file_name, line, "FchmodInterruptError", "The fchmod() function was interrupted by a signal.");
		case EINVAL:
			readErrorMex_(file_name, line, "FchmodInvalidError", "Fchmod failed because of one of the following:\n"
					"\tThe value of the mode argument is invalid.\n"
					"\tThe fildes argument refers to a pipe and the implementation disallows execution of fchmod() on a pipe.");
		default:
			readErrorMex_(file_name, line, "FchmodUnknownError", "An unknown error occurred.");
	}
}


void readMunmapError_(const char* file_name, int line, int err)
{
	switch(err)
	{
		case EINVAL:
			readErrorMex_(file_name, line, "MunmapInvalidError", "Munmap failed because of one of the following:\n"
					"\tAddresses in the range [addr,addr+len) are outside the valid range for the address space of a process.\n"
					"\tThe len argument is 0.\n"
					"\tThe addr argument is not a multiple of the page size as returned by sysconf().");
		default:
			readErrorMex_(file_name, line, "MunmapUnknownError", "An unknown error occurred.");
	}
}


void readMmapError_(const char* file_name, int line, int err)
{
	switch(err)
	{
		case EACCES:
			readErrorMex_(file_name, line, "MmapAccessError",
					    "The fildes argument is not open for read, regardless of the protection specified, or fildes is not open for write and "
							    "PROT_WRITE was specified for a MAP_SHARED type mapping.");
		case EBADF:
			readErrorMex_(file_name, line, "MmapFileClosedError", "The fildes argument is not a file descriptor open for writing.");
		case EMFILE:
			readErrorMex_(file_name, line, "MmapNumMapsError", "The number of mapped regions would exceed an implementation-defined limit (per process or per system).");
		case ENOMEM:
			readErrorMex_(file_name, line, "MmapMemoryError", "Not enough unallocated memory resources remain in the typed memory object "
					"designated by fildes to allocate len bytes.");
		case EFBIG:
			readErrorMex_(file_name, line, "MmapBigError",
					    "The file is a regular file and length is greater than the offset maximum established in the open file description associated with fildes.");
		case EROFS:
			readErrorMex_(file_name, line, "MmapReadOnlyError", "The named file resides on a read-only file system.");
		default:
			readErrorMex_(file_name, line, "MmapUnknownError", "An unknown error occurred (Error number: %i)", errno);
	}
}


void readFtruncateError_(const char* file_name, int line, int err)
{
	switch(err)
	{
		case EINTR:
			readErrorMex_(file_name, line, "FtrInterruptError", "A signal was caught during execution.");
		case EIO:
			readErrorMex_(file_name, line, "FtrIOError", "An I/O error occurred while reading from or writing to a file system.");
		case EINVAL:
			readErrorMex_(file_name, line, "FtrInvalidError", "There was an invalid argument passed to ftruncate.");
		case EBADF:
			readErrorMex_(file_name, line, "FtrFileClosedError", "The fildes argument is not a file descriptor open for writing.");
		case EFBIG:
			readErrorMex_(file_name, line, "FtrBigError",
					    "The file is a regular file and length is greater than the offset maximum established in the open file description associated with fildes.");
		case EROFS:
			readErrorMex_(file_name, line, "FtrReadOnlyError", "The named file resides on a read-only file system.");
		default:
			readErrorMex_(file_name, line, "FtrUnknownError", "An unknown error occurred (Error number: %i)", errno);
	}
}


void readShmOpenError_(const char* file_name, int line, int err)
{
	switch(err)
	{
		case EACCES:
			readErrorMex_(file_name, line, "ShmOpenAccessError",
					    "The shared memory object exists and the permissions specified by oflag are denied, or the shared memory object "
							    "does not exist and permission to create the shared memory object is denied, or O_TRUNC is specified "
							    "and write permission is denied.");
		case EEXIST:
			readErrorMex_(file_name, line, "ShmOpenExistError", "O_CREAT and O_EXCL are set and the named shared memory object already exists.");
		case EINTR:
			readErrorMex_(file_name, line, "ShmOpenInterruptError", "The shm_open() operation was interrupted by a signal.");
		case EINVAL:
			readErrorMex_(file_name, line, "ShmOpenNameError", "The shm_open() operation is not supported for the given name.");
		case EMFILE:
			readErrorMex_(file_name, line, "ShmOpenTooManyFilesError", "Too many file descriptors are currently in use by this process.");
		case ENAMETOOLONG:
			readErrorMex_(file_name, line, "ShmOpenNameTooLongError", "The length of the name argument exceeds {PATH_MAX} or a pathname component is longer than {NAME_MAX}.");
		case ENFILE:
			readErrorMex_(file_name, line, "ShmOpenTooManySharedError", "Too many shared memory objects are currently open in the system.");
		case ENOENT:
			readErrorMex_(file_name, line, "ShmOpenNoCreateNotExistError", "O_CREAT is not set and the named shared memory object does not exist.");
		case ENOSPC:
			readErrorMex_(file_name, line, "ShmOpenSpaceError", "There is insufficient space for the creation of the new shared memory object.");
		default:
			readErrorMex_(file_name, line, "ShmOpenUnknownError", "An unknown error occurred (Error number: %i)", errno);
	}
}


void readShmUnlinkError_(const char* file_name, int line, int err)
{
	switch(err)
	{
		case EACCES:
			readErrorMex_(file_name, line, "ShmUnlinkAccessError", "Permission is denied to unlink the named shared memory object.");
		case ENAMETOOLONG:
			readErrorMex_(file_name, line, "ShmUnlinkNameTooLongError", "The length of the name argument exceeds {PATH_MAX} or a pathname component is longer than {NAME_MAX}.");
		case ENOENT:
			readErrorMex_(file_name, line, "ShmUnlinkNotExistError", "The named shared memory object does not exist.");
		default:
			readErrorMex_(file_name, line, "ShmUnlinkUnknownError", "An unknown error occurred (Error number: %i)", errno);
	}
}


void readMsyncError_(const char* file_name, int line, int err)
{
	switch(err)
	{
		case EBUSY:
			readErrorMex_(file_name, line, "MsyncBusyError",
					    "Some or all of the addresses in the range starting at addr and continuing for len bytes are locked, and MS_INVALIDATE is specified.");
		case EINVAL:
			readErrorMex_(file_name, line, "MsyncInvalidError", "Msync failed because of one of the following:\n"
					"\tThe value of flags is invalid.\n"
					"\tThe value of addr is not a multiple of the page size {PAGESIZE}.");
		case ENOMEM:
			readErrorMex_(file_name, line, "MsyncNoMemError", "The addresses in the range starting at addr and continuing for len bytes are outside the "
					"range allowed for the address space of a process or specify one or more pages that are not mapped.");
		default:
			readErrorMex_(file_name, line, "MsyncUnknownError", "An unknown error occurred (Error number: %i)", errno);
	}
}


#endif


void readErrorMex_(const char* file_name, int line, const char* error_id, const char* error_message, ...)
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


void readWarnMex(const char* warn_id, const char* warn_message, ...)
{
	char full_message[FULL_WARNING_MESSAGE_SIZE] = {0};
	char message_buffer[WARNING_MESSAGE_SIZE] = {0};
	char warn_id_buffer[ERROR_ID_SIZE] = {0};
	
	va_list va;
	va_start(va, warn_message);
	vsprintf(message_buffer, warn_message, va);
	va_end(va);
	
	sprintf(warn_id_buffer, MATLAB_ERROR_ID, warn_id);
	sprintf(full_message, MATLAB_WARN_MESSAGE_FMT, message_buffer, MATLAB_WARN_MESSAGE);

#ifdef NO_MEX
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
	fprintf(stderr, message_buffer);
#pragma GCC diagnostic pop
#else
	mexWarnMsgIdAndTxt(warn_id_buffer, full_message);
#endif
}