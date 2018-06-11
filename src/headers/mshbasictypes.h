#ifndef MATSHARE_MSHBASICTYPES_H
#define MATSHARE_MSHBASICTYPES_H

#include <stddef.h>

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
#elif defined(DEBUG_UNIX) || defined(DEBUG_UNIX_ON_WINDOWS)
#  ifndef MSH_UNIX
#    define MSH_UNIX
#  endif
#elif defined(DEBUG_WINDOWS)
#  ifndef MSH_WIN
#    define MSH_WIN
#  endif
#else
#  error(No build type specified.)
#endif

#ifdef MSH_WIN
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

#  define MSH_INVALID_HANDLE INVALID_HANDLE_VALUE
#else
#  include <errno.h>

#  define MSH_DEFAULT_PERMISSIONS (S_IRUSR | S_IWUSR)
#  define MSH_INVALID_HANDLE (-1)
#endif

#ifdef MSH_DEBUG_PERF
#  include <time.h>
#  ifdef MSH_WIN
typedef LARGE_INTEGER msh_tick_t;
#  else
typedef clock_t msh_tick_t;
#  endif
typedef struct TickTracker_t
{
	msh_tick_t old;
	msh_tick_t new;
} TickTracker_t;
TickTracker_t total_time, lock_time, busy_wait_time;
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

#if MSH_BITNESS==64
#  define SIZE_FORMAT_SPEC "%llu"
#elif MSH_BITNESS==32
#  define SIZE_FORMAT_SPEC "%lu"
#endif

#ifndef LONG_MAX
#  define LONG_MAX 2147483647L
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

#define MSH_SEG_NUM_MAX 0x7FFFFFFF      /* the maximum segment number (which is int32 max) */
#define MSH_INVALID_SEG_NUM (-1L)

#if MSH_BITNESS==64
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
#elif MSH_BITNESS==32
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
#  error(matshare is only supported in 64-bit and 32-bit variants.)
#endif

#ifdef MSH_AVX_SUPPORT
#  define MXMALLOC_ALIGNMENT 0x20
#  define MXMALLOC_ALIGNMENT_SHIFT (size_t)0x1F
#else
#  define MXMALLOC_ALIGNMENT 0x10
#  define MXMALLOC_ALIGNMENT_SHIFT (size_t)0x0F
#endif


/**
 * Pads the input size to the alignment specified by ALIGN_SIZE and ALIGN_SHIFT.
 *
 * @note Do not use for curr_sz = 0.
 * @param curr_sz The size to pad.
 * @return The padded size.
 */
size_t PadToAlignData(size_t curr_sz);


#endif /* MATSHARE_MSHBASICTYPES_H */
