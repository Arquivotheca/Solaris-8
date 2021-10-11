/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_CORECTL_H
#define	_SYS_CORECTL_H

#pragma ident	"@(#)corectl.h	1.1	99/03/31 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definitions for corectl() system call.
 */

/* subcodes */
#define	CC_SET_OPTIONS		1
#define	CC_GET_OPTIONS		2
#define	CC_SET_GLOBAL_PATH	3
#define	CC_GET_GLOBAL_PATH	4
#define	CC_SET_PROCESS_PATH	5
#define	CC_GET_PROCESS_PATH	6

/* options */
#define	CC_GLOBAL_PATH		0x01	/* enable global core files */
#define	CC_PROCESS_PATH		0x02	/* enable per-process core files */
#define	CC_GLOBAL_SETID		0x04	/* allow global setid core files */
#define	CC_PROCESS_SETID	0x08	/* allow per-process setid core files */
#define	CC_GLOBAL_LOG		0x10	/* log global core dumps to syslog */

/* all of the above */
#define	CC_OPTIONS	\
	(CC_GLOBAL_PATH | CC_PROCESS_PATH | \
	CC_GLOBAL_SETID | CC_PROCESS_SETID | CC_GLOBAL_LOG)

#if	defined(_KERNEL)

extern	refstr_t	*core_file;
extern	uint32_t	core_options;
extern	kmutex_t	core_lock;

extern	void	init_core(void);

#else	/* !defined(_KERNEL) */

extern	int	core_set_options(int options);
extern	int	core_get_options(void);
extern	int	core_set_global_path(const char *buf, size_t bufsize);
extern	int	core_get_global_path(char *buf, size_t bufsize);
extern	int	core_set_process_path(const char *buf, size_t bufsize, pid_t);
extern	int	core_get_process_path(char *buf, size_t bufsize, pid_t);

#endif	/* defined(_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CORECTL_H */
