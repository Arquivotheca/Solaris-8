/*
 *	nfs_subr.h
 *
 *	Copyright (c) 1996 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#ifndef	_NFS_SUBR_H
#define	_NFS_SUBR_H

#pragma ident	"@(#)nfs_subr.h	1.2	97/11/24 SMI"

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * nfs library routines
 */
extern int remote_lock(char *, caddr_t);
extern void URLparse(char *);
extern int convert_special(char **, char *, char *, char *, char *);

#ifdef __cplusplus
}
#endif

#endif	/* _NFS_SUBR_H */
