/*
 * High Sierra filesystem internal routine definitions.
 * Copyright (c) 1989,1990,1993,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_FS_HSFS_IMPL_H
#define	_SYS_FS_HSFS_IMPL_H

#pragma ident	"@(#)hsfs_impl.h	1.7	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * global routines.
 */

extern int hsfs_putapage(vnode_t *, page_t *, u_offset_t *, size_t *, int,
	cred_t *);
/* read a sector */
extern int hs_readsector(struct vnode *vp, uint_t secno, uchar_t *ptr);
/* lookup/construct an hsnode/vnode */
extern struct vnode *hs_makenode(struct hs_direntry *dp,
	uint_t lbn, uint_t off, struct vfs *vfsp);
/* make hsnode from directory lbn/off */
extern int hs_remakenode(uint_t lbn, uint_t off, struct vfs *vfsp,
	struct vnode **vpp);
/* lookup name in directory */
extern int hs_dirlook(struct vnode *dvp, char *name, int namlen,
	struct vnode **vpp, struct cred *cred);
/* find an hsnode in the hash list */
extern struct vnode *hs_findhash(ulong_t nodeid, struct vfs *vfsp);
/* destroy an hsnode */
extern void hs_freenode(struct hsnode *hp, struct vfs *vfsp, int nopage);
/* destroy the incore hnode table */
extern void hs_freehstbl(struct vfs *vfsp);
/* parse a directory entry */
extern int hs_parsedir(struct hsfs *fsp, uchar_t *dirp,
	struct hs_direntry *hdp, char *dnp, int *dnlen);
/* convert d-characters */
extern int hs_namecopy(char *from, char *to, int size, ulong_t flags);
/* destroy the incore hnode table */
extern void hs_filldirent(struct vnode *vp, struct hs_direntry *hdp);
/* check vnode protection */
extern int hs_access(struct vnode *vp, mode_t m, struct cred *cred);

extern int hs_synchash(struct vfs *vfsp);

extern void hs_parse_dirdate(uchar_t *dp, struct timeval *tvp);
extern void hs_parse_longdate(uchar_t *dp, struct timeval *tvp);
extern int hs_uppercase_copy(char *from, char *to, int size);
extern struct hstable *hs_inithstbl(struct vfs *vfsp);
extern void hs_log_bogus_disk_warning(struct hsfs *fsp, int errtype,
	uint_t data);
extern int hsfs_valid_dir(struct hs_direntry *hd);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FS_HSFS_IMPL_H */
