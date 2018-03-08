#ifndef matshare__h
#define matshare__h

#include "mshtypes.h"
#include "matlabutils.h"
#include "mshutils.h"
#include "init.h"


/* Remove shared memory references to input matrix (in-situ), recursively    */
/* if needed.                                                                */
void shmDetach(mxArray* ret_var);

size_t shmFetch(byte_t* shm, mxArray** ret_var);

bool_t shmCompareSize(byte_t* shm, const mxArray* comp_var, size_t* offset);

size_t shmRewrite(byte_t* shm, const mxArray* in_var);

/* Recursively descend through Matlab matrix to assess how much space its    */
/* serialization will require.                                               */
size_t shmScan(header_t* hdr, data_t* dat, const mxArray* mxInput, header_t* par_hdr, mxArray** ret_var);

/* Descend through header and data structure and copy relevent data to       */
/* shared memory.                                                            */
void shmCopy(header_t* hdr, data_t* dat, byte_t* shm_anchor, header_t* par_hdr, mxArray* ret_var);

mxLogical shmCompareContent(byte_t* shm, const mxArray* comp_var, size_t* offset);


/*Function to find the number of bytes required to store all of the			 */

/*Function to copy all of the field names to a character array				*/
/*Use fieldNamesSize() to allocate the required size of the array			*/

/*This function finds the number of fields contained within in a string      */
/*the string is terminated by TERM_CHAR, a character that can't be in a      */
/*field name.  pBytes is always an aligned number																 */
int NumFieldsFromString(const char_t* pString, size_t* pfields, size_t* pBytes);

/*Function to take point a each element of the char_t** array at a list of names contained in string */
/*ppCharArray must be allocated to length num_names							 */
/*names are seperated by null termination characters					     */
/*each name must start on an aligned address (see copyFieldNames())			 */
/*e.g. pCharArray[0] = name_1, pCharArray[1] = name_2 ...					 */

/* Function to find the bytes in the string starting from the end of the string */
/* returns < 0 on error */
int BytesFromStringEnd(const char_t* pString, size_t* pBytes);


#endif
