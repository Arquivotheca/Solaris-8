/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 *	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
 *		All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 */

/*
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's Unix(r) System V.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1991,1996,1997,1999 Sun Microsystems, Inc.
 *	(c) 1984, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 */

#ifndef _SYS_MNTENT_H
#define	_SYS_MNTENT_H

#pragma ident	"@(#)mntent.h	1.36	99/08/07 SMI"	/* SVr4.0 1.4	*/

#ifdef	__cplusplus
extern "C" {
#endif

#define	MNTTAB		"/etc/mnttab"
#define	VFSTAB		"/etc/vfstab"
#define	MNTMAXSTR	128

#define	MNTTYPE_UFS	"ufs"		/* Unix file system */
#define	MNTTYPE_NFS	"nfs"		/* NFS file system */
#define	MNTTYPE_NFS3	"nfs3"		/* NFS Version 3 file system */
#define	MNTTYPE_CACHEFS	"cachefs"	/* Cache File System */
#define	MNTTYPE_PCFS	"pcfs"		/* PC (MSDOS) file system */
#define	MNTTYPE_PC	MNTTYPE_PCFS	/* Deprecated name; use MNTTYPE_PCFS */
#define	MNTTYPE_LOFS	"lofs"		/* Loop back file system */
#define	MNTTYPE_LO	MNTTYPE_LOFS	/* Deprecated name; use MNTTYPE_LOFS */
#define	MNTTYPE_HSFS	"hsfs"		/* High Sierra (9660) file system */
#define	MNTTYPE_SWAP	"swap"		/* Swap file system */
#define	MNTTYPE_TMPFS	"tmpfs"		/* Tmp volatile file system */
#define	MNTTYPE_AUTOFS	"autofs"	/* Automounter ``file'' system */

#define	MNTOPT_RO	"ro"		/* Read only */
#define	MNTOPT_RW	"rw"		/* Read/write */
#define	MNTOPT_RQ	"rq"		/* Read/write with quotas */
#define	MNTOPT_QUOTA	"quota"		/* Check quotas */
#define	MNTOPT_NOQUOTA	"noquota"	/* Don't check quotas */
#define	MNTOPT_ONERROR	"onerror"	/* action to taken on error */
#define	MNTOPT_SOFT	"soft"		/* Soft mount */
#define	MNTOPT_SEMISOFT	"semisoft"	/* partial soft, uncommited interface */
#define	MNTOPT_HARD	"hard"		/* Hard mount */
#define	MNTOPT_SUID	"suid"		/* Set uid allowed */
#define	MNTOPT_NOSUID	"nosuid"	/* Set uid not allowed */
#define	MNTOPT_GRPID	"grpid"		/* SysV-compatible gid on create */
#define	MNTOPT_REMOUNT	"remount"	/* Change mount options */
#define	MNTOPT_NOSUB	"nosub"		/* Disallow mounts on subdirs */
#define	MNTOPT_MULTI	"multi"		/* Do multi-component lookup */
#define	MNTOPT_INTR	"intr"		/* Allow NFS ops to be interrupted */
#define	MNTOPT_NOINTR	"nointr"	/* Don't allow interrupted ops */
#define	MNTOPT_PORT	"port"		/* NFS server IP port number */
#define	MNTOPT_SECURE	"secure"	/* Secure (AUTH_DES) mounting */
#define	MNTOPT_RSIZE	"rsize"		/* Max NFS read size (bytes) */
#define	MNTOPT_WSIZE	"wsize"		/* Max NFS write size (bytes) */
#define	MNTOPT_TIMEO	"timeo"		/* NFS timeout (1/10 sec) */
#define	MNTOPT_RETRANS	"retrans"	/* Max retransmissions (soft mnts) */
#define	MNTOPT_ACTIMEO	"actimeo"	/* Attr cache timeout (sec) */
#define	MNTOPT_ACREGMIN	"acregmin"	/* Min attr cache timeout (files) */
#define	MNTOPT_ACREGMAX	"acregmax"	/* Max attr cache timeout (files) */
#define	MNTOPT_ACDIRMIN	"acdirmin"	/* Min attr cache timeout (dirs) */
#define	MNTOPT_ACDIRMAX	"acdirmax"	/* Max attr cache timeout (dirs) */
#define	MNTOPT_NOAC	"noac"		/* Don't cache attributes at all */
#define	MNTOPT_NOCTO	"nocto"		/* No close-to-open consistency */
#define	MNTOPT_BG	"bg"		/* Do mount retries in background */
#define	MNTOPT_FG	"fg"		/* Do mount retries in foreground */
#define	MNTOPT_RETRY	"retry"		/* Number of mount retries */
#define	MNTOPT_DEV	"dev"		/* Device id of mounted fs */
#define	MNTOPT_POSIX	"posix"		/* Get static pathconf for mount */
#define	MNTOPT_MAP	"map"		/* Automount map */
#define	MNTOPT_DIRECT	"direct"	/* Automount   direct map mount */
#define	MNTOPT_INDIRECT	"indirect"	/* Automount indirect map mount */
#define	MNTOPT_LLOCK	"llock"		/* Local locking (no lock manager) */
#define	MNTOPT_IGNORE	"ignore"	/* Ignore this entry */
#define	MNTOPT_VERS	"vers"		/* protocol version number indicator */
#define	MNTOPT_PROTO	"proto"		/* protocol network_id indicator */
#define	MNTOPT_SEC	"sec"		/* Security flavor indicator */
#define	MNTOPT_SYNCDIR	"syncdir"	/* Synchronous local directory ops */
#define	MNTOPT_NOSETSEC	"nosec"		/* Do no allow setting sec attrs */
#define	MNTOPT_NOPRINT	"noprint"	/* Do not print messages */
#define	MNTOPT_LARGEFILES "largefiles"	/* allow large files */
#define	MNTOPT_NOLARGEFILES "nolargefiles" /* don't allow large files */
#define	MNTOPT_FORCEDIRECTIO "forcedirectio" /* Force DirectIO on all files */
#define	MNTOPT_NOFORCEDIRECTIO "noforcedirectio" /* No Force DirectIO */
#define	MNTOPT_DISABLEDIRECTIO "disabledirectio" /* Disable DirectIO ioctls */
#define	MNTOPT_PUBLIC	"public"	/* Use NFS public file handlee */
#define	MNTOPT_LOGGING "logging" 	/* enable logging */
#define	MNTOPT_NOLOGGING "nologging" 	/* disable logging */
#define	MNTOPT_NOATIME  "noatime"	/* Do not update i_atime for inodes */
#define	MNTOPT_GLOBAL	"global"	/* Cluster-wide global mount */
#define	MNTOPT_NOGLOBAL	"noglobal"	/* Mount local to single node */
#define	MNTOPT_DFRATIME	"dfratime"	/* Deferred access time updates */
#define	MNTOPT_NODFRATIME "nodfratime"	/* No Deferred access time updates */
#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MNTENT_H */
