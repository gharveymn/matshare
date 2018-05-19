#ifndef MATSHARE_MSHBASICTYPES_H
#define MATSHARE_MSHBASICTYPES_H

#include "mex.h"

#if defined(MATLAB_UNIX)
#  include <sys/mman.h>
#ifndef MSH_UNIX
#  define MSH_UNIX
#endif
#elif defined(MATLAB_WINDOWS)
#ifndef MSH_WIN
#  define MSH_WIN
#endif
#elif defined(DEBUG_UNIX)
#  include <sys/mman.h>
#  include <sys/stat.h>
#ifndef MSH_UNIX
#  define MSH_UNIX
#endif
#elif defined(DEBUG_UNIX_ON_WINDOWS)
#define ftruncate ftruncate64
#  include "../extlib/mman-win32/sys/mman.h"
extern int shm_open(const char* name, int oflag, mode_t mode);
extern int shm_unlink(const char* name);
extern int lockf(int fildes, int function, off_t size);
extern int fchmod(int fildes, mode_t mode);
#  include <sys/stat.h>
#define F_LOCK 1
#define F_ULOCK 0


#ifndef MSH_UNIX
#  define MSH_UNIX
#endif
#elif defined(DEBUG_WINDOWS)
#ifndef MSH_WIN
#  define MSH_WIN
#endif
#else
#  error(No build type specified.)
#endif

#ifndef FALSE
#  define FALSE 0
#endif

#ifndef TRUE
#  define TRUE 1
#endif

#ifndef SIZE_MAX
#  define SIZE_MAX ((size_t)(-1))
#endif

#ifndef LONG_MAX
#  define LONG_MAX 2147483647L
#endif

#ifdef MSH_WIN
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <unistd.h>
#  include <semaphore.h>
#  include <pthread.h>
#  include <fcntl.h>
#endif

/** these are basic readability typedefs **/
typedef char char_t;                     /* characters */
typedef byte_T byte_t;                  /* reading physical memory */
typedef int8_T bool_t;                  /* conditionals */
typedef int32_T alignedbool_t;		/* for word sized alignment */
typedef int32_T msh_segmentnumber_t; /* segment number identifiers */
typedef uint32_T msh_sharetype_t;		/* ensure word-size alignment */
typedef uint8_T msh_classid_t;             /* for packing mex class ids to prevent compiler from padding SharedVariableHeader_t */
#ifdef MSH_WIN
typedef HANDLE handle_t;
typedef DWORD pid_t;
#else
typedef int handle_t;				 /* give fds a uniform identifier */
#endif

#define SEG_NUM_MAX LONG_MAX      /* the maximum segment number */

#ifdef MSH_32BIT
#  define MXMALLOC_MAGIC_CHECK 0xFEED
#  define MXMALLOC_SIG_LEN 0x08
#  define MXMALLOC_SIG_LEN_SHIFT 0x07

typedef struct AllocationHeader_t
{
	uint32_T aligned_size;
	uint16_T check;
	uint8_T alignment;
	uint8_T offset;
} AllocationHeader_t;

#else
#  define MXMALLOC_MAGIC_CHECK 0xFEEDFACE
#  define MXMALLOC_SIG_LEN 0x10
#  define MXMALLOC_SIG_LEN_SHIFT 0x0F

typedef struct AllocationHeader_t
{
	uint64_T aligned_size;
	uint32_T check;
	uint16_T alignment;
	uint16_T offset;
} AllocationHeader_t;
#endif

#ifdef MSH_NO_AVX_SUPPORT
#  define MXMALLOC_ALIGNMENT 0x10
#  define MXMALLOC_ALIGNMENT_SHIFT (size_t)0x0F
#else
#  define MXMALLOC_ALIGNMENT 0x20
#  define MXMALLOC_ALIGNMENT_SHIFT (size_t)0x1F
#endif


/**
 * Pads the input size to the alignment specified by ALIGN_SIZE and ALIGN_SHIFT.
 *
 * @note Do not use for curr_sz = 0.
 * @param curr_sz The size to pad.
 * @return The padded size.
 */
size_t PadToAlignData(size_t curr_sz);

#ifdef MSH_WIN
#define msh_AtomicIncrement InterlockedIncrement
#define msh_AtomicDecrement InterlockedDecrement
#else
#define msh_AtomicIncrement(x) __sync_fetch_and_add(x, 1)
#define msh_AtomicDecrement(x) __sync_fetch_and_sub(x, 1)
#endif


#endif /* MATSHARE_MSHBASICTYPES_H */
