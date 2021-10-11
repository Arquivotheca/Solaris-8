/*
 * Copyright (c) 1994-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef	_SYS_SACACHE_H
#define	_SYS_SACACHE_H

#pragma ident	"@(#)sacache.h	1.3	96/06/05 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/filep.h>
#include <sys/dirent.h>

/*
 * there are common cache interface routines
 */

extern	void		*get_icache(int dev, int inum);
extern	void		set_icache(int dev, int inum, void *buf, int len);
extern	int		get_dcache(int dev, char *name, int pnum);
extern	void		set_dcache(int dev, char *name, int pnum, int inum);
extern  caddr_t		get_bcache(fileid_t *fp);
extern  int		set_bcache(fileid_t *fp);
extern	ino64_t		get_dnlc(int dev, char *name);
extern	void 		set_dnlc(int dev, char *name, ino64_t inum);
extern	char		*get_string_cache(int dev, char *string);
extern 	char		*set_string_cache(int dev, char *string);
extern	char		*get_negative_filename(int dev, char *path);
extern	void		set_negative_filename(int dev, char *path);
extern	char		*get_cfsb(int dev, ino64_t blkno, u_int size);
extern	void		set_cfsb(int dev, ino64_t blkno, void *buf, u_int size);
extern	void		release_cache(int dev);
extern	void		print_cache_data(void);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SACACHE_H */
