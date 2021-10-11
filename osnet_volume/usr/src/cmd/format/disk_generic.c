
/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)disk_generic.c	1.6	98/06/05 SMI"

/*
 * This file contains functions that implement the fdisk menu commands.
 */
#include "global.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

#include <sys/dklabel.h>
#include <errno.h>


#include "main.h"
#include "analyze.h"
#include "menu.h"
#include "menu_command.h"
#include "menu_defect.h"
#include "menu_partition.h"
#if defined(_FIRMWARE_NEEDS_FDISK)
#include "menu_fdisk.h"
#endif	/* defined(_FIRMWARE_NEEDS_FDISK) */
#include "param.h"
#include "misc.h"
#include "label.h"
#include "startup.h"
#include "partition.h"
#include "prompts.h"
#include "checkmount.h"
#include "io.h"
#include "ctlr_scsi.h"
#include "auto_sense.h"
#include "disk_generic.h"

struct  ctlr_ops genericops = {
	generic_rdwr,
	generic_ck_format,
	0,
	0,
	0,
	0,
	0,
};


/*
 * Check to see if the disk has been formatted.
 * If we are able to read the first track, we conclude that
 * the disk has been formatted.
 */
int
generic_ck_format()
{
	int	status;

	/*
	 * Try to read the first four blocks.
	 */
	status = generic_rdwr(DIR_READ, cur_file, 0, 4, (caddr_t)cur_buf,
			F_SILENT, NULL);
	return (!status);
}

/*
 * Read or write the disk.
 * Temporary interface until IOCTL interface finished.
 */
/*ARGSUSED*/
int
generic_rdwr(dir, fd, blkno, secnt, bufaddr, flags, xfercntp)
	int	dir;
	int	fd;
	daddr_t	blkno;
	int	secnt;
	caddr_t	bufaddr;
	int	flags;
	int	*xfercntp;
{

	int	tmpsec, status, tmpblk;

	tmpsec = secnt * UBSIZE;
	tmpblk = blkno * UBSIZE;

	if (dir == DIR_READ) {
		status = lseek(fd, tmpblk, SEEK_SET);
		if (status != tmpblk)
			return (status);

		status = read(fd, bufaddr, tmpsec);
		if (status != tmpsec)
			return (tmpsec);
		else
			return (0);
	} else {
		status = lseek(fd, tmpblk, SEEK_SET);
		if (status != tmpblk)
			return (status);

		status = write(fd, bufaddr, tmpsec);
		if (status != tmpsec)
			return (tmpsec);
		else
			return (0);
	}
}
