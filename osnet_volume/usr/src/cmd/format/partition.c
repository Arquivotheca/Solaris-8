
/*
 * Copyright (c) 1991-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef lint
#pragma ident	"@(#)partition.c	1.12	97/11/22 SMI"
#endif	lint

/*
 * This file contains functions that operate on partition tables.
 */
#include "global.h"
#include "partition.h"
#include "misc.h"
#include "menu_command.h"
#include "menu_partition.h"
#include <string.h>


/*
 * Default vtoc information for non-SVr4 partitions
 */
struct dk_map2	default_vtoc_map[NDKMAP] = {
	{	V_ROOT,		0	},		/* a - 0 */
	{	V_SWAP,		V_UNMNT	},		/* b - 1 */
	{	V_BACKUP,	V_UNMNT	},		/* c - 2 */
	{	V_UNASSIGNED,	0	},		/* d - 3 */
	{	V_UNASSIGNED,	0	},		/* e - 4 */
	{	V_UNASSIGNED,	0	},		/* f - 5 */
	{	V_USR,		0	},		/* g - 6 */
	{	V_UNASSIGNED,	0	},		/* h - 7 */

#if defined(_SUNOS_VTOC_16)

#if defined(i386)
	{	V_BOOT,		V_UNMNT	},		/* i - 8 */
	{	V_ALTSCTR,	0	},		/* j - 9 */

#else
#error No VTOC format defined.
#endif			/* defined(i386) */

	{	V_UNASSIGNED,	0	},		/* k - 10 */
	{	V_UNASSIGNED,	0	},		/* l - 11 */
	{	V_UNASSIGNED,	0	},		/* m - 12 */
	{	V_UNASSIGNED,	0	},		/* n - 13 */
	{	V_UNASSIGNED,	0	},		/* o - 14 */
	{	V_UNASSIGNED,	0	},		/* p - 15 */
#endif			/* defined(_SUNOS_VTOC_16) */
};


/*
 * This routine allows the user to change the boundaries of the given
 * partition in the current partition map.
 */
void
change_partition(int num)
{
	int		i;
	int		j;
	int		deflt;
	u_ioparam_t	ioparam;
	int		tag;
	int		flag;
	char		msg[256];

	/*
	 * check if there exists a partition table for the disk.
	 */
	if (cur_parts == NULL) {
		err_print("Current Disk has no partition table.\n");
		return;
	}


	/*
	 * Print out the given partition so the user knows what he/she's
	 * getting into.
	 */
	print_partition(cur_parts, num, 1);
	fmt_print("\n");

	/*
	 * Prompt for p_tag and p_flag values for this partition.
	 */
	assert(cur_parts->vtoc.v_version == V_VERSION);
	deflt = cur_parts->vtoc.v_part[num].p_tag;
	(void) sprintf(msg, "Enter partition id tag");
	ioparam.io_slist = ptag_choices;
	tag = input(FIO_SLIST, msg, ':', &ioparam, &deflt, DATA_INPUT);

	deflt = cur_parts->vtoc.v_part[num].p_flag;
	(void) sprintf(msg, "Enter partition permission flags");
	ioparam.io_slist = pflag_choices;
	flag = input(FIO_SLIST, msg, ':', &ioparam, &deflt, DATA_INPUT);

	/*
	 * Ask for the new values.  The old values are the defaults, and
	 * strict bounds checking is done on the values given.
	 */
	ioparam.io_bounds.lower = 0;
	ioparam.io_bounds.upper = ncyl - 1;
	deflt = cur_parts->pinfo_map[num].dkl_cylno;
	i = input(FIO_INT, "Enter new starting cyl", ':', &ioparam,
	    &deflt, DATA_INPUT);

	ioparam.io_bounds.lower = 0;
	ioparam.io_bounds.upper = (ncyl - i) * spc();
	deflt = min(cur_parts->pinfo_map[num].dkl_nblk,
			ioparam.io_bounds.upper);
	j = input(FIO_CYL, "Enter partition size", ':', &ioparam,
	    &deflt, DATA_INPUT);

	/*
	 * If the current partition has a size of zero change the
	 * tag to Unassigned and the starting cylinder to zero
	 */
	if (j == 0) {
		tag = V_UNASSIGNED;
		i = 0;
	}

	/*
	 * If user has entered a V_BACKUp tag then the partition
	 * size should specify full disk capacity else
	 * return an Error.
	 */
	if (tag == V_BACKUP) {
		int fullsz;

		fullsz = ncyl * nhead * nsect;
		if (fullsz != j) {
		/*
		 * V_BACKUP Tag Partition != full disk capacity.
		 * print useful messages.
		 */
		fmt_print("\nWarning: Partition with V_BACKUP tag should ");
		fmt_print("specify full disk capacity. \n");
		return;
		}
	}


	/*
	 * If the current partition is named, we can't change it.
	 * We create a new current partition map instead.
	 */
	if (cur_parts->pinfo_name != NULL)
		make_partition();
	/*
	 * Change the values.
	 */
	cur_parts->pinfo_map[num].dkl_cylno = i;
	cur_parts->pinfo_map[num].dkl_nblk = j;

#if defined(_SUNOS_VTOC_16)
	cur_parts->vtoc.v_part[num].p_start = (daddr_t)(i * (nhead * nsect));
	cur_parts->vtoc.v_part[num].p_size = (long)j;
#endif			/* defined(_SUNOS_VTOC_16) */

	/*
	 * Install the p_tag and p_flag values for this partition
	 */
	assert(cur_parts->vtoc.v_version == V_VERSION);
	cur_parts->vtoc.v_part[num].p_tag = (u_short) tag;
	cur_parts->vtoc.v_part[num].p_flag = (u_short) flag;
}


/*
 * This routine picks to closest partition table which matches the
 * selected disk type.  It is called each time the disk type is
 * changed.  If no match is found, it uses the first element
 * of the partition table.  If no table exists, a dummy is
 * created.
 */
int
get_partition()
{
	register struct partition_info *pptr;
	register struct partition_info *parts;

	/*
	 * If there are no pre-defined maps for this disk type, it's
	 * an error.
	 */
	parts = cur_dtype->dtype_plist;
	if (parts == NULL) {
		err_print("No defined partition tables.\n");
		make_partition();
		return (-1);
	}
	/*
	 * Loop through the pre-defined maps searching for one which match
	 * disk type.  If found copy it into unmamed partition.
	 */
	enter_critical();
	for (pptr = parts; pptr != NULL; pptr = pptr->pinfo_next) {
		if (pptr->pinfo_name != NULL && strcmp(pptr->pinfo_name,
				cur_dtype->dtype_asciilabel) == 0) {
			/*
			 * Set current partition and name it.
			 */
			cur_disk->disk_parts = cur_parts = pptr;
			cur_parts->pinfo_name = pptr->pinfo_name;
			exit_critical();
			return (0);
		}
	}
	/*
	 * If we couldn't find a match, take the first one.
	 * Set current partition and name it.
	 */
	cur_disk->disk_parts = cur_parts = cur_dtype->dtype_plist;
	cur_parts->pinfo_name = parts->pinfo_name;
	exit_critical();
	return (0);
}


/*
 * This routine creates a new partition map and sets it current.  If there
 * was a current map, the new map starts out identical to it.  Otherwise
 * the new map starts out all zeroes.
 */
void
make_partition()
{
	register struct partition_info *pptr, *parts;
	int	i;

	/*
	 * Lock out interrupts so the lists don't get mangled.
	 */
	enter_critical();
	/*
	 * Get space for for the new map and link it into the list
	 * of maps for the current disk type.
	 */
	pptr = (struct partition_info *)zalloc(sizeof (struct partition_info));
	parts = cur_dtype->dtype_plist;
	if (parts == NULL) {
		cur_dtype->dtype_plist = pptr;
	} else {
		while (parts->pinfo_next != NULL) {
			parts = parts->pinfo_next;
		}
		parts->pinfo_next = pptr;
		pptr->pinfo_next = NULL;
	}
	/*
	 * If there was a current map, copy its values.
	 */
	if (cur_parts != NULL) {
		for (i = 0; i < NDKMAP; i++) {
			pptr->pinfo_map[i] = cur_parts->pinfo_map[i];
		}
		pptr->vtoc = cur_parts->vtoc;
	} else {
		/*
		** Otherwise set initial default vtoc values
		*/
		set_vtoc_defaults(pptr);
	}

	/*
	 * Make the new one current.
	 */
	cur_disk->disk_parts = cur_parts = pptr;
	exit_critical();
}


/*
 * This routine deletes a partition map from the list of maps for
 * the given disk type.
 */
void
delete_partition(struct partition_info *parts)
{
	struct	partition_info *pptr;

	/*
	 * If there isn't a current map, it's an error.
	 */
	if (cur_dtype->dtype_plist == NULL) {
		err_print("Error: unexpected null partition list.\n");
		fullabort();
	}
	/*
	 * Remove the map from the list.
	 */
	if (cur_dtype->dtype_plist == parts)
		cur_dtype->dtype_plist = parts->pinfo_next;
	else {
		for (pptr = cur_dtype->dtype_plist; pptr->pinfo_next != parts;
		    pptr = pptr->pinfo_next)
			;
		pptr->pinfo_next = parts->pinfo_next;
	}
	/*
	 * Free the space it was using.
	 */
	destroy_data((char *)parts);
}


/*
 * Set all partition vtoc fields to defaults
 */
void
set_vtoc_defaults(struct partition_info *part)
{
	int	i;

	bzero((caddr_t)&part->vtoc, sizeof (struct dk_vtoc));

	part->vtoc.v_version = V_VERSION;
	part->vtoc.v_nparts = NDKMAP;
	part->vtoc.v_sanity = VTOC_SANE;

	for (i = 0; i < NDKMAP; i++) {
		part->vtoc.v_part[i].p_tag = default_vtoc_map[i].p_tag;
		part->vtoc.v_part[i].p_flag = default_vtoc_map[i].p_flag;
	}
}
