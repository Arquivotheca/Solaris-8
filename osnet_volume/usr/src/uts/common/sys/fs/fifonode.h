/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_SYS_FS_FIFONODE_H
#define	_SYS_FS_FIFONODE_H

#pragma ident	"@(#)fifonode.h	1.32	99/11/03 SMI"	/* SVr4.0 1.15	*/

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * Each FIFOFS object is identified by a struct fifonode/vnode pair.
 * This is also the hierarchy
 * flk_lock protects:
 *		fn_mp
 *		fn_tail
 *		fn_count
 *		fn_flag
 *		fn_wcnt
 *		fn_rcnt
 *		fn_open
 *		fn_rsynccnt
 *		fn_wsynccnt
 *		fn_atime
 *		fn_mtime
 *		fn_ctime
 *		fn_insync
 *		flk_ref
 *		flk_ocsync
 * ftable lock protects		- actually this is independent
 *		fifoalloc[]
 *		fn_nextp
 *		fn_backp
 */
typedef struct fifolock {
	kmutex_t	flk_lock;	/* fifo lock */
	int		flk_ref;	/* number of fifonodes using this */
	short		flk_ocsync;	/* sync open/close */
	kcondvar_t	flk_wait_cv;	/* conditional for flk_ocsync */
	uint_t		flk_fill[4];	/* cache align lock structure */
} fifolock_t;

typedef struct fifonode fifonode_t;

struct fifonode {
	struct vnode	fn_vnode;	/* represents the fifo/pipe */
	struct vnode	*fn_realvp;	/* node being shadowed by fifo */
	ino_t		fn_ino;		/* node id for pipes */
	fifonode_t	*fn_dest;	/* the other end of a pipe */
	struct msgb	*fn_mp;		/* message waiting to be read */
	struct msgb	*fn_tail;	/* last message to read */
	uint_t		fn_count;	/* Number of bytes on fn_mp */
	fifolock_t	*fn_lock;	/* pointer to per fifo lock */
	kcondvar_t	fn_wait_cv;	/* fifo conditional variable */
	ushort_t	fn_fill1;	/* flags set to filler */
	ushort_t	fn_wcnt;	/* number of writers */
	ushort_t	fn_rcnt;	/* number of readers */
	ushort_t	fn_open;	/* open count of node */
	ushort_t	fn_wsynccnt;	/* fifos waiting for open write sync */
	ushort_t	fn_rsynccnt;	/* fifos waiting for open read sync */
	ushort_t	fn_fill;
	time_t		fn_atime;	/* access times */
	time_t		fn_mtime;	/* modification time */
	time_t		fn_ctime;	/* change time */
	fifonode_t	*fn_nextp;	/* next link in the linked list */
	fifonode_t	*fn_backp;	/* back link in linked list */
	int		fn_insync;
	uint_t		fn_flag;	/* flags as defined below */
};


typedef struct fifodata {
	fifolock_t	fifo_lock;
	fifonode_t	fifo_fnode[2];
} fifodata_t;

/*
 * Valid flags for fifonodes.
 */
#define	ISPIPE		0x0001	/* fifonode is that of a pipe */
#define	FIFOSEND	0x0002	/* file descriptor at stream head of pipe */
#define	FIFOOPEN	0x0004	/* fifo is opening */
#define	FIFOCLOSE	0x0008	/* fifo is closing */
#define	FIFOCONNLD	0x0010	/* connld pushed on pipe */
#define	FIFOFAST	0x0020	/* FIFO in fast mode */
#define	FIFOWANTR	0x0040	/* reader waiting for data */
#define	FIFOWANTW	0x0080	/* writer waiting to write */
#define	FIFOSETSIG	0x0100	/* I_SETSIG ioctl was issued */
#define	FIFOHIWATW	0x0200	/* We have gone over hi water mark */
#define	FIFORWBUSY	0x0400	/* Fifo is busy in read or write */
#define	FIFOPOLLW	0x0800	/* process waiting on poll write */
#define	FIFOPOLLR	0x1000	/* process waiting on poll read */
#define	FIFOISOPEN	0x2000	/* pipe is open */
#define	FIFOSYNC	0x4000	/* FIFO is waiting for open sync */
#define	FIFOWOCR	0x8000	/* Write open occured */
#define	FIFOROCR	0x10000	/* Read open occured */
/*
 * process waiting on poll read on band data
 * this can only occur if we go to streams
 * mode
 */
#define	FIFOPOLLRBAND	0x2000

#define	FIFOHIWAT	(9 * 1024)
#define	FIFOLOWAT	(0)

/*
 * Macros to convert a vnode to a fifnode, and vice versa.
 */
#define	VTOF(vp) ((struct fifonode *)((vp)->v_data))
#define	FTOV(fp) (&(fp)->fn_vnode)

#if defined(_KERNEL)

/*
 * Fifohiwat defined as a variable is to allow tuning of the high
 * water mark if needed. It is not meant to be released.
 */
#if FIFODEBUG
extern int Fifohiwat;
#else /* FIFODEBUG */
#define	Fifohiwat	FIFOHIWAT
#endif /* FIFODEBUG */

extern struct vnodeops fifo_vnodeops;
extern struct kmem_cache *fnode_cache;
extern struct kmem_cache *pipe_cache;

struct vfssw;
struct queue;

extern int	fifoinit(struct vfssw *, int);
extern int	fifo_stropen(vnode_t **, int, cred_t *, int, int);
extern int	fifo_open(vnode_t **, int, cred_t *);
extern int	fifo_close(vnode_t *, int, int, offset_t, cred_t *);
extern void	fifo_cleanup(vnode_t *, int);
extern void	fifo_remove(fifonode_t *);
extern void	fiforemove(fifonode_t *);
extern ino_t	fifogetid(void);
extern vnode_t	*fifovp(vnode_t *, cred_t *);
extern void	makepipe(vnode_t **, vnode_t **);
extern void	fifo_fastflush(fifonode_t *);
extern void	fifo_vfastoff(vnode_t *vp);
extern void	fifo_fastoff(fifonode_t *);
extern struct streamtab *fifo_getinfo();
extern void	fifo_wakereader(fifonode_t *fn_dest, fifolock_t *fn_lock);
extern void	fifo_wakewriter(fifonode_t *fn_dest, fifolock_t *fn_lock);

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FS_FIFONODE_H */
