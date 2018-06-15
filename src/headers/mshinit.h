/** mshinit.h
 * Declares functions for initializing matshare.
 *
 * Copyright (c) 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#ifndef MATSHARE_INIT_H
#define MATSHARE_INIT_H

/**
 * Runs initialization for matshare. Hooks into necessary shared segments
 * and loads the configuration.
 */
void msh_InitializeMatshare(void);

#endif /* MATSHARE_INIT_H */
