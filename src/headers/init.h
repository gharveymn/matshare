#ifndef MATSHARE_INIT_H
#define MATSHARE_INIT_H

#include "mshtypes.h"
#include "matlabutils.h"
#include "mshutils.h"


void init();
void procStartup(void);
void initProcLock(void);
void initUpdateSegment(void);
void mapUpdateSegment(void);
void globStartup(header_t* hdr);
void initDataSegment(void);
void mapDataSegment(void);

extern size_t shmFetch(byte_t* shm, mxArray** ret_var);

#endif //MATSHARE_INIT_H
