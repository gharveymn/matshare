#ifndef MATSHARE_INIT_H
#define MATSHARE_INIT_H

#include "msh_types.h"
#include "utils.h"

#define MSH_LOCK_NAME "/MATSHARE_LOCK"
#define MATSHARE_STARTUP_FLAG_PRENAME "MATSHARE_STARTUP_FLAG%lu"


void init(bool_t is_mem_safe);
void procStartup(void);
void initProcLock(void);
void initUpdateSegment(void);
void mapUpdateSegment(void);
void globStartup(header_t* hdr);
void initDataSegment(void);
void mapDataSegment(void);
void makeDummyVar(mxArray**);

extern size_t shallowfetch(byte_t* shm, mxArray** ret_var);
extern mxArray* mxCreateSharedDataCopy(mxArray *);
extern mxArray* glob_shm_var;
extern mex_info* glob_info;
extern void acquireProcLock(void);
extern void releaseProcLock(void);
extern void onExit(void);
extern size_t pad_to_align(size_t size);

#endif //MATSHARE_INIT_H
