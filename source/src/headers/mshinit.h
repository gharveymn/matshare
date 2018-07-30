/** mshinit.h
 * Declares functions for initializing matshare.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
 
#ifndef MATSHARE_INIT_H
#define MATSHARE_INIT_H

#include "mshtypes.h"

/**
 * Runs exit hooks for when MATLAB clears the MEX function.
 */
void msh_OnExit(void);


/**
 * Runs hooks for when matshare has an error.
 */
void msh_OnError(int error_severity);

/**
 * Runs initialization for matshare. Hooks into necessary shared segments
 * and loads the configuration.
 */
void msh_InitializeMatshare(void);


/**
 * Sets the configuration to default.
 */
void msh_SetDefaultConfiguration(UserConfig_t* user_config);

#endif /* MATSHARE_INIT_H */
