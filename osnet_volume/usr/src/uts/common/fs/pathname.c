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
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

#ident	"@(#)pathname.c	1.16	96/10/27 SMI"	/* SVr4.0 1.8	*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/errno.h>
#include <sys/pathname.h>
#include <sys/kmem.h>
#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/debug.h>

/*
 * Pathname utilities.
 *
 * In translating file names we copy each argument file
 * name into a pathname structure where we operate on it.
 * Each pathname structure can hold "pn_bufsize" characters
 * including a terminating null, and operations here support
 * allocating and freeing pathname structures, fetching
 * strings from user space, getting the next character from
 * a pathname, combining two pathnames (used in symbolic
 * link processing), and peeling off the first component
 * of a pathname.
 */

/*
 * Allocate contents of pathname structure.  Structure is typically
 * an automatic variable in calling routine for convenience.
 *
 * May sleep in the call to kmem_alloc() and so must not be called
 * from interrupt level.
 */
void
pn_alloc(struct pathname *pnp)
{
	pnp->pn_path = pnp->pn_buf = kmem_alloc(MAXPATHLEN, KM_SLEEP);
	pnp->pn_pathlen = 0;
	pnp->pn_bufsize = MAXPATHLEN;
}

/*
 * Free pathname resources.
 */
void
pn_free(struct pathname *pnp)
{
	kmem_free(pnp->pn_buf, MAXPATHLEN);
}

/*
 * Pull a path name from user or kernel space.
 */
int
pn_get(char *str, enum uio_seg seg, struct pathname *pnp)
{
	int error;
	char *path;

	pnp->pn_path = pnp->pn_buf = path = kmem_alloc(MAXPATHLEN, KM_SLEEP);
	pnp->pn_bufsize = MAXPATHLEN;
	if (seg == UIO_USERSPACE)
		error = copyinstr(str, path, MAXPATHLEN, &pnp->pn_pathlen);
	else
		error = copystr(str, path, MAXPATHLEN, &pnp->pn_pathlen);
	pnp->pn_pathlen--;		/* don't count null byte */
	if (error)
		pn_free(pnp);
	return (error);
}

/*
 * Set path name to argument string.  Storage has already been allocated
 * and pn_buf points to it.
 *
 * On error, all fields except pn_buf will be undefined.
 */
int
pn_set(struct pathname *pnp, char *path)
{
	int error;

	pnp->pn_path = pnp->pn_buf;
	error = copystr(path, pnp->pn_path, pnp->pn_bufsize, &pnp->pn_pathlen);
	pnp->pn_pathlen--;		/* don't count null byte */
	return (error);
}

/*
 * Combine two argument path names by putting the second argument before
 * the first in the first's buffer, and freeing the second argument.
 * This isn't very general: it is designed specifically for symbolic
 * link processing.
 */
int
pn_insert(struct pathname *pnp, struct pathname *sympnp)
{

	if (pnp->pn_pathlen + sympnp->pn_pathlen >= pnp->pn_bufsize)
		return (ENAMETOOLONG);
	ovbcopy(pnp->pn_path, pnp->pn_buf + sympnp->pn_pathlen,
	    pnp->pn_pathlen);
	bcopy(sympnp->pn_path, pnp->pn_buf, sympnp->pn_pathlen);
	pnp->pn_pathlen += sympnp->pn_pathlen;
	pnp->pn_buf[pnp->pn_pathlen] = '\0';
	pnp->pn_path = pnp->pn_buf;
	return (0);
}

int
pn_getsymlink(vnode_t *vp, struct pathname *pnp, cred_t *crp)
{
	struct iovec aiov;
	struct uio auio;
	int error;

	aiov.iov_base = pnp->pn_buf;
	aiov.iov_len = pnp->pn_bufsize;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_loffset = 0;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_resid = pnp->pn_bufsize;
	if ((error = VOP_READLINK(vp, &auio, crp)) == 0)
		pnp->pn_pathlen = pnp->pn_bufsize - auio.uio_resid;
	return (error);
}

/*
 * Get next component from a path name and leave in
 * buffer "component" which should have room for
 * MAXNAMELEN bytes (including a null terminator character).
 */
int
pn_getcomponent(struct pathname *pnp, char *component)
{
	char c, *cp, *path, saved;
	size_t pathlen;

	path = pnp->pn_path;
	pathlen = pnp->pn_pathlen;
	if (pathlen >= MAXNAMELEN) {
		saved = path[MAXNAMELEN];
		path[MAXNAMELEN] = '/';	/* guarantees loop termination */
		for (cp = path; (c = *cp) != '/'; cp++)
			*component++ = c;
		path[MAXNAMELEN] = saved;
		if (cp - path == MAXNAMELEN)
			return (ENAMETOOLONG);
	} else {
		path[pathlen] = '/';	/* guarantees loop termination */
		for (cp = path; (c = *cp) != '/'; cp++)
			*component++ = c;
		path[pathlen] = 0;
	}

	pnp->pn_path = cp;
	pnp->pn_pathlen = pathlen - (cp - path);
	*component = 0;
	return (0);
}

/*
 * Skip over consecutive slashes in the path name.
 */
void
pn_skipslash(struct pathname *pnp)
{
	while (pnp->pn_pathlen > 0 && *pnp->pn_path == '/') {
		pnp->pn_path++;
		pnp->pn_pathlen--;
	}
}

/*
 * Sets pn_path to the last component in the pathname, updating
 * pn_pathlen.  If pathname is empty, or degenerate, leaves pn_path
 * pointing at NULL char.  The pathname is explicitly null-terminated
 * so that any trailing slashes are effectively removed.
 */
void
pn_setlast(struct pathname *pnp)
{
	char *buf = pnp->pn_buf;
	char *path = pnp->pn_path + pnp->pn_pathlen - 1;
	char *endpath;

	while (path > buf && *path == '/')
		--path;
	endpath = path;
	while (path > buf && *path != '/')
		--path;
	if (*path == '/')
		path++;
	*(endpath + 1) = '\0';
	pnp->pn_path = path;
	pnp->pn_pathlen = endpath - path + 1;
}

/*
 * Eliminate any trailing slashes in the pathname.
 */
void
pn_fixslash(struct pathname *pnp)
{
	char *start = pnp->pn_path;
	char *end = start + pnp->pn_pathlen;

	while (end > start && *(end - 1) == '/')
		end--;
	*end = '\0';
	pnp->pn_pathlen = end - start;
}
