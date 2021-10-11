/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)roll_log.c	1.16	99/06/14 SMI"

/*
 * This file contains functions that allow applications to roll the log.
 * It is intended for use by applications that open a raw device with the
 * understanding that it contains a Unix File System.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/filio.h>
#include <sys/mnttab.h>
#include <sys/mntent.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fs/ufs_mount.h>
#include <sys/fs/ufs_log.h>
#include <libintl.h>
#include "roll_log.h"

#define	SYSERR		(-1)

/*
 * Structure definitions:
 */

typedef struct log_info {
	char *li_blkname;	/* Path of block device. */
	char *li_mntpoint;	/* Path of mounted device. */
	char *li_tmpmp;		/* Temporary mount point. */
} log_info_t;

/*
 * Static function declarations:
 */

static rl_result_t	is_mounted(log_info_t *lip, char *dev);
static void		cleanup(log_info_t *lip);
static rl_result_t	make_mp(log_info_t *lip);
static rl_result_t	rlflush(log_info_t *lip);
static rl_result_t	rlmount(log_info_t *lip);

/*
 * NAME
 *	rl_roll_log
 *
 * SYNOPSIS
 *	rl_roll_log(const char * block_dev)
 *
 * DESCRIPTION
 *	Roll the log for the block device "block_dev".
 */

rl_result_t
rl_roll_log(char *bdev)
{
	log_info_t		li;
	rl_result_t		rv = RL_SUCCESS;

	(void) memset((void *)&li, 0, (size_t)sizeof (li));
	if (is_mounted(&li, bdev) == RL_TRUE) {
		rv = rlflush(&li);
	} else {
		/*
		 * Device appears not to be mounted.
		 * We need to mount the device read
		 * only.  This will cause the log to be rolled, then we can
		 * unmount the device again.  To do the mount, we need to
		 * create a temporary directory, and then remove it when we
		 * are done.
		 */
		rv = make_mp(&li);
		switch (rv) {
		case RL_CORRUPT:
			/* corrupt mnttab - the file sys really was mounted */
			rv = rlflush(&li);
			break;
		case RL_SUCCESS:
			if (rlmount(&li) == RL_SUCCESS) {
				(void) umount(li.li_blkname);
			}
			break;
		}
	}
	cleanup(&li);
	return (rv);
}

/*
 * Static function definitions:
 */

/*
 * NAME
 *	cleanup
 *
 * SYNOPSIS
 *	cleanup(log_infop)
 *
 * DESCRIPTION
 *	Remove the temporary mount directroy and free the dynamically
 *	allocated memory that is pointed to by log_infop.
 */

static void
cleanup(log_info_t *lip)
{
	if (lip->li_blkname != (char *)NULL) {
		free(lip->li_blkname);
		lip->li_blkname = (char *)NULL;
	}
	if (lip->li_mntpoint != (char *)NULL) {
		free(lip->li_mntpoint);
		lip->li_mntpoint = (char *)NULL;
	}
	if (lip->li_tmpmp != (char *)NULL) {
		(void) rmdir(lip->li_tmpmp);
		free(lip->li_tmpmp);
		lip->li_tmpmp = (char *)NULL;
	}
}

/*
 * NAME
 *	is_mounted
 *
 * SYNOPSIS
 *	is_mounted(log_infop, dev)
 *
 * DESCRIPTION
 *	Determine if device dev is mounted, and return RL_TRUE if it is.
 *	As a side effect, li_blkname is set to point the the full path
 *	names of the block device.  Memory for this path is dynamically
 *	allocated and must be freed by the caller.
 */

extern char *getfullblkname(char *);

static rl_result_t
is_mounted(log_info_t *lip, char *dev)
{

	struct mnttab		mntbuf;
	FILE			*mnttable;
	rl_result_t		rv = RL_FALSE;

	/* Make sure that we have the full path name. */
	lip->li_blkname = getfullblkname(dev);
	if (lip->li_blkname == NULL)
		lip->li_blkname = strdup(dev);

	/* Search mnttab to see if it device is mounted. */
	if ((mnttable = fopen(MNTTAB, "r")) == NULL)
		return (rv);
	while (getmntent(mnttable, &mntbuf) == NULL) {
		if (strcmp(mntbuf.mnt_fstype, MNTTYPE_UFS) == 0) {
			/* Entry is UFS */
			if ((strcmp(mntbuf.mnt_mountp, dev) == 0) ||
			    (strcmp(mntbuf.mnt_special, lip->li_blkname)
				== 0) ||
			    (strcmp(mntbuf.mnt_special, dev) == 0)) {
				lip->li_mntpoint = strdup(mntbuf.mnt_mountp);
				rv = RL_TRUE;
				break;
			}
		}
	}
	(void) fclose(mnttable);


	return (rv);
}

/*
 * NAME
 *	make_mp
 *
 * SYNOPSIS
 *	make_mp(loginfop)
 *
 * DESCRIPTION
 *	Create a temporary directory to be used as a mount point.  li_tmpmp
 *	will be set to the path of the mount point.  Memory pointed to by
 *	li_tmpmp should be freed by the caller.
 */

static rl_result_t
make_mp(log_info_t *lip)
{
	size_t			i;
	rl_result_t		rv = RL_FAIL;
	/*
	 * Note tmp_dir_list[] should all be directories in the
	 * original root file system.
	 */
	static const char 	*tmp_dir_list[] = {
							"/tmp/",
							"/var/tmp/",
							"/",
						};
	char			tmp_dir[MAXPATHLEN + 1];
	static size_t		list_len = sizeof (tmp_dir_list) /
						sizeof (const char *);
	int			merr;

	/* Attempt to make the directory. */
	for (i = 0; i < list_len; i++) {
		/*
		 * Create directory base name.  Start is with a ".",
		 * so that it will be less noticable.
		 */
		(void) sprintf(tmp_dir, "%s.rlg.XXXXXX", tmp_dir_list[i]);
		(void) mktemp(tmp_dir);
		if (mkdir(tmp_dir, 0) == SYSERR) {
			merr = errno;
			continue;
		}
		rv = RL_SUCCESS;
		lip->li_tmpmp = strdup(tmp_dir);
		break;
	}

	/* Get some help if we cannot make the directory. */
	if (rv != RL_SUCCESS) {
		/*
		 * If we get a read only filesystem failure (EROFS)
		 * to make a directory in "/", then we must be fsck'ing
		 * at boot with a incorrect mnttab.
		 *
		 * Just return RL_CORRUPT to indicate it really
		 * was mounted.
		 */
		if (merr == EROFS) {
			lip->li_mntpoint = strdup("/");
			return (RL_CORRUPT);
		}

		(void) fprintf(stderr, gettext(
			"Unable to create temporary "
			"directory in any of the directories listed "
			"below:\n"));
		for (i = 0; i < list_len; i++) {
			(void) fprintf(stderr, "\t%s\n", tmp_dir_list[i]);
		}
		(void) fprintf(stderr, gettext(
			"Please correct this problem "
			"and rerun the program.\n"));
	}

	return (rv);
}

/*
 * NAME
 *	rlflush
 *
 * SYNOPSIS
 *	rlflush(log_infop)
 *
 * DESCRIPTION
 *	Open the mount point of the file system (li_mntpoint) to get a
 *	file descriptor.  Issue the _FIOFFS ioctl to flush the file system
 *	and then close the device.
 */

static rl_result_t
rlflush(log_info_t *lip)
{
	int			fd;	/* File descriptor. */
	rl_result_t		rv = RL_SUCCESS;

	if ((fd = open(lip->li_mntpoint, O_RDONLY)) == SYSERR) {
		return (RL_SYSERR);
	}
	if (ioctl(fd, _FIOFFS, NULL) == SYSERR) {
		rv = RL_SYSERR;
	}
	(void) close(fd);
	return (rv);
}

/*
 * NAME
 *	rlmount
 *
 * SYNOPSIS
 *	rlmount(log_infop)
 *
 * DESCRIPTION
 *	Mount the device specified by li_blkname on li_tmpmp.
 */

static rl_result_t
rlmount(log_info_t *lip)
{
	struct ufs_args		args;
	rl_result_t		rv = RL_SUCCESS;

	args.flags = 0;
	if (mount(lip->li_blkname, lip->li_tmpmp, MS_RDONLY | MS_DATA,
		MNTTYPE_UFS, &args, sizeof (args)) == SYSERR)
		rv = RL_SYSERR;
	return (rv);
}
