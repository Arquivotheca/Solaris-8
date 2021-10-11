/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)volrmmount.c	1.11	97/10/13 SMI"

/*
 * Program to to allow non-root users to call rmmount
 *
 * XXX: much of this program is copied from eject.c.  It would be nice
 *	to combine the common code in these two programs (in libvolgmt)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <libintl.h>
#include <locale.h>
#include <sys/stat.h>
#include <sys/fdio.h>
#include <sys/dkio.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include <volmgt.h>
#include <sys/vol.h>
#include <dirent.h>
#include <rpc/types.h>


/*
 * ON-private libvolmgt routines
 */
extern char	*_media_oldaliases(char *);

/* to specify user-requested action */
typedef enum {fake_insert_act, fake_eject_act, no_act}		action_t;

/* volmgt name for floppy and cdrom types of media */
#define	FLOPPY_MEDIA_TYPE	"floppy"
#define	CDROM_MEDIA_TYPE	"cdrom"

/* for env. vars (room for pathname and some) */
#define	ENV_VAR_BUFLEN		(MAXPATHLEN + 20)

/* path for the rmmount program */
#define	RMMOUNT_PATH		"/usr/sbin/rmmount"
#define	RMMOUNT_PROG		"rmmount"

/* max # of args to rmmount */
#define	RMM_MAX_ARGS		25

#ifdef DEBUG
char		*rmm_config = NULL;		/* from "-c" */
#endif /* DEBUG */


int
main(int argc, char **argv)
{
	static char	*vol_basename(char *);
	static void	usage(char *);
	static char	*getdefault(void);
	static bool_t	work(action_t, char *, bool_t);
	extern int	optind;				/* for getopt() */
	char		*prog_name;			/* our prog's name */
	int		c;				/* for getopt() */
#ifdef DEBUG
	const char	*opts = "iedDc:";		/* for getopt() */
#else
	const char	*opts = "iedD";		/* for getopt() */
#endif /* DEBUG */
	bool_t		do_pdefault = FALSE;		/* from "-d" */
	bool_t		do_debug = FALSE;		/* from "-D" */
	action_t	user_act = no_act;		/* from "-i" or "-e" */
	char		*vol = NULL;
	int		ret_val = 0;



	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif

	(void) textdomain(TEXT_DOMAIN);

	prog_name = vol_basename(argv[0]);

	/* process arguments */
	while ((c = getopt(argc, argv, opts)) != EOF) {
		switch (c) {
		case 'i':
			user_act = fake_insert_act;
			break;
		case 'e':
			user_act = fake_eject_act;
			break;
		case 'd':
			do_pdefault = TRUE;
			break;
		case 'D':
			do_debug = TRUE;
			break;
#ifdef DEBUG
		case 'c':
			rmm_config = (char *)optarg;
			break;
#endif /* DEBUG */
		default:
			usage(prog_name);
			return (1);
		}
	}

	/* ensure volmgt is running */
	if (!volmgt_running()) {
		(void) fprintf(stderr,
		    gettext("error: Volume Management must be running\n"));
		return (1);
	}

	/* print default device, if requested */
	if (do_pdefault) {
		if ((vol = getdefault()) == NULL) {
			(void) fprintf(stderr,
			    gettext("Default device is: nothing inserted\n"));
		} else {
			(void) fprintf(stderr,
			    gettext("Default device is: %s\n"), vol);
		}
		return (0);
	}

	/* ensure at least some action was specified */
	if (user_act == no_act) {
		(void) fprintf(stderr,
		    gettext("error: must specify an action\n"));
		usage(prog_name);
		return (1);
	}

	/* was any name passed in ?? */
	if (argc == optind) {
		/* no argument -- use the default */
		if ((vol = getdefault()) == NULL) {
			(void) fprintf(stderr,
			    gettext("No default media available\n"));
			return (1);
		}
		if (!work(user_act, vol, do_debug)) {
			ret_val = 1;
		}
	} else {
		/* multiple thingys to handle */
		for (; optind < argc; optind++) {
			if (!work(user_act, argv[optind], do_debug)) {
				ret_val = 1;
			}
			/* continue handling other names specified */
		}
	}

	return (ret_val);
}


static void
usage(char *prog_name)
{
	(void) fprintf(stderr,
	    gettext(
	    "\nusage: %s [-D] [-i | -e] [NAME | NICKNAME]\n"),
	    prog_name);
	(void) fprintf(stderr,
	    gettext("or:    %s -d\n"), prog_name);
	(void) fprintf(stderr,
	    gettext(
	    "options:\t-i        simulate volume being put in/inserted\n"));
	(void) fprintf(stderr,
	    gettext(
	    "options:\t-e        simulate volume being taken out/ejected\n"));
	(void) fprintf(stderr,
	    gettext("options:\t-D        call rmmount in debug mode\n"));
#ifdef DEBUG
	(void) fprintf(stderr,
	    gettext("options:\t-c CONFIG call rmmount in debug mode\n"));
#endif /* DEBUG */
	(void) fprintf(stderr,
	    gettext("options:\t-d        show default device\n"));
	(void) fprintf(stderr, gettext("\nFor example:\n"));
	(void) fprintf(stderr, gettext("\n\t%s -e floppy0\n"), prog_name);
	(void) fprintf(stderr,
	    gettext("\nmight tell %s to unmount the floppy (if mounted))\n\n"),
	    prog_name);
}


/*
 * derefence (if needed) the user-supplied name
 */
static char *
name_deref(char *name)
{
	char		*name1;
	char		*res;


	/* check to see if name is an alias (e.g. "fd" or "cd") */
	if ((name1 = _media_oldaliases(name)) == NULL) {
		name1 = name;
	}

	/*
	 * name is not an alias -- check for abs. path or
	 * /vol/rdsk name
	 */
	if ((res = media_findname(name1)) == NULL) {
		/*
		 * name is not an alias, an absolute path, or a name
		 *  under /vol/rdsk -- let's just use the name given
		 */
		res = name1;
	}

	return (res);
}


/*
 * The assumption is that someone typed volrmmount to handle some piece
 * of media that's currently in a drive.  So, what we do is
 * check for floppy then cdrom.  If there's nothing in either,
 * we just return NULL.
 */
static char *
getdefault(void)
{
	static bool_t	query(char *);
	char		*s = NULL;



	/* look for floppy, then CD-ROM, using new naming */
	if ((s = media_findname(FLOPPY_MEDIA_TYPE)) != NULL) {
		if (query(s)) {
			goto dun;
		}
	}
	if ((s = media_findname(CDROM_MEDIA_TYPE)) != NULL) {
		if (query(s)) {
			goto dun;
		}
	}
	/* look for floppy, then CD-ROM, using old naming */
	if ((s = _media_oldaliases(FLOPPY_MEDIA_TYPE)) != NULL) {
		if (query(s)) {
			goto dun;
		}
	}
	if ((s = _media_oldaliases(CDROM_MEDIA_TYPE)) != NULL) {
		if (query(s)) {
			goto dun;
		}
	}

	s = NULL;	/* no match */
dun:
	return (s);
}


/*
 * In my experience with removable media drivers so far... the
 * most reliable way to tell if a piece of media is in a drive
 * is simply to open it.  If the open works, there's something there,
 * if it fails, there's not.  We check for two errnos which we
 * want to interpret for the user,  ENOENT and EPERM.  All other
 * errors are considered to be "media isn't there".
 * (halt, 1993)
 *
 * return TRUE if media found, else FALSE
 */
static bool_t
query(char *path)
{
	int		fd = -1;
	int		rval;			/* FDGETCHANGE return value */
	enum dkio_state	state;
	bool_t		res = FALSE;		/* return value */



	/* open the specifed path */
	if ((fd = open(path, O_RDONLY|O_NONBLOCK)) < 0) {
		goto dun;
	}

	rval = 0;
	if (ioctl(fd, FDGETCHANGE, &rval) >= 0) {
		/* hey, it worked, what a deal, it must be a floppy */
		if (!(rval & FDGC_CURRENT)) {
			res = TRUE;
		}
		goto dun;
	}

again:
	state = DKIO_NONE;
	if (ioctl(fd, DKIOCSTATE, &state) >= 0) {
		/* great, the fancy ioctl is supported. */
		if (state == DKIO_INSERTED) {
			res = TRUE;
			goto dun;
		}
		if (state == DKIO_EJECTED) {
			goto dun;
		}
		/* state must be DKIO_NONE: do a silly retry loop */
		(void) sleep(1);
		goto again;		/* how many times? */
	}
	(void) close(fd);

	/*
	 * Ok, we've tried the non-blocking/ioctl route.  The
	 * device doesn't support any of our nice ioctls, so
	 * we'll just say that if it opens it's there, if it
	 * doesn't, it's not.
	 */
	if ((fd = open(path, O_RDONLY)) < 0) {
		goto dun;
	}

	res = TRUE;			/* success */
dun:
	if (fd >= 0) {
		(void) close(fd);
	}
	return (res);
}


/*
 * do the main work of handling a name
 */
static bool_t
work(action_t user_act, char *vol, bool_t do_debug)
{
	static bool_t	access_ok(char *);
	static bool_t	setup_env(action_t, char *);
	static bool_t	call_rmmount(bool_t);
	char		*path;
	bool_t		res = FALSE;		/* return value */


	/* convert nickname to real name (if needed) */
	path = name_deref(vol);

	/* ensure user has right to access volume */
	if (!access_ok(path)) {
		(void) fprintf(stderr,
		    gettext("error: can't access \"%s\": %s\n"),
		    path, strerror(errno));
		goto dun;
	}

	/* set up environment variables */
	if (!setup_env(user_act, path)) {
		goto dun;
	}

	/* handle the request */
	if (!call_rmmount(do_debug)) {
		goto dun;
	}

	/* all went ok */
	res = TRUE;
dun:
	return (res);
}


/*
 * Since volrmmount is a suid root program, we must make sure
 * that the user running us is allowed to access the media.
 * All a user has to do to request an action is open
 * the file for reading, so that's as restrictive as we'll be.
 */
static bool_t
access_ok(char *path)
{
	if (access(path, R_OK) != 0) {
		(void) fprintf(stderr,
		    gettext("error: can't access \"%s\": %s\n"),
		    path, strerror(errno));
		return (FALSE);
	}

	return (TRUE);	/* access is ok */
}


/*
 * set up env. vars. needed, based on requested action
 *
 * set up the following environment variables for rmmount:
 *	VOLUME_NAME		- the volume's name
 *	VOLUME_PATH		- /vol/dev pathname to the volume
 *	VOLUME_ACTION		- "insert" or "eject"
 *	VOLUME_MEDIATYPE	- media type (e.g. "floppy", "cdrom")
 *	VOLUME_SYMDEV		- the symname (e.g. "floppy0", "cdrom1")
 *
 * return FALSE iff we have a fatal error
 */
static bool_t
setup_env(action_t act, char *vol)
{
	static char	*vol_basename(char *);
	static char	*get_symname(char *);
	static char	*symname_to_mt(char *);
	static char	*vol_dirname(char *);
	static int	my_putenv(char *);
	static char	*volrmm_getfullblkname(char *);
	char		env_buf[ENV_VAR_BUFLEN+1];
	char		*sn;			/* symname */
	bool_t		res = FALSE;



	/* the action: "insert" or "eject" */
	(void) sprintf(env_buf, "VOLUME_ACTION=%s",
	    act == fake_insert_act ? "insert" : "eject");
	if (my_putenv(env_buf) != 0) {
		goto dun;
	}

	/* the vol raw pathname */
	(void) sprintf(env_buf, "VOLUME_PATH=%s", volrmm_getfullblkname(vol));
	if (my_putenv(env_buf) != 0) {
		goto dun;
	}

	/* just the media name itself */
	(void) sprintf(env_buf, "VOLUME_NAME=%s", vol_basename(vol));
	if (my_putenv(env_buf) != 0) {
		goto dun;
	}

	/* get the symname for the next two env. vars (sed below) */
	if ((sn = get_symname(vol)) == NULL) {
		goto dun;
	}

	/* the media type: "floppy", "cdrom", ... */
	(void) sprintf(env_buf, "VOLUME_MEDIATYPE=%s", symname_to_mt(sn));
	if (my_putenv(env_buf) != 0) {
		goto dun;
	}

	/* the symbolic device name: "floppy0", "cdrom2", ... */
	(void) sprintf(env_buf, "VOLUME_SYMDEV=%s", sn);
	if (my_putenv(env_buf) != 0) {
		goto dun;
	}

	/* the vol raw device directory */
	(void) sprintf(env_buf, "VOLUME_DEVICE=%s", vol_dirname(vol));
	if (my_putenv(env_buf) != 0) {
		goto dun;
	}

	/* whether or not the ejection is forced (always FALSE) */
	(void) strcpy(env_buf, "VOLUME_FORCEDEJECT=false");
	if (my_putenv(env_buf) != 0) {
		goto dun;
	}

	res = TRUE;
dun:
	return (res);
}


static char *
vol_basename(char *path)
{
	register char	*cp;


	/* check for the degenerate case */
	if (strcmp(path, "/") == 0) {
		return (path);
	}

	/* look for the last slash in the name */
	if ((cp = strrchr(path, '/')) == NULL) {
		/* no slash */
		return (path);
	}

	/* ensure something is after the slash */
	if (*++cp != '\0') {
		return (cp);
	}

	/* a name that ends in slash -- back up until previous slash */
	while (cp != path) {
		if (*--cp == '/') {
			return (--cp);
		}
	}

	/* the only slash is the end of the name */
	return (path);
}


/*
 * returns a malloced string
 */
static char *
vol_dirname(char *path)
{
	static char	*my_strdup(char *);
	register char	*cp;
	size_t		len;
	char		*new;



	/* check for degenerates */
	if (strcmp(path, "/") == 0) {
		return (my_strdup("/"));
	}
	if (*path == '\0') {
		return (my_strdup("."));
	}

	/* find the last seperator in the path */
	if ((cp = strrchr(path, '/')) == NULL) {
		/* must be just a local name -- use the local dir */
		return (my_strdup("."));
	}

	/* allocate room for the new dirname string */
	len = (size_t)(cp - path);
	if ((new = malloc(len + 1)) == NULL) {
		(void) fprintf(stderr,
		    gettext("error: can't allocate memory: %s\n"),
		    strerror(errno));
		return (NULL);
	}

	/* copy the string in */
	(void) memcpy(new, path, len);
	new[len] = '\0';

	/* return all but the last component */
	return (new);
}


/*
 * given a raw device path in /vol/dev, return the symbolic name
 */
static char *
get_symname(char *vol_path)
{
	const char	*vm_root = volmgt_root();
	char		*vol_path_basenm;
	char		vol_symdir[MAXPATHLEN];
	DIR		*dirp;
	struct dirent64	*dp = NULL;
	char		lpath[MAXPATHLEN];
	char		link_buf[MAXPATHLEN];
	int		lb_len;
	char		*res = NULL;



	/* get path of alias directory */
	(void) sprintf(vol_symdir, "%s/dev/aliases", vm_root);

	/* scan for aliases that might match */
	if ((dirp = opendir(vol_symdir)) == NULL) {
		(void) fprintf(stderr, gettext(
		    "error: can't open volmgt symlink directory \"%s\"; %s\n"),
		    vol_symdir, strerror(errno));
		goto dun;
	}

	/* get source basename to avoid repeating in loop */
	vol_path_basenm = vol_basename(vol_path);

	while ((dp = readdir64(dirp)) != NULL) {

		/* this is *probably* a link, so proceed as if it is */

		(void) sprintf(lpath, "%s/%s", vol_symdir, dp->d_name);
		if ((lb_len = readlink(lpath, link_buf, MAXPATHLEN)) < 0) {
			continue;		/* not a link ?? */
		}
		link_buf[lb_len] = '\0';

		/*
		 * if link name matches input name we've found a winner
		 * Using basename because paths can differ
		 *
		 * XXX: shouldn't dev_t be compared, instaed of names??
		 */
		if (strcmp(vol_path_basenm, vol_basename(link_buf)) == 0) {
			/* found a match! */
			res = strdup(dp->d_name);
			break;
		}
	}

dun:
	if (dirp != NULL) {
		(void) closedir(dirp);
	}
	return (res);
}


/*
 * return media type given symname (by removing number at end)
 */
static char *
symname_to_mt(char *sn)
{
	static char	mt[MAXNAMELEN];
	char		*cpi;
	char		*cpo;


	for (cpi = sn, cpo = mt; *cpi != '\0'; cpi++, cpo++) {
		if (isdigit(*cpi)) {
			break;
		}
		*cpo = *cpi;
	}
	*cpo = '\0';

	return (mt);
}


static bool_t
call_rmmount(bool_t do_debug)
{
	pid_t	fork_pid;
	char	*args[RMM_MAX_ARGS + 1];	/* a little extra room */
	int	arg_ind = 0;
	int	exit_val;



	if ((fork_pid = fork()) < 0) {
		(void) fprintf(stderr,
		    gettext("error: can't fork to call \"%s\": %s\n"),
		    RMMOUNT_PATH, strerror(errno));
		return (FALSE);
	}

	/* get name of program */
	if (fork_pid == 0) {
		/* the child */

		/* set up the arg list */
		args[arg_ind++] = RMMOUNT_PROG;
		if (do_debug) {
			args[arg_ind++] = "-D";
		}
#ifdef DEBUG
		if (rmm_config != NULL) {
			args[arg_ind++] = "-c";
			args[arg_ind++] = rmm_config;
		}
#endif /* DEBUG */
		args[arg_ind] = NULL;

		(void) execv(RMMOUNT_PATH, args);

		(void) fprintf(stderr,
		    gettext("error: can't exec \"%s\": %s\n"),
		    RMMOUNT_PATH, strerror(errno));
		_exit(1);
	}

	/* the parent -- wait for that darned child */
	if (waitpid(fork_pid, &exit_val, 0) < 0) {
		/* signal ?? */
#ifdef	WE_SHOULD_BE_VERBOSE
		/*
		 * XXX: should user really get an error message for
		 * interrupting rmmount ??
		 */
		if (errno == EINTR) {
			(void) fprintf(stderr,
			    gettext("error: \"%s\" was interrupted\n"),
			    RMMOUNT_PATH);
		} else {
			(void) fprintf(stderr,
			    gettext("error: running \"%s\": %s\n"),
			    RMMOUNT_PATH, strerror(errno));
		}
#endif
		return (FALSE);
	}

	/* evaluate return status */
	if (WIFEXITED(exit_val)) {
		if (WEXITSTATUS(exit_val) == 0) {
			return (TRUE);		/* success */
		}
		(void) fprintf(stderr, gettext("error: \"%s\" failed\n"),
		    RMMOUNT_PATH);
	} else if (WIFSIGNALED(exit_val)) {
		(void) fprintf(stderr,
		    gettext("error: \"%s\" terminated by signal %d\n"),
		    RMMOUNT_PATH, WSTOPSIG(exit_val));
	} else if (WCOREDUMP(exit_val)) {
		(void) fprintf(stderr, gettext("error: \"%s\" core dumped\n"));
	}

	return (FALSE);
}


static char *
my_strdup(char *s)
{
	register char	*cp;


	if ((cp = strdup(s)) == NULL) {
		(void) fprintf(stderr,
		    gettext("error: can't allocate memory: %s\n"),
		    strerror(errno));
	}

	return (cp);
}


static int
my_putenv(char *e)
{
	int	res;
	char	*env;


	if ((env = my_strdup(e)) == NULL) {
		return (1);
	}
	if ((res = putenv(env)) != 0) {
		(void) fprintf(stderr,
		gettext("error: can't allocate memory for environment: %s\n"),
		    strerror(errno));
	}

	return (res);
}


/*
 * this routine will return the volmgt block name given the volmgt
 *  raw (char spcl) name
 *
 * if anything but a volmgt raw pathname is supplied that pathname will
 *  be returned
 *
 * NOTE: non-null return value will point to static data, overwritten with
 *  each call
 *
 * e.g. names starting with "/vol/r" will be changed to start with "/vol/",
 * and names starting with "vol/dev/r" will be changed to start with
 * "/vol/dev/"
 */
static char *
volrmm_getfullblkname(char *path)
{
	char		vm_raw_root[MAXPATHLEN];
	const char	*vm_root = volmgt_root();
	static char	res_buf[MAXPATHLEN];
	uint		vm_raw_root_len;



	/* get first volmgt root dev directory (and its length) */
	(void) sprintf(vm_raw_root, "%s/r", vm_root);
	vm_raw_root_len = strlen(vm_raw_root);

	/* see if we have a raw volmgt pathname (e.g. "/vol/r*") */
	if (strncmp(path, vm_raw_root, vm_raw_root_len) == 0) {
		(void) sprintf(res_buf, "%s/%s", vm_root,
		    path + vm_raw_root_len);
		goto dun;
	}

	/* get second volmgt root dev directory (and its length) */
	(void) sprintf(vm_raw_root, "%s/dev/r", vm_root);
	vm_raw_root_len = strlen(vm_raw_root);

	/* see if we have a raw volmgt pathname (e.g. "/vol/dev/r*") */
	if (strncmp(path, vm_raw_root, vm_raw_root_len) == 0) {
		(void) sprintf(res_buf, "%s/dev/%s", vm_root,
		    path + vm_raw_root_len);
		goto dun;
	}

	/* no match -- return what we got */
	(void) strcpy(res_buf, path);

dun:
	return (res_buf);
}
