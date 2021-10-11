/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)mkdev.c	1.11	97/08/12 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include <sys/mkdev.h>
#include <errno.h>

/*
 * Create a formatted device number
 */
dev_t
__makedev(const int version, const major_t majdev, const minor_t mindev)
{
	dev_t devnum;
	switch (version) {
	case OLDDEV:
		if (majdev > OMAXMAJ || mindev > OMAXMIN) {
			errno = EINVAL;
			return ((o_dev_t)NODEV);
		}
		devnum = ((majdev << ONBITSMINOR) | mindev);
		break;

	case NEWDEV:
		if (majdev > MAXMAJ || mindev > MAXMIN) {
			errno = EINVAL;
			return (NODEV);
		}
		if ((devnum = (((dev_t)majdev << NBITSMINOR) |
		    mindev)) == NODEV) {
			errno = EINVAL;
			return (NODEV);
		}
		break;

	default:
		errno = EINVAL;
		return (NODEV);
	}

	return (devnum);
}

/*
 * Return major number part of formatted device number
 */
major_t
__major(const int version, const dev_t devnum)
{
	major_t maj;

	switch (version) {
	case OLDDEV:
		maj = (devnum >> ONBITSMINOR);
		if (devnum == NODEV || maj > OMAXMAJ) {
			errno = EINVAL;
			return (NODEV);
		}
		break;

	case NEWDEV:
		maj = (devnum >> NBITSMINOR);
		if (devnum == NODEV || maj > MAXMAJ) {
			errno = EINVAL;
			return (NODEV);
		}
		break;

	default:
		errno = EINVAL;
		return (NODEV);
	}

	return (maj);
}


/*
 * Return minor number part of formatted device number
 */
minor_t
__minor(const int version, const dev_t devnum)
{
	switch (version) {
	case OLDDEV:
		if (devnum == NODEV) {
			errno = EINVAL;
			return (NODEV);
		}
		return (devnum & OMAXMIN);

	case NEWDEV:
		if (devnum == NODEV) {
			errno = EINVAL;
			return (NODEV);
		}
		return (devnum & MAXMIN);

	default:
		errno = EINVAL;
		return (NODEV);
	}
}
