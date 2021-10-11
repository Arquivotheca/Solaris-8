/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights reserved.
 */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)g_init.c	1.8	96/01/18 SMI"	/* SVr4.0 1.4	*/

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mkdev.h>
#include <sys/statvfs.h>
#include <libgenIO.h>


/*
 * table of devices major numbers and their device type.
 */

int devices[] = {
    G_NO_DEV,		/* major 0			*/
    G_TM_TAPE,		/*  1: Tapemaster controller    */
    G_NO_DEV,		/* major 2			*/
    G_XY_DISK,		/* 3: xy 450/451 disk ctrls	*/
    G_NO_DEV,		/* major 4			*/
    G_NO_DEV,		/* major 5			*/
    G_NO_DEV,		/* major 6			*/
    G_SD_DISK,		/* 7: sd disks			*/
    G_XT_TAPE,		/* 8: xt tapes			*/
    G_SF_FLOPPY,		/* 9: sf floppy			*/
    G_XD_DISK,		/* 10: xd disk			*/
    G_ST_TAPE,		/* 11: st tape			*/
    G_NS,		/* 12: noswap pseudo-dev	*/
    G_RAM,		/* 13: ram pseudo-dev		*/
    G_FT,		/* 14: tftp			*/
    G_HD,		/* 15: 386 network disk		*/
    G_FD,		/* 16: 386 AT disk		*/
    G_NO_DEV,		/* major 17			*/
    G_NO_DEV,		/* major 18			*/
    G_NO_DEV		/* major 19			*/
    };



/*
 * g_init: Determine the device being accessed, set the buffer size,
 * and perform any device specific initialization. Since at this point
 * Sun has no system call to read the configuration, the major numbers
 * are assumed to be static and types are figured out as such. However,
 * as a rough estimate, the buffer size for all types is set to 512
 * as a default.
 */

int
g_init(int *devtype, int *fdes)
{
	major_t maj;
	int bufsize;
	struct stat64 st_buf;
	struct statvfs64 stfs_buf;

	*devtype = G_NO_DEV;
	if (fstat64(*fdes, &st_buf) == -1)
		return (-1);
	if ((st_buf.st_mode & S_IFCHR) == 0 &&
	    (st_buf.st_mode & S_IFBLK) == 0) {
		if (st_buf.st_mode & S_IFIFO) {
			bufsize = 512;
		} else {
			/* find block size for this file system */
			*devtype = G_FILE;
			if (fstatvfs64(*fdes, &stfs_buf) < 0) {
					bufsize = -1;
					errno = ENODEV;
			} else
				bufsize = stfs_buf.f_bsize;
		}
		return (bufsize);

	/*
	 * We'll have to add a remote attribute to stat but this
	 * should work for now.
	 */
	} else if (st_buf.st_dev & 0x8000)	/* if remote  rdev */
		return (512);

	maj = major(st_buf.st_rdev);
	if ((int) maj < G_DEV_MAX)	/* prevention */
		*devtype = devices[(int)maj];
	switch (*devtype) {
		case G_NO_DEV:
		case G_TM_TAPE:
			bufsize = 512;
			break;
		case G_XY_DISK:
		case G_SD_DISK:
		case G_XT_TAPE:
		case G_SF_FLOPPY:
		case G_XD_DISK:
		case G_ST_TAPE:
			bufsize = 512;	/* for now */
			break;
		case G_NS:
		case G_RAM:
		case G_FT:
		case G_HD:
		case G_FD:
			bufsize = 512;	/* for now */
			break;
		default:
			bufsize = -1;
			errno = ENODEV;
	} /* *devtype */
	return (bufsize);
}

/*
 * ident: Used to determine if the pathname and stat(2) structure
 * passwd by ftw(3C) coorresponds to the device that was specified
 * as the standard input or standard output of the parent command.
 * This is necessary for streaming on the 3B2 CTC, as the device
 * must be closed and then re-opened by name to permit streaming.
 */
#if 0
static
int
ident(path, new_stat, ftype)
char *path;
struct stat *new_stat;
int ftype;
{

	if (ftype != FTW_F || new_stat->st_mode != orig_st_buf.st_mode ||
		new_stat->st_rdev != orig_st_buf.st_rdev ||
		new_stat->st_uid != orig_st_buf.st_uid ||
		new_stat->st_gid != orig_st_buf.st_gid)
		return (0);
	if ((ctcpath = malloc((unsigned)strlen(path) + 1)) == (char *)NULL)
		return (-1);
	(void) strcpy(ctcpath, path);
	return (1);
}
#endif
