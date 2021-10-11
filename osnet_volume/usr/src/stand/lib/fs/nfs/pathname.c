/*
 * Copyright (c) 1989, 1996-1999 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)pathname.c	1.10	99/02/23 SMI" /* from SunOS 4.1 2.24 89/08/16 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <pathname.h>
#include <sys/promif.h>
#include <nfs_prot.h>
#include <sys/salib.h>
#include <sys/bootdebug.h>

/*
 * Pathname utilities.
 *
 * In translating file names we copy each argument file
 * name into a pathname structure where we operate on it.
 * Each pathname structure can hold MAXPATHLEN characters
 * including a terminating null, and operations here support
 * fetching strings from user space, getting the next character from
 * a pathname, combining two pathnames (used in symbolic
 * link processing), and peeling off the first component
 * of a pathname.
 */

#define	dprintf	if (boothowto & RB_DEBUG) printf

/*
 * Setup contents of pathname structure. Warn about missing allocations.
 * Structure itself is typically automatic
 * variable in calling routine for convenience.
 *
 * NOTE: if buf is NULL, failure occurs.
 */
int
pn_alloc(pnp)
	register struct pathname *pnp;
{
	if (pnp->pn_buf == NULL)
		return (-1);
	pnp->pn_path = (char *)pnp->pn_buf;
	pnp->pn_pathlen = 0;
	return (0);
}

/*
 * Pull a pathname from user user or kernel space
 */
int
pn_get(str, pnp)
	register char *str;
	register struct pathname *pnp;
{
	if (pn_alloc(pnp) != 0)
		return (-1);
	bcopy(str, pnp->pn_path, strlen(str));
	pnp->pn_pathlen = strlen(str);		/* don't count null byte */
	return (0);
}

/*
 * Set pathname to argument string.
 */
pn_set(pnp, path)
	register struct pathname *pnp;
	register char *path;
{
	pnp->pn_path = pnp->pn_buf;
	pnp->pn_pathlen = strlen(pnp->pn_path); /* don't count null byte */
	bcopy(pnp->pn_path, path, pnp->pn_pathlen);
	return (0);
}

/*
 * Combine two argument pathnames by putting
 * second argument before first in first's buffer,
 * and freeing second argument.
 * This isn't very general: it is designed specifically
 * for symbolic link processing.
 */
pn_combine(pnp, sympnp)
	register struct pathname *pnp;
	register struct pathname *sympnp;
{

	if (pnp->pn_pathlen + sympnp->pn_pathlen >= MAXPATHLEN)
		return (ENAMETOOLONG);
	bcopy(pnp->pn_path, pnp->pn_buf + sympnp->pn_pathlen,
	    (u_int)pnp->pn_pathlen);
	bcopy(sympnp->pn_path, pnp->pn_buf, (u_int)sympnp->pn_pathlen);
	pnp->pn_pathlen += sympnp->pn_pathlen;
	pnp->pn_buf[pnp->pn_pathlen] = '\0';
	pnp->pn_path = pnp->pn_buf;
	return (0);
}

/*
 * Get next component off a pathname and leave in
 * buffer comoponent which should have room for
 * NFS_MAXNAMLEN bytes and a null terminator character.
 * If PEEK is set in flags, just peek at the component,
 * i.e., don't strip it out of pnp.
 */
pn_getcomponent(pnp, component, flags)
	register struct pathname *pnp;
	register char *component;
	int flags;
{
	register char *cp;
	register int l;
	register int n;

	cp = pnp->pn_path;
	l = pnp->pn_pathlen;
	n = NFS_MAXNAMLEN;
	while ((l > 0) && (*cp != '/')) {
		if (--n < 0)
			return (ENAMETOOLONG);
		*component++ = *cp++;
		--l;
	}
	if (!(flags & PN_PEEK)) {
		pnp->pn_path = cp;
		pnp->pn_pathlen = l;
	}
	*component = 0;
	return (0);
}

/*
 * skip over consecutive slashes in the pathname
 */
void
pn_skipslash(pnp)
	register struct pathname *pnp;
{
	while ((pnp->pn_pathlen != 0) && (*pnp->pn_path == '/')) {
		pnp->pn_path++;
		pnp->pn_pathlen--;
	}
}

/*
 * free pathname resources. This is a nop - the user of these
 * routines is responsible for allocating and freeing their memory.
 */
/*ARGSUSED*/
void
pn_free(struct pathname *pnp)
{
	/* nop */
	dprintf("pn_free(): you shouldn't be calling pn_free()!\n");
}
