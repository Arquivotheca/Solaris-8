/*
 *	auto_mnttab.c
 *
 *	Copyright (c) 1988-1999 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)auto_mnttab.c	1.28	99/09/27 SMI"

#include <stdio.h>
#include <sys/mnttab.h>
#include <sys/mkdev.h>
#include "automount.h"

/*
 * Return device number from extmnttab struct
 */
dev_t
get_devid(mnt)
	struct extmnttab *mnt;
{
	return (makedev(mnt->mnt_major, mnt->mnt_minor));
}
