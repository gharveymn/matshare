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
void globStartup(SegmentMetadata_t* metadata, Header_t* hdr);
void initDataSegment(void);
void mapDataSegment(void);
void autoInit(mshdirective_t directive);

extern size_t shmFetch_(byte_t* shm, mxArray** ret_var);

#endif //MATSHARE_INIT_H
