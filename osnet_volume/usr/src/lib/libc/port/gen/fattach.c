/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright(c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fattach.c	1.13	99/05/14 SMI"	/* SVr4.0 1.4	*/
/*LINTLIBRARY*/

/*
 * Attach a STREAMS or door based file descriptor to an object in the file
 * system name space.
 */
#pragma weak fattach = _fattach
#include "synonyms.h"
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stropts.h>
#include <sys/vnode.h>
#include <sys/door.h>
#include <sys/fs/namenode.h>
#include <sys/mount.h>

/*
 * XXX - kludge.  We'd like to get the proto from door.h, but we
 * can't #define door_info _door_info because there is a struct
 * of the same name. So we just duplicate the proto here...ugh...
 */
int _door_info(int, struct door_info *);

int
fattach(int fildes, const char *path)
{
	struct namefd  namefdp;
	struct door_info dinfo;
	int	s;

	/* Only STREAMS and doors allowed to be mounted */
	if ((s = isastream(fildes)) == 1 || _door_info(fildes, &dinfo) == 0) {
		namefdp.fd = fildes;
		return (mount((char *)NULL, path, MS_DATA|MS_NOMNTTAB,
		    (const char *)"namefs", (char *)&namefdp,
		    sizeof (struct namefd)));
	} else if (s == 0) {
		/* Not a STREAM */
		errno = EINVAL;
		return (-1);
	} else {
		/* errno already set */
		return (-1);
	}
}
