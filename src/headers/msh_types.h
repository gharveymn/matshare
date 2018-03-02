#ifndef MATSHARE_MSH_TYPES_H
#define MATSHARE_MSH_TYPES_H

#include "mex.h"
#include <stdint.h>

#define MSH_UPDATE_SEGMENT_NAME "/MATSHARE_UPDATE_SEGMENT"
#define MSH_LOCK_NAME "/MATSHARE_LOCK"
#define MSH_SEGMENT_NAME "/MATSHARE_SEGMENT%0llx"
#define MSH_MAX_NAME_LEN 64
#define MSH_STARTUP_SIG "MATSAHARE_START"
#define MSH_END_SIG "MATSHARE_END"

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
#  include "../extlib/mman-win32/sys/mman.h"
extern int shm_open(const char *name, int oflag, mode_t mode);
extern int shm_unlink(const char *name);
#  include <sys/stat.h>
#ifndef MSH_UNIX
#  define MSH_UNIX
#endif
#elif defined(DEBUG_WINDOWS)
#ifndef MSH_WIN
#  define MSH_WIN
#endif
#else
# error(No build type specified.)
#endif

#ifdef MSH_WIN
#  include <windows.h>
#else
#  include <unistd.h>
#  include <semaphore.h>
#  include <pthread.h>
#  include <fcntl.h>
#endif

#ifndef FALSE
#  define FALSE 0
#endif

#ifndef TRUE
#  define TRUE 1
#endif

#if UINTPTR_MAX == 0xffffffffffffffff
typedef int64_t address_t;
typedef int64_t index_t;
	#define UNDEF_ADDR (address_t)0xffffffffffffffff
#elif UINTPTR_MAX == 0xffffffff
typedef uint32_t address_t;
	typedef uint32_t index_t;
	#define UNDEF_ADDR (address_t)0xffffffff
#else
#error Your architecture is weird
#endif

typedef struct data data_t;
typedef char byte_t;
typedef bool bool_t;

typedef struct {
	void *name;             /*   prev - R2008b: Name of variable in workspace
				               R2009a - R2010b: NULL
				               R2011a - later : Reverse CrossLink pointer    */
	mxClassID ClassID;      /*  0 = unknown     10 = int16
                                1 = cell        11 = uint16
                                2 = struct      12 = int32
                                3 = logical     13 = uint32
                                4 = char        14 = int64
                                5 = void        15 = uint64
                                6 = double      16 = function_handle
                                7 = single      17 = opaque (classdef)
                                8 = int8        18 = object (old style)
                                9 = uint8       19 = index (deprecated)
                               10 = int16       20 = sparse (deprecated)     */
	int VariableType;       /*  0 = normal
                                1 = persistent
                                2 = global
                                3 = sub-element (field or cell)
                                4 = temporary
                                5 = (unknown)
                                6 = property of opaque class object
                                7 = (unknown)                                */
	mxArray *CrossLink;     /* Address of next shared-data variable          */
	size_t ndim;            /* Number of dimensions                          */
	unsigned int RefCount;  /* Number of extra sub-element copies            */
	unsigned int flags;     /*  bit  0 = is scalar double full
                                bit  2 = is empty double full
                                bit  4 = is temporary
                                bit  5 = is sparse
                                bit  9 = is numeric
                                bits 24 - 31 = User Bits                     */
	union mdim_data{
		size_t M;           /* Row size for 2D matrices, or                  */
		size_t *dims;       /* Pointer to dims array for nD > 2 arrays       */
	} Mdims;
	size_t N;               /* Product of dims 2:end                         */
	void *pr;               /* Real Data Pointer (or cell/field elements)    */
	void *pi;               /* Imag Data Pointer (or field information)      */
	union ir_data{
		mwIndex *ir;        /* Pointer to row values for sparse arrays       */
		mxClassID ClassID;  /* New User Defined Class ID (classdef)          */
		char *ClassName;    /* Pointer to Old User Defined Class Name        */
	} irClassNameID;
	union jc_data{
		mwIndex *jc;        /* Pointer to column values for sparse arrays    */
		mxClassID ClassID;  /* Old User Defined Class ID                     */
	} jcClassID;
	size_t nzmax;           /* Number of elements allocated for sparse       */
/*  size_t reserved;           Don't believe this! It is not really there!   */
} mxArrayStruct;


typedef enum
{
	msh_INIT,
	msh_CLONE,
	msh_ATTACH,
	msh_DETACH,
	msh_FREE,
	msh_FETCH,
	msh_COMPARE,
	msh_COPY,
	msh_DEEPCOPY,
	msh_DEBUG
} msh_directive_t;

/* captures fundamentals of the mxArray */
/* In the shared memory the storage order is [header, size array, field_names, real dat, image data, sparse index r, sparse index c]  */
typedef struct
{
	bool_t isSparse;
	bool_t isNumeric;
	bool_t isEmpty;
	mxComplexity complexity;
	mxClassID classid;       /* matlab class id */
	size_t nDims;         /* dimensionality of the matrix.  The size array immediately follows the header */
	size_t elemsiz;       /* size of each element in pr and pi */
	size_t nzmax;         /* length of pr,pi */
	int nFields;       /* the number of fields.  The field string immediately follows the size array */
	int par_hdr_off;   /* offset to the parent's header, add this to help backwards searches (double linked... sort of)*/
	size_t shmsiz;            /* size of serialized object (header + size array + field names string) */
	size_t  strBytes;
} header_t;

/* structure used to record all of the data addresses */
struct data
{
	mwSize* dims;               /* pointer to the size array */
	void* pr;                    /* real data portion */
	union
	{
		void* pi;               /* imaginary data portion */
		char* field_str;   /* list of a structures fields, each field name will be seperated by a null character and terminated with a ";" */
	};
	mwIndex* ir;                    /* row indexes, for sparse */
	mwIndex* jc;                    /* cumulative column counts, for sparse */
	data_t* child_dat;          /* array of children data structures, for cell */
	header_t* child_hdr;          /* array of corresponding children header structures, for cell */
};

typedef struct
{
	uint16_t num_procs;
	uint64_t lead_num;		/* 64 bit width is temporary ugly fix for theoretical issue of name collision */
	uint64_t seg_num;
	uint64_t rev_num;
	size_t seg_sz;
#ifdef MSH_WIN
	DWORD upd_pid;
#else
	pid_t upd_pid;
#endif
} shm_segment_info;

typedef struct
{
	uint64_t seg_num;						// segment number (iterated when a new file is needed)
	uint64_t rev_num;						// total number of revisions (used for comparison, not indexing, so it's circular)
	size_t seg_sz;							// size of the segment
} lcl_segment_info;

typedef struct
{
	char name[MSH_MAX_NAME_LEN];
	bool_t is_init;
	bool_t is_mapped;
#ifdef MSH_WIN
	HANDLE handle;
#else
	int handle;
#endif
	size_t reg_sz;
	void* ptr;
} mem_region;

typedef struct
{
	mem_region shm_data_reg;
	mem_region shm_update_reg;
	mem_region startup_flag;

#ifdef MSH_WIN
	HANDLE proc_lock;
	SECURITY_ATTRIBUTES lock_sec;
#else
	sem_t* proc_lock;
#endif
	
	lcl_segment_info cur_seg_info;
	struct
	{
		bool_t is_proc_lock_init;
		bool_t is_glob_shm_var_init;
		
		bool_t is_freed;
		bool_t is_proc_locked;
		bool_t is_mem_safe;
	} flags;
	
#ifdef MSH_WIN
	DWORD this_pid;
#else
	pid_t this_pid;
#endif
	
	int num_lcl_objs_using;
	
} mex_info;

#define shm_data_ptr ((byte_t*)glob_info->shm_data_reg.ptr)
#define shm_update_info ((shm_segment_info*)glob_info->shm_update_reg.ptr)

#endif //MATSHARE_MSH_TYPES_H
