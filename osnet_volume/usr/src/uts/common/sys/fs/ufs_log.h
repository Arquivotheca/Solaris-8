/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_FS_LUFS_H
#define	_SYS_FS_LUFS_H

#pragma ident	"@(#)ufs_log.h	1.43	99/12/05 SMI"

#include <sys/buf.h>
#include <sys/fs/ufs_trans.h>
#include <sys/fs/ufs_filio.h>
#include <sys/fs/ufs_inode.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct lufs_save {
	buf_t		*sv_bp;
	size_t		sv_nb_left;
	int		sv_error;
} lufs_save_t;

typedef struct lufs_buf {
	buf_t		lb_buf;
	void		*lb_ptr;
} lufs_buf_t;

/*
 * Log space is stored as extents
 */
#define	LUFS_EXTENTS	(UINT32_C(0))
#define	LS_SECTORS	2

typedef struct extent {
	uint32_t	lbno;	/* Logical block # within the space */
	uint32_t	pbno;	/* Physical block number of extent. */
				/*   UFS cannot handle more than 32 bit */
				/*   block numbers. */
	uint32_t	nbno;	/* # blocks in this extent */
} extent_t;

typedef struct extent_block {
	uint32_t	type;		/* Set to LUFS_EXTENTS to identify */
					/*   structure on disk. */
	int32_t		chksum;		/* Checksum over entire block. */
	uint32_t	nextents;	/* Size of extents array. */
	uint32_t	nbytes;		/* # bytes mapped by extent_block. */
	uint32_t	nextbno;	/* blkno of next extent_block. */
	extent_t	extents[1];
} extent_block_t;

/*
 * Don't size the incore buffers too small or too large
 */
#define	LDL_MINTRANSFER		(UINT32_C(32768))	/* 32 k */
#define	LDL_MAXTRANSFER		(UINT32_C(1048576))	/* 1 M */

/*
 * LDL_DIVISOR (ldl_divisor) is the number to calculate the log size
 * from the file system size according to the calculation in lufs_enable()
 */
#define	LDL_DIVISOR		1024 /* 1024 gives 1MB per 1GB */

/*
 * But set reasonable min/max units
 *   BUT never set LDL_MAXLOGSIZE to greater than LDL_REALMAXLOGSIZE.  The
 *   scan code will break (See sect_trailer).
 */
#define	LDL_MINLOGSIZE		(1024 * 1024)
#define	LDL_MAXLOGSIZE		(64 * 1024 * 1024)

#define	LDL_MINBUFSIZE		(32 * 1024)
#define	LDL_USABLE_BSIZE	(DEV_BSIZE - sizeof (sect_trailer_t))
#define	NB_LEFT_IN_SECTOR(off) 	(LDL_USABLE_BSIZE - ((off) - dbtob(btodb(off))))

typedef struct cirbuf {
	buf_t		*cb_bp;		/* buf's with space in circular buf */
	buf_t		*cb_dirty;	/* filling this buffer for log write */
	buf_t		*cb_free;	/* free bufs list */
	caddr_t		cb_va;		/* address of circular buffer */
	size_t		cb_nb;		/* size of circular buffer */
	krwlock_t	cb_rwlock;	/* r/w lock to protect list mgmt. */
} cirbuf_t;

#define	LUFS_VERSION		(UINT32_C(1))	/* Version 1 */
#define	LUFS_VERSION_LATEST	LUFS_VERSION

/*
 * The old Disksuite unit structure has been split into two parts -- the
 * incore part which is created at run time and the ondisk structure.  To
 * minimize code changes, the incore structure retains the old name,
 * ml_unit_t and the ondisk structure is called ml_odunit_t.  The ondisk
 * structure is stored at the beginning of the log.
 *
 * This structure must fit into a sector (512b)
 *
 */
typedef struct ml_odunit {
	uint32_t	od_version;	/* version number */
	uint32_t	od_badlog;	/* is the log okay? */
	uint32_t	od_logalloc;	/* block # of log allocation block. */

	/*
	 * Important constants
	 */
	uint32_t	od_maxtransfer;	/* max transfer in bytes */
	uint32_t	od_devbsize;	/* device bsize */
	int32_t		od_bol_lof;	/* byte offset to begin of log */
	int32_t		od_eol_lof;	/* byte offset to end of log */

	/*
	 * The disk space is split into state, prewrite, and circular log
	 */
	uint32_t	od_requestsize;	/* size requested by user */
	uint32_t	od_statesize;	/* size of state area in bytes */
	uint32_t	od_logsize;	/* size of log area in bytes */
	int32_t		od_statebno;	/* first block of state area */
	int32_t		od_logbno;	/* first block of log area */

	/*
	 * Head and tail of log
	 */
	int32_t		od_head_lof;	/* byte offset of head */
	uint32_t	od_head_ident;	/* head sector id # */
	int32_t		od_tail_lof;	/* byte offset of tail */
	uint32_t	od_tail_ident;	/* tail sector id # */
	uint32_t	od_chksum;	/* checksum to verify ondisk contents */

	/*
	 * Used for error recovery
	 */
	uint32_t	od_head_tid;	/* used for logscan; set at sethead */

	/*
	 * Debug bits
	 */
	int32_t		od_debug;

	/*
	 * Misc
	 */
	struct timeval	od_timestamp;	/* time of last state change */
} ml_odunit_t;

typedef struct ml_unit {
	struct ml_unit	*un_next;	/* next incore log */
	int		un_flags;	/* Incore state */
	buf_t		*un_bp;		/* contains memory for un_ondisk */
	void		*un_ufsvfs;	/* backpointer to ufsvfs */
	dev_t		un_dev;		/* for convenience */
	extent_block_t	*un_ebp;	/* block of extents */
	size_t		un_nbeb;	/* # bytes used by *un_ebp */
	struct mt_map	*un_deltamap;	/* deltamap */
	struct mt_map	*un_udmap;	/* userdata map */
	struct mt_map	*un_logmap;	/* logmap includes moby trans stuff */
	struct mt_map	*un_matamap;	/* optional - matamap */

	/*
	 * Used for managing transactions
	 */
	uint32_t	un_maxresv;	/* maximum reservable space */
	uint32_t	un_resv;	/* reserved byte count for this trans */
	uint32_t	un_resv_wantin;	/* reserved byte count for next trans */

	/*
	 * Used during logscan
	 */
	uint32_t	un_tid;

	/*
	 * Read/Write Buffers
	 */
	cirbuf_t	un_rdbuf;	/* read buffer space */
	cirbuf_t	un_wrbuf;	/* write buffer space */

	/*
	 * Interface to UFS
	 */
	struct ufstrans	*un_ut;		/* ufstrans struct */

	/*
	 * Ondisk state
	 */
	ml_odunit_t	un_ondisk;	/* ondisk log information */

	/*
	 * locks
	 */
	kmutex_t	un_log_mutex;	/* allows one log write at a time */
	kmutex_t	un_state_mutex;	/* only 1 state update at a time */
} ml_unit_t;

/*
 * Macros to allow access to the ondisk elements via the ml_unit_t incore
 * structure.
 */

#define	un_version	un_ondisk.od_version
#define	un_logalloc	un_ondisk.od_logalloc
#define	un_badlog	un_ondisk.od_badlog
#define	un_maxtransfer	un_ondisk.od_maxtransfer
#define	un_devbsize	un_ondisk.od_devbsize
#define	un_bol_lof	un_ondisk.od_bol_lof
#define	un_eol_lof	un_ondisk.od_eol_lof
#define	un_statesize	un_ondisk.od_statesize
#define	un_logsize	un_ondisk.od_logsize
#define	un_statebno	un_ondisk.od_statebno
#define	un_logbno	un_ondisk.od_logbno
#define	un_requestsize	un_ondisk.od_requestsize
#define	un_head_lof	un_ondisk.od_head_lof
#define	un_head_ident	un_ondisk.od_head_ident
#define	un_tail_lof	un_ondisk.od_tail_lof
#define	un_tail_ident	un_ondisk.od_tail_ident
#define	un_chksum	un_ondisk.od_chksum
#define	un_head_tid	un_ondisk.od_head_tid
#define	un_debug	un_ondisk.od_debug
#define	un_timestamp	un_ondisk.od_timestamp

/*
 *	un_flags
 */
#define	LDL_SCAN	0x0001	/* log scan in progress */
#define	LDL_ERROR	0x0002	/* in error state */

typedef struct sect_trailer {
	uint32_t	st_tid;		/* transaction id */
	uint32_t	st_ident;	/* unique sector id */
} sect_trailer_t;

/*
 * map block
 */
#define	MAPBLOCKSIZE	(8192)
#define	MAPBLOCKSHIFT	(13)
#define	MAPBLOCKOFF	(MAPBLOCKSIZE-1)
#define	MAPBLOCKMASK	(~MAPBLOCKOFF)

/*
 * delta header
 */
struct delta {
	int64_t		d_mof;	/* byte offset on device to start writing */
				/*   delta */
	int32_t		d_nb;	/* # bytes in the delta */
	delta_t 	d_typ;	/* Type of delta.  Defined in ufs_trans.h */
};
/*
 * common map entry
 */
typedef struct mapentry	mapentry_t;
struct mapentry {
	/*
	 * doubly linked list of all mapentries in map -- MUST BE FIRST
	 */
	mapentry_t	*me_next;
	mapentry_t	*me_prev;

	mapentry_t	*me_hash;
	mapentry_t	*me_agenext;
	mapentry_t	*me_cancel;
	int		(*me_func)();
	ulong_t		me_arg;
	off_t		me_lof;
	ulong_t		me_flags;
	uint32_t	me_tid;
	ulong_t		me_age;
	struct delta	me_delta;
};

#define	me_mof	me_delta.d_mof
#define	me_nb	me_delta.d_nb
#define	me_dt	me_delta.d_typ

/*
 * me_flags
 */
#define	ME_FREE		(0x0001)	/* on free   list */
#define	ME_HASH		(0x0002)	/* on hash   list */
#define	ME_CANCEL	(0x0004)	/* on cancel list */
#define	ME_AGE		(0x0008)	/* on age    list */
#define	ME_LIST		(0x0010)	/* on list   list */
#define	ME_ROLL		(0x0020)	/* on pseudo-roll list */

/*
 * MAP TYPES
 */
enum maptypes	{
	deltamaptype, udmaptype, logmaptype, matamaptype
};

/*
 * MAP
 */
#define	DELTAMAP_NHASH	(512)
#define	LOGMAP_NHASH	(2048)
#define	MAP_INDEX(mof, mtm) \
	(((mof) >> MAPBLOCKSHIFT) & (mtm->mtm_nhash-1))
#define	MAP_HASH(mof, mtm) \
	(mtm->mtm_hash + MAP_INDEX(mof, mtm))

typedef struct mt_map {
	/*
	 * anchor doubly linked list this map's entries -- MUST BE FIRST
	 */
	mapentry_t	*mtm_next;
	mapentry_t	*mtm_prev;

	enum maptypes	mtm_type;	/* map type */
	int		mtm_flags;	/* generic flags */
	int		mtm_ref;	/* PTE like ref bit */
	ulong_t		mtm_debug;	/* set at create time */
	ulong_t		mtm_age;	/* mono-inc; tags mapentries */
	mapentry_t	*mtm_cancel;	/* to be canceled at commit */
	ulong_t		mtm_nhash;	/* # of hash anchors */
	mapentry_t	**mtm_hash;	/* array of singly linked lists */
	struct topstats	*mtm_tops;	/* trans ops - enabled by an ioctl */
	long		mtm_nme;	/* # of mapentries */
	long		mtm_nmet;	/* # of mapentries this transaction */
	long		mtm_nud;	/* # of active userdata writes */
	long		mtm_nsud;	/* # of userdata scanned deltas */

	/*
	 * used after logscan to set the log's tail
	 */
	off_t		mtm_tail_lof;
	size_t		mtm_tail_nb;

	/*
	 * debug field for Scan test
	 */
	off_t		mtm_trimlof;	/* log was trimmed to this lof */
	off_t		mtm_trimtail;	/* tail lof before trimming */
	off_t		mtm_trimalof;	/* lof of last allocation delta */
	off_t		mtm_trimclof;	/* lof of last commit delta */
	off_t		mtm_trimrlof;	/* lof of last rolled delta */
	ml_unit_t	*mtm_ul;	/* log unit for this map */

	/*
	 * moby trans stuff
	 */
	uint32_t		mtm_tid;
	uint32_t		mtm_committid;
	ushort_t		mtm_closed;
	ushort_t		mtm_seq;
	long			mtm_wantin;
	long			mtm_active;
	long			mtm_activesync;
	ulong_t			mtm_dirty;
	kmutex_t		mtm_lock;
	kcondvar_t		mtm_cv_commit;
	kcondvar_t		mtm_cv_next;
	kcondvar_t		mtm_cv_eot;

	/*
	 * mutex that protects all the fields in mt_map except
	 * mtm_mapnext and mtm_refcnt
	 */
	kmutex_t	mtm_mutex;
	kcondvar_t	mtm_cv;		/* generic conditional */

	/*
	 * rw lock for the mapentry fields agenext and locnext
	 */
	krwlock_t	mtm_rwlock;
	/*
	 * DEBUG: runtestscan
	 */
	kmutex_t	mtm_scan_mutex;
} mt_map_t;
/*
 * mtm_flags
 */
#define	MTM_ROLL_EXIT		(0x00000001)
#define	MTM_ROLL_RUNNING	(0x00000002)
#define	MTM_FORCE_ROLL		(0x00000004)

/*
 * Generic range checking macros
 */
#define	OVERLAP(sof, snb, dof, dnb) \
	((sof >= dof && sof < (dof + dnb)) || \
	(dof >= sof && dof < (sof + snb)))

#define	WITHIN(sof, snb, dof, dnb) ((sof >= dof) && ((sof+snb) <= (dof+dnb)))

#define	DATAoverlapME(mof, hnb, me) (OVERLAP(mof, hnb, me->me_mof, me->me_nb))
#define	MEwithinDATA(me, mof, hnb) (WITHIN(me->me_mof, me->me_nb, mof, hnb))
#define	DATAwithinME(mof, hnb, me) (WITHIN(mof, hnb, me->me_mof, me->me_nb))

/*
 * TRANSACTION OPS STATS
 */
struct topstats {
	ulong_t		mtm_top_num[TOP_MAX];
	ulong_t		mtm_top_size_etot[TOP_MAX];
	ulong_t		mtm_top_size_rtot[TOP_MAX];
	ulong_t		mtm_top_size_max[TOP_MAX];
	ulong_t		mtm_top_size_min[TOP_MAX];
	ulong_t		mtm_delta_num[DT_MAX];
};

/*
 * prewrite info (per buf); stored as array at beginning of prewrite area
 */
typedef struct prewrite {
	uint32_t pw_bufsize;	/* every buffer is this size */
	int32_t	pw_blkno;	/* block number */
	uint16_t pw_secmap;	/* bitmap -- 1 = write this sector in the buf */
	uint16_t pw_flags;
} prewrite_t;
/*
 * pw_flags
 */
#define	PW_INUSE	0x0001	/* this prewrite buf is in use */
#define	PW_WAIT		0x0002	/* write in progress; wait for completion */
#define	PW_REM		0x0004	/* remove deltas */

/*
 * un_debug
 *	MT_TRANSACT		- keep per thread accounting of tranactions
 *	MT_MATAMAP		- double check deltas and ops against matamap
 *	MT_WRITE_CHECK		- check master+deltas against metadata write
 *	MT_LOG_WRITE_CHECK	- read after write for log writes
 *	MT_CHECK_MAP		- check map after every insert/delete
 *	MT_TRACE		- trace transactions (used with MT_TRANSACT)
 *	MT_SIZE			- fail on size errors (used with MT_TRANSACT)
 *	MT_NOASYNC		- force every op to be sync
 *	MT_FORCEROLL		- forcibly roll the log after every commit
 *	MT_SCAN			- running runtestscan; special case as needed
 *	MT_PREWRITE		- process prewrite area every roll
 */
#define	MT_NONE			(0x00000000)
#define	MT_TRANSACT		(0x00000001)
#define	MT_MATAMAP		(0x00000002)
#define	MT_WRITE_CHECK		(0x00000004)
#define	MT_LOG_WRITE_CHECK	(0x00000008)
#define	MT_CHECK_MAP		(0x00000010)
#define	MT_TRACE		(0x00000020)
#define	MT_SIZE			(0x00000040)
#define	MT_NOASYNC		(0x00000080)
#define	MT_FORCEROLL		(0x00000100)
#define	MT_SCAN			(0x00000200)
#define	MT_PREWRITE		(0x00000400)


#ifdef _KERNEL

/*
 * UFS to LUFS OPS
 *	ufs_log module sets entry points when loaded
 */
typedef struct lufsops {
	int	(*lufs_enable)(vnode_t *, struct fiolog *, cred_t *);
	int	(*lufs_disable)(vnode_t *, struct fiolog *);
	int	(*lufs_snarf)(ufsvfs_t *, struct fs *, int);
	void	(*lufs_unsnarf)(ufsvfs_t *);
	void	(*lufs_empty)(ufsvfs_t *);
	void	(*lufs_strategy)(ml_unit_t *, buf_t *);
} lufsops_t;
/*
 * NOTE:	the following "pragma weak" was implemented to eliminate
 *		an unresolved external for genunix.  when "bio.c" attempts
 *		to use the "LUFS_STRATEGY" macro below.  All occurances are
 *		wrapped with a conditional insuring the ufs_logging is enabled.
 */
extern lufsops_t lufsops;
#pragma weak	lufsops

#define	LUFS_ENABLE(vp, fl, cr, error) \
	error = (*lufsops.lufs_enable)(vp, fl, cr)

#define	LUFS_DISABLE(vp, fl, error) \
	error = (*lufsops.lufs_disable)(vp, fl)

#define	LUFS_SNARF(ufsvfsp, fs, ronly, error) \
	error = (*lufsops.lufs_snarf)(ufsvfsp, fs, ronly)

#define	LUFS_UNSNARF(ufsvfsp) \
	(*lufsops.lufs_unsnarf)(ufsvfsp)

#define	LUFS_EMPTY(ufsvfsp) \
	(*lufsops.lufs_empty)(ufsvfsp)

#define	LUFS_STRATEGY(ul, bp) \
	(*lufsops.lufs_strategy)(ul, bp)

/*
 * Log layer protos -- lufs_log.c
 */
extern void		ldl_strategy(ml_unit_t *, buf_t *);
extern void		ldl_round_commit(ml_unit_t *);
extern void		ldl_push_commit(ml_unit_t *);
extern int		ldl_need_commit(ml_unit_t *);
extern int		ldl_has_space(ml_unit_t *, mapentry_t *);
extern void		ldl_write(ml_unit_t *, caddr_t, offset_t, mapentry_t *);
extern void		ldl_waito(ml_unit_t *);
extern int		ldl_read(ml_unit_t *, caddr_t, offset_t, off_t,
					mapentry_t *);
extern void		ldl_sethead(ml_unit_t *, off_t, uint32_t);
extern void		ldl_settail(ml_unit_t *, off_t, size_t);
extern ulong_t		ldl_logscan_nbcommit(off_t);
extern int		ldl_logscan_read(ml_unit_t *, off_t *, size_t, caddr_t);
extern void		ldl_logscan_begin(ml_unit_t *);
extern void		ldl_logscan_end(ml_unit_t *);
extern int		ldl_need_roll(ml_unit_t *);
extern int		ldl_empty(ml_unit_t *);
extern void		ldl_seterror(ml_unit_t *, char *);
extern size_t		ldl_bufsize(ml_unit_t *);
extern void		ldl_savestate(ml_unit_t *);
extern void		free_cirbuf(cirbuf_t *);
extern void		alloc_rdbuf(cirbuf_t *, size_t, size_t);
extern void		alloc_wrbuf(cirbuf_t *, size_t);

/*
 * trans driver layer -- lufs.c
 */
extern int		trans_not_wait(struct buf *cb);
extern int		trans_not_done(struct buf *cb);
extern int		trans_wait(struct buf *cb);
extern int		trans_done(struct buf *cb);
extern void		lufs_strategy(ml_unit_t *, buf_t *);
extern ml_unit_t	*lufs_getlog(dev_t dev);

/*
 * transaction op layer -- lufs_top.c
 */
extern void	_init_top(void);
extern void	top_begin_sync(struct ufstrans *, top_t, ulong_t);
extern void	top_end_sync(struct ufstrans *, int *, top_t, ulong_t);
extern void	top_read_roll(struct buf *, ml_unit_t *, ushort_t *);
extern void	top_snarf(ml_unit_t *);
extern void	top_unsnarf(ml_unit_t *);

/*
 * map layer -- lufs_map.c
 */
extern void		map_free_entries(mt_map_t *);
extern int		matamap_overlap(mt_map_t *, offset_t, off_t);
extern int		matamap_within(mt_map_t *, offset_t, off_t);
extern int		deltamap_need_commit(mt_map_t *);
extern void		deltamap_add(mt_map_t *, offset_t, off_t, delta_t,
				int (*)(), ulong_t);
extern mapentry_t	*deltamap_remove(mt_map_t *, offset_t, off_t);
extern void		deltamap_del(mt_map_t *, offset_t, off_t);
extern void		deltamap_push(ml_unit_t *);
extern int		logmap_need_commit(mt_map_t *);
extern int		logmap_need_roll_async(mt_map_t *);
extern int		logmap_need_roll_sync(mt_map_t *);
extern int		logmap_need_roll(mt_map_t *);
extern void		logmap_start_roll(ml_unit_t *);
extern void		logmap_kill_roll(ml_unit_t *);
extern void		logmap_forceroll(mt_map_t *);
extern void		logmap_start_sync();
extern int		logmap_overlap(mt_map_t *, offset_t, off_t);
extern void		logmap_remove_roll(mt_map_t *, offset_t, off_t);
extern int		logmap_next_roll(mt_map_t *, offset_t *);
extern void		logmap_list_get(mt_map_t *, offset_t, off_t,
				mapentry_t **);
extern void		logmap_list_get_roll(mt_map_t *, offset_t, off_t,
				mapentry_t **);
extern void		logmap_list_put(mt_map_t *, mapentry_t *);
extern void		logmap_read_mstr(ml_unit_t *, struct buf *);
extern void		logmap_secmap_roll(mapentry_t *, offset_t, ushort_t *);
extern void		logmap_make_space(struct mt_map *, ml_unit_t *,
				mapentry_t *);
extern void		logmap_add(ml_unit_t *, char *, offset_t,
				mapentry_t *);
extern void		logmap_add_ud(ml_unit_t *, char *, offset_t,
				mapentry_t *);
extern void		logmap_commit(ml_unit_t *);
extern void		logmap_sethead(mt_map_t *, ml_unit_t *);
extern void		logmap_roll_dev(ml_unit_t *ul);
extern void		logmap_roll_sud(mt_map_t *, ml_unit_t *ul,
				offset_t, off_t);
extern int		logmap_ud_done(struct buf *);
extern void		logmap_ud_wait();
extern void		logmap_cancel(ml_unit_t *, offset_t, off_t);
extern int		logmap_iscancel(mt_map_t *, offset_t, off_t);
extern void		logmap_logscan(ml_unit_t *);
extern mt_map_t		*map_put(mt_map_t *);
extern mt_map_t		*map_get(ml_unit_t *, enum maptypes, int);
extern void		_init_map(void);

/*
 * scan and roll threads -- lufs_thread.c
 */
extern void	trans_roll(ml_unit_t *);

/*
 * ufs_log layer -- ufs_log.c
 */
void	lufs_unload();

/*
 * DEBUG
 */
#ifdef	DEBUG
extern int	map_put_debug(mt_map_t *);
extern int	map_get_debug(ml_unit_t *, mt_map_t *);
extern int	top_write_debug(ml_unit_t *, mapentry_t *, offset_t, off_t);
extern int	top_check_debug(char *, offset_t, off_t, ml_unit_t *);
extern int	matamap_overlap(mt_map_t *, offset_t, off_t);
extern int	ldl_sethead_debug(ml_unit_t *);
extern int	map_check_linkage(mt_map_t *);
extern int	logmap_logscan_debug(mt_map_t *, mapentry_t *);
extern int	map_check_ldl_write(ml_unit_t *, caddr_t, offset_t,
								mapentry_t *);
extern int	logmap_logscan_commit_debug(off_t, mt_map_t *);
extern int	logmap_logscan_add_debug(struct delta *, mt_map_t *);
extern int	top_delta_debug(ml_unit_t *, offset_t, off_t, delta_t);
extern int	top_begin_debug(ml_unit_t *, top_t, ulong_t);
extern int	top_end_debug_1(ml_unit_t *, mt_map_t *, top_t, ulong_t);
extern int	top_roll_debug(ml_unit_t *);
extern int	top_end_debug_2(void);
extern int	top_snarf_debug(ml_unit_t *, struct ufstransops **,
							struct ufstransops *);
extern int	top_init_debug(void);
extern int	lufs_initialize_debug(ml_odunit_t *);
extern void	*trans_zalloc(size_t);
extern void	*trans_alloc(size_t);
extern void	*trans_zalloc_nosleep(size_t);
extern void	*trans_alloc_nosleep(size_t);
extern void	trans_free(void *, size_t);
#else	DEBUG
#define	trans_alloc(nb)			kmem_alloc(nb, KM_SLEEP)
#define	trans_zalloc(nb)		kmem_zalloc(nb, KM_SLEEP)
#define	trans_alloc_nosleep(nb)		kmem_alloc(nb, KM_NOSLEEP)
#define	trans_zalloc_nosleep(nb)	kmem_zalloc(nb, KM_NOSLEEP)
#define	trans_free(va, nb)		kmem_free(va, nb)
#endif	DEBUG

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FS_LUFS_H */
