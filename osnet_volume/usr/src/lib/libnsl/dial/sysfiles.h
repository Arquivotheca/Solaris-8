/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sysfiles.h	1.5	92/07/14 SMI"	/* SVr4.0 1.1	*/

#define SYSDIR		(const char *)"/etc/uucp"
#define SYSFILES	(const char *)"/etc/uucp/Sysfiles"
#define SYSTEMS		(const char *)"/etc/uucp/Systems"
#define DEVICES		(const char *)"/etc/uucp/Devices"
#define DIALERS		(const char *)"/etc/uucp/Dialers"
#define	DEVCONFIG	(const char *)"/etc/uucp/Devconfig"
#define CONFIG		(const char *)"/etc/uucp/Config"

#define	SAME	0
#define	TRUE	1
#define	FALSE	0
#define	FAIL	-1

/* flags to check file access for REAL user id */
#define	ACCESS_SYSTEMS	1
#define	ACCESS_DEVICES	2
#define	ACCESS_DIALERS	3

/* flags to check file access for EFFECTIVE user id */
#define	EACCESS_SYSTEMS	4
#define	EACCESS_DEVICES	5
#define	EACCESS_DIALERS	6

