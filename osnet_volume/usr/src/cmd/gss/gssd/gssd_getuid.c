/*
 * Copyright (c) 1995,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)gssd_getuid.c	1.9	97/11/12 SMI"

/*
 *  Routines to set gssd value of uid and replace getuid libsys call.
 */

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <libintl.h>

uid_t gssd_uid;

void
set_gssd_uid(uid)
	uid_t	uid;
{

	/*
	 * set the value of gssd_uid, so it can be retrieved when getuid()
	 * is called by the underlying mechanism libraries
	 */
	printf(gettext("set_gssd_uid called with uid = %d\n"), uid);
	gssd_uid = uid;
}

uid_t
getuid(void)

{

	/*
	 * return the value set when one of the gssd procedures was
	 * entered. This is the value of the uid under which the
	 * underlying mechanism library must operate in order to
	 * get the user's credentials. This call is necessary since
	 * gssd runs as root and credentials are many times stored
	 * in files and directories specific to the user
	 */
	printf(gettext(
		"getuid called and returning gsssd_uid = %d\n"), gssd_uid);
	return (gssd_uid);
}
