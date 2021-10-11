/*
 * Copyright (c) 1989-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lufs.c	1.57	99/11/29 SMI"

#include <sys/systm.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_filio.h>
#include <sys/sysmacros.h>
#include <sys/modctl.h>
#include <sys/fs/ufs_log.h>
#include <sys/fs/ufs_bio.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/inttypes.h>
#include <sys/vfs.h>
#include <sys/mntent.h>
#include <sys/conf.h>
#include <sys/param.h>

static kmutex_t	log_mutex;	/* general purpose log layer lock */
kmutex_t	ml_scan;	/* Scan thread syncronization */
kcondvar_t	ml_scan_cv;	/* Scan thread syncronization */

struct kmem_cache	*lufs_sv;
struct kmem_cache	*lufs_bp;

/* Adjustable constants */
uint_t		ldl_maxlogsize	= LDL_MAXLOGSIZE;
uint_t		ldl_minlogsize	= LDL_MINLOGSIZE;
uint32_t	ldl_divisor	= LDL_DIVISOR;
uint32_t	ldl_mintransfer	= LDL_MINTRANSFER;
uint32_t	ldl_maxtransfer	= LDL_MAXTRANSFER;
uint32_t	ldl_minbufsize	= LDL_MINBUFSIZE;

int
trans_not_done(struct buf *cb)
{
	sema_v(&cb->b_io);
	return (0);
}

static void
trans_wait_panic(struct buf *cb)
{
	while ((cb->b_flags & B_DONE) == 0)
		drv_usecwait(10);
}

int
trans_not_wait(struct buf *cb)
{
	/*
	 * In case of panic, busy wait for completion
	 */
	if (panicstr)
		trans_wait_panic(cb);
	else
		sema_p(&cb->b_io);

	return (geterror(cb));
}

int
trans_wait(struct buf *cb)
{
	/*
	 * In case of panic, busy wait for completion and run md daemon queues
	 */
	if (panicstr)
		trans_wait_panic(cb);
	return (biowait(cb));
}

int	nlogs;

static void
lufs_empty(ufsvfs_t *ufsvfsp)
{
	if (ufsvfsp->vfs_log)
		logmap_roll_dev(ufsvfsp->vfs_log);
}

ml_unit_t	*logs;
ml_unit_t *
lufs_getlog(dev_t dev)
{
	ml_unit_t		*ul;

	mutex_enter(&log_mutex);
	for (ul = logs; ul != NULL; ul = ul->un_next)
		if (ul->un_dev == dev)
			break;
	mutex_exit(&log_mutex);

	return (ul);
}

static void
lufs_addlog(ml_unit_t *ul)
{
	mutex_enter(&log_mutex);
	ul->un_next = logs;
	logs = ul;
	++nlogs;
	mutex_exit(&log_mutex);
}

static void
lufs_dellog(ufsvfs_t *ufsvfsp)
{
	ml_unit_t	**pp	= &logs;

	mutex_enter(&log_mutex);
	while (*pp) {
		if ((*pp)->un_ufsvfs == ufsvfsp) {
			*pp = (*pp)->un_next;
			--nlogs;
			mutex_exit(&log_mutex);
			return;
		}
		pp = &(*pp)->un_next;
	}
	mutex_exit(&log_mutex);
}

static void
setsum(int32_t *sp, int32_t *lp, int nb)
{
	int32_t csum = 0;

	*sp = 0;
	nb /= sizeof (int32_t);
	while (nb--)
		csum += *lp++;
	*sp = csum;
}

static int
checksum(int32_t *sp, int32_t *lp, int nb)
{
	int32_t ssum = *sp;

	setsum(sp, lp, nb);
	if (ssum != *sp) {
		*sp = ssum;
		return (0);
	}
	return (1);
}

static void
lufs_unsnarf(ufsvfs_t *ufsvfsp)
{
	ml_unit_t *ul;

	ul = ufsvfsp->vfs_log;
	if (ul == NULL)
		return;

	/* Roll committed transactions */
	logmap_roll_dev(ul);

	/* Kill the roll thread */
	logmap_kill_roll(ul);

	/* Remove our ops table from UFS's list of trans ops structs */
	top_unsnarf(ul);

	/* remove log struct from global linked list */
	lufs_dellog(ufsvfsp);

	/* release saved alloction info */
	if (ul->un_ebp)
		trans_free(ul->un_ebp, ul->un_nbeb);

	/* release circular bufs */
	free_cirbuf(&ul->un_rdbuf);
	free_cirbuf(&ul->un_wrbuf);

	/* release maps */
	if (ul->un_logmap)
		ul->un_logmap = map_put(ul->un_logmap);
	if (ul->un_deltamap)
		ul->un_deltamap = map_put(ul->un_deltamap);
	if (ul->un_udmap)
		ul->un_udmap = map_put(ul->un_udmap);
	if (ul->un_matamap)
		ul->un_matamap = map_put(ul->un_matamap);

	mutex_destroy(&ul->un_log_mutex);
	mutex_destroy(&ul->un_state_mutex);

	/* release state buffer MUST BE LAST!! (contains our ondisk data) */
	if (ul->un_bp)
		brelse(ul->un_bp);
	trans_free(ul, sizeof (*ul));

	ufsvfsp->vfs_log = NULL;
}

static int
lufs_snarf(ufsvfs_t *ufsvfsp, struct fs *fs, int ronly)
{
	buf_t		*bp, *tbp;
	ml_unit_t	*ul;
	extent_block_t *ebp, *nebp;
	size_t		nb;
	daddr_t		bno;

	/* LINTED: warning: logical expression always true: op "||" */
	ASSERT(sizeof (ml_odunit_t) < DEV_BSIZE);

	/*
	 * Get the allocation table
	 *	During a remount the superblock pointed to by the ufsvfsp
	 *	is out of date.  Hence the need for the ``new'' superblock
	 *	pointer, fs, passed in as a parameter.
	 */
	bp = UFS_BREAD(ufsvfsp, ufsvfsp->vfs_dev, fs->fs_logbno, fs->fs_bsize);
	if (bp->b_flags & B_ERROR) {
		brelse(bp);
		return (EIO);
	}
	ebp = (void *)bp->b_un.b_addr;
	if (!checksum(&ebp->chksum, (int32_t *)bp->b_un.b_addr,
		fs->fs_bsize)) {
		brelse(bp);
		return (ENODEV);
	}
	if (ebp->type != LUFS_EXTENTS) {
		brelse(bp);
		return (EDOM);
	}
	/*
	 * Put allocation into memory
	 */
	nb = (size_t)(sizeof (extent_block_t) +
			((ebp->nextents - 1) * sizeof (extent_t)));
	nebp = (void *)trans_alloc(nb);
	bcopy(ebp, nebp, nb);
	brelse(bp);

	/*
	 * Get the log state
	 */
	bno = (daddr_t)nebp->extents[0].pbno;
	bp = UFS_BREAD(ufsvfsp, ufsvfsp->vfs_dev, bno, DEV_BSIZE);
	if (bp->b_flags & B_ERROR) {
		brelse(bp);
		bp = UFS_BREAD(ufsvfsp, ufsvfsp->vfs_dev, bno + 1, DEV_BSIZE);
		if (bp->b_flags & B_ERROR) {
			brelse(bp);
			trans_free(nebp, nb);
			return (EIO);
		}
	}

	/*
	 * Put ondisk struct into an anonymous buffer
	 *	This buffer will contain the memory for the ml_odunit struct
	 */
	tbp = ngeteblk(dbtob(LS_SECTORS));
	tbp->b_edev = bp->b_edev;
	tbp->b_dev = bp->b_dev;
	tbp->b_blkno = bno;
	bcopy(bp->b_un.b_addr, tbp->b_un.b_addr, DEV_BSIZE);
	bcopy(bp->b_un.b_addr, tbp->b_un.b_addr + DEV_BSIZE, DEV_BSIZE);
	bp->b_flags |= (B_STALE | B_AGE);
	brelse(bp);
	bp = tbp;

	/*
	 * Verify the log state
	 *
	 * read/only mounts w/bad logs are allowed.  umount will
	 * eventually roll the bad log until the first IO error.
	 * fsck will then repair the file system.
	 *
	 * read/write mounts with bad logs are not allowed.
	 *
	 */
	ul = (ml_unit_t *)trans_zalloc(sizeof (*ul));
	bcopy(bp->b_un.b_addr, &ul->un_ondisk, sizeof (ml_odunit_t));
	if ((ul->un_chksum != ul->un_head_ident + ul->un_tail_ident) ||
	    (ul->un_version != LUFS_VERSION_LATEST) ||
	    (!ronly && ul->un_badlog)) {
		trans_free(ul, sizeof (*ul));
		brelse(bp);
		trans_free(nebp, nb);
		return (EIO);
	}
	/*
	 * Initialize the incore-only fields
	 */
	ul->un_bp = bp;
	ul->un_ufsvfs = ufsvfsp;
	ul->un_dev = ufsvfsp->vfs_dev;
	ul->un_ebp = nebp;
	ul->un_nbeb = nb;
	ul->un_maxresv = btodb(ul->un_logsize) * LDL_USABLE_BSIZE;
	ul->un_deltamap = map_get(ul, deltamaptype, DELTAMAP_NHASH);
	ul->un_udmap = map_get(ul, udmaptype, DELTAMAP_NHASH);
	ul->un_logmap = map_get(ul, logmaptype, LOGMAP_NHASH);
	if (ul->un_debug & MT_MATAMAP)
		ul->un_matamap = map_get(ul, matamaptype, DELTAMAP_NHASH);
	mutex_init(&ul->un_log_mutex, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&ul->un_state_mutex, NULL, MUTEX_DEFAULT, NULL);
	ufsvfsp->vfs_log = (void *)ul;
	lufs_addlog(ul);

	/* remember the state of the log before the log scan */
	logmap_logscan(ul);

	/*
	 * Inform UFS of our existence
	 */
	top_snarf(ul);

	/*
	 * Error during scan
	 *
	 * If this is a read/only mount; ignore the error.
	 * At a later time umount/fsck will repair the fs.
	 *
	 */
	if (ul->un_flags & LDL_ERROR) {
		if (!ronly) {
			lufs_unsnarf(ufsvfsp);
			return (EIO);
		}
		ul->un_flags &= ~LDL_ERROR;
	}
	logmap_start_roll(ul);
	logmap_start_sync();
	return (0);
}

static int
lufs_initialize(
	ufsvfs_t *ufsvfsp,
	daddr_t bno,
	int32_t logbno,
	size_t nb,
	struct fiolog *flp)
{
	ml_odunit_t	*ud, *ud2;
	buf_t		*bp;
	struct timeval	tv;

	/* LINTED: warning: logical expression always true: op "||" */
	ASSERT(sizeof (ml_odunit_t) < DEV_BSIZE);
	ASSERT(nb >= ldl_minlogsize);

	bp = UFS_GETBLK(ufsvfsp, ufsvfsp->vfs_dev, bno, dbtob(LS_SECTORS));
	bzero(bp->b_un.b_addr, bp->b_bcount);

	ud = (void *)bp->b_un.b_addr;
	ud->od_version = LUFS_VERSION_LATEST;
	ud->od_maxtransfer = MIN(ufsvfsp->vfs_wrclustsz, ldl_maxtransfer);
	if (ud->od_maxtransfer < ldl_mintransfer)
		ud->od_maxtransfer = ldl_mintransfer;
	ud->od_devbsize = DEV_BSIZE;

	ud->od_requestsize = flp->nbytes_actual;
	ud->od_statesize = dbtob(LS_SECTORS);
	ud->od_logsize = nb - ud->od_statesize;

	ud->od_statebno = INT32_C(0);
	ud->od_logbno = ud->od_statebno + btodb(ud->od_statesize);

	uniqtime(&tv);
	ud->od_head_ident = tv.tv_sec;
	ud->od_tail_ident = tv.tv_sec;
	ud->od_chksum = ud->od_head_ident + ud->od_tail_ident;

	ud->od_bol_lof = dbtob(ud->od_statebno) + ud->od_statesize;
	ud->od_eol_lof = ud->od_bol_lof + ud->od_logsize;
	ud->od_head_lof = ud->od_bol_lof;
	ud->od_tail_lof = ud->od_bol_lof;

	ud->od_logalloc = (uint32_t)logbno;

	ASSERT(lufs_initialize_debug(ud));

	ud2 = (void *)(bp->b_un.b_addr + DEV_BSIZE);
	bcopy(ud, ud2, sizeof (*ud));

	UFS_BWRITE2(ufsvfsp, bp);
	if (bp->b_flags & B_ERROR) {
		brelse(bp);
		return (EIO);
	}
	brelse(bp);

	return (0);
}

/*
 * Free log space
 *	Assumes the file system is write locked and is not logging
 */
static int
lufs_free(struct ufsvfs *ufsvfsp)
{
	int		error = 0, i, j;
	buf_t		*bp = NULL;
	extent_t	*ep;
	extent_block_t	*ebp;
	struct fs	*fs = ufsvfsp->vfs_fs;
	daddr_t		fno;
	int32_t		logbno, reclaim;
	long		nfno;
	inode_t		*ip = NULL;
	char		clean;

	/*
	 * Nothing to free
	 */
	if (fs->fs_logbno == 0)
		return (0);

	/*
	 * Mark the file system as FSACTIVE, no log, and no reclaim
	 */
	ufsvfsp->vfs_ulockfs.ul_sbowner = curthread;
	mutex_enter(&ufsvfsp->vfs_lock);
	clean = fs->fs_clean;
	reclaim = fs->fs_reclaim;
	logbno = fs->fs_logbno;
	fs->fs_clean = FSACTIVE;
	fs->fs_reclaim = 0;
	fs->fs_logbno = INT32_C(0);
	ufs_sbwrite(ufsvfsp);
	mutex_exit(&ufsvfsp->vfs_lock);
	ufsvfsp->vfs_ulockfs.ul_sbowner = (kthread_id_t)-1;
	if (ufsvfsp->vfs_bufp->b_flags & B_ERROR) {
		error = EIO;
		fs->fs_clean = clean;
		fs->fs_reclaim = reclaim;
		fs->fs_logbno = logbno;
		goto errout;
	}

	/*
	 * fetch the allocation block
	 *	superblock -> one block of extents -> log data
	 */
	bp = UFS_BREAD(ufsvfsp, ufsvfsp->vfs_dev, logbno, fs->fs_bsize);
	if (bp->b_flags & B_ERROR) {
		error = EIO;
		goto errout;
	}

	/*
	 * Free up the allocated space (dummy inode needed for free())
	 */
	ip = ufs_alloc_inode(ufsvfsp, UFSROOTINO);
	ebp = (void *)bp->b_un.b_addr;
	for (i = 0, ep = &ebp->extents[0]; i < ebp->nextents; ++i, ++ep) {
		fno = (daddr_t)dbtofsb(fs, ep->pbno);
		nfno = dbtofsb(fs, ep->nbno);
		for (j = 0; j < nfno; j += fs->fs_frag, fno += fs->fs_frag)
			free(ip, fno, fs->fs_bsize, 0);
	}
	free(ip, dbtofsb(fs, logbno), fs->fs_bsize, 0);
	brelse(bp);
	bp = NULL;

	/*
	 * Push the metadata dirtied during the allocations
	 */
	ufsvfsp->vfs_ulockfs.ul_sbowner = curthread;
	sbupdate(ufsvfsp->vfs_vfs);
	ufsvfsp->vfs_ulockfs.ul_sbowner = (kthread_id_t)-1;
	bflush(ufsvfsp->vfs_dev);
	error = bfinval(ufsvfsp->vfs_dev, 0);
	if (error)
		goto errout;

	/*
	 * Free the dummy inode
	 */
	ufs_free_inode(ip);

	return (0);

errout:
	/*
	 * Free up all resources
	 */
	if (bp)
		brelse(bp);
	if (ip)
		ufs_free_inode(ip);
	return (error);
}

/*
 * Allocate log space
 *	Assumes the file system is write locked and is not logging
 */
static int
lufs_alloc(struct ufsvfs *ufsvfsp, struct fiolog *flp, cred_t *cr)
{
	int		error = 0;
	buf_t		*bp = NULL;
	extent_t	*ep, *nep;
	extent_block_t	*ebp;
	struct fs	*fs = ufsvfsp->vfs_fs;
	daddr_t		fno, bno;
	int32_t		logbno = INT32_C(0);
	struct inode	*ip = NULL;
	size_t		nb = flp->nbytes_actual;
	size_t		tb = 0;

	/*
	 * Mark the file system as FSACTIVE
	 */
	ufsvfsp->vfs_ulockfs.ul_sbowner = curthread;
	mutex_enter(&ufsvfsp->vfs_lock);
	fs->fs_clean = FSACTIVE;
	ufs_sbwrite(ufsvfsp);
	mutex_exit(&ufsvfsp->vfs_lock);
	ufsvfsp->vfs_ulockfs.ul_sbowner = (kthread_id_t)-1;

	/*
	 * Allocate the allocation block (need dummy shadow inode;
	 * we use a shadow inode so the quota sub-system ignores
	 * the block allocations.)
	 *	superblock -> one block of extents -> log data
	 */
	ip = ufs_alloc_inode(ufsvfsp, UFSROOTINO);
	ip->i_mode = IFSHAD;		/* make the dummy a shadow inode */
	rw_enter(&ip->i_contents, RW_WRITER);
	fno = contigpref(ufsvfsp, nb + fs->fs_bsize);
	error = alloc(ip, fno, fs->fs_bsize, &fno, cr);
	if (error)
		goto errout;
	bno = fsbtodb(fs, fno);

	bp = UFS_BREAD(ufsvfsp, ufsvfsp->vfs_dev, bno, fs->fs_bsize);
	if (bp->b_flags & B_ERROR) {
		error = EIO;
		goto errout;
	}

	ebp = (void *)bp->b_un.b_addr;
	ebp->type = LUFS_EXTENTS;
	ebp->nextbno = UINT32_C(0);
	ebp->nextents = UINT32_C(0);
	ebp->chksum = INT32_C(0);
	logbno = bno;

	/*
	 * Initialize the first extent
	 */
	ep = &ebp->extents[0];
	error = alloc(ip, fno + fs->fs_frag, fs->fs_bsize, &fno, cr);
	if (error)
		goto errout;
	bno = fsbtodb(fs, fno);

	ep->lbno = UINT32_C(0);
	ep->pbno = (uint32_t)bno;
	ep->nbno = (uint32_t)fsbtodb(fs, fs->fs_frag);
	ebp->nextents = UINT32_C(1);
	tb = fs->fs_bsize;
	nb -= fs->fs_bsize;

	while (nb) {
		error = alloc(ip, fno + fs->fs_frag, fs->fs_bsize, &fno, cr);
		if (error) {
			if (tb < ldl_minlogsize)
				goto errout;
			error = 0;
			break;
		}
		bno = fsbtodb(fs, fno);
		if ((daddr_t)(ep->pbno + ep->nbno) == bno)
			ep->nbno += (uint32_t)(fsbtodb(fs, fs->fs_frag));
		else {
			nep = ep + 1;
			if ((caddr_t)(nep + 1) >
			    (bp->b_un.b_addr + fs->fs_bsize)) {
				free(ip, fno, fs->fs_bsize, 0);
				break;
			}
			nep->lbno = ep->lbno + ep->nbno;
			nep->pbno = (uint32_t)bno;
			nep->nbno = (uint32_t)(fsbtodb(fs, fs->fs_frag));
			ebp->nextents++;
			ep = nep;
		}
		tb += fs->fs_bsize;
		nb -= fs->fs_bsize;
	}
	ebp->nbytes = (uint32_t)tb;
	setsum(&ebp->chksum, (int32_t *)bp->b_un.b_addr, fs->fs_bsize);
	UFS_BWRITE2(ufsvfsp, bp);
	if (bp->b_flags & B_ERROR) {
		error = EIO;
		goto errout;
	}
	/*
	 * Initialize the first two sectors of the log
	 */
	error = lufs_initialize(ufsvfsp, ebp->extents[0].pbno, logbno, tb, flp);
	if (error)
		goto errout;

	/*
	 * We are done initializing the allocation block and the log
	 */
	brelse(bp);
	bp = NULL;

	/*
	 * Update the superblock and push the dirty metadata
	 */
	ufsvfsp->vfs_ulockfs.ul_sbowner = curthread;
	sbupdate(ufsvfsp->vfs_vfs);
	ufsvfsp->vfs_ulockfs.ul_sbowner = (kthread_id_t)-1;
	bflush(ufsvfsp->vfs_dev);
	error = bfinval(ufsvfsp->vfs_dev, 1);
	if (error)
		goto errout;
	if (ufsvfsp->vfs_bufp->b_flags & B_ERROR) {
		error = EIO;
		goto errout;
	}

	/*
	 * Everything is safely on disk; update log space pointer in sb
	 */
	ufsvfsp->vfs_ulockfs.ul_sbowner = curthread;
	mutex_enter(&ufsvfsp->vfs_lock);
	fs->fs_logbno = (uint32_t)logbno;
	ufs_sbwrite(ufsvfsp);
	mutex_exit(&ufsvfsp->vfs_lock);
	ufsvfsp->vfs_ulockfs.ul_sbowner = (kthread_id_t)-1;

	/*
	 * Free the dummy inode
	 */
	rw_exit(&ip->i_contents);
	ufs_free_inode(ip);

	/* inform user of real log size */
	flp->nbytes_actual = tb;
	return (0);

errout:
	/*
	 * Free all resources
	 */
	if (bp)
		brelse(bp);
	if (logbno) {
		fs->fs_logbno = logbno;
		(void) lufs_free(ufsvfsp);
	}
	if (ip) {
		rw_exit(&ip->i_contents);
		ufs_free_inode(ip);
	}
	return (error);
}

/*
 * Disable logging
 */
static int
lufs_disable(vnode_t *vp, struct fiolog *flp)
{
	int		error = 0;
	inode_t		*ip = VTOI(vp);
	ufsvfs_t	*ufsvfsp = ip->i_ufsvfs;
	struct fs	*fs = ufsvfsp->vfs_fs;
	struct lockfs	lf;
	struct ulockfs	*ulp;

	flp->error = FIOLOG_ENONE;

	/*
	 * Logging is already disabled; done
	 */
	if (fs->fs_logbno == 0 || ufsvfsp->vfs_log == NULL)
		return (0);

	/*
	 * Readonly file system
	 */
	if (fs->fs_ronly) {
		flp->error = FIOLOG_EROFS;
		return (0);
	}

	/*
	 * File system must be write locked to disable logging
	 */
	error = ufs_fiolfss(vp, &lf);
	if (error) {
		return (error);
	}
	if (!LOCKFS_IS_ULOCK(&lf)) {
		flp->error = FIOLOG_EULOCK;
		return (0);
	}
	lf.lf_lock = LOCKFS_WLOCK;
	lf.lf_flags = 0;
	lf.lf_comment = NULL;
	error = ufs_fiolfs(vp, &lf, 1);
	if (error) {
		flp->error = FIOLOG_EWLOCK;
		return (0);
	}

	if (ufsvfsp->vfs_log == NULL || fs->fs_logbno == 0)
		goto errout;

	/*
	 * WE ARE COMMITTED TO DISABLING LOGGING PAST THIS POINT
	 */

	/*
	 * Disable logging
	 * 	Stop the delete and reclaim threads
	 *	Freeze and drain reader ops
	 *		commit any outstanding reader transactions (ufs_flush)
	 *		set the ``unmounted'' bit in the ufstrans struct
	 *		if debug, remove metadata from matamap
	 *		disable matamap processing
	 *		NULL the trans ops table
	 *		Free all of the incore structs related to logging
	 *	Allow reader ops
	 *
	 */
	ufs_thread_exit(&ufsvfsp->vfs_delete);
	ufs_thread_exit(&ufsvfsp->vfs_reclaim);

	vfs_lock_wait(ufsvfsp->vfs_vfs);
	ulp = &ufsvfsp->vfs_ulockfs;
	mutex_enter(&ulp->ul_lock);
	(void) ufs_quiesce(ulp);

	(void) ufs_flush(ufsvfsp->vfs_vfs);

	(void) ufs_trans_put(ufsvfsp->vfs_dev);
	TRANS_MATA_UMOUNT(ufsvfsp);
	ufsvfsp->vfs_domatamap = 0;
	ufsvfsp->vfs_trans = NULL;

	/*
	 * Free all of the incore structs
	 */
	(void) lufs_unsnarf(ufsvfsp);

	mutex_exit(&ulp->ul_lock);
	vfs_setmntopt(&ufsvfsp->vfs_vfs->vfs_mntopts, MNTOPT_NOLOGGING, NULL,
		0);
	vfs_unlock(ufsvfsp->vfs_vfs);

	/*
	 * Free the log space and mark the superblock as FSACTIVE
	 */
	(void) lufs_free(ufsvfsp);

	/*
	 * Unlock the file system
	 */
	lf.lf_lock = LOCKFS_ULOCK;
	lf.lf_flags = 0;
	error = ufs_fiolfs(vp, &lf, 1);
	if (error)
		flp->error = FIOLOG_ENOULOCK;

	return (0);

errout:
	lf.lf_lock = LOCKFS_ULOCK;
	lf.lf_flags = 0;
	(void) ufs_fiolfs(vp, &lf, 1);
	return (error);
}

/*
 * Enable logging
 */
static int
lufs_enable(struct vnode *vp, struct fiolog *flp, cred_t *cr)
{
	int		error;
	int		reclaim;
	inode_t		*ip = VTOI(vp);
	ufsvfs_t	*ufsvfsp = ip->i_ufsvfs;
	struct fs	*fs;
	ml_unit_t	*ul;
	struct lockfs	lf;
	struct ulockfs	*ulp;

	/*
	 * Logging (either ufs logging or SDS logging) is already enabled
	 */
	if (ufs_trans_check(ufsvfsp->vfs_dev)) {
		flp->error = FIOLOG_ETRANS;
		/*
		 * If ufs logging is enabled, set the "logging" mount
		 * option.
		 */
		if (ufsvfsp->vfs_log)
			vfs_setmntopt(&ufsvfsp->vfs_vfs->vfs_mntopts,
			    MNTOPT_LOGGING, NULL, 0);
		return (0);
	}
	fs = ufsvfsp->vfs_fs;

	/*
	 * Come back here to recheck if we had to disable the log.
	 */
recheck:
	error = 0;
	reclaim = 0;
	flp->error = FIOLOG_ENONE;

	/*
	 * Adjust requested log size
	 */
	flp->nbytes_actual = flp->nbytes_requested;
	if (flp->nbytes_actual == 0)
		flp->nbytes_actual =
		    (fs->fs_size / ldl_divisor) << fs->fs_fshift;
	flp->nbytes_actual = MAX(flp->nbytes_actual, ldl_minlogsize);
	flp->nbytes_actual = MIN(flp->nbytes_actual, ldl_maxlogsize);
	flp->nbytes_actual = blkroundup(fs, flp->nbytes_actual);

	/*
	 * logging is enabled and the log is the right size; done
	 */
	ul = (ml_unit_t *)ufsvfsp->vfs_log;
	if (ul && fs->fs_logbno && (flp->nbytes_actual == ul->un_requestsize))
			return (0);

	/*
	 * Readonly file system
	 */
	if (fs->fs_ronly) {
		flp->error = FIOLOG_EROFS;
		return (0);
	}

	/*
	 * File system must be write locked to enable logging
	 */
	error = ufs_fiolfss(vp, &lf);
	if (error) {
		return (error);
	}
	if (!LOCKFS_IS_ULOCK(&lf)) {
		flp->error = FIOLOG_EULOCK;
		return (0);
	}
	lf.lf_lock = LOCKFS_WLOCK;
	lf.lf_flags = 0;
	lf.lf_comment = NULL;
	error = ufs_fiolfs(vp, &lf, 1);
	if (error) {
		flp->error = FIOLOG_EWLOCK;
		return (0);
	}

	/*
	 * File system must be fairly consistent to enable logging
	 */
	if (fs->fs_clean != FSLOG &&
	    fs->fs_clean != FSACTIVE &&
	    fs->fs_clean != FSSTABLE &&
	    fs->fs_clean != FSCLEAN) {
		flp->error = FIOLOG_ECLEAN;
		goto unlockout;
	}

	/*
	 * A write-locked file system is only active if there are
	 * open deleted files; so remember to set FS_RECLAIM later.
	 */
	if (fs->fs_clean == FSACTIVE)
		reclaim = FS_RECLAIM;

	/*
	 * Logging is already enabled; must be changing the log's size
	 */
	if (fs->fs_logbno && ufsvfsp->vfs_log) {
		/*
		 * Before we can disable logging, we must give up our
		 * lock.  As a consequence of unlocking and disabling the
		 * log, the fs structure may change.  Because of this, when
		 * disabling is complete, we will go back to recheck to
		 * repeat all of the checks that we performed to get to
		 * this point.  Disabling sets fs->fs_logbno to 0, so this
		 * will not put us into an infinite loop.
		 */
		lf.lf_lock = LOCKFS_ULOCK;
		lf.lf_flags = 0;
		error = ufs_fiolfs(vp, &lf, 1);
		if (error) {
			flp->error = FIOLOG_ENOULOCK;
			return (0);
		}
		error = lufs_disable(vp, flp);
		if (error || (flp->error != FIOLOG_ENONE))
			return (0);
		goto recheck;
	}

	error = lufs_alloc(ufsvfsp, flp, cr);
	if (error)
		goto errout;

	/*
	 * Create all of the incore structs
	 */
	error = lufs_snarf(ufsvfsp, fs, 0);
	if (error)
		goto errout;

	/*
	 * DON'T ``GOTO ERROUT'' PAST THIS POINT
	 */

	/*
	 * Fix up the superblock
	 */
	ufsvfsp->vfs_ulockfs.ul_sbowner = curthread;
	mutex_enter(&ufsvfsp->vfs_lock);
	fs->fs_clean = FSLOG;
	ufs_sbwrite(ufsvfsp);
	mutex_exit(&ufsvfsp->vfs_lock);
	ufsvfsp->vfs_ulockfs.ul_sbowner = (kthread_id_t)-1;

	/*
	 * Pretend we were just mounted with logging enabled
	 *	freeze and drain the file system of readers
	 *		Get the ops vector
	 *		If debug, record metadata locations with log subsystem
	 *		Start the delete thread
	 *		Start the reclaim thread, if necessary
	 *	Thaw readers
	 */
	vfs_lock_wait(ufsvfsp->vfs_vfs);
	vfs_setmntopt(&ufsvfsp->vfs_vfs->vfs_mntopts, MNTOPT_LOGGING, NULL, 0);
	ulp = &ufsvfsp->vfs_ulockfs;
	mutex_enter(&ulp->ul_lock);
	(void) ufs_quiesce(ulp);

	ufsvfsp->vfs_trans = ufs_trans_get(ufsvfsp->vfs_dev, ufsvfsp->vfs_vfs);
	TRANS_DOMATAMAP(ufsvfsp);
	TRANS_MATA_MOUNT(ufsvfsp);
	TRANS_MATA_SI(ufsvfsp, fs);
	ufs_thread_start(&ufsvfsp->vfs_delete, ufs_thread_delete,
							ufsvfsp->vfs_vfs);
	if (fs->fs_reclaim & (FS_RECLAIM|FS_RECLAIMING)) {
		fs->fs_reclaim &= ~FS_RECLAIM;
		fs->fs_reclaim |=  FS_RECLAIMING;
		ufs_thread_start(&ufsvfsp->vfs_reclaim,
					ufs_thread_reclaim, ufsvfsp->vfs_vfs);
	} else
		fs->fs_reclaim |= reclaim;

	mutex_exit(&ulp->ul_lock);
	vfs_unlock(ufsvfsp->vfs_vfs);

	/*
	 * Unlock the file system
	 */
	lf.lf_lock = LOCKFS_ULOCK;
	lf.lf_flags = 0;
	error = ufs_fiolfs(vp, &lf, 1);
	if (error) {
		flp->error = FIOLOG_ENOULOCK;
		return (0);
	}

	return (0);

errout:
	(void) lufs_unsnarf(ufsvfsp);
	(void) lufs_free(ufsvfsp);
unlockout:
	lf.lf_lock = LOCKFS_ULOCK;
	lf.lf_flags = 0;
	(void) ufs_fiolfs(vp, &lf, 1);
	return (error);
}

static void
lufs_read_strategy(ml_unit_t *ul, buf_t *bp)
{
	mt_map_t	*logmap	= ul->un_logmap;
	offset_t	mof	= ldbtob(bp->b_blkno);
	off_t		nb	= bp->b_bcount;
	mapentry_t	*age;
	char		*va;
	klwp_t		*lwp	= ttolwp(curthread);

	/*
	 * get a linked list of overlapping deltas
	 * returns with &mtm->mtm_rwlock held
	 */
	logmap_list_get(logmap, mof, nb, &age);

	/*
	 * no overlapping deltas were found; read master
	 */
	if (age == NULL) {
		rw_exit(&logmap->mtm_rwlock);
		if (ul->un_flags & LDL_ERROR) {
			bp->b_flags |= B_ERROR;
			bp->b_error = EIO;
			biodone(bp);
		} else {
			((ufsvfs_t *)ul->un_ufsvfs)->vfs_iotstamp = lbolt;
			ub.ub_lreads.value.ul++;
			(void) bdev_strategy(bp);
			if (lwp != NULL)
				lwp->lwp_ru.inblock++;
		}
		return;
	}

	bp_mapin(bp);
	va = bp->b_un.b_addr;
	/*
	 * sync read the data from master
	 *	errors are returned in bp
	 */
	logmap_read_mstr(ul, bp);

	/*
	 * sync read the data from the log
	 *	errors are returned inline
	 */
	if (ldl_read(ul, va, mof, nb, age)) {
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
	}

	/*
	 * unlist the deltas
	 */
	logmap_list_put(logmap, age);

	/*
	 * all done
	 */
	if (ul->un_flags & LDL_ERROR) {
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
	}
	biodone(bp);
}

static void
lufs_write_strategy(ml_unit_t *ul, buf_t *bp)
{
	offset_t	mof	= ldbtob(bp->b_blkno);
	off_t		nb	= bp->b_bcount;
	char		*va;
	mt_map_t	*logmap	= ul->un_logmap;
	int		udelta;
	mapentry_t	*me;
	lufs_buf_t	*lbp;
	buf_t		*cb;
	klwp_t		*lwp	= ttolwp(curthread);

	logmap->mtm_ref = 1;

	/*
	 * wait for any overlapping userdata (discovered during the log scan)
	 * to be rolled
	 */
	if (logmap->mtm_nsud)
		logmap_roll_sud(logmap, ul, mof, nb);

	/*
	 * if there are deltas, move into log
	 */
	if (me = deltamap_remove(ul->un_deltamap, mof, nb)) {

		bp_mapin(bp);
		va = bp->b_un.b_addr;

		udelta = (me->me_dt == DT_UD);

		ASSERT(((ul->un_debug & MT_WRITE_CHECK) == 0) ||
			top_write_debug(ul, me, mof, nb));

		/*
		 * move to logmap (special case userdata)
		 */
		if (udelta)
			logmap_add_ud(ul, va, mof, me);
		else
			logmap_add(ul, va, mof, me);

		ASSERT((udelta) ||
			((ul->un_debug & MT_WRITE_CHECK) == 0) ||
			top_check_debug(va, mof, nb, ul));

		/*
		 * userdata deltas are both logged and written to the master
		 */
		if (!udelta) {
			if (ul->un_flags & LDL_ERROR) {
				bp->b_flags |= B_ERROR;
				bp->b_error = EIO;
			}
			biodone(bp);
		} else {
			lbp = kmem_cache_alloc(lufs_bp, KM_SLEEP);
			bioinit(&lbp->lb_buf);
			lbp->lb_ptr = bp;

			cb = bioclone(bp, 0, bp->b_bcount, bp->b_dev,
			    bp->b_blkno, logmap_ud_done, &lbp->lb_buf,
			    KM_SLEEP);

			((ufsvfs_t *)ul->un_ufsvfs)->vfs_iotstamp = lbolt;
			ub.ub_uds.value.ul++;
			(void) bdev_strategy(cb);
			if (lwp != NULL)
				lwp->lwp_ru.oublock++;
		}
		return;
	}
	if (ul->un_flags & LDL_ERROR) {
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
		biodone(bp);
		return;
	}

	/*
	 * Check that we are not updating metadata, or if so then via B_PHYS.
	 */
	ASSERT((ul->un_matamap == NULL) ||
		!(matamap_overlap(ul->un_matamap, mof, nb) &&
		((bp->b_flags & B_PHYS) == 0)));

	((ufsvfs_t *)ul->un_ufsvfs)->vfs_iotstamp = lbolt;
	ub.ub_lwrites.value.ul++;
	(void) bdev_strategy(bp);
	if (lwp != NULL)
		lwp->lwp_ru.oublock++;
}

void
lufs_strategy(ml_unit_t *ul, buf_t *bp)
{
	if (bp->b_flags & B_READ)
		lufs_read_strategy(ul, bp);
	else
		lufs_write_strategy(ul, bp);
}


/*
 * LOADABLE MODULE STUFF
 */
char _depends_on[] = "fs/ufs";

/*
 * put this stuff at end so we don't have to create forward
 * references for everything
 */
static struct modlmisc modlmisc = {
	&mod_miscops, "Logging UFS Module"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

extern struct lufsops	lufsops;

/*
 * Tell UFS that we can handle directio requests
 */
extern int	ufs_trans_directio;
#pragma weak	ufs_trans_directio

int
_init(void)
{
	int error;

	error = mod_install(&modlinkage);
	if (error)
		return (error);

	/* Create kmem caches */
	lufs_sv = kmem_cache_create("lufs save", sizeof (lufs_save_t), 0,
	    NULL, NULL, NULL, NULL, NULL, 0);
	lufs_bp = kmem_cache_create("lufs bufs", sizeof (lufs_buf_t), 0,
	    NULL, NULL, NULL, NULL, NULL, 0);

	mutex_init(&log_mutex, NULL, MUTEX_DEFAULT, NULL);

	_init_top();

	lufsops.lufs_enable = lufs_enable;
	lufsops.lufs_disable = lufs_disable;
	lufsops.lufs_snarf = lufs_snarf;
	lufsops.lufs_unsnarf = lufs_unsnarf;
	lufsops.lufs_empty = lufs_empty;
	lufsops.lufs_strategy = lufs_strategy;

	if (&bio_lufs_strategy != NULL)
		bio_lufs_strategy = (void (*) (void *, buf_t *)) lufs_strategy;

	/*
	 * Tell UFS that we can handle directio requests
	 */
	if (&ufs_trans_directio != NULL)
		ufs_trans_directio = 1;

	return (0);
}

int
_fini(void)
{
	/*
	 * misc modules are not safely unloadable: 1170668
	 */
	return (EBUSY);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
