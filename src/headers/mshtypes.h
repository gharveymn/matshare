#ifndef MATSHARE_MSH_TYPES_H
#define MATSHARE_MSH_TYPES_H

#include "mex.h"
#include "externtypes.h"


extern mxArray* mxCreateSharedDataCopy(mxArray*);

#ifndef FALSE
#  define FALSE 0
#endif

#ifndef TRUE
#  define TRUE 1
#endif

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)(-1))
#endif

#define MSH_MAX_NAME_LEN 64
#define MSH_UPDATE_SEGMENT_NAME "/MATSHARE_UPDATE_SEGMENT"
#define MSH_LOCK_NAME "/MATSHARE_LOCK"
#define MSH_SEGMENT_NAME "/MATSHARE_SEGMENT%0llx"

#ifdef MSH_AUTO_INIT
#define MSH_INIT_CHECK_NAME "MATSHARE_INIT%lu"
#endif

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
	msh_SHARE, msh_FETCH, msh_DETACH, msh_PARAM, msh_DEEPCOPY, msh_DEBUG, msh_OBJ_REGISTER, msh_OBJ_DEREGISTER, msh_INIT
} mshdirective_t;


typedef enum
{
	msh_SHARETYPE_COPY,               /* always create a new segment */
	msh_SHARETYPE_OVERWRITE          /* reuse the same segment if the new variable is smaller than or the same size as the old one */
} mshsharetype_t;


/* captures fundamentals of the mxArray */
/* In the shared memory the storage order is [header, size array, field_names, real dat, image data, sparse index r, sparse index c]  */
typedef struct
{
	struct
	{
		size_t dims;
		size_t pr;
		size_t pi;
		size_t ir;
		size_t jc;
		size_t field_str;
		size_t child_hdrs;          /* offset of array of the offsets of the children*/
	} data_offsets;               /* these are actually the relative offsets of data in shared memory (needed because memory maps are to virtual pointers) */
	size_t num_dims;          /* dimensionality of the matrix */
	size_t elem_size;               /* size of each element in pr and pi */
	size_t num_elems;          /* length of pr,pi */
	size_t nzmax;
	size_t obj_sz;               /* size of serialized object */
	int num_fields;       /* the number of fields.  The field string immediately follows the size array */
	mxClassID classid;       /* matlab class id */
	mxComplexity complexity;
	bool_t is_sparse;
	bool_t is_numeric;
	bool_t is_empty;
} Header_t;

/* pointers to the data in virtual memory */
typedef struct
{
	mwSize* dims;				/* pointer to the size array */
	void* pr;					/* real data portion */
	void* pi;					/* imaginary data portion */
	mwIndex* ir;				/* row indexes, for sparse */
	mwIndex* jc;				/* cumulative column counts, for sparse */
	char_t* field_str;			/* list of a structures fields, each field name will be separated by a null character */
	size_t* child_hdrs;           /* array of corresponding children headers */
} ShmData_t;

typedef struct
{
	/* use these to link together the memory segments */
	signed long prev_seg_num;
	signed long next_seg_num;
	size_t seg_sz;
	unsigned int procs_using;		/* number of processes using this variable */
	unsigned int procs_tracking;		/* number of processes tracking this memory segment */
	bool_t is_used;				/* ensures that this memory segment has been referenced at least once before removing */
} SegmentMetadata_t;

typedef struct
{
	SegmentMetadata_t* ptr;
	size_t seg_sz;
	char_t name[MSH_MAX_NAME_LEN];
	bool_t is_init;
	bool_t is_mapped;
#ifdef MSH_WIN
	HANDLE handle;
#else
	handle_t handle;
#endif
} MemorySegment_t;

struct SegmentNode_t;
struct SegmentList_t;
struct VariableList_t;

typedef struct VariableNode_t
{
	struct VariableList_t* parent_var_list;
	struct VariableNode_t* next; /* next variable that is currently fetched and being used */
	struct VariableNode_t* prev;
	mxArray* var;
	mxArray** crosslink;
	struct SegmentNode_t* seg_node;
} VariableNode_t;

typedef struct SegmentNode_t
{
	struct SegmentList_t* parent_seg_list;
	struct SegmentNode_t* next;
	struct SegmentNode_t* prev;
	VariableNode_t* var_node;
	signed long next_seg_num;
	signed long prev_seg_num;
	MemorySegment_t data_seg;
	signed long seg_num;
	bool_t will_free;
} SegmentNode_t;

typedef struct VariableList_t
{
	VariableNode_t* first;
	VariableNode_t* last;
	size_t num_vars;
} VariableList_t;

typedef struct SegmentList_t
{
	SegmentNode_t* first;
	SegmentNode_t* last;
	size_t num_segs;
} SegmentList_t;

/* structure of shared info about the shared segments */
typedef struct
{
	/* these are also all size_t to guarantee alignment for atomic operations */
	signed long lead_seg_num; 		/* caches the predicted next segment number */
	signed long first_seg_num;		/* the first segment number in the list */
	size_t rev_num;
	size_t num_shared_vars;
	unsigned int num_procs;
#ifdef MSH_WIN
	DWORD update_pid;
#else
	pid_t update_pid;
	mode_t security;
#endif
	mshsharetype_t sharetype;
#ifdef MSH_THREAD_SAFE
	bool_t is_thread_safe;
#endif
} ShmInfo_t;

typedef struct
{
	VariableList_t var_list;
	SegmentList_t seg_list;
	MemorySegment_t shm_info_seg;
	
	size_t rev_num;

#ifdef MSH_AUTO_INIT
	MemorySegment_t lcl_init_seg;
#endif

#ifdef MSH_THREAD_SAFE
#ifdef MSH_WIN
	SECURITY_ATTRIBUTES lock_sec;
	HANDLE proc_lock;
#else
	handle_t proc_lock;
#endif
#endif
	
	struct
	{
		bool_t is_proc_lock_init;
		bool_t is_proc_locked;
	} flags;

#ifdef MSH_WIN
	DWORD this_pid;
#else
	pid_t this_pid;
#endif
	
	size_t num_registered_objs;
	
} LocalInfo_t;

LocalInfo_t* g_info;
#define g_var_list g_info->var_list
#define g_seg_list g_info->seg_list
#define shm_info ((ShmInfo_t*)g_info->shm_info_seg.ptr)

#endif //MATSHARE_MSH_TYPES_H
