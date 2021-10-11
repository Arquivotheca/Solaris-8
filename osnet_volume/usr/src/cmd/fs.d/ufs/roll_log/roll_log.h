/*
 * Copyright (c) 1996,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_ROLL_LOG_H
#define	_ROLL_LOG_H

#pragma ident	"@(#)roll_log.h	1.6	97/06/09 SMI"

#include <sys/fs/ufs_fs.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file contains definitions for the module that rolls the Unix File
 * System log.
 */

/*
 * rl_result_t defines the type of the value that is returned by all roll
 * log functions.
 */

typedef enum rl_result {
	/*
	 * Choose values so that all passing returns are >= 0, and all
	 * failing returns are < 0.
	 */

	RL_CORRUPT = -4,		/* Corrupted on disk structure. */
	RL_FAIL = -3,			/* Generic failure. */
	RL_SYSERR = -2,			/* Failing system call. */
	RL_FALSE = -1,
	RL_SUCCESS = 0,
	RL_TRUE = 1
} rl_result_t;

/* Functions defined in roll_log.c */

extern rl_result_t	rl_roll_log(char *dev);

#ifdef	__cplusplus
}
#endif

#endif	/* _ROLL_LOG_H */
