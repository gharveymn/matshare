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
void globStartup(void);
void initDataSegment(void);
void mapDataSegment(void);
void autoInit(mshdirective_t directive);

#endif //MATSHARE_INIT_H
