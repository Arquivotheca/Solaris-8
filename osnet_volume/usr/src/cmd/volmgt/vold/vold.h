/*
 * Copyright (c) 1995-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef __VOLD_H
#define	__VOLD_H

#pragma ident	"@(#)vold.h	1.48	97/04/05 SMI"

#include <sys/types.h>
#include <sys/resource.h>
#include <rpcsvc/nfs_prot.h>
#include <nfs/mount.h>
#include <sys/vtoc.h>
#include <libintl.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* forward declarations for the following include files */
struct label;
struct devs;
struct vvnode;
struct vol;

#include "util.h"
#include "action.h"
#include "label.h"
#include "dev.h"
#include "obj.h"
#include "node.h"
#include "db.h"

/*
 * Create a vol_t from a label and a name.
 */
vol_t		*vol_mkvol(struct devs *, label *);

extern uid_t	default_uid;
extern gid_t	default_gid;
extern rlim_t	original_nofile;
extern char	*volume_group;
extern char	*vold_config;
extern char	*nisplus_group;
extern char	*vold_root;

/*
 * define P0_WA (P0 workaround) for Intel (see bug 1153841, 1164198, and
 *      1164200)
 */
#if defined(_FIRMWARE_NEEDS_FDISK)
#define	P0_WA	/* work around bug# 1153841 */
#else
#undef	P0_WA	/* won't work on SPARCs */
#endif	/* defined(_FIRMWARE_NEEDS_FDISK) */

/*
 * default media types name -- this is just a list of know/used types
 * at the current time
 */
#define	CDROM_MTYPE		"cdrom"
#define	FLOPPY_MTYPE		"floppy"
#define	PCMEM_MTYPE		"pcmem"
#define	MO_MTYPE		"mo"
#define	HARD_MTYPE		"hard"
#define	OTHER_MTYPE		"other"
#define	TEST_MTYPE		"test"

/*
 * entry for the third field of the "use ..." lines in vold.conf
 */
#define	DRIVE_CLASS		"drive"
#define	TEST_CLASS		"test"

/*
 * entries for file-system types
 */
#define	PCFS_LTYPE		"dos"
#define	UFS_LTYPE		"sun"
#define	ISO9660_LTYPE		"cdrom"
#define	TEST_LTYPE		"test"

/*
 * defuault values
 */
#ifdef DEBUG
#define	DEFAULT_VERBOSE		1
#define	DEFAULT_DEBUG		6
#else
#define	DEFAULT_VERBOSE		0
#define	DEFAULT_DEBUG		0
#endif

#define	DEFAULT_USER		"nobody"
#define	DEFAULT_GROUP		"nobody"
#define	DEFAULT_TOP_UID		0
#define	DEFAULT_TOP_GID		0
#define	DEFAULT_ROOT_MODE	0555
#define	DEFAULT_TOP_MODE	01777
#define	DEFAULT_MODE		0666
#define	DEFAULT_UNLAB		"unlabeled"
#define	DEFAULT_UNFORMAT	"unformatted"
#define	DEFAULT_NOTUNIQUE	"nonunique"
#define	DEFAULT_VOLUME_GROUP	""
#define	DEFAULT_VOLD_CONFIG	"/etc/vold.conf"
#define	DEFAULT_VOLD_ROOT	"/vol"
#define	DEFAULT_VOLD_LOG	"/var/adm/vold.log"
#define	DEFAULT_VOLD_DEVDIR	"/usr/lib/vold"
#define	DEFAULT_UNSAFE		20
#define	DEFAULT_SERVICE		"vold"
#define	DEFAULT_NISPLUS_GROUP	"volmgt"
#define	DEFAULT_POLLTIME	(-1)

/* default slice to use when none present */
#define	DEFAULT_PARTITION	2

/* size of ctime_r buffers */
#define	CTBSIZE			26

/* null character */
#define	NULLC			'\0'

/* error reporting mechanism */
void 	fatal(const char *, ...);
void 	quit(const char *, ...);
void 	noise(const char *, ...);
void 	warning(const char *, ...);
void 	info(const char *, ...);
void 	debug(u_int, const char *, ...);
void	nfstrace(const char *, ...);
void	dbxtrap(const char *);
void	setlog(char *path);

#ifdef DEBUG
#define	ASSERT(EX)	((void)((EX) || failass(#EX, __FILE__, __LINE__)))
extern int		failass(char *, char *, int);
#else
#define	ASSERT(EX)
#endif

#ifdef	__cplusplus
}
#endif

#endif /* __VOLD_H */
