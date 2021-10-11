
/*
 * Copyright (c) 1993-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MENU_FDISK_H
#define	_MENU_FDISK_H

#pragma ident	"@(#)menu_fdisk.h	1.6	96/11/12 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	Prototypes for ANSI
 */
int	get_solaris_part(int fd, struct ipart *ipart);
int	copy_solaris_part(struct ipart *ipart);
void	open_cur_file(int);

/*
 * These flags are used to open file descriptor for current
 *	disk (cur_file) with "p0" path or cur_disk->disk_path
 */
#define	FD_USE_P0_PATH		0
#define	FD_USE_CUR_DISK_PATH	1

#ifdef	__cplusplus
}
#endif

#endif	/* _MENU_FDISK_H */
