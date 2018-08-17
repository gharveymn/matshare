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

#define MSH_VERSION_STRING "1.1.1"

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
   typedef struct FileLock_T
   {
      handle_T lock_handle;
      size_t lock_size;
   } FileLock_T;
#define HANDLE_FORMAT "%i"
#define PID_FORMAT "%i"
#endif

#ifdef MSH_AVX_SUPPORT
#  define DATA_ALIGNMENT 0x20
#  define DATA_ALIGNMENT_SHIFT (size_t)0x1F
#else
#  define DATA_ALIGNMENT 0x10
#  define DATA_ALIGNMENT_SHIFT (size_t)0x0F
#endif

#define MSH_NAME_LEN_MAX 64
#define MSH_SEG_NUM_MAX 0x7FFFFFFF      /* the maximum segment number (which is int32 max) */
#define MSH_INVALID_SEG_NUM (-1L)

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
