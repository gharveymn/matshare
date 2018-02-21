/* ------------------------------------------------------------------------- */
/* --- MATSHARE.H ------------------------------------------------------ */
/* ------------------------------------------------------------------------- */

#ifndef shared_memory__hpp
#define shared_memory__hpp

/* Define this to test */
/* #define SAFEMODE    */                 /* This copies memory across, defeating the purpose of this function but useful for testing */
/* #define DEBUG	   */			  /* Verbose outputs */


/* Possibily useful undocumented functions (see links at end for details): */
/* extern mxArray *mxCreateSharedDataCopy(const mxArray *pr);			*/
/* extern bool mxUnshareArray(const mxArray *pr, const bool noDeepCopy);   */
/* extern mxArray *mxUnreference(const mxArray *pr);					   */



// #ifndef SAFEMODE
// #define ARRAY_ACCESS_INLINING
// typedef struct mxArray_tag Internal_mxArray;					/* mxArray_tag defined in "mex.h" */
// #endif


/* standard mex include; */
#include "matrix.h"
#include "mex.h"

/* inbuilt libs */
#include "SharedMemStack.hpp"
#include <boost/interprocess/shared_memory_object.hpp>                  /* Prefer this but get permission errors sadly */
#include <boost/interprocess/windows_shared_memory.hpp>                  /* Have to ensure one windows_shared_memory object is attached to the memory, or else the memory is free'd */
#include <boost/interprocess/mapped_region.hpp>
#include <cstring>
#include <cstddef>
#include <windows.h>
#include <memory.h>

/* max length of directive string */
#define MAXDIRECTIVELEN 256

#define MATSHARE_SEGMENT_NAME "MATSHARE_SEGMENT"

/* these are used for recording structure field names */
const char term_char = ';';          /*use this character to terminate a string containing the list of fields.  Do this because it can't be in a valid field name*/
const size_t align_size = 32;   /*the pointer alignment size, so if pdata is a valid pointer then &pdata[i*align_size] will also be.  Ensure this is >= 4*/
const uint8_t MXMALLOC_SIG_LEN = 16;
const uint8_t MXMALLOC_SIGNATURE[MXMALLOC_SIG_LEN] = {16, 0, 0, 0, 0, 0, 0, 0, 206, 250, 237, 254, 32, 0, 32, 0};

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

typedef struct data data_t;
typedef struct header header_t;
typedef char byte_t;
typedef bool bool_t;

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


enum msh_directive_t
{
	msh_INIT,
	msh_CLONE,
	msh_ATTACH,
	msh_DETACH,
	msh_FREE,
	msh_FETCH,
	msh_COMPARE
};

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


/* captures fundamentals of the mxArray */
/* In the shared memory the storage order is [header, size array, field_names, real dat, image data, sparse index r, sparse index c]  */
struct header
{
	bool isSparse;
	bool isNumeric;
	mxComplexity complexity;
	mxClassID classid;       /* matlab class id */
	int nDims;         /* dimensionality of the matrix.  The size array immediately follows the header */
	size_t elemsiz;       /* size of each element in pr and pi */
	size_t nzmax;         /* length of pr,pi */
	int nFields;       /* the number of fields.  The field string immediately follows the size array */
	int par_hdr_off;   /* offset to the parent's header, add this to help backwards searches (double linked... sort of)*/
	size_t shmsiz;            /* size of serialized object (header + size array + field names string) */
	size_t  strBytes;
};

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

void onExit();

/* Pads the size to something that guarantees pointer alignment.			*/
__inline size_t pad_to_align(size_t size)
{
	if(size%align_size)
	{
		size += align_size - (size%align_size);
	}
	return size;
}


/*Function to find the number of bytes required to store all of the			 */
/*field names of a structure */
int FieldNamesSize(const mxArray* mxStruct);

/*Function to copy all of the field names to a character array				*/
/*Use FieldNamesSize() to allocate the required size of the array			*/
/*returns the number of bytes used in pList									*/
int CopyFieldNames(const mxArray* mxStruct, char* pList, const char** field_names);

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
