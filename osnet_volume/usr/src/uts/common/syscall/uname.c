/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/
/*	Copyright (c) 1994 Sun Microsystems, Inc. */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)uname.c	1.3	94/10/04 SMI"	/* from SVr4.0 1.78 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/utsname.h>
#include <sys/debug.h>

int
uname(struct utsname *buf)
{
	if (copyout(utsname.sysname, buf->sysname, strlen(utsname.sysname)+1)) {
		return (set_errno(EFAULT));
	}
	if (copyout(utsname.nodename, buf->nodename,
	    strlen(utsname.nodename)+1)) {
		return (set_errno(EFAULT));
	}
	if (copyout(utsname.release, buf->release, strlen(utsname.release)+1)) {
		return (set_errno(EFAULT));
	}
	if (copyout(utsname.version, buf->version, strlen(utsname.version)+1)) {
		return (set_errno(EFAULT));
	}
	if (copyout(utsname.machine, buf->machine, strlen(utsname.machine)+1)) {
		return (set_errno(EFAULT));
	}
	return (1);	/* XXX why 1 and not 0? 1003.1 says "non-negative" */
}
