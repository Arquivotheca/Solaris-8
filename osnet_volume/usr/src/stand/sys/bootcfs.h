/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef	_SYS_BOOTCFS_H
#define	_SYS_BOOTCFS_H

#pragma ident	"@(#)bootcfs.h	1.6	97/12/22 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NULL
#if defined(_LP64) && !defined(__cplusplus)
#define NULL    0L
#else
#define	NULL	0
#endif
#endif

/*
 * Return value for *integer* functions
 */
#define	SUCCESS	0
#define	FAILURE	-1

/*
 * These files reside in the front (ufs) file system.
 */
#define	CFS_FSINFO	"/.cachefsinfo"
#define	CFS_MONIKER	CFS_FSINFO
/*
 * example (content):
 * (line 1) backfs=nfs
 * (line 2) backfsdev=/sbus@1,f8000000/le@0,c00000
 */

/*
 * default cachefs root directory and names
 * ROOTCACHEID has ROOTCACHE as prefix
 */
#define	ROOTCACHE "/rootcache"
#define	ROOTCACHEID ROOTCACHE "/rootcache"
#define	BACKFS_FD_OFFSET 10000
#define	CACHEFSMD_VALID	(MD_FILE | MD_POPULATED)
#define	CDIRBUFSIZE	1024	/* MAXNAMELEN is 512, add c_dirent size */
#define	CDIR_INVALID	0
#define	CDIR_DIRENT	1	/* it is a directory w/ non-zero length */
#define	CDIR_NONDIRENT	2	/* it is not a directory */
#define	CDIR_SYMLINKENT	3	/* it is a symlink, use dir_buf as pointer */
#define	CACHEFSDEV	10	/* boot device # for cache */

extern	char	*backfs_dev, *backfs_fstype;
extern	char	*frontfs_dev, *frontfs_fstype;
extern	struct	boot_fs_ops *backfs_ops;
extern	struct	boot_fs_ops *frontfs_ops;

extern	int	cachefs_getbfs(char *frontfstype, char *rootdev,
		char *backfstypep, char *backfsdev);
extern  int 	get_backfsinfo(char *bpath, char *backfsdev);
extern  ino_t	get_root_ino(void);
extern	int	nfs_has_cfs(char *bpath);
extern	int	has_ufs_fs(char *bpath, int *hascfsflag);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_BOOTCFS_H */
