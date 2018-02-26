/* ------------------------------------------------------------------------- */
/* --- MATSHARE.H ------------------------------------------------------ */
/* ------------------------------------------------------------------------- */

#ifndef shared_memory__hpp
#define shared_memory__hpp

/* Define this to test */
/* #define SAFEMODE    */                 /* This copies memory across, defeating the purpose of this function but useful for testing */
/* #define DEBUG	   */			  /* Verbose outputs */


/* Possibily useful undocumented functions (see links at end for details): */
/* extern bool mxUnshareArray(const mxArray *pr, const bool noDeepCopy);   */
/* extern mxArray *mxUnreference(const mxArray *pr);					   */



// #ifndef SAFEMODE
// #define ARRAY_ACCESS_INLINING
// typedef struct mxArray_tag Internal_mxArray;					/* mxArray_tag defined in "mex.h" */
// #endif


/* standard mex include; */
#include "matrix.h"
#include "mex.h"
#include <stdint.h>
#include <stddef.h>
#include <windows.h>
#include <memory.h>

/* Possibily useful undocumented functions (see links at end for details): */
/* extern bool mxUnshareArray(const mxArray *pr, const bool noDeepCopy);   */
/* extern mxArray *mxUnreference(const mxArray *pr);					   */
extern mxArray* mxCreateSharedDataCopy(mxArray *);

/* max length of directive string */
#define MAXDIRECTIVELEN 256

#define MSH_NAME_SEGMENT_NAME "MATSHARE_NAME_SEGMENT"
#define NULLSEGMENT_SZ 1

#define MSH_LOCK_NAME "MATSHARE_LOCK"

typedef struct data data_t;
typedef struct header header_t;
typedef char byte_t;
typedef bool bool_t;

/* these are used for recording structure field names */
const char term_char = ';';          /*use this character to terminate a string containing the list of fields.  Do this because it can't be in a valid field name*/
const size_t align_size = 32;   /*the pointer alignment size, so if pdata is a valid pointer then &pdata[i*align_size] will also be.  Ensure this is >= 4*/
#define MXMALLOC_SIG_LEN 16
const uint8_t MXMALLOC_SIGNATURE[MXMALLOC_SIG_LEN] = {16, 0, 0, 0, 0, 0, 0, 0, 206, 250, 237, 254, 32, 0, 32, 0};
#define MSH_SEG_NAME_PREAMB_LEN 16
#define MSH_SEGMENT_NAME "MATSHARE_SEGMENT%d"
#define MAX_INT_STR_LEN 19
#define MSH_SEG_NAME_LEN (MSH_SEG_NAME_PREAMB_LEN + MAX_INT_STR_LEN)


mxArray* global_shared_variable;
HANDLE* shm_handle;
HANDLE* shm_name_handle;
HANDLE* proc_lock;
byte_t** current_segment_p;
SECURITY_ATTRIBUTES* lock_sec;

/*
 * The header_t object will be copied to shared memory in its entirety.
 *
 * Immediately after each copied header_t will be the matrix data values
 * [size array, field_names, pr, pi, ir, jc]
  *
 * The data_t objects will never be copied to shared memory and serve only
 * to abstract away mex calls and simplify the deep traversals in matlab.
 *
 */

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
	union {
		size_t M;           /* Row size for 2D matrices, or                  */
		size_t *dims;       /* Pointer to dims array for nD > 2 arrays       */
	} Mdims;
	size_t N;               /* Product of dims 2:end                         */
	void *pr;               /* Real Data Pointer (or cell/field elements)    */
	void *pi;               /* Imag Data Pointer (or field information)      */
	union {
		mwIndex *ir;        /* Pointer to row values for sparse arrays       */
		mxClassID ClassID;  /* New User Defined Class ID (classdef)          */
		char *ClassName;    /* Pointer to Old User Defined Class Name        */
	} irClassNameID;
	union {
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

/* structure used to record all of the data addresses */
struct data
{
	mwSize* pSize;               /* pointer to the size array */
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
	uint32_t segment_number;
	size_t segment_size;
} segment_info;


/* captures fundamentals of the mxArray */
/* In the shared memory the storage order is [header, size array, field_names, real dat, image data, sparse index r, sparse index c]  */
struct header
{
	bool isSparse;
	bool isNumeric;
	mxComplexity complexity;
	mxClassID classid;       /* matlab class id */
	size_t nDims;         /* dimensionality of the matrix.  The size array immediately follows the header */
	size_t elemsiz;       /* size of each element in pr and pi */
	size_t nzmax;         /* length of pr,pi */
	int nFields;       /* the number of fields.  The field string immediately follows the size array */
	int par_hdr_off;   /* offset to the parent's header, add this to help backwards searches (double linked... sort of)*/
	size_t shmsiz;            /* size of serialized object (header + size array + field names string) */
	size_t  strBytes;
};

segment_info* current_segment_info;

void init();

/* Remove shared memory references to input matrix (in-situ), recursively    */
/* if needed.                                                                */
void deepdetach(mxArray* ret_var);

/* Shallow copy matrix from shared memory into Matlab form.                  */
size_t shallowrestore(byte_t* shm, mxArray* ret_var);

size_t shallowfetch(byte_t* shm, mxArray** ret_var);

/* Recursively descend through Matlab matrix to assess how much space its    */
/* serialization will require.                                               */
size_t deepscan(header_t* hdr, data_t* dat, const mxArray* mxInput, header_t* par_hdr, mxArray** ret_var);

/* Descend through header and data structure and copy relevent data to       */
/* shared memory.                                                            */
void deepcopy(header_t* hdr, data_t* dat, byte_t* shm, header_t* par_hdr);

/* Descend through header and data structure and free the memory.            */
void deepfree(data_t* dat);

mxLogical deepcompare(byte_t* shm, const mxArray* comp_var, size_t* offset);

void onExit(void);

/* Pads the size to something that guarantees pointer alignment.			*/
size_t pad_to_align(size_t size);


/*Function to find the number of bytes required to store all of the			 */
/*field names of a structure */
size_t FieldNamesSize(const mxArray* mxStruct);

/*Function to copy all of the field names to a character array				*/
/*Use FieldNamesSize() to allocate the required size of the array			*/
/*returns the number of bytes used in pList									*/
size_t CopyFieldNames(const mxArray* mxStruct, char* pList, const char** field_names);

/*This function finds the number of fields contained within in a string      */
/*the string is terminated by term_char, a character that can't be in a      */
/*field name.  pBytes is always an aligned number																 */
int NumFieldsFromString(const char* pString, size_t* pfields, size_t* pBytes);

/*Function to take point a each element of the char** array at a list of names contained in string */
/*ppCharArray must be allocated to length num_names							 */
/*names are seperated by null termination characters					     */
/*each name must start on an aligned address (see CopyFieldNames())			 */
/*e.g. pCharArray[0] = name_1, pCharArray[1] = name_2 ...					 */
/*returns 0 if successful													 */
int PointCharArrayAtString(char** pCharArray, char* pString, int nFields);

/* Function to find the bytes in the string starting from the end of the string */
/* returns < 0 on error */
int BytesFromStringEnd(const char* pString, size_t* pBytes);

void acquireProcLock(void);

void releaseProcLock(void);

#ifdef SAFEMODE
/* A convenient function for safe assignment of memory to an mxArray */
void* safeCopy(void* pBuffer, mwSize Bytes)
{
	void* pSafeBuffer;
	
	/* ensure Matlab knows it */
	pSafeBuffer = mxMalloc(Bytes);
	if (pSafeBuffer != NULL)
		memcpy(pSafeBuffer, pBuffer, Bytes); /* and copy the data */

	return pSafeBuffer;
}
#endif

#endif