#include "headers/matlabutils.h"
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>


#define FULL_MESSAGE_SIZE 2048 /* twice the size of max POSIX error buffer size */
#define ERROR_STRING_SIZE 1024   /* size of POSIX error buffer size */
#define ID_BUFFER_SIZE 128
#define MATLAB_ERROR_ID "matshare:%s"
#define MATLAB_ERROR_MESSAGE_FORMAT "%s in %s, at line %d.\n\n%s%s"

#define MATLAB_WARN_MESSAGE_FORMAT "%s%s"
#define MATLAB_HELP_MESSAGE ""
#define MATLAB_WARN_MESSAGE ""

#ifdef MSH_WIN
#define MATLAB_ERROR_MESSAGE_WITH_CODE_FORMAT "%s in %s, at line %d.\n\n%s\nSystem error code 0x%lX: %s%s"
#else
#include <errno.h>
#define MATLAB_ERROR_MESSAGE_WITH_CODE_FORMAT "%s in %s, at line %d.\n\n%s\nSystem error code %d: %s%s"
#endif

static void (*matlab_error_callback)(void) = NullCallback;

#ifdef MSH_UNIX


void ReadFchmodError_(const char* file_name, int line, int err)
{
	switch(err)
	{
		case EBADF:
			ReadMexError(file_name, line, "FchmodBadFileError", "The fildes argument is not an open file descriptor.");
		case EPERM:
			ReadMexError(file_name, line, "FchmodPermissionError", "The effective user ID does not match the owner of the file and the process does not have appropriate privilege.");
		case EROFS:
			ReadMexError(file_name, line, "FchmodReadOnlyError", "The file referred to by fildes resides on a read-only file system.");
		case EINTR:
			ReadMexError(file_name, line, "FchmodInterruptError", "The fchmod() function was interrupted by a signal.");
		case EINVAL:
			ReadMexError(file_name, line, "FchmodInvalidError", "Fchmod failed because of one of the following:\n"
													   "\tThe value of the mode argument is invalid.\n"
													   "\tThe fildes argument refers to a pipe and the implementation disallows execution of fchmod() on a pipe.");
		default:
			ReadMexError(file_name, line, "FchmodUnknownError", "An unknown error occurred.");
	}
}


void ReadMunmapError_(const char* file_name, int line, int err)
{
	switch(err)
	{
		case EINVAL:
			ReadMexError(file_name, line, "MunmapInvalidError", "Munmap failed because of one of the following:\n"
													   "\tAddresses in the range [addr,addr+len) are outside the valid range for the address space of a process.\n"
													   "\tThe len argument is 0.\n"
													   "\tThe addr argument is not a multiple of the page size as returned by sysconf().");
		default:
			ReadMexError(file_name, line, "MunmapUnknownError", "An unknown error occurred.");
	}
}


void ReadMmapError_(const char* file_name, int line, int err)
{
	switch(err)
	{
		case EACCES:
			ReadMexError(file_name, line, "MmapAccessError", "The fildes argument is not open for read, regardless of the protection specified, or fildes is not open for write and "
													"PROT_WRITE was specified for a MAP_SHARED type mapping.");
		case EBADF:
			ReadMexError(file_name, line, "MmapFileClosedError", "The fildes argument is not a file descriptor open for writing.");
		case EMFILE:
			ReadMexError(file_name, line, "MmapNumMapsError", "The number of mapped regions would exceed an implementation-defined limit (per process or per system).");
		case ENOMEM:
			ReadMexError(file_name, line, "MmapMemoryError", "Not enough unallocated memory resources remain in the typed memory object "
													"designated by fildes to allocate len bytes.");
		case EFBIG:
			ReadMexError(file_name, line, "MmapBigError",
					    "The file is a regular file and length is greater than the offset maximum established in the open file description associated with fildes.");
		case EROFS:
			ReadMexError(file_name, line, "MmapReadOnlyError", "The named file resides on a read-only file system.");
		default:
			ReadMexError(file_name, line, "MmapUnknownError", "An unknown error occurred (Error number: %i)", errno);
	}
}


void ReadFtruncateError_(const char* file_name, int line, int err)
{
	switch(err)
	{
		case EINTR:
			ReadMexError(file_name, line, "FtrInterruptError", "A signal was caught during execution.");
		case EIO:
			ReadMexError(file_name, line, "FtrIOError", "An I/O error occurred while reading from or writing to a file system.");
		case EINVAL:
			ReadMexError(file_name, line, "FtrInvalidError", "There was an invalid argument passed to ftruncate.");
		case EBADF:
			ReadMexError(file_name, line, "FtrFileClosedError", "The fildes argument is not a file descriptor open for writing.");
		case EFBIG:
			ReadMexError(file_name, line, "FtrBigError",
					    "The file is a regular file and length is greater than the offset maximum established in the open file description associated with fildes.");
		case EROFS:
			ReadMexError(file_name, line, "FtrReadOnlyError", "The named file resides on a read-only file system.");
		default:
			ReadMexError(file_name, line, "FtrUnknownError", "An unknown error occurred (Error number: %i)", errno);
	}
}


void ReadShmOpenError_(const char* file_name, int line, int err)
{
	switch(err)
	{
		case EACCES:
			ReadMexError(file_name, line, "ShmOpenAccessError", "The shared memory object exists and the permissions specified by oflag are denied, or the shared memory object "
													   "does not exist and permission to create the shared memory object is denied, or O_TRUNC is specified "
													   "and write permission is denied.");
		case EEXIST:
			ReadMexError(file_name, line, "ShmOpenExistError", "O_CREAT and O_EXCL are set and the named shared memory object already exists.");
		case EINTR:
			ReadMexError(file_name, line, "ShmOpenInterruptError", "The shm_open() operation was interrupted by a signal.");
		case EINVAL:
			ReadMexError(file_name, line, "ShmOpenNameError", "The shm_open() operation is not supported for the given name.");
		case EMFILE:
			ReadMexError(file_name, line, "ShmOpenTooManyFilesError", "Too many file descriptors are currently in use by this process.");
		case ENAMETOOLONG:
			ReadMexError(file_name, line, "ShmOpenNameTooLongError", "The length of the name argument exceeds {PATH_MAX} or a pathname component is longer than {NAME_MAX}.");
		case ENFILE:
			ReadMexError(file_name, line, "ShmOpenTooManySharedError", "Too many shared memory objects are currently open in the system.");
		case ENOENT:
			ReadMexError(file_name, line, "ShmOpenNoCreateNotExistError", "O_CREAT is not set and the named shared memory object does not exist.");
		case ENOSPC:
			ReadMexError(file_name, line, "ShmOpenSpaceError", "There is insufficient space for the creation of the new shared memory object.");
		default:
			ReadMexError(file_name, line, "ShmOpenUnknownError", "An unknown error occurred (Error number: %i)", errno);
	}
}


void ReadShmUnlinkError_(const char* file_name, int line, int err)
{
	switch(err)
	{
		case EACCES:
			ReadMexError(file_name, line, "ShmUnlinkAccessError", "Permission is denied to unlink the named shared memory object.");
		case ENAMETOOLONG:
			ReadMexError(file_name, line, "ShmUnlinkNameTooLongError", "The length of the name argument exceeds {PATH_MAX} or a pathname component is longer than {NAME_MAX}.");
		case ENOENT:
			ReadMexError(file_name, line, "ShmUnlinkNotExistError", "The named shared memory object does not exist.");
		default:
			ReadMexError(file_name, line, "ShmUnlinkUnknownError", "An unknown error occurred (Error number: %i)", errno);
	}
}


void ReadMsyncError_(const char* file_name, int line, int err)
{
	switch(err)
	{
		case EBUSY:
			ReadMexError(file_name, line, "MsyncBusyError", "Some or all of the addresses in the range starting at addr and continuing for len bytes are locked, and MS_INVALIDATE is specified.");
		case EINVAL:
			ReadMexError(file_name, line, "MsyncInvalidError", "Msync failed because of one of the following:\n"
													  "\tThe value of flags is invalid.\n"
													  "\tThe value of addr is not a multiple of the page size {PAGESIZE}.");
		case ENOMEM:
			ReadMexError(file_name, line, "MsyncNoMemError", "The addresses in the range starting at addr and continuing for len bytes are outside the "
													"range allowed for the address space of a process or specify one or more pages that are not mapped.");
		default:
			ReadMexError(file_name, line, "MsyncUnknownError", "An unknown error occurred (Error number: %i)", errno);
	}
}


#endif


void ReadMexError(const char* file_name, int line, const char* error_id, const char* error_message, ...)
{
	
	char full_message[FULL_MESSAGE_SIZE] = {0};
	char message_prebuffer[FULL_MESSAGE_SIZE] = {0};
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
	char message_prebuffer[FULL_MESSAGE_SIZE] = {0};
	char id_buffer[ID_BUFFER_SIZE] = {0};
	char errstr_buffer[ERROR_STRING_SIZE] = {0};
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
			    ERROR_STRING_SIZE, NULL);
	sys_errstr = errstr_buffer;
#else
#  ifdef __GNUC__
	sys_errstr = strerror_r(error_code, errstr_buffer, ERROR_STRING_SIZE);
#  else
	strerror_r(error_code, errstr_buffer, ERROR_STRING_SIZE);
	sys_errstr = errstr_buffer;
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
