/*
 * Copyright (c) 1993, by Sun Microsystems, Inc.
 */

#ifndef _SYS_FS_UFS_MOUNT_H
#define	_SYS_FS_UFS_MOUNT_H

#pragma ident	"@(#)s5_mount.h	1.1	93/02/17 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct s5_args {
	int	flags;		/* flags */
};

/*
 * UFS mount option flags
 */
#define	S5MNT_NOINTR	0x00000001	/* disallow interrupts on lockfs */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FS_UFS_MOUNT_H */
