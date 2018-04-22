#ifndef MATSHARE_INIT_H
#define MATSHARE_INIT_H

#include "mshtypes.h"
#include "matlabutils.h"
#include "mshutils.h"

void InitializeMatshare();
void ProcStartup(void);
void InitProcLock(void);
void InitInfoSegment(void);
void MapInfoSegment(void);
void GlobalStartup(void);

#endif //MATSHARE_INIT_H
