#ifndef MATSHARE_MATSHAREUTILS_H
#define MATSHARE_MATSHAREUTILS_H

#include <ctype.h>
#include <sys/stat.h>
#include "mshtypes.h"
#include "matshare_.h"
#include "matlabutils.h"

#define MSH_PARAM_THRSAFE_U "ThreadSafety"
#define MSH_PARAM_THRSAFE_L "threadsafety"

#define MSH_PARAM_SECURITY_U "Security"
#define MSH_PARAM_SECURITY_L "security"


/*field names of a structure */
size_t getFieldNamesSize(const mxArray* mxStruct);

void onExit(void);

void getNextFieldName(const char_t** field_str);

/* Pads the size to something that guarantees pointer alignment.			*/
void locateDataPointers(ShmData_t* data_ptrs, Header_t* hdr, byte_t* shm_anchor);

void* memCpyMex(byte_t* dest, byte_t* orig, size_t cpy_sz);

size_t padToAlign(size_t size);

void makeMxMallocSignature(unsigned char* sig, size_t seg_size);

void acquireProcLock(void);

void releaseProcLock(void);

mshdirective_t parseDirective(const mxArray* in);

bool_t precheck(void);

void parseParams(int num_params, const mxArray* in[]);

void updateAll(void);

void RemoveUnusedVariables(VariableList_t* var_list);

SegmentNode_t* CreateSegment(size_t seg_sz);

SegmentNode_t* OpenSegment(signed long seg_num);

void AddSegment(SegmentList_t* seg_list, SegmentNode_t* seg_node);

void RemoveSegment(SegmentList_t* seg_list, SegmentNode_t* seg_node);

VariableNode_t* CreateVariable(SegmentNode_t* seg_node);

void AddVariable(VariableList_t* var_list, VariableNode_t* var_node);

void RemoveVariable(VariableList_t* var_list, VariableNode_t* var_node);

void CleanVariableList(VariableList_t* var_list);

void CleanSegmentList(SegmentList_t* seg_list);

void NullFunction(void);


#endif //MATSHARE_MATSHAREUTILS_H
