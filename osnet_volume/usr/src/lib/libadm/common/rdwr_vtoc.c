/*
 * Copyright (c) 1991-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)rdwr_vtoc.c	1.10	97/07/22 SMI"

/*LINTLIBRARY*/


#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/dkio.h>
#include <sys/vtoc.h>


/*
 * Read VTOC - return partition number.
 */
int
read_vtoc(int fd, struct vtoc *vtoc)
{
	struct dk_cinfo		dki_info;

	/*
	 * Read the vtoc.
	 */
	if (ioctl(fd, DKIOCGVTOC, (caddr_t) vtoc) == -1) {
		switch (errno) {
		case EIO:
			return (VT_EIO);
		case EINVAL:
			return (VT_EINVAL);
		default:
			return (VT_ERROR);
		}
	}

	/*
	 * Sanity-check the vtoc.
	 */
	if (vtoc->v_sanity != VTOC_SANE) {
		return (VT_EINVAL);
	}

	/*
	 * Convert older-style vtoc's.
	 */
	switch (vtoc->v_version) {
	case 0:
		/*
		 * No vtoc information.  Install default
		 * nparts/sectorsz and version.  We are
		 * assuming that the driver returns the
		 * current partition information correctly.
		 */

		vtoc->v_version = V_VERSION;
		if (vtoc->v_nparts == 0)
			vtoc->v_nparts = V_NUMPAR;
		if (vtoc->v_sectorsz == 0)
			vtoc->v_sectorsz = DEV_BSIZE;

		break;

	case V_VERSION:
		break;

	default:
		return (VT_EINVAL);
	}

	/*
	 * Return partition number for this file descriptor.
	 */
	if (ioctl(fd, DKIOCINFO, (caddr_t) &dki_info) == -1) {
		switch (errno) {
		case EIO:
			return (VT_EIO);
		case EINVAL:
			return (VT_EINVAL);
		default:
			return (VT_ERROR);
		}
	}
	if (dki_info.dki_partition > V_NUMPAR) {
		return (VT_EINVAL);
	}
	return ((int)dki_info.dki_partition);
}



/*
 * Write VTOC
 */
int
write_vtoc(int fd, struct vtoc *vtoc)
{
	int i;
	/*
	 * Sanity-check the vtoc
	 */
	if (vtoc->v_sanity != VTOC_SANE || vtoc->v_nparts > V_NUMPAR) {
		return (-1);
	}

	/*
	 * since many drivers won't allow opening a device make sure
	 * all partitions aren't being set to zero. If all are zero then
	 * we have no way to set them to something else
	 */

	for (i = 0; i < (int)vtoc->v_nparts; i++)
		if (vtoc->v_part[i].p_size > 0)
			break;
	if (i == (int)vtoc->v_nparts)
		return (-1);

	/*
	 * Write the vtoc
	 */
	if (ioctl(fd, DKIOCSVTOC, (caddr_t) vtoc) == -1) {
		switch (errno) {
		case EIO:
			return (VT_EIO);
		case EINVAL:
			return (VT_EINVAL);
		default:
			return (VT_ERROR);
		}
	}
	return (0);
}
