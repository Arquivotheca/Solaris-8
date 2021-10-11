/*	Copyright (c) 1993 SMI	*/
/*	 All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_POS4OBJ_H
#define	_POS4OBJ_H

#pragma ident	"@(#)pos4obj.h	1.7	98/04/28 SMI"

/*
 * pos4obj.h - Header file for POSIX.4 related object names
 */

#ifdef	__cplusplus
extern "C" {
#endif

/* flags used to indicate current state of open */
#define	DFILE_CREATE	0x01
#define	DFILE_OPEN	0x02
#define	ALLOC_MEM	0x04
#define	DFILE_MMAP	0x08
#define	PFILE_CREATE	0x10
#define	NFILE_CREATE	0x20

/* semaphore object types - used in constructing file name */
#define	SEM_DATA_TYPE	".SEMD"
#define	SEM_LOCK_TYPE	".SEML"

/* message queue object types - used in constructing file name */
#define	MQ_DATA_TYPE	".MQD"
#define	MQ_PERM_TYPE	".MQP"
#define	MQ_DSCN_TYPE	".MQN"
#define	MQ_LOCK_TYPE	".MQL"

/* shared memory object types - used in constructing file name */
#define	SHM_DATA_TYPE	".SHMD"
#define	SHM_LOCK_TYPE	".SHML"

/* functions defined related to object names in POSIX.4 */
extern	int	__pos4obj_lock(const char *, const char *);
extern	int	__pos4obj_unlock(const char *, const char *);
extern	int	__pos4obj_unlink(const char *, const char *);
extern	int	__pos4obj_open(const char *, char *, int, mode_t, int *);
extern	int	__pos4obj_check(const char *);

/* non-cancelable file operations */
int	__open_nc(const char *, int, mode_t);
int	__close_nc(int);

#ifdef	__cplusplus
}
#endif

#endif	/* _POS4OBJ_H */
