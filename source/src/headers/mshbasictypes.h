/** mshbasictypes.h
 * Provides some basic typedefs needed in most files.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#ifndef MATSHARE_MSHBASICTYPES_H
#define MATSHARE_MSHBASICTYPES_H

#include "mex.h"
#include <errno.h>

#if defined(DEBUG_UNIX)
#  ifndef MSH_UNIX
#    define MSH_UNIX
#  endif
#elif defined(DEBUG_WINDOWS)
#  ifndef MSH_WIN
#    define MSH_WIN
#  endif
#endif

#ifdef MSH_WIN
#    ifndef WIN32_LEAN_AND_MEAN
#      define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>
#  define MSH_INVALID_HANDLE INVALID_HANDLE_VALUE
#else
#  include <sys/types.h>
#  define MSH_INVALID_HANDLE (-1)
#endif

#ifndef TRUE
#  define TRUE 1
#endif

#ifndef FALSE
#  define FALSE 0
#endif

#ifndef MAX
#  define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#  define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef SIZE_MAX
#  define SIZE_MAX ((size_t)(-1))
#endif

#if MSH_BITNESS==64
#  define SIZE_FORMAT "%llu"
#elif MSH_BITNESS==32
#  define SIZE_FORMAT "%lu"
#endif

#define XSTR(x) #x
#define STR(x) XSTR(x)

#define MSH_VERSION_STRING "1.2.0"
#define MSH_VERSION_NUM    0x010200

/** these are basic readability typedefs **/
typedef int8_T bool_T;                  /* conditionals */
typedef int32_T alignedbool_T;		/* for word sized alignment */
typedef int32_T segmentnumber_T; /* segment number identifiers */

#define MSH_SEG_NUM_FORMAT "%li"

#ifdef MSH_WIN
   typedef HANDLE handle_T;
   typedef DWORD pid_T;
   typedef handle_T FileLock_T;
#  define HANDLE_FORMAT SIZE_FORMAT
#  define PID_FORMAT "%lu"
#else
   typedef int handle_T;				 /* give fds a uniform identifier */
   typedef pid_t pid_T;
   typedef struct FileLock_T
   {
      handle_T lock_handle;
      size_t lock_size;
   } FileLock_T;
#define HANDLE_FORMAT "%i"
#define PID_FORMAT "%i"
#endif

#ifdef MSH_AVX_SUPPORT
#  define MATLAB_ALIGNMENT 0x20
#else
#  define MATLAB_ALIGNMENT 0x10
#endif

/* always actually use 32 byte alignment */
#define MSH_ALIGNMENT 0x20

#if MSH_BITNESS==64
#define MSH_SIZE_WIDTH 8
#elif MSH_BITNESS==32
#define MSH_SIZE_WIDTH 4
#endif

#define MSH_NAME_LEN_MAX 64
#define MSH_SEG_NUM_MAX 0x7FFFFFFF      /* the maximum segment number (which is int32 max) */
#define MSH_INVALID_SEG_NUM (-1L)

#if   defined(_MSC_VER)
#  define MSH_ALIGN(ALIGNMENT) __declspec(align(ALIGNMENT))
#elif defined(__GNUC__)
#  define MSH_ALIGN(ALIGNMENT) __attribute__ ((aligned(ALIGNMENT)))
#endif

typedef float single;

#define ALIGNED_TYPEDEF(TYPE, ALIGNMENT) typedef TYPE MSH_ALIGN(ALIGNMENT)

#ifdef MSH_USE_AVX
ALIGNED_TYPEDEF(int8_T, 32)   a32_int8_T;
ALIGNED_TYPEDEF(int16_T, 32)  a32_int16_T;
ALIGNED_TYPEDEF(int32_T, 32)  a32_int32_T;

ALIGNED_TYPEDEF(uint8_T, 32)  a32_uint8_T;
ALIGNED_TYPEDEF(uint16_T, 32) a32_uint16_T;
ALIGNED_TYPEDEF(uint32_T, 32) a32_uint32_T;

#if MSH_BITNESS==64
ALIGNED_TYPEDEF(int64_T, 32)  a32_int64_T;
ALIGNED_TYPEDEF(uint64_T, 32) a32_uint64_T;
#endif

ALIGNED_TYPEDEF(single, 32)   a32_single;
ALIGNED_TYPEDEF(double, 32)   a32_double;
#endif

#ifdef MSH_USE_SSE2
ALIGNED_TYPEDEF(int8_T, 16)   a16_int8_T;
ALIGNED_TYPEDEF(int16_T, 16)  a16_int16_T;
ALIGNED_TYPEDEF(int16_T, 16)  a16_int32_T;

ALIGNED_TYPEDEF(uint8_T, 16)  a16_uint8_T;
ALIGNED_TYPEDEF(uint16_T, 16) a16_uint16_T;
ALIGNED_TYPEDEF(uint16_T, 16) a16_uint32_T;

#if MSH_BITNESS==64
ALIGNED_TYPEDEF(int64_T, 16)  a16_int64_T;
ALIGNED_TYPEDEF(uint64_T, 16) a16_uint64_T;
#endif

ALIGNED_TYPEDEF(single, 16)   a16_single;
ALIGNED_TYPEDEF(double, 16)   a16_double;
#endif

typedef union LockFreeCounter_Tag
{
	long span;
	struct values_tag
	{
		unsigned long count : 30;
		unsigned long flag : 1;
		unsigned long post : 1;
	} values;
} LockFreeCounter_T;

#endif /* MATSHARE_MSHBASICTYPES_H */
