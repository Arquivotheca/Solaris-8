
/*
 * Copyright (c) 1991-1994 by Sun Microsystems, Inc.
 */

#ifndef lint
#pragma ident	"@(#)menu_partition.c	1.13	96/05/17 SMI"
#endif	lint

/*
 * This file contains functions to implement the partition menu commands.
 */
#include "global.h"
#include <stdlib.h>
#include <string.h>

#include "partition.h"
#include "menu_partition.h"
#include "menu_command.h"
#include "misc.h"
#include "param.h"

/*
 * This routine implements the 'a' command.  It changes the 'a' partition.
 */
int
p_apart()
{

	change_partition(0);
	return (0);
}

/*
 * This routine implements the 'b' command.  It changes the 'b' partition.
 */
int
p_bpart()
{

	change_partition(1);
	return (0);
}

/*
 * This routine implements the 'c' command.  It changes the 'c' partition.
 */
int
p_cpart()
{

	change_partition(2);
	return (0);
}

/*
 * This routine implements the 'd' command.  It changes the 'd' partition.
 */
int
p_dpart()
{

	change_partition(3);
	return (0);
}

/*
 * This routine implements the 'e' command.  It changes the 'e' partition.
 */
int
p_epart()
{

	change_partition(4);
	return (0);
}

/*
 * This routine implements the 'f' command.  It changes the 'f' partition.
 */
int
p_fpart()
{

	change_partition(5);
	return (0);
}

/*
 * This routine implements the 'g' command.  It changes the 'g' partition.
 */
int
p_gpart()
{

	change_partition(6);
	return (0);
}

/*
 * This routine implements the 'h' command.  It changes the 'h' partition.
 */
int
p_hpart()
{

	change_partition(7);
	return (0);
}

#if defined(i386)
/*
 * This routine implements the 'j' command.  It changes the 'j' partition.
 */
int
p_jpart()
{

	change_partition(9);
	return (0);
}
#endif	/* defined(i386) */

/*
 * This routine implements the 'select' command.  It allows the user
 * to make a pre-defined partition map the current map.
 */
int
p_select()
{
	struct partition_info	*pptr, *parts;
	u_ioparam_t		ioparam;
	int			i, index, deflt, *defltptr = NULL;

	parts = cur_dtype->dtype_plist;
	/*
	 * If there are no pre-defined maps for this disk type, it's
	 * an error.
	 */
	if (parts == NULL) {
		err_print("No defined partition tables.\n");
		return (-1);
	}
	/*
	 * Loop through the pre-defined maps and list them by name.  If
	 * the current map is one of them, make it the default.  If any
	 * the maps are unnamed, label them as such.
	 */
	for (i = 0, pptr = parts; pptr != NULL; pptr = pptr->pinfo_next) {
		if (cur_parts == pptr) {
			deflt = i;
			defltptr = &deflt;
		}
		if (pptr->pinfo_name == NULL)
			fmt_print("        %d. unnamed\n", i++);
		else
			fmt_print("        %d. %s\n", i++, pptr->pinfo_name);
	}
	ioparam.io_bounds.lower = 0;
	ioparam.io_bounds.upper = i - 1;
	/*
	 * Ask which map should be made current.
	 */
	index = input(FIO_INT, "Specify table (enter its number)", ':',
	    &ioparam, defltptr, DATA_INPUT);
	for (i = 0, pptr = parts; i < index; i++, pptr = pptr->pinfo_next)
		;
	/*
	 * Before we blow the current map away, do some limits checking.
	 */
	for (i = 0; i < NDKMAP; i++)  {
		if (pptr->pinfo_map[i].dkl_cylno < 0 ||
			pptr->pinfo_map[i].dkl_cylno > (ncyl-1)) {
			err_print(
"partition %c: starting cylinder %d is out of range\n",
				(PARTITION_BASE+i),
				pptr->pinfo_map[i].dkl_cylno);
			return (0);
		}
		if (pptr->pinfo_map[i].dkl_nblk < 0 ||
			(int)pptr->pinfo_map[i].dkl_nblk > ((ncyl -
				pptr->pinfo_map[i].dkl_cylno) * spc())) {
			err_print(
"partition %c: specified # of blocks, %d, is out of range\n",
				(PARTITION_BASE+i),
				pptr->pinfo_map[i].dkl_nblk);
			return (0);
		}
	}
	/*
	 * Lock out interrupts so the lists don't get mangled.
	 */
	enter_critical();
	/*
	 * If the old current map is unnamed, delete it.
	 */
	if (cur_parts != NULL && cur_parts != pptr &&
	    cur_parts->pinfo_name == NULL)
		delete_partition(cur_parts);
	/*
	 * Make the selected map current.
	 */
	cur_disk->disk_parts = cur_parts = pptr;

#if defined(_SUNOS_VTOC_16)
	for (i = 0; i < NDKMAP; i++)  {
		cur_parts->vtoc.v_part[i].p_start =
		    (daddr_t)(cur_parts->pinfo_map[i].dkl_cylno *
		    (nhead * nsect));
		cur_parts->vtoc.v_part[i].p_size =
		    (long)cur_parts->pinfo_map[i].dkl_nblk;
	}
#endif	/* defined(_SUNOS_VTOC_16) */

	exit_critical();
	fmt_print("\n");
	return (0);
}

/*
 * This routine implements the 'name' command.  It allows the user
 * to name the current partition map.  If the map was already named,
 * the name is changed.  Once a map is named, the values of the partitions
 * cannot be changed.  Attempts to change them will cause another map
 * to be created.
 */
int
p_name()
{
	char	*name;

	/*
	 * check if there exists a partition table for the disk.
	 */
	if (cur_parts == NULL) {
		err_print("Current Disk has no partition table.\n");
		return (-1);
	}


	/*
	 * Ask for the name.  Note that the input routine will malloc
	 * space for the name since we are using the OSTR input type.
	 */
	name = (char *)input(FIO_OSTR, "Enter table name (remember quotes)",
	    ':', (u_ioparam_t *)NULL, (int *)NULL, DATA_INPUT);
	/*
	 * Lock out interrupts.
	 */
	enter_critical();
	/*
	 * If it was already named, destroy the old name.
	 */
	if (cur_parts->pinfo_name != NULL)
		destroy_data(cur_parts->pinfo_name);
	/*
	 * Set the name.
	 */
	cur_parts->pinfo_name = name;
	exit_critical();
	fmt_print("\n");
	return (0);
}


/*
 * This routine implements the 'print' command.  It lists the values
 * for all the partitions in the current partition map.
 */
int
p_print()
{
	/*
	 * check if there exists a partition table for the disk.
	 */
	if (cur_parts == NULL) {
		err_print("Current Disk has no partition table.\n");
		return (-1);
	}

	/*
	 * Print the volume name, if it appears to be set
	 */
	if (chk_volname(cur_disk)) {
		fmt_print("Volume:  ");
		print_volname(cur_disk);
		fmt_print("\n");
	}
	/*
	 * Print the name of the current map.
	 */
	if (cur_parts->pinfo_name != NULL) {
		fmt_print("Current partition table (%s):\n",
		    cur_parts->pinfo_name);
		fmt_print(
"Total disk cylinders available: %d + %d (reserved cylinders)\n\n", ncyl, acyl);
	} else {
		fmt_print("Current partition table (unnamed):\n");
		fmt_print(
"Total disk cylinders available: %d + %d (reserved cylinders)\n\n", ncyl, acyl);
	}


	/*
	 * Print the partition map itself
	 */
	print_map(cur_parts);
	return (0);
}


/*
 * Print a partition map
 */
void
print_map(struct partition_info *map)
{
	int	i;
	int	want_header;

	/*
	 * Loop through each partition, printing the header
	 * the first time.
	 */
	want_header = 1;
	for (i = 0; i < NDKMAP; i++) {
		if (i > 9) {
			break;
		}
		print_partition(map, i, want_header);
		want_header = 0;
	}

	fmt_print("\n");
}


/*
 * Print out one line of partition information,
 * with optional header.
 */
/*ARGSUSED*/
void
print_partition(struct partition_info *pinfo, int partnum, int want_header)
{
	int		i;
	int		nblks;
	int		cyl1;
	int		cyl2;
	float		scaled;
	int		maxcyl2;
	int		ncyl2_digits;
	char		*s;
	int		maxnblks = 0;

	/*
	 * To align things nicely, we need to know the maximum
	 * width of the number of cylinders field.
	 */
	maxcyl2 = 0;
	for (i = 0; i < NDKMAP; i++) {
		nblks	= pinfo->pinfo_map[i].dkl_nblk;
		cyl1	= pinfo->pinfo_map[i].dkl_cylno;
		cyl2	= cyl1 + (nblks / spc()) - 1;
		if (nblks > 0) {
			maxcyl2 = max(cyl2, maxcyl2);
			maxnblks = max(nblks, maxnblks);
		}
	}
	/*
	 * Get the number of digits required
	 */
	ncyl2_digits = ndigits(maxcyl2);

	/*
	 * Print the header, if necessary
	 */
	if (want_header) {
		fmt_print("Part      ");
		fmt_print("Tag    Flag     ");
		fmt_print("Cylinders");
		nspaces(ncyl2_digits);
		fmt_print("    Size            Blocks\n");
	}

	/*
	 * Print the partition information
	 */
	nblks	= pinfo->pinfo_map[partnum].dkl_nblk;
	cyl1	= pinfo->pinfo_map[partnum].dkl_cylno;
	cyl2	= cyl1 + (nblks / spc()) - 1;

	fmt_print("  %x ", partnum);

	/*
	 * Print the partition tag.  If invalid, print -
	 */
	s = find_string(ptag_choices,
		(int)pinfo->vtoc.v_part[partnum].p_tag);
	if (s == (char *)NULL)
		s = "-";
	nspaces(10 - (int)strlen(s));
	fmt_print("%s", s);

	/*
	 * Print the partition flag.  If invalid print -
	 */
	s = find_string(pflag_choices,
		(int)pinfo->vtoc.v_part[partnum].p_flag);
	if (s == (char *)NULL)
		s = "-";
	nspaces(6 - (int)strlen(s));
	fmt_print("%s", s);

	nspaces(2);

	if (nblks == 0) {
		fmt_print("%6d      ", cyl1);
		nspaces(ncyl2_digits);
		fmt_print("     0         ");
	} else {
		fmt_print("%6d - ", cyl1);
		nspaces(ncyl2_digits - ndigits(cyl2));
		fmt_print("%d    ", cyl2);
		scaled = bn2mb(nblks);
		if (scaled > (float)1024.0) {
			fmt_print("%8.2fGB    ", scaled/(float)1024.0);
		} else {
			fmt_print("%8.2fMB    ", scaled);
		}
	}
	fmt_print("(");
	pr_dblock(fmt_print, (daddr_t)nblks);
	fmt_print(")");

	nspaces(ndigits(maxnblks/spc()) - ndigits(nblks/spc()));
	s = " %6d\n";
	s[2] = '0' + ndigits(maxnblks);
	fmt_print(s, nblks);
}


/*
 * Return true if a disk has a volume name
 */
int
chk_volname(disk)
	struct disk_info	*disk;
{
	return (disk->v_volume[0] != 0);
}


/*
 * Print the volume name, if it appears to be set
 */
void
print_volname(disk)
	struct disk_info	*disk;
{
	int	i;
	char	*p;

	p = disk->v_volume;
	for (i = 0; i < LEN_DKL_VVOL; i++, p++) {
		if (*p == 0)
			break;
		fmt_print("%c", *p);
	}
}


/*
 * Print a number of spaces
 */
void
nspaces(n)
	int	n;
{
	while (n-- > 0)
		fmt_print(" ");
}

/*
 * Return the number of digits required to print a number
 */
int
ndigits(n)
	int	n;
{
	int	i;

	i = 0;
	while (n > 0) {
		n /= 10;
		i++;
	}

	return (i == 0 ? 1 : i);
}
