/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

#pragma ident	"@(#)quota_ufs.c	2.47	98/05/20 SMI"

/*
 * Routines used in checking limits on file system usage.
 */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/signal.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_fs.h>
#include <sys/fs/ufs_quota.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/session.h>
#include <sys/debug.h>

/*
 * Find the dquot structure that should
 * be used in checking i/o on inode ip.
 */
struct dquot *
getinoquota(ip)
	register struct inode *ip;
{
	register struct dquot *dqp;
	register struct ufsvfs *ufsvfsp = ip->i_ufsvfs;
	struct dquot *xdqp;


	ASSERT(RW_LOCK_HELD(&ufsvfsp->vfs_dqrwlock));
	ASSERT(RW_WRITE_HELD(&ip->i_contents));
	/*
	 * Check for quotas enabled.
	 */
	if ((ufsvfsp->vfs_qflags & MQ_ENABLED) == 0) {
		return (NULL);
	}

	/*
	 * Check for someone doing I/O to quota file.
	 */
	if (ip == ufsvfsp->vfs_qinod) {
		return (NULL);
	}

	if (getdiskquota((uid_t)ip->i_uid, ufsvfsp, 0, &xdqp)) {
		return (NULL);
	}
	dqp = xdqp;
	mutex_enter(&dqp->dq_lock);
	ASSERT(ip->i_uid == dqp->dq_uid);

	if (dqp->dq_fhardlimit == 0 && dqp->dq_fsoftlimit == 0 &&
	    dqp->dq_bhardlimit == 0 && dqp->dq_bsoftlimit == 0) {
		dqput(dqp);
		mutex_exit(&dqp->dq_lock);
		dqp = NULL;
	} else {
		mutex_exit(&dqp->dq_lock);
	}
	return (dqp);
}

/*
 * Update disk usage, and take corrective action.
 */
int
chkdq(ip, change, force, cr, uerrp, lenp)
	struct inode *ip;
	long change;
	int force;
	struct cred *cr;
	char **uerrp;
	size_t *lenp;
{
	struct dquot *dqp;
	unsigned int ncurblocks;
	struct ufsvfs *ufsvfsp = ip->i_ufsvfs;
	int error = 0;
	char *msg1 =
"!quota_ufs: over hard disk limit (pid %d, uid %d, inum %d, fs %s)\n";
	char *msg2 =
"!quota_ufs: Warning: over disk limit (pid %d, uid %d, inum %d, fs %s)\n";
	char *msg3 =
"!quota_ufs: over disk and time limit (pid %d, uid %d, inum %d, fs %s)\n";
	char *errmsg = NULL;


	/*
	 * Shadow inodes do not need to hold the vfs_dqrwlock lock.
	 */
	ASSERT((ip->i_mode & IFMT) == IFSHAD ||
		RW_LOCK_HELD(&ufsvfsp->vfs_dqrwlock));
	ASSERT(RW_WRITE_HELD(&ip->i_contents));

	if (change == 0)
		return (0);
	dqp = ip->i_dquot;

	/*
	 * Make sure the quota info record matches the owner.
	 */
	ASSERT(dqp == NULL || ip->i_uid == dqp->dq_uid);

#ifdef DEBUG
	/*
	 * Shadow inodes should not have quota info records.
	 */
	if ((ip->i_mode & IFMT) == IFSHAD) {
		ASSERT(dqp == NULL);
	}
	/*
	 * Paranoia for verifying that quotas are okay.
	 */
	else {
		struct dquot *expect_dq;
		int mismatch_ok = 0;

		/* Get current quota information */
		expect_dq = getinoquota(ip);
		/*
		 * We got NULL back from getinoquota(), but there is
		 * no error code return from that interface and some
		 * errors are "ok" because we may be testing via error
		 * injection.  If this is not the quota inode then we
		 * use getdiskquota() to see if there is an error and
		 * if the error is ok.
		 */
		if (expect_dq == NULL && ip != ufsvfsp->vfs_qinod) {
			int error;
			struct dquot *xdqp;

			error = getdiskquota((uid_t)ip->i_uid, ufsvfsp, 0,
						&xdqp);
			switch (error) {
			/*
			 * Either the error was transient or the quota
			 * info record has no limits which gets optimized
			 * out by getinoquota().
			 */
			case 0:
				if (xdqp->dq_fhardlimit == 0 &&
				    xdqp->dq_fsoftlimit == 0 &&
				    xdqp->dq_bhardlimit == 0 &&
				    xdqp->dq_bsoftlimit == 0) {
					mutex_enter(&xdqp->dq_lock);
					dqput(xdqp);
					mutex_exit(&xdqp->dq_lock);
				} else {
					expect_dq = xdqp;
				}
				break;

			case ESRCH:	/* quotas are not enabled */
			case EINVAL:	/* error flag set on cached record */
			case EUSERS:	/* quota table is full */
			case EIO:	/* I/O error */
				mismatch_ok = 1;
				break;
			}
		}

		/*
		 * Make sure dqp and the current quota info agree.
		 * The first part of the #ifndef is the quick way to
		 * do the check and should be part of the standard
		 * DEBUG code. The #else part is useful if you are
		 * actually chasing an inconsistency and don't want
		 * to have to look at stack frames to figure which
		 * variable has what value.
		 */
#ifndef CHASE_QUOTA
		ASSERT(mismatch_ok || dqp == expect_dq);
#else /* CHASE_QUOTA */
		if (expect_dq == NULL) {
			/*
			 * If you hit this ASSERT() you know that quota
			 * subsystem does not expect quota info for this
			 * inode, but the inode has it.
			 */
			ASSERT(mismatch_ok || dqp == NULL);
		} else {
			/*
			 * If you hit this ASSERT() you know that quota
			 * subsystem expects quota info for this inode,
			 * but the inode does not have it.
			 */
			ASSERT(dqp);
			/*
			 * If you hit this ASSERT() you know that quota
			 * subsystem expects quota info for this inode
			 * and the inode has quota info, but the two
			 * quota info pointers are not the same.
			 */
			ASSERT(dqp == expect_dq);
		}
#endif /* !CHASE_QUOTA */
		/*
		 * Release for getinoquota() above or getdiskquota()
		 * call when error is transient.
		 */
		if (expect_dq) {
			mutex_enter(&expect_dq->dq_lock);
			dqput(expect_dq);
			mutex_exit(&expect_dq->dq_lock);
		}
	}
#endif /* DEBUG */

	if (dqp == NULL)
		return (0);
	/*
	 * Quotas are not enabled on this file system so there is nothing
	 * more to do.
	 */
	if ((ufsvfsp->vfs_qflags & MQ_ENABLED) == 0) {
		return (0);
	}
	mutex_enter(&dqp->dq_lock);
	if (change < 0) {
		dqp->dq_flags |= DQ_MOD;
		if ((int)dqp->dq_curblocks + change >= 0)
			dqp->dq_curblocks += change;
		else
			dqp->dq_curblocks = 0;
		if (dqp->dq_curblocks < dqp->dq_bsoftlimit)
			dqp->dq_btimelimit = 0;
		dqp->dq_flags &= ~DQ_BLKS;
		TRANS_QUOTA(dqp);
		mutex_exit(&dqp->dq_lock);
		return (0);
	}

	ncurblocks = dqp->dq_curblocks + change;
	/*
	 * Allocation. Check hard and soft limits.
	 * Skip checks for super user - unless the
	 * super user is creating an allocation
	 * for a mortal user.
	 */
	if (cr->cr_uid == 0 && ip->i_uid == 0)
		goto out;

	/*
	 * Disallow allocation if it would bring the current usage over
	 * the hard limit or if the user is over his soft limit and his time
	 * has run out.
	 */
	if (dqp->dq_bhardlimit && ncurblocks >= dqp->dq_bhardlimit && !force) {
		/* If the user was not informed yet and the caller	*/
		/* is the owner of the file				*/
		if ((dqp->dq_flags & DQ_BLKS) == 0 &&
			ip->i_uid == cr->cr_ruid) {
			errmsg = msg1;
			dqp->dq_flags |= DQ_BLKS;
		}
		error = EDQUOT;
		goto out;
	}
	if (dqp->dq_bsoftlimit && ncurblocks >= dqp->dq_bsoftlimit) {
		if (dqp->dq_curblocks < dqp->dq_bsoftlimit ||
		    dqp->dq_btimelimit == 0) {
			dqp->dq_flags |= DQ_MOD;
			dqp->dq_btimelimit =
			    hrestime.tv_sec +
			    ((struct ufsvfs *)ip->i_vnode.v_vfsp->vfs_data)
				->vfs_btimelimit;
			if (ip->i_uid == cr->cr_ruid) {
				errmsg = msg2;
			}
		} else if (hrestime.tv_sec > dqp->dq_btimelimit && !force) {
			/* If the user was not informed yet and the	*/
			/* caller is the owner of the file		*/
			if ((dqp->dq_flags & DQ_BLKS) == 0 &&
				ip->i_uid == cr->cr_ruid) {
				errmsg = msg3;
				dqp->dq_flags |= DQ_BLKS;
			}
			error = EDQUOT;
		}
	}
out:
	if (error == 0) {
		dqp->dq_flags |= DQ_MOD;
		dqp->dq_curblocks = ncurblocks;
	}

	if (dqp->dq_flags & DQ_MOD)
		TRANS_QUOTA(dqp);

	mutex_exit(&dqp->dq_lock);
	/*
	 * Check for any error messages to be sent
	 */
	if (errmsg != NULL) {
		/*
		 * Send message to the error log.
		 */
		if (uerrp != NULL) {
			/*
			 * Set up message caller should send to user;
			 * gets copied to the message buffer as a side-
			 * effect of the caller's uprintf().
			 */
			*lenp = strlen(errmsg) + 20 + 20 +
				strlen(ip->i_fs->fs_fsmnt) + 1;
			*uerrp = (char *)kmem_alloc(*lenp, KM_NOSLEEP);
			if (*uerrp != NULL) {
				/* errmsg+1 => skip leading ! */
				(void) sprintf(*uerrp, errmsg+1,
					(int)ttoproc(curthread)->p_pid,
					(int)ip->i_uid, (int)ip->i_number,
					ip->i_fs->fs_fsmnt);
			}
		} else {
			/*
			 * Caller doesn't care, so just copy to the
			 * message buffer.
			 */
			cmn_err(CE_NOTE, errmsg,
				(int)ttoproc(curthread)->p_pid,
				(int)ip->i_uid, (int)ip->i_number,
				ip->i_fs->fs_fsmnt);
		}
	}
	return (error);
}

/*
 * Check the inode limit, applying corrective action.
 */
int
chkiq(ufsvfsp, change, ip, uid, force, cr, uerrp, lenp)
	struct ufsvfs *ufsvfsp;
	int change;
	struct inode *ip;
	uid_t uid;
	int force;
	struct cred *cr;
	char **uerrp;
	size_t *lenp;
{
	struct dquot *dqp;
	unsigned int ncurfiles;
	struct dquot *xdqp;
	char *errmsg = NULL;
	char *err1 =
"!quota_ufs: over file hard limit (pid %d, uid %d, fs %s)\n";
	char *err2 =
"!quota_ufs: Warning: too many files (pid %d, uid %d, fs %s)\n";
	char *err3 =
"!quota_ufs: over file and time limit (pid %d, uid %d, fs %s)\n";
	int error = 0;


	ASSERT(RW_READ_HELD(&ufsvfsp->vfs_dqrwlock));
	/*
	 * Change must be either a single increment or decrement.
	 * If change is an increment, then ip must be NULL.
	 */
	ASSERT(change == 1 || change == -1);
	ASSERT(change != 1 || ip == NULL);

	/*
	 * Quotas are not enabled so bail out now.
	 */
	if ((ufsvfsp->vfs_qflags & MQ_ENABLED) == 0) {
		return (0);
	}

	/*
	 * Free a specific inode.
	 */
	if (change == -1 && ip) {
		dqp = ip->i_dquot;
		if (dqp == NULL)
			return (0);
		mutex_enter(&dqp->dq_lock);
		if (dqp->dq_curfiles) {
			dqp->dq_curfiles--;
			dqp->dq_flags |= DQ_MOD;
		}
		if (dqp->dq_curfiles < dqp->dq_fsoftlimit) {
			dqp->dq_ftimelimit = 0;
			dqp->dq_flags |= DQ_MOD;
		}
		dqp->dq_flags &= ~DQ_FILES;
		if (dqp->dq_flags & DQ_MOD)
			TRANS_QUOTA(dqp);
		mutex_exit(&dqp->dq_lock);
		return (0);
	}

	/*
	 * Allocation or deallocation without a specific inode.
	 * Get dquot for for uid, fs.
	 */
	if (getdiskquota(uid, ufsvfsp, 0, &xdqp)) {
		return (0);
	}
	dqp = xdqp;
	mutex_enter(&dqp->dq_lock);
	if (dqp->dq_fsoftlimit == 0 && dqp->dq_fhardlimit == 0) {
		dqput(dqp);
		mutex_exit(&dqp->dq_lock);
		return (0);
	}

	/*
	 * Skip checks for super user - unless the
	 * super user is creating an allocation
	 * for a mortal user.
	 */
	if (cr->cr_uid == 0 && uid == 0)
		goto out;
	ncurfiles = dqp->dq_curfiles + change;

	/*
	 * Dissallow allocation if it would bring the current usage over
	 * the hard limit or if the user is over his soft limit and his time
	 * has run out.
	 */
	if (change == 1 && ncurfiles >= dqp->dq_fhardlimit &&
	    dqp->dq_fhardlimit && !force) {
		/* If the user was not informed yet and the caller	*/
		/* is the owner of the file 				*/
		if ((dqp->dq_flags & DQ_FILES) == 0 && uid == cr->cr_ruid) {
			errmsg = err1;
			dqp->dq_flags |= DQ_FILES;
		}
		error = EDQUOT;
	} else if (change == 1 && ncurfiles >= dqp->dq_fsoftlimit &&
			dqp->dq_fsoftlimit) {
		if (ncurfiles == dqp->dq_fsoftlimit ||
		    dqp->dq_ftimelimit == 0) {
			dqp->dq_flags |= DQ_MOD;
			dqp->dq_ftimelimit = hrestime.tv_sec +
			    ufsvfsp->vfs_ftimelimit;
			/* If the caller owns the file */
			if (uid == cr->cr_ruid)
				errmsg = err2;
		} else if (hrestime.tv_sec > dqp->dq_ftimelimit && !force) {
			/* If the user was not informed yet and the	*/
			/* caller is the owner of the file 		*/
			if ((dqp->dq_flags & DQ_FILES) == 0 &&
				uid == cr->cr_ruid) {
				errmsg = err3;
				dqp->dq_flags |= DQ_FILES;
			}
			error = EDQUOT;
		}
	}
out:
	if (error == 0) {
		dqp->dq_flags |= DQ_MOD;
		dqp->dq_curfiles += change;
	}
	if (dqp->dq_flags & DQ_MOD)
		TRANS_QUOTA(dqp);
	dqput(dqp);
	mutex_exit(&dqp->dq_lock);
	/*
	 * Check for any error messages to be sent
	 */
	if (errmsg != NULL) {
		/*
		 * Send message to the error log.
		 */
		if (uerrp != NULL) {
			/*
			 * Set up message caller should send to user;
			 * gets copied to the message buffer as a side-
			 * effect of the caller's uprintf().
			 */
			*lenp = strlen(errmsg) + 20 + 20 +
				strlen(ufsvfsp->vfs_fs->fs_fsmnt) + 1;
			*uerrp = (char *)kmem_alloc(*lenp, KM_NOSLEEP);
			if (*uerrp != NULL) {
				/* errmsg+1 => skip leading ! */
				(void) sprintf(*uerrp, errmsg+1,
					(int)ttoproc(curthread)->p_pid,
					(int)uid, ufsvfsp->vfs_fs->fs_fsmnt);
			}
		} else {
			/*
			 * Caller doesn't care, so just copy to the
			 * message buffer.
			 */
			cmn_err(CE_NOTE, errmsg,
				(int)ttoproc(curthread)->p_pid,
				(int)uid, ufsvfsp->vfs_fs->fs_fsmnt);
		}
	}
	return (error);
}

/*
 * Release a dquot.
 */
void
dqrele(dqp)
	register struct dquot *dqp;
{

	if (dqp != NULL) {
		mutex_enter(&dqp->dq_lock);
		if (dqp->dq_cnt == 1 && dqp->dq_flags & DQ_MOD)
			dqupdate(dqp);
		dqput(dqp);
		mutex_exit(&dqp->dq_lock);
	}
}
