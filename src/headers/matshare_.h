/* ------------------------------------------------------------------------- */
/* --- MATSHARE.H ------------------------------------------------------ */
/* ------------------------------------------------------------------------- */

#ifndef matshare__h
#define matshare__h

#if defined(MATLAB_UNIX)
#  include <sys/mman.h>
#  define MSH_UNIX
#elif defined(MATLAB_WINDOWS)
#  define MSH_WIN
#elif defined(DEBUG_UNIX)
#  include <sys/mman.h>
#  include <sys/stat.h>
#  define MSH_UNIX
#elif defined(DEBUG_UNIX_ON_WINDOWS)
#  include "../extlib/mman-win32/sys/mman.h"
extern int shm_open(const char *name, int oflag, mode_t mode);
extern int shm_unlink(const char *name);
#  include <sys/stat.h>
#  define MSH_UNIX
#elif defined(DEBUG_WINDOWS)
#  define MSH_WIN
#else
# error(No build type specified.)
#endif

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
#include "mex.h"
#include <stdint.h>
#include <stddef.h>
#include <memory.h>
#include <string.h>
#include <ctype.h>
#include "utils.h"
#include "msh_types.h"

#ifndef FALSE
#  define FALSE 0
#endif

#ifndef TRUE
#  define TRUE 1
#endif

/* Possibily useful undocumented functions (see links at end for details): */
/* extern bool mxUnshareArray(const mxArray *pr, const bool noDeepCopy);   */
/* extern mxArray *mxUnreference(const mxArray *pr);					   */
extern mxArray* mxCreateSharedDataCopy(mxArray *);

/* max length of directive string */
#define MAXDIRECTIVELEN 256

/* forward slash works fine on windows, and required for linux */
#define MSH_SEG_NAME_PREAMB_LEN (sizeof(MSH_SEGMENT_NAME)-1)
#define MAX_UINT64_STR_LEN 16
#define MSH_SEG_NAME_LEN (MSH_SEG_NAME_PREAMB_LEN + MAX_UINT64_STR_LEN)

/* these are used for recording structure field names */
const char term_char = ';';          /*use this character to terminate a string containing the list of fields.  Do this because it can't be in a valid field name*/
#define align_size (size_t)0x20   /*the pointer alignment size, so if pdata is a valid pointer then &pdata[i*align_size] will also be.  Ensure this is >= 4*/

/* HACK */
#define MXMALLOC_SIG_LEN 16
const uint8_t MXMALLOC_SIGNATURE[MXMALLOC_SIG_LEN] = {16, 0, 0, 0, 0, 0, 0, 0, 206, 250, 237, 254, 32, 0, 32, 0};
/*
 * NEW MXMALLOC SIGNATURE INFO:
 * HEADER:
 * 		bytes 0-7 - size iterator, increases as (size/16+1)*16 mod 256
 * 			byte 0 = (((size+16-1)/2^4)%2^4)*2^4
 * 			byte 1 = (((size+16-1)/2^8)%2^8)
 * 			byte 2 = (((size+16-1)/2^16)%2^16)
 * 			byte n = (((size+16-1)/2^(2^(2+n)))%2^(2^(2+n)))
 *
 * 		bytes  8-11 - a signature for the vector check
 * 		bytes 12-13 - the alignment (should be 32 bytes for new MATLAB)
 * 		bytes 14-15 - the offset from the original pointer to the newly aligned pointer (should be 16 or 32)
 */

mxArray* glob_shm_var;
mex_info* glob_info;

/* Remove shared memory references to input matrix (in-situ), recursively    */
/* if needed.                                                                */
void deepdetach(mxArray* ret_var);

/* Shallow copy matrix from shared memory into Matlab form.                  */
size_t shallowrestore(byte_t* shm, mxArray* ret_var);

size_t shallowfetch(byte_t* shm, mxArray** ret_var);

bool_t shallowcompare(byte_t* shm, const mxArray* comp_var, size_t* offset);

size_t shallowrewrite(byte_t* shm, const mxArray* input_var);

/* Recursively descend through Matlab matrix to assess how much space its    */
/* serialization will require.                                               */
size_t deepscan(header_t* hdr, data_t* dat, const mxArray* mxInput, header_t* par_hdr, mxArray** ret_var);

/* Descend through header and data structure and copy relevent data to       */
/* shared memory.                                                            */
void deepcopy(header_t* hdr, data_t* dat, byte_t* shm, header_t* par_hdr, mxArray* ret_var);

/* Descend through header and data structure and free the memory.            */
void deepfreetmp(data_t* dat);

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

msh_directive_t parseDirective(const mxArray* in);

bool_t precheck(void);

void nullfcn(void);

void make_mxmalloc_signature(uint8_t sig[MXMALLOC_SIG_LEN], size_t seg_size);

void makeDummyVar(mxArray**);

extern void init(bool_t is_mem_safe);


#endif
