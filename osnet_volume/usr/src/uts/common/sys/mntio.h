/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_MNTIO_H
#define	_SYS_MNTIO_H

#pragma ident	"@(#)mntio.h	1.3	99/08/11 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Mntfs io control commands
 */
#define	MNTIOC			('m' << 8)
#define	MNTIOC_NMNTS		(MNTIOC|1)	/* Get # of mounted resources */
#define	MNTIOC_GETDEVLIST	(MNTIOC|2)	/* Get mounted dev no.'s */
#define	MNTIOC_SETTAG		(MNTIOC|3)	/* Set a tag on a mounted fs */
#define	MNTIOC_CLRTAG		(MNTIOC|4)	/* Clear a tag from a fs */

#define	MAX_MNTOPT_TAG	64	/* Maximum size for a mounted file system tag */

struct mnttagdesc {
	uint_t	mtd_major;		/* major number of mounted resource */
	uint_t	mtd_minor;		/* minor number of mounted resource */
	char	*mtd_mntpt;		/* mount point for mounted resource */
	char	*mtd_tag;		/* tag to set/clear */
};

#ifdef _SYSCALL32
struct mnttagdesc32 {
	uint32_t	mtd_major;	/* major number of mounted resource */
	uint32_t	mtd_minor;	/* minor number of mounted resource */
	caddr32_t	mtd_mntpt;	/* mount point for mounted resource */
	caddr32_t	mtd_tag;	/* tag to set/clear */
};
#endif /* _SYSCALL32 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MNTIO_H */
