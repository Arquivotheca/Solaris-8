/*
 * Copyright (c) 1991, 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_FS_UFS_MOUNT_H
#define	_SYS_FS_UFS_MOUNT_H

#pragma ident	"@(#)ufs_mount.h	1.24	99/08/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/isa_defs.h>

struct ufs_args {
	int	flags;
};

/*
 * UFS mount option flags
 */
#define	UFSMNT_NOINTR	0x00000001	/* disallow interrupts on lockfs */
#define	UFSMNT_SYNCDIR	0x00000002	/* synchronous local directory ops */
#define	UFSMNT_NOSETSEC	0x00000004	/* disallow use of ufs_setsecattr */
#define	UFSMNT_LARGEFILES 0x00000008	/* allow large files */
#define	UFSMNT_NOATIME  0x00001000	/* disable updates of i_atime */
/* deferred inode time */
#define	UFSMNT_NODFRATIME	0x00002000	/* no deferred access time */
/* action to take when internal inconsistency is detected */
#define	UFSMNT_ONERROR_PANIC	0x00000020	/* forced system shutdown */
#define	UFSMNT_ONERROR_LOCK	0x00000040	/* error lock the fs */
#define	UFSMNT_ONERROR_UMOUNT	0x00000080	/* forced umount of the fs */
#define	UFSMNT_ONERROR_FLGMASK	0x000000E0
/* default action is to repair fs */
#define	UFSMNT_ONERROR_DEFAULT		UFSMNT_ONERROR_PANIC
#define	UFSMNT_DISABLEDIRECTIO	0x00000100	/* disable directio ioctls */
/* Force DirectIO */
#define	UFSMNT_FORCEDIRECTIO	0x00000200	/* directio for all files */
#define	UFSMNT_NOFORCEDIRECTIO	0x00000400	/* no directio for all files */
/* logging */
#define	UFSMNT_LOGGING		0x00000800	/* enable logging */

#define	UFSMNT_ONERROR_PANIC_STR	"panic"
#define	UFSMNT_ONERROR_LOCK_STR		"lock"
#define	UFSMNT_ONERROR_UMOUNT_STR	"umount"

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FS_UFS_MOUNT_H */
