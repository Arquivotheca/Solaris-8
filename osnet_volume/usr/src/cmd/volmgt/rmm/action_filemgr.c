/*
 * Copyright (c) 1995-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)action_filemgr.c	1.19	97/04/01 SMI"

/*
 * action_filemgr -- filemgr interface routines for rmmount
 *
 * This shared lib allows rmmount to communicate with filemgr.
 * This is done by communicating over a named pipe (see defines for
 * REM_DIR and NOTIFY_NAME).
 *
 * For insertion, a file is placed in REM_DIR named after the symbolic
 * name for the media (e.g. "cdrom0").  This file contains the mount
 * point of the media and the device name where it's located.  We then
 * send a "signal" over the named pipe (NOTIFY_NAME), which instruct
 * filemgr to look for new files in REM_DIR.
 *
 * For CD-ROMs we only notify filemgr for media that are mounted (i.e. *not*
 * for data-only CDs such as music CDs).  For floppies we notify filemgr
 * even if the floppy isn't mounted, since it'll probably want to
 * format it.
 *
 * The following environment variables must be present:
 *
 *	VOLUME_MEDIATYPE	media type (e.g. "cdrom" or "floppy")
 *	VOLUME_ACTION		action to take (e.g. "insert", "eject")
 *	VOLUME_SYMDEF		symbolic name (e.g. "cdrom0", "floppy1")
 *	VOLUME_NAME		volume name (e.g. "unnamed_cdrom")
 */


#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/dkio.h>
#include	<sys/cdio.h>
#include	<sys/vtoc.h>
#include	<sys/param.h>
#include	<rpc/types.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<fcntl.h>
#include	<string.h>
#include	<dirent.h>
#include	<rmmount.h>
#include	<signal.h>

/*
 * Tell file manager about the new media.
 */


/* for debug messages -- from rmmount */
extern char	*prog_name;
extern pid_t	prog_pid;


#define	REM_DIR		"/tmp/.removable"	/* dir where filemgr looks */
#define	NOTIFY_NAME	"notify"		/* named pipe to talk over */


static int	insert_media(struct action_arg **, char *);
static void	eject_media(struct action_arg **, char *);
static int	notify_clients(void);

extern void	dprintf(const char *, ...);
extern int	makepath(char *, mode_t);


/*ARGSUSED*/

/*
 * pop_dir: go back to the directory pointed to by fd, bail out on
 * failure,
 * close the fd
 */
static void
pop_dir(int fd)
{
	if (fchdir(fd) < 0) {
		dprintf("action_filemgr: failed to pop dir\n");
		exit(1);
	}
	(void) close(fd);
}


/*
 * In: directory name
 * Out: -1 or file descriptor pointing to previous wd
 * If return value other than -1, then the cwd is equal to
 *  the argument
 */
static int
pushdir_and_check(const char *dir)
{
	int		cwdfd;
	struct stat	lbuf;

	/*
	 * Determine if dir is a symlink, if it is
	 * remove it and return -1 telling the dir
	 * does not exist
	 */

	if (lstat(dir, &lbuf) < 0) {
		dprintf("action_filemgr: %s does not exist\n", dir);
		return (-1);
	}

	if (!(S_ISDIR(lbuf.st_mode))) {
		/* this is not a dir, remove it! */
		dprintf("action_filemanager:%s:is not a dir\n", dir);
		/* remove it ! */
		(void) remove(dir);
		return (-1);
	}

	/*
	 * now we know that dir exists and it is legal dir
	 * calling fcn() expects to be in 'dir' upon successful
	 * return
	 */

	if ((cwdfd = open(".", O_RDONLY)) < 0) {
		return (-1);
	}

	if (chdir(dir) < 0) {
		(void) close(cwdfd);
		return (-1);
	}

	return (cwdfd);
}

/* ARGSUSED1 */
int
action(struct action_arg **aa, int argc, char **argv)
{
	char		*media_type = getenv("VOLUME_MEDIATYPE");
	char		*atype = getenv("VOLUME_ACTION");
	int		rval;


	dprintf("action_filemgr: media type '%s'\n", media_type);

	if (strcmp(atype, "insert") == 0) {
		/*
		 * check media
		 * don't notify file manager if there is nothing to tell
		 * (e.g. with a music CD)
		 */
		rval = insert_media(aa, media_type);
		if (rval == FALSE)
			return (FALSE);
	} else if (strcmp(atype, "eject") == 0) {
		eject_media(aa, media_type);
	}

	rval = notify_clients();

	/*
	 * if it's eject, always return false because we want
	 * the other actions to happen.
	 */
	if (strcmp(atype, "eject") == 0) {
		return (FALSE);
	}

	return (rval);
}


static int
insert_media(struct action_arg **aa, char *media_type)
{
	char		*symdev = getenv("VOLUME_SYMDEV");
	FILE		*fp;
	char		*rdev = NULL;
	char		*mountp = NULL;
	char		*s;
	int		ai;
	int		cwdfd;
	int		fd;



	/* scan all supplied pieces, stopping at the first mounted one */
	for (ai = 0; aa[ai]->aa_path; ai++) {

		if (aa[ai]->aa_mountpoint == NULL) {
			continue;	/* not mounted -- keep looking */
		}

		/* found a mounted piece */

		/* save a copy of the mount directory name */
		mountp = strdup(aa[ai]->aa_mountpoint);

		/* save the raw device name (if any) */
		if (aa[ai]->aa_rawpath) {
			rdev = aa[ai]->aa_rawpath;
		} else {
			rdev = "none";
		}

		/*
		 * This gets rid of the partition name (if any).
		 * We do this so that filemgr is positioned
		 * above the partitions.
		 */
		if (aa[ai]->aa_partname != NULL) {
			if ((s = strrchr(mountp, '/')) != NULL) {
				*s = '\0';
			}
		}
		break;
	}

	/* if no mount point found */
	if (mountp == NULL) {

		/* skip telling filemgr about unmounted CD-ROMs */
		if (strcmp(media_type, "cdrom") == 0) {
			return (FALSE);			/* all done */
		}

		/* use the volume name as the "mount point" entry */
		mountp = strdup(getenv("VOLUME_NAME"));

		/* save the raw device name (if any) */
		if (aa[0]->aa_rawpath) {
			rdev = aa[0]->aa_rawpath;
		} else {
			rdev = "none";
		}
	}

	/* time to notify filemgr of new media */

	/*
	 * We have to do something very difficult here.
	 * We need to create a file as root in a world writable
	 * directory that itself resides in a worldwritable directory.
	 *
	 * We take the two possible race conditions one at the time.
	 *
	 * Because the directory can be changed at any time, we
	 * verify that we actually have a proper secure dir., if
	 * we remove a *non dir*, recreate it as a legal dir, we
	 * then check it again, if it has been changed again to
	 * a non-dir, filemgr will not be able to view cdroms etc.
	 * After we have ;
	 * we then follow by creating the file *in the current directory*,
	 * using a *relative* pathname.
	 */

	/*
	 * Thwarting race condition #1
	 *   if REM_DIR exists, besure it is not a symlink.
	 *   if it is, it will be removed and try to create
	 *   it again. Be very sure that new REM_DIR is also
	 *   not a symlink. If it is, it is gone and we return
	 *   without communicating with filemgr. This can only
	 *   happen if a security breach is being attempted by
	 *   a pgm running in a loop to create REM_DIR as a symlink.
	 */
	if ((cwdfd = pushdir_and_check(REM_DIR)) < 0) {
		/* filemgr needs to creat/write here */
		(void) makepath(REM_DIR, 0777);
		if ((cwdfd = pushdir_and_check(REM_DIR)) < 0) {
			goto free;
		}
	}

	/*
	 * Thwarting race condition #2
	 *
	 * Safely create the file that filemgr will examine in
	 * the current working directory
	 * First we remove whatever is still there, use remove() as
	 * it safely removes directories.
	 * Then we use O_CREAT|O_EXCL which doesn't follow symlinks and
	 * requires non existance of the file, so we make a new file
	 * that actually lives in the current working directory.
	 *
	 * If we did miss something, we the file will turn out to
	 * be mode 644, not usable as a 666 file is.
	 */
	(void) remove(symdev);
	if ((fd = open(symdev, O_CREAT|O_EXCL|O_WRONLY, 0644)) < 0) {
		dprintf("action_filemgr: cannot write %s/%s; %m\n",
			REM_DIR, symdev);
	} else {
		fp = fdopen(fd, "w");
		if (fp != NULL) {
			(void) fprintf(fp, "%s %s", mountp, rdev);
			(void) fclose(fp);
		} else {
			(void) close(fd);
		}
	}
	pop_dir(cwdfd);
free:
	free(mountp);
	return (TRUE);
}


/*
 * Remove the file containing the relevant mount info.
 *
 * NOTE: the action_arg array and the media_type arg are passed in,
 * even though not used, for possible future use (and symetry with
 * insert_media() (&^)).
 *
 * Again, make sure we don't kill any innocent bystanders, though admittedly
 * the invoker does not have a choice of filename
 */
/*ARGSUSED*/
static void
eject_media(struct action_arg **aa, char *media_type)
{
	char	*symdev = getenv("VOLUME_SYMDEV");
	int	cwdfd = pushdir_and_check(REM_DIR);

	if (cwdfd < 0 || remove(symdev) < 0) {
		dprintf("action_filemgr: remove %s/%s; %m\n",
			REM_DIR, symdev);
	}

	if (cwdfd != -1) {
		pop_dir(cwdfd);
	}
}


/*
 * Notify interested parties of change in the state.  Interested
 * parties are ones that put a "notify" named pipe in the
 * directory.  We'll open it up and write a character down it.
 *
 * Return TRUE or FALSE
 */
static bool_t
notify_clients()
{
	DIR		*dirp;
	struct dirent	*dp;
	size_t		len;
	int		fd;
	char		c = 'a';	/* character to write */
	char		namebuf[MAXPATHLEN];
	struct stat	sb;
	int		rval = FALSE;
	void		(*osig)();
	int		cwdfd;

	/*
	 * Use relative pathnames after chdir() so we do
	 * not have this race condition to hurt us.
	 * Also check pipeness of the file to insure
	 * that it hasn't moved on us
	 * unlinks don't hurt here, we are in a safe dir
	 */

	if ((cwdfd = pushdir_and_check(REM_DIR)) < 0) {
		dprintf("%s failed on pushdir_and_check()\n",
			REM_DIR);
		return (FALSE);
	}

	if ((dirp = opendir(".")) == NULL) {
		dprintf("%s(%d):opendir failed on '.'; %m\n",
			prog_name, prog_pid);
		pop_dir(cwdfd);
		return (FALSE);
	}

	osig = signal(SIGPIPE, SIG_IGN);

	/*
	 * Read through the directory looking for names that start
	 * with "notify". If we find one, open it and write a
	 * character to it. If we get an error when we open it,
	 * we assume that the process on the other end of the named
	 * pipe has gone away, so we get rid of the file.
	 */

	len = strlen(NOTIFY_NAME);

	while (dp = readdir(dirp)) {
		if (strncmp(dp->d_name, NOTIFY_NAME, len) != 0) {
			continue;
		}

		(void) sprintf(namebuf, "%s/%s", REM_DIR, dp->d_name);

		if ((fd = open(namebuf, O_WRONLY|O_NDELAY)) < 0) {
			dprintf("%s(%ld) open failed for %s; %m\n",
			    prog_name, prog_pid, namebuf);

			/*
			 * If we couldn't open the file, assume that
			 * the process on the other end has died.
			 */
			if (unlink(namebuf) < 0) {
				dprintf("%s(%ld) unlink failed for %s; %m\n",
				    prog_name, prog_pid, namebuf);
			}
			continue;
		}

		/*
		 * make sure that we have a named pipe
		 * to close a small security hole that can damage
		 * the system
		 */
		if ((fstat(fd, &sb) < 0) || (!S_ISFIFO(sb.st_mode))) {
			dprintf("%s(%d) %s is not a fifo\n",
				prog_name, prog_pid, namebuf);

			(void) close(fd);
			continue;
		}


		if (write(fd, &c, 1) < 0) {
			dprintf("%s(%ld) write failed for %s; %m\n",
			    prog_name, prog_pid, namebuf);
			(void) close(fd);
			continue;
		}
		(void) close(fd);

		rval = TRUE;	/* we found something */
	}
	(void) closedir(dirp);

	(void) signal(SIGPIPE, osig);

	pop_dir(cwdfd);

	return (rval);
}
