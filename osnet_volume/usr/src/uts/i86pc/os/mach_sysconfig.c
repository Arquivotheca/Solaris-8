/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T		*/
/*	All Rights Reserved					*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)mach_sysconfig.c 1.2	95/08/03 SMI"

#include <sys/errno.h>
#include <sys/unistd.h>
#include <sys/systm.h>
#include <sys/sysconfig.h>
#include <sys/sysconfig_impl.h>

/*ARGSUSED*/
int
mach_sysconfig(int which)
{
	return (set_errno(EINVAL));
}
