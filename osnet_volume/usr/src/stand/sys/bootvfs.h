/*
 * Copyright (c) 1994-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef	_SYS_BOOTVFS_H
#define	_SYS_BOOTVFS_H

#pragma ident	"@(#)bootvfs.h	1.10	97/06/30 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/filep.h>
#include <sys/dirent.h>

/* same as those in /usr/include/unistd.h */
#define	SEEK_SET	0	/* Offset */
#define	SEEK_CUR	1	/* Current + Offset */
#define	SEEK_END	2	/* EOF + Offset */

/*
 * unified (vfs-like) file system operations for booters
 */

struct boot_fs_ops {
    char	*fsw_name;
    int		(*fsw_mountroot)(char *str);
    int		(*fsw_unmountroot)(void);
    int		(*fsw_open)(char *filename, int flags);
    int		(*fsw_close)(int fd);
    ssize_t	(*fsw_read)(int fd, caddr_t buf, size_t size);
    off_t	(*fsw_lseek)(int filefd, off_t addr, int whence);
    int		(*fsw_fstat)(int filefd, struct stat *buf);
    void	(*fsw_closeall)(int flag);
    int		(*fsw_getdents)(int fd, struct dirent *buf, unsigned size);
};

/*
 *  Function prototypes
 *
 *	fstat() (if exists) supports size and mode right now.
 */

extern	int	mountroot(char *str);
extern	int	unmountroot(void);
extern	int	open(char *filename, int flags);
extern	int	close(int fd);
extern	ssize_t	read(int fd, caddr_t buf, size_t size);
extern	off_t	lseek(int filefd, off_t addr, int whence);
extern	int	fstat(int fd, struct stat *buf);
extern	void	closeall(int flag);

extern	ssize_t	kern_read(int fd, caddr_t buf, size_t size);
extern	int	kern_open(char *filename, int flags);
extern	off_t	kern_seek(int fd, off_t hi, off_t lo);
extern	off_t	kern_lseek(int fd, off_t hi, off_t lo);
extern	int	kern_close(int fd);
extern	int	kern_fstat(int fd, struct stat *buf);
extern	int	kern_getdents(int fd, struct dirent *buf, size_t size);

/*
 * these are for common fs switch interface routines
 */
extern	int	boot_no_ops();		/* no ops entry */
extern	void	boot_no_ops_void();	/* no ops entry */

extern	struct boot_fs_ops *get_fs_ops_pointer(char *fsw_name);
extern	void	set_default_fs(char *fsw_name);
extern	char 	*set_fstype(char *v2path, char *bpath);

extern	struct boot_fs_ops *boot_fsw[];
extern	int	boot_nfsw;		/* number of entries in fsw[] */


#ifdef __cplusplus
}
#endif

#endif /* _SYS_BOOTVFS_H */
