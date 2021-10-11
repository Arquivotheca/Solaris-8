/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SUNOS_DHCP_CLASS_H
#define	_SUNOS_DHCP_CLASS_H

#pragma ident	"@(#)sunos_dhcp_class.h	1.1	99/02/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* SunOS/Solaris vendor class specific Options */
#define	VS_OPTION_START		0
#define	VS_NFSMNT_ROOTOPTS	1	/* ASCII NFS root fs mount options */
#define	VS_NFSMNT_ROOTSRVR_IP	2	/* IP address of root server */
#define	VS_NFSMNT_ROOTSRVR_NAME	3	/* ASCII hostname of root server */
#define	VS_NFSMNT_ROOTPATH	4	/* ASCII UNIX pathname of root */
#define	VS_NFSMNT_SWAPSERVER	5	/* IP address of swap server */
#define	VS_NFSMNT_SWAPFILE	6	/* ASCII path to swapfile */
#define	VS_NFSMNT_BOOTFILE	7	/* ASCII pathname of file to boot */
#define	VS_POSIX_TIMEZONE	8	/* ASCII 1003 posix timezone spec */
#define	VS_BOOT_NFS_READSIZE	9	/* 16bit int for Boot NFS read size */
#define	VS_INSTALL_SRVR_IP	10	/* IP address of Install server */
#define	VS_INSTALL_SRVR_NAME	11	/* ASCII hostname of Install server */
#define	VS_INSTALL_PATH		12	/* ASCII path to Install directory */
#define	VS_SYSID_SRVR_PATH	13	/* ASCII server:/path of sysid */
					/* configuration file. */
#define	VS_JUMPSTART_SRVR_PATH	14	/* ASCII server:/path of JumpStart */
					/* configuration file. */
#define	VS_TERM			15	/* ASCII terminal type */
#define	VS_OPTION_END		15	/* Must be same as entry above */

#ifdef	__cplusplus
}
#endif

#endif	/* _SUNOS_DHCP_CLASS_H */
