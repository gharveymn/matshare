#ifndef MATSHARE_MSH_TYPES_H
#define MATSHARE_MSH_TYPES_H

#include "mex.h"
#include <stdint.h>
#include "externtypes.h"

extern mxArray* mxCreateSharedDataCopy(mxArray *);

#ifndef FALSE
#  define FALSE 0
#endif

#ifndef TRUE
#  define TRUE 1
#endif

#define MSH_UPDATE_SEGMENT_NAME "/MATSHARE_UPDATE_SEGMENT"
#define MSH_LOCK_NAME "/MATSHARE_LOCK"
#define MSH_SEGMENT_NAME "/MATSHARE_SEGMENT%0llx"
#define MSH_INIT_CHECK_NAME "MATSHARE_INIT%lu"
#define MSH_MAX_NAME_LEN 64

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
extern int shm_open(const char* name, int oflag, mode_t mode);
extern int shm_unlink(const char* name);
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

/* these are used for recording structure field names */
#define TERM_CHAR ';'          /*use this character to terminate a string containing the list of fields.  Do this because it can't be in a valid field name*/
#define ALIGN_SIZE (size_t)0x20   /*the pointer alignment size, so if pdata is a valid pointer then &pdata[i*align_size] will also be.  Ensure this is >= 4*/
#define MXMALLOC_SIG_LEN 16

typedef char char_t;
typedef char_t byte_t;
typedef bool bool_t;
#ifdef MSH_UNIX
typedef int handle_t;
#endif

typedef enum
{
	msh_SHARE,
	msh_FETCH,
	msh_DETACH,
	msh_PARAM,
	msh_DEEPCOPY,
	msh_DEBUG,
	msh_OBJ_REGISTER,
	msh_OBJ_DEREGISTER
} mshdirective_t;


/* captures fundamentals of the mxArray */
/* In the shared memory the storage order is [header, size array, field_names, real dat, image data, sparse index r, sparse index c]  */
typedef struct Header_tag Header_t;
struct Header_tag
{
	struct
	{
		size_t dims;
		size_t pr;
		size_t pi;
		size_t ir;
		size_t jc;
		size_t field_str;
		size_t child_hdrs;		/* offset of array of the offsets of the children*/
	} data_offsets; 			/* these are actually the relative offsets of data in shared memory (needed because memory maps are to virtual pointers) */
	size_t num_dims;         	/* dimensionality of the matrix */
	size_t elem_size;       		/* size of each element in pr and pi */
	size_t num_elems;         	/* length of pr,pi */
	size_t nzmax;
	size_t obj_sz;            	/* size of serialized object */
	int num_fields;       /* the number of fields.  The field string immediately follows the size array */
	mxClassID classid;       /* matlab class id */
	mxComplexity complexity;
	bool_t is_sparse;
	bool_t is_numeric;
	bool_t is_empty;
};

typedef struct InputData_tag InputData_t;
struct InputData_tag
{
	size_t* child_hdr_offs;
	Header_t** child_hdrs;          	/* array of corresponding children header structures, for cell */
	InputData_t** child_dat;
};

typedef struct ShmData_tag ShmData_t;
struct ShmData_tag
{
	mwSize* dims;               		/* pointer to the size array */
	void* pr;                    		/* real data portion */
	void* pi;               			/* imaginary data portion */
	mwIndex* ir;                    	/* row indexes, for sparse */
	mwIndex* jc;                  	/* cumulative column counts, for sparse */
	char_t* field_str;   			/* list of a structures fields, each field name will be seperated by a null character */
	size_t* child_hdrs;          		/* array of corresponding children headers */
};

typedef struct ShmSegmentInfo_tag ShmSegmentInfo_t;
struct ShmSegmentInfo_tag
{
	/* these are also all size_t to guarantee alignment for atomic operations */
	size_t lead_seg_num;          /* 64 bit width is temporary ugly fix for theoretical issue of name collision */
	size_t seg_num;
	size_t rev_num;
	size_t seg_sz;
	size_t num_procs;
#ifdef MSH_WIN
	DWORD upd_pid;
#else
	pid_t upd_pid;
	mode_t security;
#endif
	bool_t is_thread_safe;
};

typedef struct LocalSegmentInfo_tag LocalSegmentInfo_t;
struct LocalSegmentInfo_tag
{
	size_t seg_num;                              // segment number (iterated when a new file is needed)
	size_t rev_num;                              // total number of revisions (used for comparison, not indexing, so it's circular)
};

typedef struct MemorySegment_tag MemorySegment_t;
struct MemorySegment_tag
{
	char_t name[MSH_MAX_NAME_LEN];
	bool_t is_init;
	bool_t is_mapped;
#ifdef MSH_WIN
	HANDLE handle;
#else
	handle_t handle;
#endif
	size_t seg_sz;
	void* ptr;
};

typedef struct MexInfo_tag MexInfo_t;
struct MexInfo_tag
{
	MemorySegment_t shm_data_seg;
	MemorySegment_t shm_update_seg;
	MemorySegment_t lcl_init_seg;

#ifdef MSH_WIN
	SECURITY_ATTRIBUTES lock_sec;
	HANDLE proc_lock;
#else
	handle_t proc_lock;
#endif
	
	LocalSegmentInfo_t cur_seg_info;
	
	struct
	{
		bool_t is_proc_lock_init;
		bool_t is_glob_shm_var_init;
		
		bool_t is_proc_locked;
	} flags;

#ifdef MSH_WIN
	DWORD this_pid;
#else
	pid_t this_pid;
#endif
	
	uint32_t num_lcl_objs;
};

mxArray* g_shm_var;
MexInfo_t* g_info;

#define shm_data_ptr ((byte_t*)g_info->shm_data_seg.ptr)
#define shm_update_info ((ShmSegmentInfo_t*)g_info->shm_update_seg.ptr)

#endif //MATSHARE_MSH_TYPES_H
