/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_BUF_H
#define	_SYS_BUF_H

#pragma ident	"@(#)buf.h	1.44	99/04/14 SMI"	/* SVr4.0 11.21	*/

#include <sys/types32.h>
#include <sys/t_lock.h>
#include <sys/kstat.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	Each buffer in the pool is usually doubly linked into 2 lists:
 *	the device with which it is currently associated (always)
 *	and also on a list of blocks available for allocation
 *	for other use (usually).
 *	The latter list is kept in last-used order, and the two
 *	lists are doubly linked to make it easy to remove
 *	a buffer from one list when it was found by
 *	looking through the other.
 *	A buffer is on the available list, and is liable
 *	to be reassigned to another disk block, if and only
 *	if it is not marked BUSY.  When a buffer is busy, the
 *	available-list pointers can be used for other purposes.
 *	Most drivers use the forward ptr as a link in their I/O active queue.
 *	A buffer header contains all the information required to perform I/O.
 *	Most of the routines which manipulate these things are in bio.c.
 *
 *	There are a number of locks associated with the buffer management
 *	system.
 *	hbuf.b_lock:	protects hash chains, buffer hdr freelists
 *			and delayed write freelist
 *	bfree_lock;	protects the bfreelist structure
 *	bhdr_lock:	protects the free header list
 *	blist_lock:	protects b_list fields
 *	buf.b_sem:	protects all remaining members in the buf struct
 *	buf.b_io:	I/O synchronization variable
 *
 *	A buffer header is never "locked" (b_sem) when it is on
 *	a "freelist" (bhdrlist or bfreelist avail lists).
 */
typedef struct	buf {
	int	b_flags;		/* see defines below */
	struct buf *b_forw;		/* headed by d_tab of conf.c */
	struct buf *b_back;		/*  "  */
	struct buf *av_forw;		/* position on free list, */
	struct buf *av_back;		/* if not BUSY */
	o_dev_t	b_dev;			/* OLD major+minor device name */
	size_t b_bcount;		/* transfer count */
	union {
		caddr_t b_addr;		/* low order core address */
		int	*b_words;	/* words for clearing */
		struct fs *b_fs;	/* superblocks */
		struct csum *b_cs;	/* superblock summary information */
		struct cg *b_cg;	/* cylinder group block */
		struct dinode *b_dino;	/* ilist */
		daddr32_t *b_daddr;	/* disk blocks */
	} b_un;

#define	paddr(X)	(paddr_t)(X->b_un.b_addr)

	lldaddr_t	_b_blkno;	/* block # on device (union) */
#define	b_lblkno	_b_blkno._f
#ifdef _LP64
#define	b_blkno		_b_blkno._f
#else
#define	b_blkno		_b_blkno._p._l
#endif /* _LP64 */

	char	b_oerror;		/* OLD error field returned after I/O */
	size_t	b_resid;		/* words not transferred after error */
	clock_t	b_start;		/* request start time */
	struct  proc  *b_proc;		/* process doing physical or swap I/O */
	struct	page  *b_pages;		/* page list for PAGEIO */
	clock_t b_reltime;		/* previous release time */
	/* Begin new stuff */
#define	b_actf	av_forw
#define	b_actl	av_back
#define	b_active b_bcount
#define	b_errcnt b_resid
	size_t	b_bufsize;		/* size of allocated buffer */
	int	(*b_iodone)(struct buf *);	/* function called by iodone */
	struct	vnode *b_vp;		/* vnode associated with block */
	struct 	buf *b_chain;		/* chain together all buffers here */
	int	b_reqcnt;		/* number of I/O request generated */
	int	b_error;		/* expanded error field */
	void	*b_private;		/* "opaque" driver private area */
	dev_t	b_edev;			/* expanded dev field */
	ksema_t	b_sem;			/* Exclusive access to buf */
	ksema_t	b_io;			/* I/O Synchronization */
	struct buf *b_list;		/* List of potential B_DELWRI bufs */
	struct page **b_shadow;		/* shadow page list */
} buf_t;

/*
 * Bufhd structures used at the head of the hashed buffer queues.
 * We only need three words for these, so this abbreviated
 * definition saves some space.
 */
struct bufhd {
	int	b_flags;		/* see defines below */
	struct buf *b_forw, *b_back;	/* fwd/bkwd pointer in chain */
};
struct diskhd {
	int	b_flags;		/* not used, needed for consistency */
	struct buf *b_forw, *b_back;	/* queue of unit queues */
	struct buf *av_forw, *av_back;	/* queue of bufs for this unit */
	long	b_bcount;		/* active flag */
};


/*
 * Statistics on the buffer cache
 */
struct biostats {
	kstat_named_t	bio_lookup;	/* requests to assign buffer */
	kstat_named_t	bio_hit;	/* buffer already associated with blk */
	kstat_named_t	bio_bufwant;	/* kmem_allocs NOSLEEP failed new buf */
	kstat_named_t	bio_bufwait;	/* kmem_allocs with KM_SLEEP for buf */
	kstat_named_t	bio_bufbusy;	/* buffer locked by someone else */
	kstat_named_t	bio_bufdup;	/* duplicate buffer found for block */
};

/*
 * These flags are kept in b_flags.
 * The first group is part of the DDI
 */
#define	B_BUSY		0x0001	/* not on av_forw/back list */
#define	B_DONE		0x0002	/* transaction finished */
#define	B_ERROR		0x0004	/* transaction aborted */
#define	B_PAGEIO	0x0010	/* do I/O to pages on bp->p_pages */
#define	B_PHYS		0x0020	/* Physical IO potentially using UNIBUS map */
#define	B_READ		0x0040	/* read when I/O occurs */
#define	B_WRITE		0x0100	/* non-read pseudo-flag */

/* Not part of the DDI */
#define	B_KERNBUF	0x0008		/* buffer is a kernel buffer */
#define	B_WANTED	0x0080		/* issue wakeup when BUSY goes off */
#define	B_AGE		0x000200	/* delayed write for correct aging */
#define	B_ASYNC		0x000400	/* don't wait for I/O completion */
#define	B_DELWRI	0x000800	/* delayed write-wait til buf needed */
#define	B_STALE		0x001000
#define	B_DONTNEED	0x002000	/* after write, need not be cached */
#define	B_REMAPPED	0x004000	/* buffer is kernel addressable */
#define	B_FREE		0x008000	/* free page when done */
#define	B_INVAL		0x010000	/* does not contain valid info  */
#define	B_FORCE		0x020000	/* semi-permanent removal from cache */
#define	B_HEAD		0x040000	/* a buffer header, not a buffer */
#define	B_NOCACHE	0x080000 	/* don't cache block when released */
#define	B_TRUNC		0x100000	/* truncate page without I/O */
#define	B_SHADOW	0x200000	/* shadow page list */
#define	B_RETRYWRI	0x400000	/* retry write til works or bfinval */


/*
 * Insq/Remq for the buffer hash lists.
 */
#define	bremhash(bp) { \
	ASSERT((bp)->b_forw != NULL); \
	ASSERT((bp)->b_back != NULL); \
	(bp)->b_back->b_forw = (bp)->b_forw; \
	(bp)->b_forw->b_back = (bp)->b_back; \
	(bp)->b_forw = (bp)->b_back = NULL; \
}
#define	binshash(bp, dp) { \
	ASSERT((bp)->b_forw == NULL); \
	ASSERT((bp)->b_back == NULL); \
	ASSERT((dp)->b_forw != NULL); \
	ASSERT((dp)->b_back != NULL); \
	(bp)->b_forw = (dp)->b_forw; \
	(bp)->b_back = (dp); \
	(dp)->b_forw->b_back = (bp); \
	(dp)->b_forw = (bp); \
}


/*
 * The hash structure maintains two lists:
 *
 * 	1) The hash list of buffers (b_forw & b_back)
 *	2) The LRU free list of buffers on this hash bucket (av_forw & av_back)
 *
 * The dwbuf structure keeps a list of delayed write buffers per hash bucket
 * hence there are exactly the same number of dwbuf structures as there are
 * the hash buckets (hbuf structures) in the system.
 *
 * The number of buffers on the freelist may not be equal to the number of
 * buffers on the hash list. That is because when buffers are busy they are
 * taken off the freelist but not off the hash list. "b_length" field keeps
 * track of the number of free buffers (including delayed writes ones) on
 * the hash bucket. The "b_lock" mutex protects the free list as well as
 * the hash list. It also protects the counter "b_length".
 *
 * Enties b_forw, b_back, av_forw & av_back must be at the same offset
 * as the ones in buf structure.
 */
struct	hbuf {
	int	b_flags;

	struct	buf	*b_forw;	/* hash list forw pointer */
	struct	buf	*b_back;	/* hash list back pointer */

	struct	buf	*av_forw;	/* free list forw pointer */
	struct	buf	*av_back;	/* free list back pointer */

	int		b_length;	/* # of entries on free list */
	kmutex_t	b_lock;		/* lock to protect this structure */
};


/*
 * The delayed list pointer entries should match with the buf strcuture.
 */
struct	dwbuf {
	int	b_flags;		/* not used */

	struct	buf	*b_forw;	/* not used */
	struct	buf	*b_back;	/* not used */

	struct	buf	*av_forw;	/* delayed write forw pointer */
	struct	buf	*av_back;	/* delayed write back pointer */
};


/*
 * Unlink a buffer from the available (free or delayed write) list and mark
 * it busy (internal interface).
 */
#define	notavail(bp) \
{\
	ASSERT(SEMA_HELD(&bp->b_sem)); \
	ASSERT((bp)->av_forw != NULL); \
	ASSERT((bp)->av_back != NULL); \
	ASSERT((bp)->av_forw != (bp)); \
	ASSERT((bp)->av_back != (bp)); \
	(bp)->av_back->av_forw = (bp)->av_forw; \
	(bp)->av_forw->av_back = (bp)->av_back; \
	(bp)->b_flags |= B_BUSY; \
	(bp)->av_forw = (bp)->av_back = NULL; \
}

#if defined(_KERNEL)
/*
 * Macros to avoid the extra function call needed for binary compat.
 *
 * B_RETRYWRI is not included in clear_flags for BWRITE(), BWRITE2(),
 * or brwrite() so that the retry operation is persistent until the
 * write either succeeds or the buffer is bfinval()'d.
 *
 */
#define	BREAD(dev, blkno, bsize) \
	bread_common(/* ufsvfsp */ NULL, dev, blkno, bsize)

#define	BWRITE(bp) \
	bwrite_common(/* ufsvfsp */ NULL, bp, /* force_wait */ 0, \
		/* do_relse */ 1, \
		/* clear_flags */ (B_READ | B_DONE | B_ERROR | B_DELWRI))

#define	BWRITE2(bp) \
	bwrite_common(/* ufsvfsp */ NULL, bp, /* force_wait */ 1, \
		/* do_relse */ 0, \
		/* clear_flags */ (B_READ | B_DONE | B_ERROR | B_DELWRI))

#define	GETBLK(dev, blkno, bsize) \
	getblk_common(/* ufsvfsp */ NULL, dev, blkno, bsize, /* errflg */ 0)


/*
 * Macros for new retry write interfaces.
 */

/*
 * Same as bdwrite() except write failures are retried.
 */
#define	bdrwrite(bp) { \
	(bp)->b_flags |= B_RETRYWRI; \
	bdwrite((bp)); \
}

/*
 * Same as bwrite() except write failures are retried.
 */
#define	brwrite(bp) { \
	(bp)->b_flags |= B_RETRYWRI; \
	bwrite_common((bp), /* force_wait */ 0, /* do_relse */ 1, \
		/* clear_flags */ (B_READ | B_DONE | B_ERROR | B_DELWRI)); \
}

extern struct hbuf	*hbuf;		/* Hash table */
extern struct dwbuf	*dwbuf;		/* delayed write hash table */
extern struct buf	*buf;		/* The buffer pool itself */
extern struct buf	bfreelist;	/* head of available list */

extern void (*bio_lufs_strategy)(void *, buf_t *);	/* UFS Logging */

int	bcheck(dev_t, struct buf *);
int	iowait(struct buf *);
int	hash2ints(int x, int y);
int	bio_busy(int);
int	biowait(struct buf *);
int	biomodified(struct buf *);
int	geterror(struct buf *);
void	minphys(struct buf *);
/*
 * ufsvfsp is declared as a void * to avoid having everyone that uses
 * this header file include sys/fs/ufs_inode.h.
 */
void	bwrite_common(void *ufsvfsp, struct buf *, int force_wait,
	int do_relse, int clear_flags);
void	bwrite(struct buf *);
void	bwrite2(struct buf *);
void	bdwrite(struct buf *);
void	bawrite(struct buf *);
void	brelse(struct buf *);
void	iodone(struct buf *);
void	clrbuf(struct buf *);
void	bflush(dev_t);
void	blkflush(dev_t, daddr_t);
void	binval(dev_t);
int	bfinval(dev_t, int);
void	binit(void);
void	biodone(struct buf *);
void	bioinit(struct buf *);
void	biofini(struct buf *);
void	bp_mapin(struct buf *);
void	bp_mapout(struct buf *);
void	bp_init(size_t, uint_t);
int	bp_color(struct buf *);
void	pageio_done(struct buf *);
struct buf *bread(dev_t, daddr_t, long);
struct buf *bread_common(void *, dev_t, daddr_t, long);
struct buf *breada(dev_t, daddr_t, daddr_t, long);
struct buf *getblk(dev_t, daddr_t, long);
struct buf *getblk_common(void *, dev_t, daddr_t, long, int);
struct buf *ngeteblk(long);
struct buf *geteblk(void);
struct buf *pageio_setup(struct page *, size_t, struct vnode *, int);
void bioerror(struct buf *bp, int error);
void bioreset(struct buf *bp);
struct buf *bioclone(struct buf *, off_t, size_t, dev_t, daddr_t,
	int (*)(struct buf *), struct buf *, int);
size_t	biosize(void);
#endif	/* defined(_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_BUF_H */
