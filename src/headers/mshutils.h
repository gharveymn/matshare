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

#define MSH_PARAM_COPYONWRITE_U "CopyOnWrite"
#define MSH_PARAM_COPYONWRITE_L "copyonwrite"


/*field names of a structure */
size_t GetFieldNamesSize(const mxArray* mxStruct);

void OnExit(void);

void GetNextFieldName(const char_t** field_str);

/* Pads the size to something that guarantees pointer alignment.			*/
ShmData_t LocateDataPointers(const Header_t* const hdr, byte_t* const shm_anchor);

void* MemCpyMex(byte_t* dest, byte_t* orig, size_t cpy_sz);

size_t PadToAlign(size_t size);

void MakeMxMallocSignature(unsigned char* sig, size_t seg_size);

void AcquireProcessLock(void);

void ReleaseProcessLock(void);

mshdirective_t ParseDirective(const mxArray* in);

bool_t Precheck(void);

void ParseParams(int num_params, const mxArray** in);

void UpdateAll(void);

void RemoveUnusedVariables(VariableList_t* var_list);

SegmentInfo_t CreateSegment(size_t seg_sz);

SegmentInfo_t OpenSegment(signed long seg_num);

SegmentNode_t* CreateSegmentNode(SegmentInfo_t seg_info);

void AddSegmentNode(SegmentList_t* seg_list, SegmentNode_t* seg_node);

void RemoveSegmentNode(SegmentList_t* seg_list, SegmentNode_t* seg_node);

void DestroySegmentNode(SegmentNode_t* seg_node);

VariableNode_t* CreateVariableNode(SegmentNode_t* seg_node);

void AddVariableNode(VariableList_t* var_list, VariableNode_t* var_node);

void RemoveVariableNode(VariableList_t* var_list, VariableNode_t* var_node);

void DestroyVariable(VariableNode_t* var_node);

void DestroyVariableNode(VariableNode_t* var_node);

void CleanVariableList(VariableList_t* var_list);

void CleanSegmentList(SegmentList_t* seg_list);

void NullFunction(void);

#endif //MATSHARE_MATSHAREUTILS_H
