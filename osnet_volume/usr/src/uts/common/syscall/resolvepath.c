#ident	"@(#)resolvepath.c	1.1	97/05/02 SMI"

/*
 * Copyright 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/pathname.h>

int
resolvepath(char *path, char *buf, size_t count)
{
	struct pathname lookpn;
	struct pathname resolvepn;
	int error;

	if (count == 0)
		return (0);
	if (error = pn_get(path, UIO_USERSPACE, &lookpn))
		return (set_errno(error));
	pn_alloc(&resolvepn);
	error = lookuppn(&lookpn, &resolvepn, FOLLOW, NULL, NULL);
	if (error == 0) {
		if (count > resolvepn.pn_pathlen)
			count = resolvepn.pn_pathlen;
		if (copyout(resolvepn.pn_path, buf, count))
			error = EFAULT;
	}
	pn_free(&resolvepn);
	pn_free(&lookpn);

	if (error)
		return (set_errno(error));
	return ((int)count);
}
