/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */
#ident	"@(#)hsfsconf.c	1.5	96/11/13 SMI"

#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>

/*
 * Cachefs support
 */
char *frontfs_fstype = NULL;
char *backfs_fstype = NULL;
char *frontfs_dev = NULL;
char *backfs_dev = NULL;

/*
 *  Function prototypes (Global/Imported)
 */
#ifdef	lint
extern void translate_v2tov0(char *bkdev, char *npath);
#endif

/* HSFS Support */
extern	struct boot_fs_ops	boot_hsfs_ops;

struct boot_fs_ops *boot_fsw[] = {
	&boot_hsfs_ops,
};

int boot_nfsw = sizeof (boot_fsw) / sizeof (boot_fsw[0]);
static char *fstype = "hsfs";

/*ARGSUSED*/
char *
set_fstype(char *v2path, char *bpath)
{
#ifdef	lint
	translate_v2tov0(v2path, bpath);
#endif	lint
	set_default_fs(fstype);
	return (fstype);
}
