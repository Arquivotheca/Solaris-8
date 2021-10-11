/*
 * Copyright (c) 1995-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)volprivate.c	1.14	97/04/09 SMI"

/*
 * routines in this module are meant to be called by other libvolmgt
 * routines only
 */

#include	<stdio.h>
#include	<string.h>
#include	<dirent.h>
#include	<string.h>
#include	<libintl.h>
#include	<limits.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/param.h>
#include	<sys/vol.h>
#ifdef	DEBUG
#include	<sys/varargs.h>
#endif
#include	"volmgt_private.h"



/*
 * We have been passed a path which (presumably) is a volume.
 * We look through the directory until we find a name which is
 * a character device.
 */
char *
getrawpart0(char *path)
{
	DIR		*dirp = NULL;
	struct dirent64	*dp;
	static char	fname[MAXPATHLEN+1];
	struct stat64	sb;
	char		*res;
	int		len;



	/* open the directory */
	if ((dirp = opendir(path)) == NULL) {
		res = NULL;
		goto dun;
	}

	/* get length of directory part */
	len = strlen(path);

	/* scan the directory */
	while (dp = readdir64(dirp)) {

		/* skip "." and ".." */
		if (strcmp(dp->d_name, ".") == 0) {
			continue;
		}
		if (strcmp(dp->d_name, "..") == 0) {
			continue;
		}

		/* ensure we have room for this name */
		if ((len + strlen(dp->d_name) + 1) > MAXPATHLEN) {
			/* XXX: just give up? */
			continue;
		}

		/* create a pathname for this device */
		(void) concat_paths(fname, path, dp->d_name, NULL);
		if (stat64(fname, &sb) < 0) {
			continue;		/* this shouldn't happen */
		}
		/* check for a char-spcl device */
		if (S_ISCHR(sb.st_mode)) {
			res = strdup(fname);
			goto dun;
		}
	}

	/* raw part not found */
	res = NULL;
dun:
	if (dirp != NULL) {
		(void) closedir(dirp);
	}
	return (res);
}


/*
 * fix the getfull{raw,blk}name problem for the fd and diskette case
 *
 * return value is malloc'ed, and must be free'd
 *
 * no match gets a malloc'ed null string
 */

char *
volmgt_getfullblkname(char *n)
{
	extern char	*getfullblkname(char *);
	char		*rval;
	char		namebuf[MAXPATHLEN+1];
	char		*s;
	char		c;
	char		*res;



	/* try to get full block-spcl device name */
	rval = getfullblkname(n);
	if ((rval != NULL) && (*rval != NULLC)) {
		/* found it */
		res = rval;
		goto dun;
	}

	/* we have a null-string result */
	if (rval != NULL) {
		/* free null string */
		free(rval);
	}

	/* ok, so we either have a bad device or a floppy */

	/* try the rfd# or rdiskette forms */
	if (((s = strstr(n, "/rfd")) != NULL) ||
	    ((s = strstr(n, "/rdiskette")) != NULL) ||
	    ((s = strstr(n, "/rdsk/")) != NULL)) {
		/*
		 * we do not have to check for room here, since we will
		 * be making the string one shorter
		 */
		c = *++s;			/* save the first char */
		*s = NULLC;			/* replace it with a null */
		(void) strcpy(namebuf, n);	/* save first part of it */
		*s++ = c;			/* give the first char back */
		(void) strcat(namebuf, s);	/* copy the rest */
		res = strdup(namebuf);
		goto dun;
	}

	/* no match found */
	res = strdup("");

dun:
	return (res);
}


char *
volmgt_getfullrawname(char *n)
{
	extern char	*getfullrawname(char *);
	char		*rval;
	char		namebuf[MAXPATHLEN+1];
	char		*s;
	char		c;
	char		*res;


#ifdef	DEBUG
	denter("volmgt_getfullrawname(%s): entering\n", n);
#endif
	/* try to get full char-spcl device name */
	rval = getfullrawname(n);
	if ((rval != NULL) && (*rval != NULLC)) {
		/* found it */
		res = rval;
		goto dun;
	}

	/* we have a null-string result */
	if (rval != NULL) {
		/* free null string */
		free(rval);
	}

	/* ok, so we either have a bad device or a floppy */

	/* try the "fd", "diskette", and the "dsk" form */
	if (((s = strstr(n, "/fd")) != NULL) ||
	    ((s = strstr(n, "/diskette")) != NULL) ||
	    ((s = strstr(n, "/dsk/")) != NULL)) {
		/*
		 * ensure we have room to add one more char
		 */
		if (strlen(n) < (MAXPATHLEN - 1)) {
			c = *++s;		/* save the first char */
			*s = NULLC;		/* replace it with a null */
			(void) strcpy(namebuf, n); /* save first part of str */
			*s = c;			/* put first charback */
			(void) strcat(namebuf, "r"); /* insert an 'r' */
			(void) strcat(namebuf, s); /* copy rest of str */
			res = strdup(namebuf);
			goto dun;
		}
	}

	/* no match found */
	res = strdup("");
dun:
#ifdef	DEBUG
	dexit("volmgt_getfullrawname: returning %s\n",
	    res ? res : "<null ptr>");
#endif
	return (res);
}


/*
 * volctl_name -- return name of volctl device
 */
const char *
volctl_name(void)
{
	static char	dev_name[] = "/dev/" VOLCTLNAME;

	return (dev_name);
}


/*
 * concat_paths -- create a pathname from two (or three) components
 *
 * truncate the result if it is too large
 *
 * assume that res has a defined length of MAXPATHLEN+1
 *
 * ("head" and "tail" are required, but "tail2" is optional)
 */
char *
concat_paths(char *res, char *head, char *tail, char *tail2)
{
	int	head_len = strlen(head);
	int	len_avail = MAXPATHLEN;



	/* put in as much of the head as will fit */
	(void) strncpy(res, head, len_avail);
	len_avail -= head_len;

	/* see if there is room to proceed */
	if (len_avail > 0) {
		char	*cp = res + head_len;

		/* there is room to append a slash */
		*cp++ = '/';
		len_avail--;

		/* see if there is room to proceed */
		if (len_avail > 0) {
			int	tail_len = strlen(tail);

			/* there is room to append the tail */
			(void) strncpy(cp, tail, len_avail);
			cp += tail_len;
			len_avail -= tail_len;

			/* see if there is room to proceed */
			if ((len_avail > 0) && (tail2 != NULL)) {

				/* there is room to add tail2 (and need) */
				(void) strncpy(cp, tail2, len_avail);
			}
		}
	}

	/* null terminate result (just in case) and return */
	res[MAXPATHLEN] = NULLC;
	return (res);
}



#ifdef	DEBUG

/*
 * debug print routines -- private to libvolmgt
 */

#define	DEBUG_INDENT_SPACES	"  "

int	debug_level = 0;


static void
derrprint(char *fmt, va_list ap)
{
	int		i;
	int		j;
	char		date_buf[256];
	time_t		t;
	struct tm	*tm;


	(void) time(&t);
	tm = localtime(&t);
	(void) fprintf(stderr, "%02d/%02d/%02d %02d:%02d:%02d ",
	    tm->tm_mon+1, tm->tm_mday, tm->tm_year % 100,
	    tm->tm_hour, tm->tm_min, tm->tm_sec);
	for (i = 0; i < debug_level; i++) {
		(void) fprintf(stderr, DEBUG_INDENT_SPACES);
	}
	(void) vfprintf(stderr, fmt, ap);
}

/*
 * denter -- do a derrprint(), then increment debug level
 */
void
denter(char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);
	derrprint(fmt, ap);
	va_end(ap);
	debug_level++;
}

/*
 * dexit -- decrement debug level then do a derrprint()
 */
void
dexit(char *fmt, ...)
{
	va_list		ap;

	if (--debug_level < 0) {
		debug_level = 0;
	}
	va_start(ap, fmt);
	derrprint(fmt, ap);
	va_end(ap);
}

/*
 * dprintf -- print debug info, indenting based on debug level
 */
void
dprintf(char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);
	derrprint(fmt, ap);
	va_end(ap);
}

#endif	/* DEBUG */
