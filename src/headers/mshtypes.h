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

#ifndef LONG_MAX
#define LONG_MAX 2147483647L
#endif

#define MSH_MAX_NAME_LEN 64
#define MSH_UPDATE_SEGMENT_NAME "/MATSHARE_UPDATE_SEGMENT"
#define MSH_LOCK_NAME "/MATSHARE_LOCK"
#define MSH_SEGMENT_NAME "/MATSHARE_SEGMENT%0lx"

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
#define ALIGN_SIZE (size_t)0x20u    /* The pointer alignment size; ensure this is a multiple of 32 for AVX alignment */
#define ALIGN_SHIFT (size_t)0x1Fu /* (ALIGN_SIZE-1) micro-optimization */
#define MXMALLOC_SIG_LEN 0x10u      /* length of the mxMalloc signature */

/* readability typedefs */
typedef char char_t;                     /* characters */
typedef signed char schar_t;             /* signed 8 bits */
typedef unsigned char uchar_t;           /* unsigned 8 bits */
typedef uchar_t byte_t;                  /* reading physical memory */
typedef uchar_t bool_t;                  /* conditionals */
typedef signed long msh_segmentnumber_t; /* segment number identifiers */
#ifdef MSH_UNIX
typedef int handle_t;			/* give fds a uniform identifier */
#endif

#define SEG_NUM_MAX LONG_MAX      /* the maximum segment number */

typedef enum
{
	msh_SHARE = 0,
	msh_FETCH = 1,
	msh_DETACH = 2,
	msh_PARAM = 3,
	msh_DEEPCOPY = 4,
	msh_DEBUG = 5, /* unused */
	msh_OBJ_REGISTER = 6,
	msh_OBJ_DEREGISTER = 7,
	msh_INIT = 8, /* unused */
	msh_CLEAR = 9
} msh_directive_t;


typedef enum
{
	msh_SHARETYPE_COPY = 0,               /* always create a new segment */
	msh_SHARETYPE_OVERWRITE = 1          /* reuse the same segment if the new variable is smaller than or the same size as the old one */
} msh_sharetype_t;


/* captures fundamentals of the mxArray */
/* A variable shall be packed as so: [{s_Header_t, dims}, (child offsets, field names, data, imag_data, ir, jc)] where {} is required, () is optional  */
typedef struct
{
	struct
	{
		size_t data;
		size_t imag_data;
		union
		{
			size_t ir;
			size_t field_str;
		};
		union
		{
			size_t jc;
			size_t child_offs; /* offset of array of the offsets of the children*/
		};
	} data_offsets;               /* these are actually the relative offsets of data in shared memory (needed because memory maps are to virtual pointers) */
	size_t num_dims;          /* dimensionality of the matrix */
	size_t elem_size;               /* size of each element in data and imag_data */
	size_t num_elems;          /* length of data, imag_data */
	size_t nzmax;
	int num_fields;       /* the number of fields.  The field string immediately follows the size array */
	mxClassID classid;       /* matlab class id */
	bool_t is_sparse;
	bool_t is_numeric;
} s_Header_t;

#define MshGetDimensions(shm_anchor) (mwSize*)((shm_anchor) + sizeof(s_Header_t))
#define MshGetData(shm_anchor) ((shm_anchor) + (hdr)->data_offsets.data)
#define MshGetImagData(shm_anchor) ((shm_anchor) + (hdr)->data_offsets.imag_data)
#define MshGetIr(shm_anchor) (mwIndex*)((shm_anchor) + (hdr)->data_offsets.ir)
#define MshGetJc(shm_anchor) (mwIndex*)((shm_anchor) + (hdr)->data_offsets.jc)
#define MshIsEmpty(hdr_ptr) ((hdr_ptr)->num_elems == 0)
#define MshIsComplex(hdr_ptr) ((hdr_ptr)->data_offsets.imag_data != SIZE_MAX)
#define MshGetComplexity(hdr_ptr) MshIsComplex(hdr_ptr)? mxCOMPLEX : mxREAL

/* pointers to the data in virtual memory */
typedef struct
{
	mwSize* dims;                   /* pointer to the dimensions array */
	void* data;                     /* real data portion */
	void* imag_data;
	union
	{
		mwIndex* ir;                 /* row indexes, for sparse */
		char_t* field_str;          /* cumulative column counts, for sparse */
	};
	union
	{
		mwIndex* jc;                 /* list of a structures fields, each field name will be separated by a null character */
		size_t* child_hdrs;         /* array of corresponding children headers */
	};
} SharedDataPointers_t;


typedef struct s_SegmentMetadata_t
{
	/* use these to link together the memory segments */
	msh_segmentnumber_t seg_num;
	msh_segmentnumber_t prev_seg_num;
	msh_segmentnumber_t next_seg_num;
	size_t seg_sz;
	unsigned int procs_using;          /* number of processes using this variable */
	unsigned int procs_tracking;          /* number of processes tracking this memory segment */
	bool_t is_used;                    /* ensures that this memory segment has been referenced at least once before removing */
	bool_t is_invalid;                    /* set to TRUE if this segment is to be freed by all processes */
} s_SegmentMetadata_t;

typedef struct SegmentInfo_t
{
	void* s_ptr;
	size_t seg_sz;
	char_t name[MSH_MAX_NAME_LEN];
	bool_t is_init;
	bool_t is_mapped;
#ifdef MSH_WIN
	HANDLE handle;
#else
	handle_t handle;
#endif
} SegmentInfo_t;

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
	struct SegmentNode_t* next;
	struct SegmentNode_t* prev;
	VariableNode_t* var_node;
	SegmentInfo_t seg_info;
} SegmentNode_t;

#define SegmentRawPointer(seg_node) ((seg_node)->seg_info.s_ptr)
#define SegmentMetadata(seg_node) ((s_SegmentMetadata_t*)(seg_node)->seg_info.s_ptr)
#define SegmentStart(seg_node) ((byte_t*)(seg_node)->seg_info.s_ptr)
#define SegmentData(seg_node) (SegmentStart(seg_node) + PadToAlign(sizeof(s_SegmentMetadata_t)))
#define SegmentSize(seg_node) (seg_node)->seg_info.seg_sz;



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
	msh_segmentnumber_t last_seg_num;          /* caches the predicted next segment number */
	msh_segmentnumber_t first_seg_num;          /* the first segment number in the list */
	size_t rev_num;
	size_t num_shared_vars;
	unsigned int num_procs;
#ifdef MSH_WIN
	DWORD update_pid;
#else
	pid_t update_pid;
	mode_t security;
#endif
	
	struct
	{
		bool_t is_thread_safe;
		bool_t will_remove_unused;
		msh_sharetype_t sharetype;
	} user_def;
} s_SharedInfo_t;

typedef struct
{
	VariableList_t var_list;
	SegmentList_t seg_list;
	SegmentInfo_t shm_info_seg;
	
	size_t rev_num;

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
	
} GlobalInfo_t;


extern GlobalInfo_t* g_info;
#define g_var_list (g_info->var_list)
#define g_seg_list (g_info->seg_list)
#define s_info ((s_SharedInfo_t*)g_info->shm_info_seg.s_ptr)

#endif //MATSHARE_MSH_TYPES_H
