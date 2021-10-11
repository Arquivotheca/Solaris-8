/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_FILE_H
#define	_SYS_FILE_H

#pragma ident	"@(#)file.h	1.60	99/08/31 SMI"	/* SVr4.0 11.28 */

#include <sys/t_lock.h>
#ifdef _KERNEL
#include <sys/model.h>
#include <sys/user.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * fio locking:
 *   f_rwlock	protects f_vnode and f_cred
 *   f_tlock	protects the rest
 *
 *   The purpose of locking in this layer is to keep the kernel
 *   from panicing if, for example, a thread calls close() while
 *   another thread is doing a read().  It is up to higher levels
 *   to make sure 2 threads doing I/O to the same file don't
 *   screw each other up.
 */
/*
 * One file structure is allocated for each open/creat/pipe call.
 * Main use is to hold the read/write pointer associated with
 * each open file.
 */
typedef struct file {
	kmutex_t	f_tlock;	/* short term lock */
	ushort_t	f_flag;
	ushort_t	f_pad;		/* Explicit pad to 4 byte boundary */
	struct vnode	*f_vnode;	/* pointer to vnode structure */
	offset_t	f_offset;	/* read/write character pointer */
	struct cred	*f_cred;	/* credentials of user who opened it */
	caddr_t		f_audit_data;	/* file audit data */
	int		f_count;	/* reference count */
} file_t;

/*
 * fpollinfo struct - used by poll caching to track who has polled the fd
 */
typedef struct fpollinfo {
	struct _kthread		*fp_thread;	/* thread caching poll info */
	struct fpollinfo	*fp_next;
} fpollinfo_t;

/* flags */

#define	FOPEN		0xFFFFFFFF
#define	FREAD		0x01	/* <sys/aiocb.h> LIO_READ must be identical */
#define	FWRITE		0x02	/* <sys/aiocb.h> LIO_WRITE must be identical */
#define	FNDELAY		0x04
#define	FAPPEND		0x08
#define	FSYNC		0x10	/* file (data+inode) integrity while writing */
#ifdef C2_AUDIT
#define	FREVOKED	0x20	/* C2 Security - Revoke Subsystem */
#endif
#define	FDSYNC		0x40	/* file data only integrity while writing */
#define	FRSYNC		0x8000	/* sync flag for read operations */
				/* combined with FSYNC or FDSYNC, it has */
				/* effect on read same as write		*/
				/* Should be within first 8 bits but */
				/* no space, so we change FMASK also */
/* FOFFMAX is not a open-only mode */
#define	FOFFMAX		0x2000	/* Large file */
				/* Cannot be within first eight bits */
				/* So we change FMASK also */
#define	FNONBLOCK	0x80

#define	FMASK		0xa0FF	/* should be disjoint from FASYNC */
				/* should include FRSYNC also */

/* open-only modes */

#define	FCREAT		0x0100
#define	FTRUNC		0x0200
#define	FEXCL		0x0400
#define	FNOCTTY		0x0800
#define	FASYNC		0x1000	/* asyncio is in progress */

/* fsync pseudo flag */

#define	FNODSYNC	0x10000

#ifdef _KERNEL

/*
 * Fake flags for driver ioctl calls to inform them of the originating
 * process' model.  See <sys/model.h>
 *
 * Part of the Solaris 2.6+ DDI/DKI
 */
#define	FMODELS	DATAMODEL_MASK	/* Note: 0x0FF00000 */
#define	FILP32	DATAMODEL_ILP32
#define	FLP64	DATAMODEL_LP64
#define	FNATIVE	DATAMODEL_NATIVE

/*
 * Large Files: The macro gets the offset maximum (refer to LFS API doc)
 * corresponding to a file descriptor. We had the choice of storing
 * this value in file descriptor. Right now we only have two
 * offset maximums one if MAXOFF_T and other is MAXOFFSET_T. It is
 * inefficient to store these two values in a separate member in
 * file descriptor. To avoid wasting spaces we define this macro.
 * The day there are more than two offset maximum we may want to
 * rewrite this macro.
 */

#define	OFFSET_MAX(fd)	((fd->f_flag & FOFFMAX) ? MAXOFFSET_T : MAXOFF32_T)

/*
 * Fake flag => internal ioctl call for layered drivers.
 * Note that this flag deliberately *won't* fit into
 * the f_flag field of a file_t.
 *
 * Part of the Solaris 2.x DDI/DKI.
 */
#define	FKIOCTL		0x80000000	/* ioctl addresses are from kernel */

/*
 * Fake flag => this time to specify that the open(9E)
 * comes from another part of the kernel, not userland.
 *
 * Part of the Solaris 2.x DDI/DKI.
 */
#define	FKLYR		0x40000000	/* layered driver call */

#endif	/* _KERNEL */

/* miscellaneous defines */

#ifndef L_SET
#define	L_SET	0	/* for lseek */
#endif /* L_SET */

#if defined(_KERNEL)

/*
 * Routines dealing with user per-open file flags and
 * user open files.
 */
struct proc;	/* forward reference for function prototype */

extern file_t *getf(int);
extern void releasef(int);
extern void areleasef(int, uf_info_t *);
#ifndef	_BOOT
extern void closeall(uf_info_t *);
#endif
extern void flist_fork(uf_info_t *, uf_info_t *);
extern int closef(file_t *);
extern int closeandsetf(int, file_t *);
extern int ufalloc(int);
extern int falloc(struct vnode *, int, file_t **, int *);
extern void finit(void);
extern void unfalloc(file_t *);
extern void setf(int, file_t *);
extern char f_getfd(int);
extern void f_setfd(int, char);
extern int fassign(struct vnode **, int, int *);
extern void fcnt_add(uf_info_t *, int);
extern void close_exec(uf_info_t *);
extern void clear_stale_fd(void);
extern void clear_active_fd(int);
extern void free_afd(afd_t *afd);
extern int fisopen(struct vnode *);
extern void delfpollinfo(int);
extern void addfpollinfo(int);

#ifdef DEBUG
/* The following functions are only used in ASSERT()s */
extern void checkwfdlist(struct vnode *, fpollinfo_t *);
extern void checkfpollinfo(void);
extern int infpollinfo(int);
#endif	/* DEBUG */

#endif	/* defined(_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FILE_H */
