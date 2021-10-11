/*
 * Copyright (c) 1987, 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _AUDITR_H
#define	_AUDITR_H

#pragma ident	"@(#)auditr.h	1.9	99/07/08 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <stdio.h>
#include <sys/types.h>

#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>
#include <values.h>

#include <dirent.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <tzfile.h>
#include <sys/resource.h>
#include <netdb.h>
#include <unistd.h>
#include <libgen.h>
#include <stdlib.h>

#include <bsm/audit.h>
#include <bsm/audit_record.h>
#include <bsm/libbsm.h>

#include "auditrt.h"

/*
 * Flags for on/off code.
 * The release setting would be 0 0 0 0 1.
 */
#define	AUDIT_PROC_TRACE	0	/* process trace code */
#define	AUDIT_FILE		0	/* file trace code (use -V also) */
#define	AUDIT_REC		0	/* record trace code (very verbose) */
#define	AUDIT_RENAME		1	/* rename output file w/time stamps */

#define	TRUE	1
#define	FALSE	0

#define	FM_ALLDIR	1	/* f_mode in o.c - all dirs in this dir */
#define	FM_ALLFILE	0	/* f_mode in o.c - all audit files in dir */

#define	MAXFILELEN	(MAXPATHLEN+MAXNAMLEN+1)

/*
 * Initial size of a record buffer.
 * Never smaller than (2 * sizeof (short)).
 * If a buffer is too small for the record being read then the
 * current buffer is freed and a large-enough one is allocated.
 */
#define	AUDITBUFSIZE	512	/* size of default record buffer */

/*
 * Controls size of audit_pcbs[] array.
 * INITSIZE is the initial allocation for the array.
 * INC is the growth jump when the array becomes too small.
 */
#define	PCB_INITSIZE	100
#define	PCB_INC		50


/*
 * Memory allocation functions.
 */
extern void	*a_calloc();	/* audit calloc that checks for NULL ret */

/*
 * System time support functions.
 */
extern char	*ctime();
extern char	*asctime();

/*
 * Auditreduce time support functions.
 */
extern struct tm *localtime();
extern struct tm *gmtime();
extern time_t tm_to_secs();
extern void	derive_str();

/*
 * Statistical reporting for error conditions.
 */
extern void	audit_stats();
extern int	errno;

/*
 * More regular expression support functions.
 * For matching multiple files.
 */
extern char *re_comp2();
extern int  re_exec2();

#ifdef	__cplusplus
}
#endif

#endif /* _AUDITR_H */
