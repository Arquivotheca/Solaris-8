/*
 * Copyright (c) 1984,1986,1987,1988,1989,1990,1991,1996 by
 *	Sun Microsystems, Inc.
 * All Rights Reserved.
 */


#pragma ident	"@(#)fslib.c	1.18	99/09/23 SMI"

#include	<stdio.h>
#include	<stdarg.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<libintl.h>
#include	<string.h>
#include	<fcntl.h>
#include	<errno.h>
#include	<syslog.h>
#include	<sys/vfstab.h>
#include	<sys/mnttab.h>
#include	<sys/mntent.h>
#include	<sys/mount.h>
#include	<sys/stat.h>
#include	<sys/param.h>
#include	<signal.h>
#include	"fslib.h"

#define	BUFLEN		256

#define	TIME_MAX 16

/*
 * Reads all of the entries from the in-kernel mnttab, and returns the
 * linked list of the entries.
 */
mntlist_t *
fsgetmntlist()
{
	FILE *mfp;
	mntlist_t *mntl;
	char buf[BUFLEN];

	if ((mfp = fopen(MNTTAB, "r")) == NULL) {
		(void) sprintf(buf, "fsgetmntlist: fopen %s", MNTTAB);
		perror(buf);
		return (NULL);
	}

	mntl = fsmkmntlist(mfp);

	(void) fclose(mfp);
	return (mntl);
}


static struct extmnttab zmnttab = { 0 };

struct extmnttab *
fsdupmnttab(struct extmnttab *mnt)
{
	struct extmnttab *new;

	new = (struct extmnttab *)malloc(sizeof (*new));
	if (new == NULL)
		goto alloc_failed;

	*new = zmnttab;
	/*
	 * Allocate an extra byte for the mountpoint
	 * name in case a space needs to be added.
	 */
	new->mnt_mountp = (char *)malloc(strlen(mnt->mnt_mountp) + 2);
	if (new->mnt_mountp == NULL)
		goto alloc_failed;
	(void) strcpy(new->mnt_mountp, mnt->mnt_mountp);

	if ((new->mnt_special = strdup(mnt->mnt_special)) == NULL)
		goto alloc_failed;

	if ((new->mnt_fstype = strdup(mnt->mnt_fstype)) == NULL)
		goto alloc_failed;

	if (mnt->mnt_mntopts != NULL)
		if ((new->mnt_mntopts = strdup(mnt->mnt_mntopts)) == NULL)
			goto alloc_failed;

	if (mnt->mnt_time != NULL)
		if ((new->mnt_time = strdup(mnt->mnt_time)) == NULL)
			goto alloc_failed;

	new->mnt_major = mnt->mnt_major;
	new->mnt_minor = mnt->mnt_minor;
	return (new);

alloc_failed:
	(void) fprintf(stderr, gettext("fsdupmnttab: Out of memory\n"));
	fsfreemnttab(new);
	return (NULL);
}

/*
 * Free a single mnttab structure
 */
void
fsfreemnttab(struct extmnttab *mnt)
{

	if (mnt) {
		if (mnt->mnt_special)
			free(mnt->mnt_special);
		if (mnt->mnt_mountp)
			free(mnt->mnt_mountp);
		if (mnt->mnt_fstype)
			free(mnt->mnt_fstype);
		if (mnt->mnt_mntopts)
			free(mnt->mnt_mntopts);
		if (mnt->mnt_time)
			free(mnt->mnt_time);
		free(mnt);
	}
}

void
fsfreemntlist(mntlist_t *mntl)
{
	mntlist_t *mntl_tmp;

	while (mntl) {
		fsfreemnttab(mntl->mntl_mnt);
		mntl_tmp = mntl;
		mntl = mntl->mntl_next;
		free(mntl_tmp);
	}
}

/*
 * Read the mnttab file and return it as a list of mnttab structs.
 * Returns NULL if there was a memory failure.
 */
mntlist_t *
fsmkmntlist(FILE *mfp)
{
	struct extmnttab 	mnt;
	mntlist_t 	*mhead, *mtail;
	int 		ret;

	mhead = mtail = NULL;

	resetmnttab(mfp);
	while ((ret = getextmntent(mfp, &mnt, sizeof (struct extmnttab)))
		!= -1) {
		mntlist_t	*mp;

		if (ret != 0)		/* bad entry */
			continue;

		mp = (mntlist_t *)malloc(sizeof (*mp));
		if (mp == NULL)
			goto alloc_failed;
		if (mhead == NULL)
			mhead = mp;
		else
			mtail->mntl_next = mp;
		mtail = mp;
		mp->mntl_next = NULL;
		mp->mntl_flags = 0;
		if ((mp->mntl_mnt = fsdupmnttab(&mnt)) == NULL)
			goto alloc_failed;
	}
	return (mhead);

alloc_failed:
	fsfreemntlist(mhead);
	return (NULL);
}

/*
 * Return the last entry that matches mntin's special
 * device and/or mountpt.
 * Helps to be robust here, so we check for NULL pointers.
 */
mntlist_t *
fsgetmlast(mntlist_t *ml, struct mnttab *mntin)
{
	mntlist_t 	*delete = NULL;

	for (; ml; ml = ml->mntl_next) {
		if (mntin->mnt_mountp && mntin->mnt_special) {
			/*
			 * match if and only if both are equal.
			 */
			if ((strcmp(ml->mntl_mnt->mnt_mountp,
					mntin->mnt_mountp) == 0) &&
			    (strcmp(ml->mntl_mnt->mnt_special,
					mntin->mnt_special) == 0))
				delete = ml;
		} else if (mntin->mnt_mountp) {
			if (strcmp(ml->mntl_mnt->mnt_mountp,
					mntin->mnt_mountp) == 0)
				delete = ml;
		} else if (mntin->mnt_special) {
			if (strcmp(ml->mntl_mnt->mnt_special,
					mntin->mnt_special) == 0)
				delete = ml;
	    }
	}
	return (delete);
}


/*
 * Returns the mountlevel of the pathname in cp.  As examples,
 * / => 1, /bin => 2, /bin/ => 2, ////bin////ls => 3, sdf => 0, etc...
 */
int
fsgetmlevel(char *cp)
{
	int	mlevel;
	char	*cp1;

	if (cp == NULL || *cp == NULL || *cp != '/')
		return (0);	/* this should never happen */

	mlevel = 1;			/* root (/) is the minimal case */

	for (cp1 = cp + 1; *cp1; cp++, cp1++)
		if (*cp == '/' && *cp1 != '/')	/* "///" counts as 1 */
			mlevel++;

	return (mlevel);
}

/*
 * Returns non-zero if string s is a member of the strings in ps.
 */
int
fsstrinlist(const char *s, const char **ps)
{
	const char *cp;
	cp = *ps;
	while (cp) {
		if (strcmp(s, cp) == 0)
			return (1);
		ps++;
		cp = *ps;
	}
	return (0);
}

static char *empty_opt_vector[] = {
	NULL
};
/*
 * Compare the mount options that were requested by the caller to
 * the options actually supported by the file system.  If any requested
 * options are not supported, print a warning message.
 *
 * WARNING: this function modifies the string pointed to by 
 *	the requested_opts argument.
 *
 * Arguments:
 *	requested_opts - the string containing the requested options.
 *	actual_opts - the string returned by mount(2), which lists the
 *		options actually supported.  It is normal for this
 *		string to contain more options than the requested options.
 *		(The actual options may contain the default options, which
 *		may not have been included in the requested options.)
 *	special - device being mounted (only used in error messages).
 *	mountp - mount point (only used in error messages).
 */
void
cmp_requested_to_actual_options(char *requested_opts, char *actual_opts,
	char *special, char *mountp)
{
	char	*option_ptr, *actopt, *equalptr;
	int	found;
	char	buf[MAX_MNTOPT_STR];
	char	*actual_opt_hold;

	if (requested_opts == NULL)
		return;

	actual_opt_hold = buf;

	while (*requested_opts != '\0') {
		(void) getsubopt(&requested_opts, empty_opt_vector,
			&option_ptr);

		/*
		 * Truncate any "=<value>" string from the end of
		 * the option.
		 */
		if ((equalptr = strchr(option_ptr, '=')) != NULL)
			*equalptr = '\0';

		if (*option_ptr == '\0')
			continue;

		/*
		 * Search for the requested option in the list of options
		 * actually supported.
		 */
		found = 0;
		
		/*
		 * Need to make a copy of actual_opts in the
		 * buffer pointed to by actual_opt_hold
		 * because getsubopt is destructive and we need to scan
		 * the actual_opts string more than once.
		 */
		if (actual_opts != NULL)
			(void) strcpy(actual_opt_hold, actual_opts);
		else
			*actual_opt_hold = '\0';

		while (*actual_opt_hold != '\0') {
			(void) getsubopt(&actual_opt_hold, empty_opt_vector,
				&actopt);

			/* Truncate the "=<value>", if any. */
			if ((equalptr = strchr(actopt, '=')) != NULL)
				*equalptr = '\0';
			
			if ((strcmp(option_ptr, actopt)) == 0) {
				found = 1;
				break;
			}
		}

		if (found == 0) {
			(void) fprintf(stderr, gettext(
			    "mount: %s on %s - WARNING unknown option "
			    "\"%s\"\n"), special, mountp, option_ptr);
		}
	}

	return;
}
