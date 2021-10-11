/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)volumes.c	1.1	95/11/22 SMI"

#include <string.h>

/*
 *  volume_specified
 *	Make an educated guess if a volume is specified
 *	at the beginning of a path.
 */
int
volume_specified(char *pn)
{
	char *eov, *fs;
	int vol = 0;

	eov = strchr(pn, ':');
	if (eov) {
		/*
		 * Possibly have a volume specified. Check for path-
		 * specifying characters prior to the colon, though.
		 */
		fs = strchr(pn, '\\');
		if (!fs || fs > eov) {
			fs = strchr(pn, '/');
			if (!fs || fs > eov)
				vol = 1;
		}
	}

	return (vol);
}
