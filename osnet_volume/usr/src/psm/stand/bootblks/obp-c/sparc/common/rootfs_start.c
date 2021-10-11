
/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)rootfs_start.c 1.2     95/06/27 SMI"

static u_int unix_start;

/*
 * Find the starting block of the root slice in the Solaris
 * fdisk partition.  This routine cracks the fdisk table,
 * finds the solaris fdisk partition, and finds the root
 * slice.
 */
void
get_rootfs_start(char *device)
{
	unix_start = 0;
}

u_int
fdisk2rootfs(u_int offset)
{
	return (offset + unix_start);
}
