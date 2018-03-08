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
#define MSH_INIT_CHECK_NAME "/MATSHARE_INIT%u"
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
} msh_directive_t;

/* captures fundamentals of the mxArray */
/* In the shared memory the storage order is [header, size array, field_names, real dat, image data, sparse index r, sparse index c]  */
typedef struct header_tag header_t;
struct header_tag
{
	struct
	{
		size_t dims;
		size_t pr;
		union
		{
			size_t pi;
			size_t field_str;
		};
		size_t ir;
		size_t jc;
		size_t child_hdr;
		int parent_hdr;
	} data_off; 			/* these are actually the relative offsets of data */
	size_t num_dims;         /* dimensionality of the matrix.  The size array immediately follows the header */
	size_t elem_size;       /* size of each element in pr and pi */
	size_t num_elems;         /* length of pr,pi */
	size_t shm_sz;            /* size of serialized object (header + size array + field names string) */
	size_t str_sz;
	int num_fields;       /* the number of fields.  The field string immediately follows the size array */
	mxComplexity complexity;
	mxClassID classid;       /* matlab class id */
	bool_t is_sparse;
	bool_t is_numeric;
	bool_t is_empty;
};

/* structure used to record all of the data addresses */
typedef struct data_tag data_t;
struct data_tag
{
	mwSize* dims;               /* pointer to the size array */
	void* pr;                    /* real data portion */
	union
	{
		void* pi;               /* imaginary data portion */
		char_t* field_str;   /* list of a structures fields, each field name will be seperated by a null character and terminated with a ";" */
	};
	mwIndex* ir;                    /* row indexes, for sparse */
	mwIndex* jc;                    /* cumulative column counts, for sparse */
	data_t* child_dat;          /* array of children data structures, for cell */
	header_t* child_hdr;          /* array of corresponding children header structures, for cell */
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
};

typedef struct LocalSegmentInfo_tag LocalSegmentInfo_t;
struct LocalSegmentInfo_tag
{
	uint64_t seg_num;                              // segment number (iterated when a new file is needed)
	uint64_t rev_num;                              // total number of revisions (used for comparison, not indexing, so it's circular)
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
	int handle;
#endif
	size_t seg_sz;
	void* ptr;
};

typedef struct MexInfo_tag MexInfo_t;
struct MexInfo_tag
{
	MemorySegment_t shm_data_seg;
	MemorySegment_t shm_update_seg;
	MemorySegment_t init_seg;

#ifdef MSH_WIN
	HANDLE proc_lock;
	SECURITY_ATTRIBUTES lock_sec;
#else
	int proc_lock;
#endif
	
	LocalSegmentInfo_t cur_seg_info;
	struct
	{
		bool_t is_proc_lock_init;
		bool_t is_glob_shm_var_init;
		
		bool_t is_proc_locked;
		bool_t is_mem_safe;
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
