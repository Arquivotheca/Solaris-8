/*
 * Copyright (c) 1991,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_FS_UFS_FILIO_H
#define	_SYS_FS_UFS_FILIO_H

#pragma ident	"@(#)ufs_filio.h	1.19	98/08/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * _FIOIO
 *
 * struct for _FIOIO ioctl():
 *	Input:
 *		fio_ino	- inode number
 *		fio_gen	- generation number
 *	Output:
 *		fio_fd	- readonly file descriptor
 *
 */

struct fioio {
	ino_t	fio_ino;	/* input : inode number */
	int	fio_gen;	/* input : generation number */
	int	fio_fd;		/* output: readonly file descriptor */
};

#if defined(_SYSCALL32)

struct fioio32 {
	ino32_t	fio_ino;	/* input : inode number */
	int32_t	fio_gen;	/* input : generation number */
	int32_t	fio_fd;		/* output: readonly file descriptor */
};

#endif	/* _SYSCALL32 */

/*
 * _FIOTUNE
 */
struct fiotune {
	int	maxcontig;	/* cluster and directio size */
	int	rotdelay;	/* skip blocks between contig allocations */
	int	maxbpg;		/* currently defaults to 2048 */
	int	minfree;	/* %age to reserve for root */
	int	optim;		/* space or time */
};

/*
 * UFS Logging
 */
typedef struct fiolog {
	uint_t	nbytes_requested;
	uint_t	nbytes_actual;
	int	error;
} fiolog_t;

#define	FIOLOG_ENONE	0
#define	FIOLOG_ETRANS	1
#define	FIOLOG_EROFS	2
#define	FIOLOG_EULOCK	3
#define	FIOLOG_EWLOCK	4
#define	FIOLOG_ECLEAN	5
#define	FIOLOG_ENOULOCK	6

#if defined(_KERNEL) && defined(__STDC__)

extern	int	ufs_fiosatime(struct vnode *, struct timeval *, struct cred *);
extern	int	ufs_fiosdio(struct vnode *, uint_t *, int flag, struct cred *);
extern	int	ufs_fiogdio(struct vnode *, uint_t *, int flag, struct cred *);
extern	int	ufs_fioio(struct vnode *, struct fioio *, struct cred *);
extern	int	ufs_fioisbusy(struct vnode *, int *, struct cred *);
extern	int	ufs_fiodirectio(struct vnode *, int, struct cred *);
extern	int	ufs_fiotune(struct vnode *, struct fiotune *, struct cred *);
extern	int	ufs_fiologenable(vnode_t *, fiolog_t *, cred_t *);
extern	int	ufs_fiologdisable(vnode_t *, fiolog_t *, cred_t *);
extern	int	ufs_fioislog(vnode_t *, uint32_t *, cred_t *);

#endif	/* defined(_KERNEL) && defined(__STDC__) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FS_UFS_FILIO_H */
