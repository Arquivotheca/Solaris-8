/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)defaults.c	1.1	99/10/22 SMI"

/*
 * This module contains the functions implementing the interface to the
 * /etc/defaults/dhcp dhcp service data store selection file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <alloca.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <deflt.h>
#include <dhcdata.h>

/*
 * open() of the lock file serials access among processes and threads. Note
 * that there is no protection for dhcp_defaults_t's; thus each thread should
 * acquire its own copy.
 *
 * Attempts to create lock file. If successful, returns 0. Otherwise -1 is
 * returned. If the reason the open failed is because the file exists, errno
 * is changed to EAGAIN.
 */
static int
dhcp_defaults_try_lock(void)
{
	int	dd;

	errno = 0;
	if ((dd = open(DHCP_DEFAULTS_LOCK_FILE, O_CREAT | O_EXCL | O_RDWR,
	    0644)) < 0) {
		errno = (errno == EEXIST) ? EAGAIN : errno;
		return (-1);
	}
	(void) close(dd);
	return (0);
}

/*
 * Remove lock file
 */
static int
dhcp_defaults_free_lock(void)
{
	return (unlink(DHCP_DEFAULTS_LOCK_FILE));
}

/*
 * Find the default called key, and return a reference to
 * it. Returns NULL if not found or an error occurred.
 */
static dhcp_defaults_t *
find_dhcp_defaults(dhcp_defaults_t *ddp, const char *key)
{
	int	i;

	if (ddp == NULL || key == NULL)
		return (NULL);

	for (i = 0; /* null condition */; i++) {
		if (ddp[i].def_type == DHCP_KEY) {
			if (ddp[i].def_key == NULL)
				break; /* hit zeroed element */
			if (strcasecmp(ddp[i].def_key, key) == 0)
				return (&ddp[i]);
		}
	}
	return (NULL);
}

/*
 * This API reads the contents of the defaults file into a dynamically
 * allocated array of dhcp_defaults_t records. A zeroed element marks the
 * end of the array. Blank lines are ignored. Caller is responsible for
 * freeing ddp.
 */
int
read_dhcp_defaults(dhcp_defaults_t **ddpp)
{
	struct stat	sb;
	int		dd, i, bytes;
	char		*tp, *cp, *dp, *ep;
	dhcp_defaults_t	*tdp, *ddp;

	if (ddpp == NULL) {
		errno = EINVAL;
		return (-1);
	}

	if (dhcp_defaults_try_lock() < 0)
		return (-1);

	if ((dd = open(DHCP_DEFAULTS_FILE, O_RDONLY)) < 0) {
		(void) dhcp_defaults_free_lock();
		return (-1);
	}
	if (fstat(dd, &sb) != 0) {
		(void) close(dd);
		(void) dhcp_defaults_free_lock();
		return (-1);
	}

	dp = (char *)alloca(sb.st_size);
	bytes = read(dd, dp, sb.st_size);
	(void) close(dd);

	(void) dhcp_defaults_free_lock();

	/* unexpected problem reading in the file */
	if (bytes != sb.st_size)
		return (-1);

	for (tp = cp = dp, i = 0, ddp = NULL; cp < &dp[sb.st_size]; cp++) {
		if (*cp != '\n')
			continue;
		else {
			if (tp == cp)
				continue; /* ignore blank lines */
			*cp = '\0';
		}

		/*
		 * Handled garbled non-comment lines (by ignoring them).
		 */
		if (*tp != '#') {
			if ((ep = strchr(tp, '=')) == NULL)
				continue;
			*ep++ = '\0'; /* terminate keyword, set up value */
		}

		/* allocate new array element */
		if ((tdp = (dhcp_defaults_t *)realloc(ddp,
		    (i + 1) * sizeof (dhcp_defaults_t))) == NULL) {
			/*
			 * free_dhcp_defaults() stops at a NULL entry,
			 * thus we need to free / NULL the last record,
			 * and let free_dhcp_defaults() take care of
			 * the rest.
			 */
			tdp = &ddp[i - 1];
			if (tdp->def_type == DHCP_KEY) {
				free(tdp->def_key);
				free(tdp->def_value);
			} else
				free(tdp->def_comment);
			tdp->def_key = NULL;

			free_dhcp_defaults(ddp);
			ddp = NULL;
			break;
		} else
			ddp = tdp;

		/* load array element */
		if (*tp == '#') {
			ddp[i].def_type = DHCP_COMMENT;
			ddp[i].def_comment = strdup(tp + 1);
		} else {
			ddp[i].def_type = DHCP_KEY;
			ddp[i].def_key = strdup(tp);
			ddp[i].def_value = strdup(ep);
		}
		i++;
		tp = cp + 1;
	}

	if (ddp != NULL) {
		/* Add the zeroed element */
		if ((tdp = (dhcp_defaults_t *)realloc(ddp,
		    (i + 1) * sizeof (dhcp_defaults_t))) == NULL) {
			/*
			 * free_dhcp_defaults() stops at a NULL entry,
			 * thus we need to free / NULL the last record,
			 * and let free_dhcp_defaults() take care of
			 * the rest.
			 */
			tdp = &ddp[i - 1];
			if (tdp->def_type == DHCP_KEY) {
				free(tdp->def_key);
				free(tdp->def_value);
			} else
				free(tdp->def_comment);
			tdp->def_key = NULL;

			free_dhcp_defaults(ddp);
			ddp = NULL;
		} else {
			ddp = tdp;
			(void) memset((char *)&ddp[i], 0,
			    sizeof (dhcp_defaults_t));
		}
	}

	*ddpp = ddp;

	if (ddp == NULL)
		return (-1);

	return (0);
}

/*
 * This API writes ddp array to the defaults file. If the defaults file already
 * exists, its contents are replaced with the contents of the ddp array. If the
 * defaults file does not exist, it is created using the identity of the caller
 * (euid/egid) with the permission bits specified by the mode argument. Caller
 * is responsible for freeing the array.
 */
int
write_dhcp_defaults(dhcp_defaults_t *ddp, const mode_t mode)
{
	int		size, i, bytes, tdd, status = 0;
	char		*tmpbuf;
	struct stat	sb;
	boolean_t	file_exists;

	if (ddp == NULL) {
		errno = EINVAL;
		return (-1);
	}

	/* guess at final file size */
	for (i = 0, size = 0; /* null condition */; i++) {
		if (ddp[i].def_type == DHCP_KEY) {
			if (ddp[i].def_key == NULL)
				break; /* hit zeroed element */
			size += strlen(ddp[i].def_key) + 1; /* include = */
			size += strlen(ddp[i].def_value) + 1; /* include \n */
		} else
			size += strlen(ddp[i].def_comment) + 2; /* inc # + \n */
	}

	if (size == 0) {
		errno = EINVAL;
		return (-1);
	}

	if (dhcp_defaults_try_lock() < 0)
		return (-1);

	/* capture file statistics for current file, if it exists. */
	if (stat(DHCP_DEFAULTS_FILE, &sb) < 0) {
		if (errno != ENOENT) {
			(void) dhcp_defaults_free_lock();
			return (-1);
		}
		file_exists = B_FALSE;
		sb.st_mode = mode;
		sb.st_uid = geteuid();
		sb.st_gid = getegid();
	} else {
		file_exists = B_TRUE;
		sb.st_mode &= S_IAMB;
	}

	if ((tdd = open(DHCP_DEFAULTS_TEMP, O_CREAT | O_TRUNC | O_WRONLY,
	    0644)) < 0) {
		(void) dhcp_defaults_free_lock();
		return (-1);
	}

	tmpbuf = alloca(size); /* scratch buffer for writes */
	for (i = 0; /* null condition */; i++) {
		if (ddp[i].def_type == DHCP_KEY) {
			if (ddp[i].def_key == NULL)
				break;	/* hit zeroed element */
			(void) sprintf(tmpbuf, "%s=%s\n", ddp[i].def_key,
			    ddp[i].def_value);
		} else
			(void) sprintf(tmpbuf, "#%s\n", ddp[i].def_comment);

		bytes = write(tdd, tmpbuf, strlen(tmpbuf));

		/* Nuke the file if we can't successfully update it */
		if (bytes != strlen(tmpbuf)) {
			status = -1;
			break;
		}
	}
	(void) close(tdd);

	if (status == 0) {
		/* First, move original file aside if it exists */
		if (file_exists) {
			if (rename(DHCP_DEFAULTS_FILE, DHCP_DEFAULTS_ORIG) < 0)
				status = -1;
		}

		if (status == 0) {
			/*
			 * Second, move new file into place, set the same
			 * mode as the original file, and set the same
			 * owner/group. XXX - what if the following rename
			 * to restore the original file fails?
			 */
			if (rename(DHCP_DEFAULTS_TEMP,
			    DHCP_DEFAULTS_FILE) < 0 ||
			    chmod(DHCP_DEFAULTS_FILE, sb.st_mode) < 0 ||
			    chown(DHCP_DEFAULTS_FILE, sb.st_uid, sb.st_gid)
			    < 0) {
				status = -1;
				(void) rename(DHCP_DEFAULTS_ORIG,
				    DHCP_DEFAULTS_FILE);
			} else
				(void) unlink(DHCP_DEFAULTS_ORIG);
		}
	}

	/* remove temporary file if necessary */
	if (status < 0)
		(void) unlink(DHCP_DEFAULTS_TEMP);

	(void) dhcp_defaults_free_lock();

	return (status);
}

/*
 * This API frees the memory associated with the ddp array.
 */
void
free_dhcp_defaults(dhcp_defaults_t *ddp)
{
	int	i;

	if (ddp == NULL)
		return;
	for (i = 0; /* null condition */; i++) {
		if (ddp[i].def_type == DHCP_KEY) {
			if (ddp[i].def_key == NULL)
				break;	/* zeroed element found */
			free(ddp[i].def_key);
			free(ddp[i].def_value);
		} else
			free(ddp[i].def_comment);
	}
	free(ddp);
}

/*
 * This API deletes the defaults file. We lock it first before deleting it.
 * If someone else is dorking with the file, we'll return an error, and the
 * caller will need to try again later.
 */
int
delete_dhcp_defaults(void)
{
	int		status;

	if (dhcp_defaults_try_lock() < 0)
		return (-1);

	status = unlink(DHCP_DEFAULTS_FILE);

	(void) dhcp_defaults_free_lock();
	return (status);
}

/*
 * Adds a dhcp_defaults_t to the ddpp table. If the table is NULL, one
 * is created. The table is terminated by a NULL entry. The key and value
 * arguments are copied, not referenced directly. No check is done to
 * see if the default already exists.
 * Returns 0 for success, nonzero otherwise.
 */
int
add_dhcp_defaults(dhcp_defaults_t **ddpp, const char *key, const char *value)
{
	dhcp_defaults_t		*ndp, *tdp;
	int			i;

	if (ddpp == NULL || key == NULL || value == NULL) {
		errno = EINVAL;
		return (-1);
	}

	if (*ddpp == NULL) {
		ndp = (dhcp_defaults_t *)calloc(1,
		    sizeof (dhcp_defaults_t) * 2);
		tdp = &ndp[0];
	} else {
		for (i = 0; (*ddpp)[i].def_key != NULL; i++)
			/* NULL body */;
		ndp = (dhcp_defaults_t *)realloc(*ddpp,
		    (i + 2) * sizeof (dhcp_defaults_t));
		if (ndp == NULL) {
			errno = ENOMEM;
			return (-1); /* no memory */
		}
		(void) memset((char *)&ndp[i + 1], 0, sizeof (dhcp_defaults_t));
		tdp = &ndp[i];
	}

	tdp->def_type = DHCP_KEY;
	tdp->def_key = strdup(key);
	tdp->def_value = strdup(value);

	*ddpp = ndp;

	return (0);
}

/*
 * Return a copy of the value portion of the named key. Caller is responsible
 * for freeing value when they're finished using it. Returns 0 for success,
 * -1 otherwise (errno is set).
 */
int
query_dhcp_defaults(dhcp_defaults_t *ddp, const char *key, char **value)
{
	dhcp_defaults_t	*tdp;

	if (key == NULL || value == NULL) {
		errno = EINVAL;
		return (-1);
	}
	if ((tdp = find_dhcp_defaults(ddp, key)) != NULL) {
		*value = strdup(tdp->def_value);
		errno = 0;
		return (0);
	}
	errno = ENOENT;
	*value = NULL;
	return (-1);
}

/*
 * If the requested default exists, replace its value with the new value. If
 * it doesn't exist, then add the default with the new value.
 * Returns 0 for success, -1 otherwise (errno is set).
 */
int
replace_dhcp_defaults(dhcp_defaults_t **ddpp, const char *key,
    const char *value)
{
	dhcp_defaults_t	*tdp;
	int		err;

	if (ddpp == NULL || key == NULL || value == NULL) {
		errno = EINVAL;
		return (-1);
	}
	if ((tdp = find_dhcp_defaults(*ddpp, key)) != NULL) {
		char	*valp;

		if ((valp = strdup(value)) == NULL)
			return (-1); /* NOMEM */

		if (tdp->def_value != NULL)
			free(tdp->def_value);

		tdp->def_value = valp;

		errno = 0;
		err = 0;
	} else
		err = (add_dhcp_defaults(ddpp, key, value) == 0) ? 0 : -1;

	return (err);
}
