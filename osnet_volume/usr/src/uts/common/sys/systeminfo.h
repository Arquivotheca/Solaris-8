/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_SYSTEMINFO_H
#define	_SYS_SYSTEMINFO_H

#pragma ident	"@(#)systeminfo.h	1.18	99/07/18 SMI"	/* SVr4.0 1.4 */

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL
extern char architecture[];
extern char hw_serial[];
extern char hw_provider[];
extern char srpc_domain[];
extern char platform[];
#endif	/* _KERNEL */

/*
 * Commands to sysinfo(2)
 *
 * Values for sysinfo(2) commands are to be assigned by the following
 * algorithm:
 *
 *    1 -  256	Unix International assigned numbers for `get' style commands.
 *  257 -  512	Unix International assigned numbers for `set' style commands
 *		where the value is selected to be the value for the
 *		corresponding `get' command plus 256.
 *  513 -  768	Solaris specific `get' style commands.
 *  769 - 1024	Solaris specific `set' style commands where the value is
 *		selected to be the value for the corresponding `get' command
 *		plus 256.
 *
 * These values have be registered
 * with Unix International can't be corrected now.  The status of a command
 * as published or unpublished does not alter the algorithm.
 */

/* UI defined `get' commands (1-256) */
#define	SI_SYSNAME		1	/* return name of operating system */
#define	SI_HOSTNAME		2	/* return name of node */
#define	SI_RELEASE 		3	/* return release of operating system */
#define	SI_VERSION		4	/* return version field of utsname */
#define	SI_MACHINE		5	/* return kind of machine */
#define	SI_ARCHITECTURE		6	/* return instruction set arch */
#define	SI_HW_SERIAL		7	/* return hardware serial number */
#define	SI_HW_PROVIDER		8	/* return hardware manufacturer */
#define	SI_SRPC_DOMAIN		9	/* return secure RPC domain */

/* UI defined `set' commands (257-512) */
#define	SI_SET_HOSTNAME		258	/* set name of node */
#define	SI_SET_SRPC_DOMAIN	265	/* set secure RPC domain */

/* Solaris defined `get' commands (513-768) */
#define	SI_PLATFORM		513	/* return platform identifier */
#define	SI_ISALIST		514	/* return supported isa list */
#define	SI_DHCP_CACHE		515	/* return kernel-cached DHCPACK */


/* Solaris defined `set' commands (769-1024) (none currently assigned) */


#define	DOM_NM_LN		64	/* maximum length of domain name */

#if !defined(_KERNEL)
#if defined(__STDC__)
int sysinfo(int, char *, long);
#else
int sysinfo();
#endif
#endif

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_SYSTEMINFO_H */
