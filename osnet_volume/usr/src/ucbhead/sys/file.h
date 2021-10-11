/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * ****************************************************************
 *
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	Copyright (c) 1986-1989,1997-1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 *	Copyright (c) 1983-1989 by AT&T.
 *	All rights reserved.
 * ******************************************************************
 */

#ifndef _SYS_FILE_H
#define	_SYS_FILE_H

#pragma ident	"@(#)file.h	1.7	98/05/03 SMI"	/* SVr4.0 1.3	*/

#ifndef _SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * One file structure is allocated for each open/creat/pipe call.
 * Main use is to hold the read/write pointer associated with
 * each open file.
 */

typedef struct file
{
	struct file  *f_next;		/* pointer to next entry */
	struct file  *f_prev;		/* pointer to previous entry */
	ushort_t f_flag;
	cnt_t	f_count;		/* reference count */
	struct vnode *f_vnode;		/* pointer to vnode structure */
	off_t	f_offset;		/* read/write character pointer */
	struct	cred *f_cred;		/* credentials of user who opened it */
	struct	aioreq *f_aiof;		/* aio file list forward link	*/
	struct	aioreq *f_aiob;		/* aio file list backward link	*/
/* #ifdef MERGE */
	struct	file *f_slnk;		/* XENIX semaphore queue */
/* #endif MERGE */
} file_t;


#ifndef _SYS_FCNTL_H
#include <sys/fcntl.h>
#endif

/* flags - see also fcntl.h */

#ifndef FOPEN
#define	FOPEN	0xFFFFFFFF
#define	FREAD	0x01
#define	FWRITE	0x02
#define	FNDELAY	0x04
#define	FAPPEND	0x08
#define	FSYNC	0x10
#define	FNONBLOCK	0x80	/* Non-blocking flag (POSIX).	*/

#define	FMASK	0xff		/* should be disjoint from FASYNC */

/* open only modes */

#define	FCREAT	0x100
#define	FTRUNC	0x200
#define	FEXCL	0x400
#define	FNOCTTY	0x800		/* don't allocate controlling tty (POSIX). */
#define	FASYNC	0x1000		/* asyncio is in progress */
#define	FPRIV	0x1000		/* open with private access */

/* file descriptor flags */
#define	FCLOSEXEC	001	/* close on exec */
#endif

/* record-locking options. */
#define	F_ULOCK		0	/* Unlock a previously locked region */
#define	F_LOCK		1	/* Lock a region for exclusive use */
#define	F_TLOCK		2	/* Test and lock a region for exclusive use */
#define	F_TEST		3	/* Test a region for other processes locks */

/*
 * flock operations.
 */
#define	LOCK_SH		1	/* shared lock */
#define	LOCK_EX		2	/* exclusive lock */
#define	LOCK_NB		4	/* don't block when locking */
#define	LOCK_UN		8	/* unlock */

/*
 * Access call.
 */
#define	F_OK		0	/* does file exist */
#define	X_OK		1	/* is it executable by caller */
#define	W_OK		2	/* writable by caller */
#define	R_OK		4	/* readable by caller */

/*
 * Lseek call.
 */
#ifndef L_SET
#define	L_SET		0	/* absolute offset */
#define	L_INCR		1	/* relative to current offset */
#define	L_XTND		2	/* relative to end of file */
#endif


/* miscellaneous defines */

#define	NULLFP ((struct file *)0)

/*
 * Count of number of entries in file list.
 */
extern unsigned int filecnt;

/*
 * routines dealing with user per-open file flags and
 * user open files.  getf() is declared in systm.h.  It
 * probably belongs here.
 */
#if defined(__STDC__)
extern void setf(int, file_t *);
extern void setpof(int, char);
extern char getpof(int);
extern int fassign(struct vnode **, int, int *);
#else
extern void setf(), setpof();
extern char getpof();
extern int fassign();
#endif

extern off_t lseek();

#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	!defined(__PRAGMA_REDEFINE_EXTNAME))
#if defined(__STDC__)
extern off64_t lseek64(int, off64_t, int);
#else
extern off64_t llseek64();
#endif
#endif  /* _LARGEFILE64_SOURCE... */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_FILE_H */
