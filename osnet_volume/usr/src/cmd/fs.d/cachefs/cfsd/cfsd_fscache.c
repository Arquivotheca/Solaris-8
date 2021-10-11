/*
 * -----------------------------------------------------------------
 *			fscache.c
 *
 * Methods of the cfsd_fscache class.
 */

#ident   "@(#)cfsd_fscache.c 1.12     99/05/13 SMI"
/*
 * Copyright (c) 1994-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <thread.h>
#include <synch.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <limits.h>
#include <fcntl.h>
#include <locale.h>
#include <nfs/nfs.h>
#include <sys/utsname.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <rpc/rpc.h>
#include <mdbug/mdbug.h>
#include <sys/fs/cachefs_fs.h>
#include <sys/fs/cachefs_dlog.h>
#include <sys/fs/cachefs_ioctl.h>
#include "cfsd.h"
#include "cfsd_kmod.h"
#include "cfsd_maptbl.h"
#include "cfsd_logfile.h"
#include "cfsd_logelem.h"
#include "cfsd_fscache.h"

/*
 * -----------------------------------------------------------------
 *			cfsd_fscache_create
 *
 * Description:
 * Arguments:
 *	name
 *	cachepath
 * Returns:
 * Preconditions:
 *	precond(name)
 *	precond(cachepath)
 */
cfsd_fscache_object_t *
cfsd_fscache_create(const char *name, const char *cachepath,
	int fscacheid)
{
	cfsd_fscache_object_t *fscache_object_p;
	int xx;

	dbug_enter("cfsd_fscache_create");

	dbug_precond(name);
	dbug_precond(cachepath);

	fscache_object_p = cfsd_calloc(sizeof (cfsd_fscache_object_t));
	strcpy(fscache_object_p->i_name, name);
	strcpy(fscache_object_p->i_cachepath, cachepath);
	fscache_object_p->i_fscacheid = fscacheid;
	fscache_object_p->i_refcnt = 0;
	fscache_object_p->i_disconnectable = 0;
	fscache_object_p->i_mounted = 0;
	fscache_object_p->i_threaded = 0;
	fscache_object_p->i_connected = 0;
	fscache_object_p->i_reconcile = 0;
	fscache_object_p->i_changes = 0;
	fscache_object_p->i_simdis = 0;
	fscache_object_p->i_tryunmount = 0;
	fscache_object_p->i_backunmount = 0;
	fscache_object_p->i_time_state = 0;
	fscache_object_p->i_time_mnt = 0;
	fscache_object_p->i_modify = 1;

	fscache_object_p->i_threadid = 0;
	fscache_object_p->i_ofd = -1;

	fscache_object_p->i_next = NULL;

	/* initialize the locking mutex */
	xx = mutex_init(&fscache_object_p->i_lock, USYNC_THREAD, NULL);
	dbug_assert(xx == 0);

	xx = cond_init(&fscache_object_p->i_cvwait, USYNC_THREAD, 0);
	dbug_assert(xx == 0);

	dbug_leave("cfsd_fscache_create");
	return (fscache_object_p);
}

/*
 * -----------------------------------------------------------------
 *			cfsd_fscache_destroy
 *
 * Description:
 * Arguments:
 * Returns:
 * Preconditions:
 */
void
cfsd_fscache_destroy(cfsd_fscache_object_t *fscache_object_p)
{
	int xx;

	dbug_enter("cfsd_fscache_destroy");

	dbug_precond(fscache_object_p);
	/* dbug_assert(fscache_object_p->i_refcnt == 0); */

	/* close down the message file descriptor */
	if (fscache_object_p->i_ofd >= 0) {
		if (close(fscache_object_p->i_ofd))
			dbug_print(("error", "cannot close fscache fd error %d",
			    errno));
		fscache_object_p->i_ofd = -1;
	}

	/* destroy the locking mutex */
	xx = mutex_destroy(&fscache_object_p->i_lock);
	dbug_assert(xx == 0);

	/* destroy the conditional variable */
	xx = cond_destroy(&fscache_object_p->i_cvwait);
	dbug_assert(xx == 0);

	cfsd_free(fscache_object_p);

	dbug_leave("cfsd_fscache_destroy");
}

/*
 * -----------------------------------------------------------------
 *			fscache_lock
 *
 * Description:
 * Arguments:
 * Returns:
 * Preconditions:
 */
void
fscache_lock(cfsd_fscache_object_t *fscache_object_p)
{
	dbug_enter("fscache_lock");

	dbug_precond(fscache_object_p);
	mutex_lock(&fscache_object_p->i_lock);
	dbug_leave("fscache_lock");
}

/*
 * -----------------------------------------------------------------
 *			fscache_unlock
 *
 * Description:
 * Arguments:
 * Returns:
 * Preconditions:
 */
void
fscache_unlock(cfsd_fscache_object_t *fscache_object_p)
{
	dbug_enter("fscache_unlock");

	dbug_precond(fscache_object_p);
	mutex_unlock(&fscache_object_p->i_lock);
	dbug_leave("fscache_unlock");
}

/*
 * -----------------------------------------------------------------
 *			fscache_setup
 *
 * Description:
 * Arguments:
 * Returns:
 * Preconditions:
 */
void
fscache_setup(cfsd_fscache_object_t *fscache_object_p)
{
	char buf[MAXPATHLEN * 4];
	FILE *fin;
	char type[50];
	int err = 0;
	int xx;
	char *options[] = { "snr", "disconnectable", NULL };
	char *strp = buf;
	char *dummy;
	struct stat64 sinfo;
	time_t mtime;

	dbug_enter("fscache_setup");
	dbug_precond(fscache_object_p);

	fscache_object_p->i_modify++;
	fscache_object_p->i_disconnectable = 0;
	fscache_object_p->i_connected = 0;
	fscache_object_p->i_reconcile = 0;
	fscache_object_p->i_changes = 0;
	fscache_object_p->i_time_state = 0;
	fscache_object_p->i_time_mnt = 0;
	fscache_object_p->i_mntpt[0] = '\0';
	fscache_object_p->i_backfs[0] = '\0';
	fscache_object_p->i_backpath[0] = '\0';
	fscache_object_p->i_backfstype[0] = '\0';
	fscache_object_p->i_cfsopt[0] = '\0';
	fscache_object_p->i_bfsopt[0] = '\0';

	sprintf(buf, "%s/%s/%s", fscache_object_p->i_cachepath,
	    fscache_object_p->i_name, CACHEFS_MNT_FILE);

	/* get the modify time of the mount file */
	if (stat64(buf, &sinfo) == -1) {
		dbug_print(("err", "could not stat %s, %d", buf, errno));
		dbug_leave("fscache_setup");
		return;
	}
	mtime = sinfo.st_mtime;

	/* open for reading the file with the mount information */
	fin = fopen(buf, "r");
	if (fin == NULL) {
		dbug_print(("err", "could not open %s, %d", buf, errno));
		dbug_leave("fscache_setup");
		return;
	}

	/* read the mount information from the file */
	while ((xx = fscanf(fin, "%s%s", type, buf)) == 2) {
		dbug_print(("info", "\"%s\" \"%s\"", type, buf));
		if (strcmp(type, "cachedir:") == 0) {
			if (strcmp(fscache_object_p->i_cachepath, buf) != 0) {
				err = 1;
				dbug_print(("err", "caches do not match %s, %s",
				    fscache_object_p->i_cachepath, buf));
			}
		} else if (strcmp(type, "mnt_point:") == 0) {
			strcpy(fscache_object_p->i_mntpt, buf);
		} else if (strcmp(type, "special:") == 0) {
			strcpy(fscache_object_p->i_backfs, buf);
		} else if (strcmp(type, "backpath:") == 0) {
			strcpy(fscache_object_p->i_backpath, buf);
		} else if (strcmp(type, "backfstype:") == 0) {
			strcpy(fscache_object_p->i_backfstype, buf);
		} else if (strcmp(type, "cacheid:") == 0) {
			if (strcmp(fscache_object_p->i_name, buf) != 0) {
				err = 1;
				dbug_print(("err", "ids do not match %s, %s",
				    fscache_object_p->i_name, buf));
			}
		} else if (strcmp(type, "cachefs_options:") == 0) {
			strcpy(fscache_object_p->i_cfsopt, buf);
		} else if (strcmp(type, "backfs_options:") == 0) {
			strcpy(fscache_object_p->i_bfsopt, buf);
		} else if (strcmp(type, "mount_time:") == 0) {
			continue;
		} else {
			dbug_print(("err", "unknown keyword \"%s\"", type));
			err = 1;
		}
	}
	if (fclose(fin))
		dbug_print(("err", "cannot close %s, %d", buf, errno));

	/* see if this is a file system that is disconnectable */
	if ((err == 0) &&
		(fscache_object_p->i_backfs[0] &&
		fscache_object_p->i_cfsopt[0])) {
		strcpy(buf, fscache_object_p->i_cfsopt);
		while (*strp != '\0') {
			xx = getsubopt(&strp, options, &dummy);
			if (xx != -1) {
				fscache_object_p->i_disconnectable = 1;
				break;
			}
		}
	}

	/*
	 * open up a fd on the console so we have a place to write
	 * log rolling errors
	 */
	if (fscache_object_p->i_disconnectable) {
		if (fscache_object_p->i_ofd < 0)
			fscache_object_p->i_ofd = open("/dev/console",
			    O_WRONLY);
		if (fscache_object_p->i_ofd < 0) {
			fprintf(stderr,
			    gettext("cachefsd: File system %s cannot be"
			    " disconnected.\n"),
			    fscache_object_p->i_mntpt);
			fprintf(stderr,
			    gettext("cachefsd: Cannot open /dev/console\n"));
			fscache_object_p->i_disconnectable = 0;
		}
	}

	/* see if the file system is mounted */
	sprintf(buf, "%s/%s/%s", fscache_object_p->i_cachepath,
	    fscache_object_p->i_name, CACHEFS_UNMNT_FILE);
	if (stat64(buf, &sinfo) == 0) {
		fscache_object_p->i_mounted = 0;
		mtime = sinfo.st_mtime;
	} else
		fscache_object_p->i_mounted = 1;

	/* save the time of the last mount or unmount */
	fscache_object_p->i_time_mnt = mtime;

	dbug_print(("info", "disconnectable == %d, mounted == %d",
	    fscache_object_p->i_disconnectable,
	    fscache_object_p->i_mounted));
	dbug_leave("fscache_setup");
}

/*
 * -----------------------------------------------------------------
 *			fscache_process
 *
 * Description:
 * Arguments:
 * Returns:
 * Preconditions:
 */
void
fscache_process(cfsd_fscache_object_t *fscache_object_p)
{
	int xx;
	int changes;
	cfsd_kmod_object_t *kmod_object_p;
	int setup = 1;
	int state;

	dbug_enter("fscache_process");
	dbug_precond(fscache_object_p);

	kmod_object_p = cfsd_kmod_create();
	for (;;) {
		fscache_lock(fscache_object_p);
		fscache_object_p->i_time_state = time(NULL);
		fscache_object_p->i_modify++;

		/* if we should try to unmount the file system */
		if (fscache_object_p->i_tryunmount) {
			/* shut down the interface to the kmod */
			if (setup == 0) {
				kmod_shutdown(kmod_object_p);
				setup = 1;
			}

			/* try to unmount the file system */
			if (umount(fscache_object_p->i_mntpt) == -1) {
				xx = errno;
				dbug_print(("info", "unmount failed %s",
				    strerror(xx)));
			} else {
				fscache_object_p->i_mounted = 0;
			}

			/* wake up thread blocked in fscache_unmount */
			fscache_object_p->i_tryunmount = 0;
			xx = cond_broadcast(&fscache_object_p->i_cvwait);
			dbug_assert(xx == 0);

			/* all done if unmount succeeded */
			if (fscache_object_p->i_mounted == 0) {
				fscache_unlock(fscache_object_p);
				break;
			}
		}

		if (setup) {
			setup = 0;
			/*
			 * make an interface into the cachefs kmod for
			 * this fs
			 */
			xx = kmod_setup(kmod_object_p,
			    fscache_object_p->i_mntpt);
			if (xx != 0) {
				dbug_print(("err",
				    "setup of kmod interface failed %d", xx));
				fscache_object_p->i_disconnectable = 0;
				fscache_object_p->i_modify++;
				fscache_unlock(fscache_object_p);
				break;
			}

			/* verify that we got the file system we expected XXX */
		}

		/* get the current state of the file system */
		state = kmod_stateget(kmod_object_p);

		if (fscache_object_p->i_simdis && (state == CFS_FS_CONNECTED)) {
			dbug_print(("simdis", "simulating disconnection on %s",
			    fscache_object_p->i_mntpt));
			xx = kmod_stateset(kmod_object_p, CFS_FS_DISCONNECTED);
			dbug_assert(xx == 0);
			state = kmod_stateget(kmod_object_p);
			dbug_assert(state == CFS_FS_DISCONNECTED);
		}
		fscache_unlock(fscache_object_p);

		switch (state) {
		case CFS_FS_CONNECTED:
			fscache_lock(fscache_object_p);
			fscache_object_p->i_connected = 1;
			fscache_object_p->i_reconcile = 0;
			fscache_object_p->i_modify++;
			fscache_unlock(fscache_object_p);

			/* wait for fs to switch to disconnecting */
			dbug_print(("info", "about to xwait"));
			xx = kmod_xwait(kmod_object_p);
			if (xx == EINTR) {
				dbug_print(("info", "a. EINTR from xwait"));
				continue;
			}
			dbug_assert(xx == 0);
			state = kmod_stateget(kmod_object_p);
			dbug_assert(state == CFS_FS_DISCONNECTED);
			break;

		case CFS_FS_DISCONNECTED:
			fscache_lock(fscache_object_p);
			fscache_object_p->i_connected = 0;
			fscache_object_p->i_reconcile = 0;
			fscache_object_p->i_modify++;
			fscache_unlock(fscache_object_p);

			/* wait until we are reconnected */
			fscache_server_alive(fscache_object_p, kmod_object_p);
			if (fscache_object_p->i_tryunmount)
				continue;

			/* switch to reconnecting mode */
			xx = kmod_stateset(kmod_object_p, CFS_FS_RECONNECTING);
			dbug_assert(xx == 0);
			break;

		case CFS_FS_RECONNECTING:
			fscache_lock(fscache_object_p);
			fscache_object_p->i_connected = 1;
			fscache_object_p->i_reconcile = 1;
			fscache_object_p->i_modify++;
			changes = fscache_object_p->i_changes;
			fscache_unlock(fscache_object_p);

			/* roll the log */
			xx = fscache_roll(fscache_object_p, kmod_object_p);
			if (xx) {
				dbug_assert(xx == ETIMEDOUT);
				/* switch to disconnected */
				xx = kmod_stateset(kmod_object_p,
				    CFS_FS_DISCONNECTED);
				dbug_assert(xx == 0);
			} else {
				/* switch to connected */
				xx = kmod_stateset(kmod_object_p,
				    CFS_FS_CONNECTED);
				dbug_assert(xx == 0);
				changes = 0;
			}

			fscache_lock(fscache_object_p);
			fscache_object_p->i_reconcile = 0;
			fscache_changes(fscache_object_p, changes);
			fscache_object_p->i_modify++;
			fscache_unlock(fscache_object_p);

			break;

		default:
			dbug_assert(0);
			break;
		}
	}
	cfsd_kmod_destroy(kmod_object_p);
	dbug_leave("fscache_process");
}

/*
 *			fscache_simdisconnect
 *
 * Description:
 *	Simulates disconnection or reconnects from a simulated disconnection.
 * Arguments:
 *	disconnect	1 means disconnect, !1 means connect
 * Returns:
 *	Returns 0 for success, !0 on an error
 * Preconditions:
 */
int
fscache_simdisconnect(cfsd_fscache_object_t *fscache_object_p, int disconnect)
{

	int xx;
	int ret = 0;
	char *strp;
	int tcon;
	int trec;

	dbug_enter("fscache_simdisconnect");
	dbug_precond(fscache_object_p);

	strp = disconnect ? "disconnection" : "reconnection";

	dbug_print(("simdis", "About to simulate %s", strp));

	fscache_lock(fscache_object_p);

	if (disconnect) {
		/* if file system cannot be disconnected */
		if (fscache_object_p->i_disconnectable == 0) {
			ret = 1;
			goto out;
		}

		/* if file system is already disconnected */
		if (fscache_object_p->i_connected == 0) {
			ret = 2;
			goto out;
		}
		fscache_object_p->i_simdis = 1;
	} else {
		/* if file system is already connected */
		if (fscache_object_p->i_connected) {
			ret = 1;
			goto out;
		}

		/* if file system is not "simulated" disconnected */
		if (fscache_object_p->i_simdis == 0) {
			ret = 2;
			goto out;
		}
		fscache_object_p->i_simdis = 0;
	}

	/* if fs thread not running */
	if (fscache_object_p->i_threaded == 0) {
		if (fscache_object_p->i_mounted) {
			dbug_print(("simdis", "thread not running"));
			ret = -1;
		} else {
			if (fscache_object_p->i_simdis)
				fscache_object_p->i_connected = 0;
			else
				fscache_object_p->i_connected = 1;
		}
		goto out;
	}

	/* get the attention of the thread */
	dbug_print(("info", "thread %d, killing %d with sigusr1",
	    thr_self(), fscache_object_p->i_threadid));
	xx = thr_kill(fscache_object_p->i_threadid, SIGUSR1);
	if (xx) {
		dbug_print(("simdis", "thr_kill failed %d, threadid %d",
		    xx, fscache_object_p->i_threadid));
		ret = -1;
	}

out:
	fscache_unlock(fscache_object_p);

	if (ret == 0) {
		for (;;) {
			dbug_print(("simdis", "     waiting for simulated %s",
			    strp));
			fscache_lock(fscache_object_p);
			tcon = fscache_object_p->i_connected;
			trec = fscache_object_p->i_reconcile;
			fscache_unlock(fscache_object_p);
			if (disconnect) {
				if (tcon == 0)
					break;
			} else {
				if ((tcon == 1) && (trec == 0))
					break;
			}
			cfsd_sleep(1);
		}
		dbug_print(("simdis", "DONE waiting for simulated %s", strp));
	} else {
		dbug_print(("simdis", "simulated %s failed %d", strp, ret));
	}

	dbug_leave("fscache_simdisconnect");
	return (ret);
}

/*
 *			fscache_unmount
 *
 * Description:
 *	Called to unmount the file system.
 * Arguments:
 * Returns:
 *	Returns 0 if the unmount is successful
 *		EIO if an error
 *		EBUSY if did not unmount because busy
 *		EAGAIN if umounted but should not unmount nfs mount
 *		ENOTSUP -  forced unmount is not supported by cachefs
 * Preconditions:
 */

int
fscache_unmount(cfsd_fscache_object_t *fscache_object_p, int flag)
{
	int xx;
	int ret = 0;

	dbug_enter("fscache_unmount");
	dbug_precond(fscache_object_p);

	fscache_lock(fscache_object_p);

	/* if there is a thread running */
	if (fscache_object_p->i_threaded) {
		/* do not bother unmounting if rolling the log */
		if (fscache_object_p->i_reconcile) {
			ret = EBUSY;
			goto out;
		}

		/* inform the thread to try the unmount */
		fscache_object_p->i_tryunmount = 1;
		fscache_object_p->i_modify++;

		/* get the attention of the thread */
		dbug_print(("info", "about to do umount kill"));
		xx = thr_kill(fscache_object_p->i_threadid, SIGUSR1);
		if (xx) {
			dbug_print(("error", "thr_kill failed %d, threadid %d",
			    xx, fscache_object_p->i_threadid));
			ret = EIO;
			goto out;
		}

		/* wait for the thread to wake us up */
		while (fscache_object_p->i_tryunmount) {
			xx = cond_wait(&fscache_object_p->i_cvwait,
			    &fscache_object_p->i_lock);
			dbug_print(("info", "cond_wait woke up %d %d",
			    xx, fscache_object_p->i_tryunmount));
		}

		/* if the file system is still mounted */
		if (fscache_object_p->i_mounted)
			ret = EBUSY;
	}

	/* else if there is no thread running */
	else {
		/* try to unmount the file system */
		if (umount2(fscache_object_p->i_mntpt, flag) == -1) {
			xx = errno;
			dbug_print(("info", "unmount failed %s",
			    strerror(xx)));
			if (xx == EBUSY)
				ret = EBUSY;
			else if (xx == ENOTSUP)
				ret = ENOTSUP;
			else
				ret = EIO;
		} else {
			fscache_object_p->i_mounted = 0;
			fscache_object_p->i_modify++;
		}
	}
out:
	fscache_unlock(fscache_object_p);
	dbug_leave("fscache_unmount");
	return (ret);
}

/*
 * -----------------------------------------------------------------
 *			fscache_server_alive
 *
 * Description:
 * Arguments:
 * Returns:
 * Preconditions:
 */
void
fscache_server_alive(cfsd_fscache_object_t *fscache_object_p,
	cfsd_kmod_object_t *kmod_object_p)
{

	int xx;
	cfs_fid_t rootfid;
	cred_t cr;
	cfs_vattr_t va;
	char tcmd[MAXPATHLEN * 4];

	dbug_enter("fscache_server_alive");

	dbug_precond(fscache_object_p);
	dbug_precond(kmod_object_p);

	for (;;) {
		/* wait for a little while */
		if (fscache_object_p->i_simdis == 0)
			cfsd_sleep(30);
		/* if simulating disconnect */
		fscache_lock(fscache_object_p);
		while (fscache_object_p->i_simdis &&
			!fscache_object_p->i_tryunmount) {
			dbug_print(("simdis", "before calling cond_wait"));
			xx = cond_wait(&fscache_object_p->i_cvwait,
			    &fscache_object_p->i_lock);
			dbug_print(("simdis", "cond_wait woke up %d %d",
			    xx, fscache_object_p->i_simdis));
		}
		fscache_unlock(fscache_object_p);

		if (fscache_object_p->i_tryunmount)
			break;

		/* see if the server is alive */
		if (fscache_pingserver(fscache_object_p) == -1) {
			/* dead server */
			continue;
		}

		/* try to mount the back file system if needed */
		if (fscache_object_p->i_backpath[0] == '\0') {
			dbug_precond(fscache_object_p->i_cfsopt[0]);
			dbug_precond(fscache_object_p->i_backfs[0]);
			dbug_precond(fscache_object_p->i_mntpt[0]);
			sprintf(tcmd,
			    "/usr/sbin/mount -F cachefs -o %s,slide,"
			    "remount %s %s",
			    fscache_object_p->i_cfsopt,
			    fscache_object_p->i_backfs,
			    fscache_object_p->i_mntpt);
			dbug_print(("info", "about to '%s'", tcmd));
			system(tcmd);
		}

		/* get the root fid of the file system */
		xx = kmod_rootfid(kmod_object_p, &rootfid);
		if (xx) {
			dbug_print(("info", "could not mount back fs %s %d",
			    fscache_object_p->i_backfs, xx));
			continue;
		}

		/* dummy up a fake kcred */
		memset(&cr, 0, sizeof (cred_t));

		/* try to get attrs on the root */
		xx = kmod_getattrfid(kmod_object_p, &rootfid, &cr, &va);
		if ((xx == ETIMEDOUT) || (xx == EIO)) {
			dbug_print(("info", "Bogus error %d", xx));
			continue;
		}
		break;
	}
	dbug_leave("fscache_server_alive");
}

/*
 *			fscache_pingserver
 *
 * Description:
 *	Trys to ping the nfs server to see if it is alive.
 * Arguments:
 * Returns:
 *	Returns 0 if it is alive, -1 if no answer.
 * Preconditions:
 */

int
fscache_pingserver(cfsd_fscache_object_t *fscache_object_p)
{

	static struct timeval TIMEOUT = { 25, 0 };
	CLIENT *clnt;
	enum clnt_stat retval;
	int ret = 0;
	char hostname[MAXPATHLEN * 2];
	char *cptr;

	dbug_enter("fscache_pingserver");
	dbug_precond(fscache_object_p);

	strcpy(hostname, fscache_object_p->i_backfs);
	if (cptr = strchr(hostname, ':'))
		*cptr = '\0';

	dbug_assert(cptr != NULL);
	dbug_print(("info", "remote host '%s' before clnt_create", hostname));

	dbug_print(("info", "before clnt_create"));
	/* XXX this takes 75 seconds to time out */
	/* XXX should use lower level routines to reduce overhead */
	clnt = clnt_create(hostname, NFS_PROGRAM, NFS_VERSION, "udp");
	if (clnt == NULL) {
		/* XXX what if this fails other than TIMEDOUT */
		/* clnt_pcreateerror(hostname); */
		dbug_print(("info", "clnt_create failed"));
		ret = -1;
	} else {
		dbug_print(("info", "before null rpc"));
		/* XXX this takes 45 seconds to time out */
		retval = clnt_call(clnt, 0, xdr_void, NULL, xdr_void, NULL,
		    TIMEOUT);
		if (retval != RPC_SUCCESS) {
			/* clnt_perror(clnt, "null rpc call failed"); */
			dbug_print(("info", "null rpc call failed %d", retval));
			ret = -1;
		}
		clnt_destroy(clnt);
	}
	dbug_leave("fscache_pingserver");
	return (ret);
}

/*
 *			fscache_roll
 *
 * Description:
 *	Rolls the contents of the log to the server.
 * Arguments:
 *	kmodp	interface to kernel functions
 * Returns:
 *	Returns 0 for success or ETIMEDOUT if a timeout error occurred.
 * Preconditions:
 *	precond(kmodp)
 */
int
fscache_roll(cfsd_fscache_object_t *fscache_object_p,
	cfsd_kmod_object_t *kmod_object_p)
{
	int error = 0;
	cfsd_logelem_object_t *logelem_object_p;
	char namebuf[MAXPATHLEN * 2];
	int xx;
	cfs_dlog_entry_t *entp;
	off_t next_offset;
	u_long curseq = 0;
	int eof = 0;
	char *xp;
	cfsd_logfile_object_t *logfile_object_p;
	cfsd_maptbl_object_t *maptbl_object_p;

	dbug_enter("fscache_roll");

	dbug_precond(fscache_object_p);
	dbug_precond(kmod_object_p);

	/* map in the log file */
	logfile_object_p = cfsd_logfile_create();

	sprintf(namebuf, "%s/%s/%s", fscache_object_p->i_cachepath,
	    fscache_object_p->i_name, CACHEFS_DLOG_FILE);
	xx = logfile_setup(logfile_object_p, namebuf, CFS_DLOG_ENTRY_MAXSIZE);
	if (xx) {
		if (xx == ENOENT) {
			cfsd_logfile_destroy(logfile_object_p);
			dbug_leave("fscache_roll");
			return (0);
		}
		fscache_fsproblem(fscache_object_p, kmod_object_p);
		cfsd_logfile_destroy(logfile_object_p);
		dbug_leave("fscache_roll");
		return (0);
	}

	fscache_lock(fscache_object_p);
	fscache_changes(fscache_object_p, 1);
	fscache_unlock(fscache_object_p);

	/* create a hashed mapping table for changes to cids */
	maptbl_object_p = cfsd_maptbl_create();
	sprintf(namebuf, "%s/%s/%s", fscache_object_p->i_cachepath,
	    fscache_object_p->i_name, CACHEFS_DMAP_FILE);
	xx = maptbl_setup(maptbl_object_p, namebuf);
	if (xx) {
		fscache_fsproblem(fscache_object_p, kmod_object_p);
		cfsd_logfile_destroy(logfile_object_p);
		cfsd_maptbl_destroy(maptbl_object_p);
		dbug_leave("fscache_roll");
		return (0);
	}

	/*
	 * lock is not needed because they are only used when
	 * rolling the log by fscache_roll and fscache_addagain
	 */
	fscache_object_p->i_again_offset = 0;
	fscache_object_p->i_again_seq = 0;

	/* Pass 1: collect all cid to fid mappings */
	next_offset = LOGFILE_ENTRY_START;
	for (;;) {
		/* get a pointer to the next record */
		xx = logfile_entry(logfile_object_p, next_offset, &entp);
		if (xx == 1)
			break;
		if (xx == -1) {
			fscache_fsproblem(fscache_object_p, kmod_object_p);
			cfsd_logfile_destroy(logfile_object_p);
			cfsd_maptbl_destroy(maptbl_object_p);
			dbug_leave("fscache_roll");
			return (0);
		}
		next_offset += entp->dl_len;

		/* skip record if not valid */
		if (entp->dl_valid != CFS_DLOG_VAL_COMMITTED)
			continue;

		/* create an object for the appropriate log type */
		logelem_object_p = NULL;
		switch (entp->dl_op) {
		case CFS_DLOG_CREATE:
		case CFS_DLOG_REMOVE:
		case CFS_DLOG_LINK:
		case CFS_DLOG_RENAME:
		case CFS_DLOG_MKDIR:
		case CFS_DLOG_RMDIR:
		case CFS_DLOG_SYMLINK:
		case CFS_DLOG_SETATTR:
		case CFS_DLOG_SETSECATTR:
		case CFS_DLOG_MODIFIED:
		case CFS_DLOG_TRAILER:
			break;

		case CFS_DLOG_MAPFID:
			dbug_print(("info", "mapfid"));
			logelem_object_p = cfsd_logelem_mapfid_create(
			    maptbl_object_p, logfile_object_p,
			    kmod_object_p);
			break;

		default:
			dbug_assert(0);
			fscache_fsproblem(fscache_object_p, kmod_object_p);
			break;
		}

		/* do not bother if ignoring the record */
		if (logelem_object_p == NULL)
			continue;

		/* debuggging */
		logelem_dump(logelem_object_p);

		/* roll the entry */
		xx = logelem_roll(logelem_object_p, (u_long *)NULL);
		if (xx) {
			fscache_fsproblem(fscache_object_p, kmod_object_p);
			cfsd_logelem_destroy(logelem_object_p);
			cfsd_maptbl_destroy(maptbl_object_p);
			cfsd_logfile_destroy(logfile_object_p);
			dbug_leave("fscache_roll");
			return (0);
		}

		/* mark record as completed */
		entp->dl_valid = CFS_DLOG_VAL_PROCESSED;
		xx = logfile_sync(logfile_object_p);
		if (xx) {
			fscache_fsproblem(fscache_object_p, kmod_object_p);
			cfsd_logelem_destroy(logelem_object_p);
			cfsd_maptbl_destroy(maptbl_object_p);
			cfsd_logfile_destroy(logfile_object_p);
			dbug_leave("fscache_roll");
			return (0);
		}

		/* destroy the object */
		cfsd_logelem_destroy(logelem_object_p);
	}

	/* Pass 2: modify the back file system */
	next_offset = LOGFILE_ENTRY_START;
	for (;;) {
		/* if we need the seq number of a deferred modify */
		if (fscache_object_p->i_again_offset &&
			(fscache_object_p->i_again_seq == 0)) {

			/* get a pointer to the next record */
			xx = logfile_entry(logfile_object_p,
			    fscache_object_p->i_again_offset, &entp);
			if (xx == 1)
				break;
			if (xx == -1) {
				fscache_fsproblem(fscache_object_p,
					kmod_object_p);
				cfsd_logfile_destroy(logfile_object_p);
				cfsd_maptbl_destroy(maptbl_object_p);
				dbug_leave("fscache_roll");
				return (0);
			}
			dbug_assert(entp->dl_op == CFS_DLOG_MODIFIED);
			fscache_object_p->i_again_seq = entp->dl_seq;
			dbug_assert(fscache_object_p->i_again_seq != 0);
		}

		/* get a pointer to the next record to process */
		if (!eof) {
			xx = logfile_entry(logfile_object_p, next_offset,
			    &entp);
			if (xx == 1) {
				eof = 1;
				curseq = ULONG_MAX;
			} else if (xx) {
				break;
			} else {
				curseq = entp->dl_seq;
			}
		}

		/* if its time to process a deferred modify entry */
		if (fscache_object_p->i_again_seq &&
		    (eof || (fscache_object_p->i_again_seq < entp->dl_seq))) {
			xx = logfile_entry(logfile_object_p,
			    fscache_object_p->i_again_offset, &entp);
			if (xx)
				break;
			dbug_assert(entp->dl_op == CFS_DLOG_MODIFIED);
			curseq = entp->dl_seq;
			fscache_object_p->i_again_offset =
			    entp->dl_u.dl_modify.dl_next;
			fscache_object_p->i_again_seq = 0;
			entp->dl_u.dl_modify.dl_next = -1;
		} else if (eof) {
			xx = 0;
			break;
		}

		/* else move the offset to the next record */
		else {
			next_offset += entp->dl_len;
		}

		/* skip record if not valid */
		if (entp->dl_valid != CFS_DLOG_VAL_COMMITTED)
			continue;

		/* process the record */
		xx = fscache_rollone(fscache_object_p, kmod_object_p,
		    maptbl_object_p, logfile_object_p, curseq);
		if (xx == ETIMEDOUT) {
			/* timeout error, back to disconnected */
			cfsd_maptbl_destroy(maptbl_object_p);
			cfsd_logfile_destroy(logfile_object_p);
			dbug_print(("info", "timeout error occurred"));
			dbug_leave("fscache_roll");
			return (xx);
		} else if (xx == EIO) {
			break;
		} else if (xx == EAGAIN) {
			continue;
		} else if (xx) {
			/* should never happen */
			dbug_assert(0);
			break;
		} else {
			/* mark record as completed */
			entp->dl_valid = CFS_DLOG_VAL_PROCESSED;
			xx = logfile_sync(logfile_object_p);
			if (xx)
				break;
		}
	}

	/* if an unrecoverable error occurred */
	if (xx) {
		dbug_print(("error", "error processing log file"));
		fscache_fsproblem(fscache_object_p, kmod_object_p);
	}

	/* dump stats about the hash table */
	maptbl_dumpstats(maptbl_object_p);

	/* dump stats about the log file */
	logfile_dumpstats(logfile_object_p);

	/* XXX debugging hack, rename the log files */
	sprintf(namebuf, "%s/%s/%s", fscache_object_p->i_cachepath,
	    fscache_object_p->i_name, CACHEFS_DLOG_FILE);
	xp = namebuf + strlen(namebuf) + 2;

	sprintf(xp, "%s/%s/%s.bak", fscache_object_p->i_cachepath,
	    fscache_object_p->i_name, CACHEFS_DLOG_FILE);
	xx = rename(namebuf, xp);

	sprintf(namebuf, "%s/%s/%s", fscache_object_p->i_cachepath,
	    fscache_object_p->i_name, CACHEFS_DMAP_FILE);
	xp = namebuf + strlen(namebuf) + 2;

	sprintf(xp, "%s/%s/%s.bak", fscache_object_p->i_cachepath,
	    fscache_object_p->i_name, CACHEFS_DMAP_FILE);
	xx = rename(namebuf, xp);

	/* delete the log file */
	/* XXX */

	cfsd_maptbl_destroy(maptbl_object_p);
	cfsd_logfile_destroy(logfile_object_p);
	dbug_leave("fscache_roll");
	return (error);
}

/*
 *			fscache_rollone
 *
 * Description:
 * Arguments:
 *	kmodp
 *	tblp
 *	lfp
 * Returns:
 *	Returns ...
 * Preconditions:
 *	precond(kmodp)
 *	precond(tblp)
 *	precond(lfp)
 */
int
fscache_rollone(cfsd_fscache_object_t *fscache_object_p,
	cfsd_kmod_object_t *kmod_object_p,
	cfsd_maptbl_object_t *maptbl_object_p,
	cfsd_logfile_object_t *logfile_object_p,
	u_long seq)
{
	cfsd_logelem_object_t *logelem_object_p = NULL;
	cfs_dlog_entry_t *entp;
	int xx;
	char *strp;

	dbug_enter("fscache_rollone");

	dbug_precond(fscache_object_p);
	dbug_precond(kmod_object_p);
	dbug_precond(maptbl_object_p);
	dbug_precond(logfile_object_p);

	entp = logfile_object_p->i_cur_entry;

	/* create an object for the appropriate log type */
	switch (entp->dl_op) {
	case CFS_DLOG_CREATE:
		dbug_print(("info", "create"));
		logelem_object_p = cfsd_logelem_create_create(maptbl_object_p,
		    logfile_object_p, kmod_object_p);
		break;

	case CFS_DLOG_REMOVE:
		dbug_print(("info", "remove"));
		logelem_object_p = cfsd_logelem_remove_create(maptbl_object_p,
		    logfile_object_p, kmod_object_p);
		break;

	case CFS_DLOG_LINK:
		dbug_print(("info", "link"));
		logelem_object_p = cfsd_logelem_link_create(maptbl_object_p,
		    logfile_object_p, kmod_object_p);
		break;

	case CFS_DLOG_RENAME:
		dbug_print(("info", "rename"));
		logelem_object_p = cfsd_logelem_rename_create(maptbl_object_p,
		    logfile_object_p, kmod_object_p);
		break;

	case CFS_DLOG_MKDIR:
		dbug_print(("info", "mkdir"));
		logelem_object_p = cfsd_logelem_mkdir_create(maptbl_object_p,
		    logfile_object_p, kmod_object_p);
		break;

	case CFS_DLOG_RMDIR:
		dbug_print(("info", "rmdir"));
		logelem_object_p = cfsd_logelem_rmdir_create(maptbl_object_p,
		    logfile_object_p, kmod_object_p);
		break;

	case CFS_DLOG_SYMLINK:
		dbug_print(("info", "symlink"));
		logelem_object_p = cfsd_logelem_symlink_create(maptbl_object_p,
		    logfile_object_p, kmod_object_p);
		break;

	case CFS_DLOG_SETATTR:
		dbug_print(("info", "setattr"));
		logelem_object_p = cfsd_logelem_setattr_create(maptbl_object_p,
		    logfile_object_p, kmod_object_p);
		break;

	case CFS_DLOG_SETSECATTR:
		dbug_print(("info", "setsecattr"));
		logelem_object_p = cfsd_logelem_setsecattr_create(
		    maptbl_object_p, logfile_object_p, kmod_object_p);
		break;

	case CFS_DLOG_MODIFIED:
		dbug_print(("info", "modified"));
		logelem_object_p = cfsd_logelem_modified_create(maptbl_object_p,
		    logfile_object_p, kmod_object_p);
		break;

	case CFS_DLOG_MAPFID:
		dbug_print(("info", "mapfid"));
		break;

	case CFS_DLOG_TRAILER:
		dbug_print(("info", "trailer"));
		break;

	default:
		dbug_assert(0);
		dbug_leave("fscache_rollone");
		return (EIO);
	}

	/* do not bother if ignoring the record */
	if (logelem_object_p == NULL) {
		dbug_print(("info", "record ignored"));
		dbug_leave("fscache_rollone");
		return (0);
	}

	/* XXX debugging */
	logelem_dump(logelem_object_p);

	/* roll the entry */
	xx = logelem_roll(logelem_object_p, &seq);

	strp = logelem_object_p->i_messagep;
	if (strp) {
		write(fscache_object_p->i_ofd, strp, strlen(strp));
		dbug_print(("conflict", "%s", strp));
	}

	if (xx == EAGAIN) {
		dbug_assert(entp->dl_op == CFS_DLOG_MODIFIED);
		xx = fscache_addagain(fscache_object_p, logfile_object_p, seq);
		if (xx == 0)
			xx = EAGAIN;
	}

	/* destroy the object */
	cfsd_logelem_destroy(logelem_object_p);

	dbug_leave("fscache_rollone");
	return (xx);
}

/*
 *			fscache_addagain
 *
 * Description:
 * Arguments:
 *	lfp
 * Returns:
 *	Returns ...
 * Preconditions:
 *	precond(lfp)
 */
int
fscache_addagain(cfsd_fscache_object_t *fscache_object_p,
	cfsd_logfile_object_t *logfile_object_p,
	u_long nseq)
{
	int xx;
	cfs_dlog_entry_t *entp;
	off_t noffset;
	off_t prevoff = 0;
	off_t toff;

	dbug_enter("fscache_addagain");

	dbug_precond(fscache_object_p);
	dbug_precond(logfile_object_p);

	entp = logfile_object_p->i_cur_entry;

	noffset = logfile_object_p->i_cur_offset;

	dbug_assert(entp->dl_op == CFS_DLOG_MODIFIED);
	dbug_assert(nseq);

	/* both set or both zero */
	dbug_assert((!fscache_object_p->i_again_seq ^
	    !fscache_object_p->i_again_offset) == 0);

	entp->dl_seq = nseq;
	/* simple case, first one on list */
	if ((fscache_object_p->i_again_seq == 0) ||
	    (nseq < fscache_object_p->i_again_seq)) {
		entp->dl_u.dl_modify.dl_next = fscache_object_p->i_again_offset;
		fscache_object_p->i_again_seq = nseq;
		fscache_object_p->i_again_offset = noffset;
		dbug_leave("fscache_addagain");
		return (0);
	}

	/* Search until we find the element on the list prior to the */
	/* insertion point. */
	for (toff = fscache_object_p->i_again_offset; toff != 0;
		toff = entp->dl_u.dl_modify.dl_next) {
		/* get pointer to next element on the list */
		xx = logfile_entry(logfile_object_p, toff, &entp);
		if (xx) {
			dbug_leave("fscache_addagain");
			return (xx);
		}
		dbug_assert(entp->dl_op == CFS_DLOG_MODIFIED);

		/* done if we found the element after the insertion point */
		if (nseq < entp->dl_seq)
			break;
		prevoff = toff;
	}
	dbug_assert(prevoff);

	/* get pointer to element prior to the insertion point */
	xx = logfile_entry(logfile_object_p, prevoff, &entp);
	if (xx) {
		dbug_leave("fscache_addagain");
		return (xx);
	}
	dbug_assert(entp->dl_op == CFS_DLOG_MODIFIED);
	dbug_assert(entp->dl_u.dl_modify.dl_next == toff);

	/* set element to point to our new element */
	entp->dl_u.dl_modify.dl_next = noffset;

	/* get pointer to our new element */
	xx = logfile_entry(logfile_object_p, noffset, &entp);
	if (xx) {
		dbug_leave("fscache_addagain");
		return (xx);
	}
	dbug_assert(entp->dl_op == CFS_DLOG_MODIFIED);

	/* set it to point to next link or end of list */
	entp->dl_u.dl_modify.dl_next = toff;

	/* return success */
	dbug_leave("fscache_addagain");
	return (0);
}

/*
 *			fscache_fsproblem
 *
 * Description:
 * Arguments:
 *	kmodp
 * Returns:
 * Preconditions:
 *	precond(kmodp)
 */
void
fscache_fsproblem(cfsd_fscache_object_t *fscache_object_p,
	cfsd_kmod_object_t *kmod_object_p)
{
#if 0
	int xx;
#endif

	dbug_enter("fscache_fsproblem");

	dbug_precond(fscache_object_p);
	dbug_precond(kmod_object_p);

#if 0
	/* first try to put all modified files in lost+found */
	xx = kmod_lostfoundall(kmod_object_p);
	if (xx) {
		/* if that failed, put file system in read-only mode */
		kmod_rofs(kmod_object_p);
#endif
		fscache_lock(fscache_object_p);
		fscache_object_p->i_disconnectable = 0;
		fscache_object_p->i_modify++;
		fscache_unlock(fscache_object_p);
#if 0
	}
#endif
	dbug_leave("fscache_fsproblem");
}

/*
 *			fscache_changes
 *
 * Description:
 *	Used to specify whether or not there are changes to roll to the
 *	server.
 * Arguments:
 *	tt
 * Returns:
 * Preconditions:
 */
void
fscache_changes(cfsd_fscache_object_t *fscache_object_p, int tt)
{
	dbug_enter("fscache_changes");
	dbug_precond(fscache_object_p);
	fscache_object_p->i_changes = tt;
	fscache_object_p->i_modify++;
	dbug_leave("fscache_changes");
}
