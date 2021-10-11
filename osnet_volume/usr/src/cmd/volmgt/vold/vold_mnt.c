/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vold_mnt.c	1.25	99/07/13 SMI"

#include	<stdio.h>
#include	<fcntl.h>
#include	<stdlib.h>
#include	<string.h>
#include	<unistd.h>
#include	<sys/stat.h>
#include	<sys/mnttab.h>
#include	<sys/mntent.h>
#include	<syslog.h>
#include	<sys/wait.h>
#include	<time.h>

#include	"vold.h"


static struct 	mnttab *dupmnttab(struct mnttab *);
static void	freemnttab(struct mnttab *);
static int	openmnttab(void);
static int	readmnttab(void);


struct mntlist {
	struct mnttab	*mntl_mnt;
	struct mntlist	*mntl_next;
};

static time_t		mnttab_last_mtime = 0;	/* last seen MNTTAB mtime */
static struct mntlist	*mntl_head = NULL;	/* in-core mount table */
static FILE		*mnt_fp = NULL;


#ifndef TRUE
#define	TRUE	(-1)
#endif

#ifndef	FALSE
#define	FALSE	(0)
#endif

#define	UMOUNT_PATH	"/etc/umount"
#define	UMOUNT_PROG	"umount"


/*
 * Check to see if "path" is used as a special device in a mount.
 * Will match partial paths (e.g. / will match everything).  Returns
 * the first matched path.
 */
char *
mnt_special_test(char *path)
{
	struct mntlist	*mntl;
	char		*rval = NULL;
	int		len;


	if (!openmnttab()) {
		return (NULL);
	}

	if (!readmnttab()) {
		return (NULL);
	}

	len = strlen(path);

	for (mntl = mntl_head; mntl; mntl = mntl->mntl_next) {
		if (strncmp(path, mntl->mntl_mnt->mnt_special, len) == 0) {
			rval = strdup(mntl->mntl_mnt->mnt_special);
			break;
		}
	}

	return (rval);
}


struct mnttab *
mnt_mnttab(char *special)
{
	struct mntlist	*mntl;
	struct mnttab	*rval = NULL;


	if (!openmnttab()) {
		return (NULL);
	}

	if (!readmnttab()) {
		return (NULL);
	}

	for (mntl = mntl_head; mntl; mntl = mntl->mntl_next) {
		if (strcmp(special, mntl->mntl_mnt->mnt_special) == 0) {
			rval = dupmnttab(mntl->mntl_mnt);
			break;
		}
	}
	return (rval);
}


void
mnt_free_mnttab(struct mnttab *mnt)
{
	freemnttab(mnt);
}

/*
 * rename a special device in /etc/mnttab
 * place holder that can probably go away.
 * probably need to trigger something here
 * but the original code wouldn't work in this
 * case.
 */
void
mnt_special_rename(char *from, char *to)
{
}

/*
 * open mnttab
 */
static int
openmnttab(void)
{
	if (mnt_fp == NULL) {
		/* /etc/mnttab can only be opened for reading */
		if ((mnt_fp = fopen(MNTTAB, "r")) == NULL) {
			warning(gettext("can't open %s for r/w; %m\n"),
			    MNTTAB);
			return (FALSE);
		}
		(void) fcntl(fileno(mnt_fp), F_SETFD, 1); /* close on exec */
	}
	rewind(mnt_fp);
	return (TRUE);
}


/*
 * read the mount table into memory. caller must manage the lock.
 */
static int
readmnttab(void)
{
	struct mntlist	*mntl;
	struct mntlist	*mntl_prev;
	struct mntlist	*mntl_next = NULL;
	struct stat	sb;
	struct mnttab	mnt;
	char		buf1[CTBSIZE];
	char		buf2[CTBSIZE];


	/*
	 * If we have already have an incore copy, make sure it's
	 * more recent than the file on disk.
	 */
	if (mntl_head != NULL) {

		if (stat(MNTTAB, &sb) < 0) {
			warning(gettext("can't stat %s; %m\n"), MNTTAB);
			return (FALSE);
		}

		/*
		 * looks like it's fine in-core.
		 */

		debug(10,
	"readmnttab: mnttab: previous mtime %24.24s, mtime now %24.24s\n",
		    ctime_r(&mnttab_last_mtime, buf1, CTBSIZE),
		    ctime_r(&sb.st_mtime, buf2, CTBSIZE));

		if (sb.st_mtime == mnttab_last_mtime) {
			/* the file hasn't been modified */
			debug(10, "readmnttab: using internal mnttab\n");
			return (TRUE);
		}

		/*
		 * looks like we need to reread it.  free previously
		 * allocated memory.
		 */

		mnttab_last_mtime = sb.st_mtime; /* save last mod time */

		debug(7,
	"readmnttab: dumping in-memory mnttab (rereading file)\n");

		for (mntl = mntl_head; mntl; mntl = mntl_next) {
			mntl_next = mntl->mntl_next;
			freemnttab(mntl->mntl_mnt);
			free(mntl);
		}
		mntl_head = NULL;

		/*
		 * Unfortunatly, we have to close /etc/mnttab and
		 * then reopen it...
		 */
		(void) fclose(mnt_fp);
		mnt_fp = NULL;
		if (!openmnttab()) {
			return (FALSE);
		}
	}

	/*
	 * save the "last time read".  t one to avoid
	 * roundoff errors and be safe.
	 */
	mntl_prev = 0;

	/*
	 * Read the mount table into memory.
	 */
	rewind(mnt_fp);
	while (getmntent(mnt_fp, &mnt) == 0) {
		mntl = (struct mntlist *)malloc(sizeof (*mntl));
		if (mntl_head == NULL) {
			mntl_head = mntl;
		} else {
			mntl_prev->mntl_next = mntl;
		}
		mntl_prev = mntl;
		mntl->mntl_next = NULL;
		mntl->mntl_mnt = dupmnttab(&mnt);
	}

	return (TRUE);
}


static struct mnttab *
dupmnttab(struct mnttab *mnt)
{
	struct mnttab *new;


	if ((new = (struct mnttab *)malloc(sizeof (*new))) == NULL) {
		goto alloc_failed;
	}
	(void) memset((char *)new, 0, sizeof (*new));

	/*
	 * work around problem where '-' in /etc/mnttab for
	 * special device turns to NULL which isn't expected
	 */
	if (mnt->mnt_special == NULL)
		mnt->mnt_special = "-";
	if ((new->mnt_special = strdup(mnt->mnt_special)) == NULL) {
		goto alloc_failed;
	}
	if ((new->mnt_mountp = strdup(mnt->mnt_mountp)) == NULL) {
		goto alloc_failed;
	}
	if ((new->mnt_fstype = strdup(mnt->mnt_fstype)) == NULL) {
		goto alloc_failed;
	}

	/* mnt_mntopts and mnt_time can conceivably be null */
	if (mnt->mnt_mntopts != NULL) {
		if ((new->mnt_mntopts = strdup(mnt->mnt_mntopts)) == NULL) {
			goto alloc_failed;
		}
	}
	if (mnt->mnt_time != NULL) {
		if ((new->mnt_time = strdup(mnt->mnt_time)) == NULL) {
			goto alloc_failed;
		}
	}

	return (new);

alloc_failed:
	warning(gettext("dumpmnttab: no memory\n"));
	return (NULL);
}


static void
freemnttab(struct mnttab *mnt)
{
	free(mnt->mnt_special);
	free(mnt->mnt_mountp);
	free(mnt->mnt_fstype);
	if (mnt->mnt_mntopts != NULL) {
		free(mnt->mnt_mntopts);
	}
	if (mnt->mnt_time) {
		free(mnt->mnt_time);
	}
	free(mnt);
}


/*
 * call umount on each volume we've mounted, then umount our root dir
 *
 * if any of the umounts of managed volumes fails, ignore them, but return
 * the status of umounting our root dir
 *
 * return 0 for success, else non-zero
 *
 * ignore the return codes from truning to unmount managed mendia (it
 * may be busy), but return the status from trying to unmount our root dir
 */
int
umount_all(char *root_dir)
{
	static int	call_umount(char *);
	uint_t		len = strlen(root_dir);
	struct mntlist	*mntl;
	int		res = 0;



	if (!openmnttab()) {
		goto dun;
	}

	if (!readmnttab()) {
		goto dun;
	}

	/* scan for mnt entries that use our root dir */
	for (mntl = mntl_head; mntl; mntl = mntl->mntl_next) {
		if (strncmp(root_dir, mntl->mntl_mnt->mnt_special, len) == 0) {
			(void) call_umount(mntl->mntl_mnt->mnt_mountp);


		}
	}

	res = call_umount(root_dir);
dun:
	return (res);
}


/*
 * call the umount program for the specified directory
 *
 * called from child during program termination (e.g., so debug(), et. al.,
 *	can't be used)
 *
 * return 0 for success, else non-zero
 */
static int
call_umount(char *dir)
{
	pid_t	pid;
	int	stat;
	int	fd;
	int	ret_val = 1;





	if ((pid = fork1()) < 0) {
		goto dun;		/* no processes? */
	}

	if (pid == 0) {
		/* the child -- call umount */

		/* redirect stdout/stderr to the bit bucket */
		if ((fd = open("/dev/null", O_RDWR)) >= 0) {
			(void) dup2(fd, fileno(stdout));
			(void) dup2(fd, fileno(stderr));
		}

		/* now call umount to do the real work */
		(void) execl(UMOUNT_PATH, UMOUNT_PROG, dir, 0);

		/* oh oh -- shouldn't reach here! */
		syslog(LOG_ERR,
		    gettext("exec of \"%s\" on \"%s\" failed; %m\n"),
		    UMOUNT_PATH, dir);
		exit(1);
	}

	/* the parent -- wait for the child */
	if (waitpid(pid, &stat, 0) != pid) {
		/* couldn't get child status -- return error */
		goto dun;
	}

	/* get return value */
	if (WIFEXITED(stat) && (WEXITSTATUS(stat) == 0)) {
		ret_val = 0;		/* child met with success */
	}

	/* return status of child */
dun:
	return (ret_val);
}
