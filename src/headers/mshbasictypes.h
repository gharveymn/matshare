#ifndef MATSHARE_MSHBASICTYPES_H
#define MATSHARE_MSHBASICTYPES_H

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

/** these are basic readability typedefs **/
typedef char char_t;                     /* characters */
typedef signed char schar_t;             /* signed 8 bits */
typedef unsigned char uchar_t;           /* unsigned 8 bits */
typedef uchar_t byte_t;                  /* reading physical memory */
typedef schar_t bool_t;                  /* conditionals */
typedef signed long msh_segmentnumber_t; /* segment number identifiers */
typedef uchar_t msh_classid_t;             /* for packing mex class ids to prevent compiler from padding SharedVariableHeader_t */
#ifdef MSH_UNIX
typedef int handle_t;				 /* give fds a uniform identifier */
#endif

#define SEG_NUM_MAX LONG_MAX      /* the maximum segment number */

#define ALIGN_SIZE (size_t)0x20u    /* The pointer alignment size; ensure this is a multiple of 32 for AVX alignment */
#define ALIGN_SHIFT (size_t)0x1Fu /* (ALIGN_SIZE-1) micro-optimization */
#define MXMALLOC_SIG_LEN 0x10u      /* length of the mxMalloc signature */


/**
 * Pads the input size to the alignment specified by ALIGN_SIZE and ALIGN_SHIFT.
 *
 * @note Do not use for curr_sz = 0.
 * @param curr_sz The size to pad.
 * @return The padded size.
 */
size_t PadToAlign(size_t curr_sz);

#endif /* MATSHARE_MSHBASICTYPES_H */
