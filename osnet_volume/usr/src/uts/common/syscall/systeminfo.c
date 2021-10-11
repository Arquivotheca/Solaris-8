/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All rights reserved.	*/
/*	Copyright (c) 1994-1999 by Sun Microsystems, Inc. */
/*	  All rights reserved.	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)systeminfo.c	1.8	99/07/18 SMI"	/* from SVr4.0 1.78 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/tuneable.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/utsname.h>
#include <sys/systeminfo.h>
#include <sys/unistd.h>
#include <sys/debug.h>
#include <sys/bootconf.h>
#include <sys/socket.h>
#include <net/if.h>

long
systeminfo(int command, char *buf, long count)
{
	int error = 0;
	long strcnt, getcnt;

	switch (command) {

	case SI_SYSNAME:
		getcnt = ((strcnt = strlen(utsname.sysname)) >= count) ?
		    count : strcnt + 1;
		if (copyout(utsname.sysname, buf, getcnt)) {
			error = EFAULT;
			break;
		}
		if (strcnt >= count && subyte(buf+count-1, 0) < 0) {
			error = EFAULT;
			break;
		}
		return (strcnt + 1);

	case SI_HOSTNAME:
		getcnt = ((strcnt = strlen(utsname.nodename)) >= count) ?
		    count : strcnt + 1;
		if (copyout(utsname.nodename, buf, getcnt)) {
			error = EFAULT;
			break;
		}
		if (strcnt >= count && subyte(buf+count-1, 0) < 0) {
			error = EFAULT;
			break;
		}
		return (strcnt + 1);

	case SI_ISALIST:
		getcnt = ((strcnt = strlen(isa_list)) >= count) ?
		    count : strcnt + 1;
		if (copyout(isa_list, buf, getcnt)) {
			error = EFAULT;
			break;
		}
		if (strcnt >= count && subyte(buf+count-1, 0) < 0) {
			error = EFAULT;
			break;
		}
		return (strcnt + 1);


	case SI_RELEASE:
		getcnt = ((strcnt = strlen(utsname.release)) >= count) ?
		    count : strcnt + 1;
		if (copyout(utsname.release, buf, getcnt)) {
			error = EFAULT;
			break;
		}
		if (strcnt >= count && subyte(buf+count-1, 0) < 0) {
			error = EFAULT;
			break;
		}
		return (strcnt + 1);

	case SI_VERSION:
		getcnt = ((strcnt = strlen(utsname.version)) >= count) ?
		    count : strcnt + 1;
		if (copyout(utsname.version, buf, getcnt)) {
			error = EFAULT;
			break;
		}
		if (strcnt >= count && subyte(buf+count-1, 0) < 0) {
			error = EFAULT;
			break;
		}
		return (strcnt + 1);

	case SI_MACHINE:
		getcnt = ((strcnt = strlen(utsname.machine)) >= count) ?
		    count : strcnt + 1;
		if (copyout(utsname.machine, buf, getcnt)) {
			error = EFAULT;
			break;
		}
		if (strcnt >= count && subyte(buf+count-1, 0) < 0) {
			error = EFAULT;
			break;
		}
		return (strcnt + 1);

	case SI_ARCHITECTURE:
		getcnt = ((strcnt = strlen(architecture)) >= count) ?
		    count : strcnt + 1;
		if (copyout(architecture, buf, getcnt)) {
			error = EFAULT;
			break;
		}
		if ((strcnt >= count) && subyte(buf+count-1, 0) < 0) {
			error = EFAULT;
			break;
		}
		return (strcnt + 1);

	case SI_HW_SERIAL:
		getcnt = ((strcnt = strlen(hw_serial)) >= count) ?
		    count : strcnt + 1;
		if (copyout(hw_serial, buf, getcnt)) {
			error = EFAULT;
			break;
		}
		if (strcnt >= count && subyte(buf+count-1, 0) < 0) {
			error = EFAULT;
			break;
		}
		return (strcnt + 1);

	case SI_HW_PROVIDER:
		getcnt = ((strcnt = strlen(hw_provider)) >= count) ?
		    count : strcnt + 1;
		if (copyout(hw_provider, buf, getcnt)) {
			error = EFAULT;
			break;
		}
		if (strcnt >= count && subyte(buf+count-1, 0) < 0) {
			error = EFAULT;
			break;
		}
		return (strcnt + 1);

	case SI_SRPC_DOMAIN:
		getcnt = ((strcnt = strlen(srpc_domain)) >= count) ?
		    count : strcnt + 1;
		if (copyout(srpc_domain, buf, getcnt)) {
			error = EFAULT;
			break;
		}
		if (strcnt >= count && subyte(buf+count-1, 0) < 0) {
			error = EFAULT;
			break;
		}
		return (strcnt + 1);

	case SI_PLATFORM:
		getcnt = ((strcnt = strlen(platform)) >= count) ?
		    count : strcnt + 1;
		if (copyout(platform, buf, getcnt)) {
			error = EFAULT;
			break;
		}
		if (strcnt >= count && subyte(buf+count-1, 0) < 0) {
			error = EFAULT;
			break;
		}
		return (strcnt + 1);

	case SI_DHCP_CACHE:
	{
		char	*tmp;

		if (dhcack == NULL) {
			tmp = "";
			strcnt = 0;
		} else {
			tmp = dhcack;
			strcnt = IFNAMSIZ + strlen(&tmp[IFNAMSIZ]);
		}

		getcnt = (strcnt >= count) ? count : strcnt + 1;

		if (copyout(tmp, buf, getcnt)) {
			error = EFAULT;
			break;
		}

		if (strcnt >= count && subyte((buf + count - 1), 0) < 0) {
			error = EFAULT;
			break;
		}

		return (strcnt + 1);
	}

	case SI_SET_HOSTNAME:
	{
		size_t		len;
		char		name[SYS_NMLN];

		if (!suser(CRED())) {
			error = EPERM;
			break;
		}

		if ((error = copyinstr(buf, name, SYS_NMLN, &len)) != 0)
			break;

		/*
		 * Must be non-NULL string and string
		 * must be less than SYS_NMLN chars.
		 */
		if (len < 2 || (len == SYS_NMLN && name[SYS_NMLN-1] != '\0')) {
			error = EINVAL;
			break;
		}

		/*
		 * Copy the name into the global utsname structure.
		 */
		(void) strcpy(utsname.nodename, name);
		return (len);
	}

	case SI_SET_SRPC_DOMAIN:
	{
		char name[SYS_NMLN];
		size_t len;

		if (!suser(CRED())) {
			error = EPERM;
			break;
		}
		if ((error = copyinstr(buf, name, SYS_NMLN, &len)) != 0)
			break;
		/*
		 * If string passed in is longer than length
		 * allowed for domain name, fail.
		 */
		if (len == SYS_NMLN && name[SYS_NMLN-1] != '\0') {
			error = EINVAL;
			break;
		}
		(void) strcpy(srpc_domain, name);
		return (len);
	}

	default:
		error = EINVAL;
		break;
	}

	return (set_errno(error));
}
