
/*
 * Copyright (c) 1993-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)menu_fdisk.c	1.19	97/11/22 SMI"

/*
 * This file contains functions that implement the fdisk menu commands.
 */
#include "global.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/dktp/fdisk.h>
#include <sys/stat.h>
#include <sys/dklabel.h>

#include "main.h"
#include "analyze.h"
#include "menu.h"
#include "menu_command.h"
#include "menu_defect.h"
#include "menu_partition.h"
#include "menu_fdisk.h"
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

extern	struct menu_item menu_fdisk[];

static void	update_cur_parts(void);
/*
 * Byte swapping macros for accessing struct ipart
 *	to resolve little endian on Sparc.
 */
#if defined(sparc)
#define	les(val)	((((val)&0xFF)<<8)|(((val)>>8)&0xFF))
#define	lel(val)	(((unsigned)(les((val)&0x0000FFFF))<<16) | \
			(les((unsigned)((val)&0xffff0000)>>16)))
#elif defined(i386)
#define	les(val)	(val)
#define	lel(val)	(val)
#else
#error  No Platform defined
#endif /* defined(sparc) */

#define	C_PARTITION		2
#define	I_PARTITION		8

/*
 * Handling the alignment problem of struct ipart.
 */
void
fill_ipart(char *bootptr, struct ipart *partp)
{
#if defined(sparc)
	/*
	 * Sparc platform:
	 *
	 * Packing short/word for struct ipart to resolve
	 *	little endian on Sparc since it is not
	 *	properly aligned on Sparc.
	 */
	partp->bootid = getbyte(&bootptr);
	partp->beghead = getbyte(&bootptr);
	partp->begsect = getbyte(&bootptr);
	partp->begcyl = getbyte(&bootptr);
	partp->systid = getbyte(&bootptr);
	partp->endhead = getbyte(&bootptr);
	partp->endsect = getbyte(&bootptr);
	partp->endcyl = getbyte(&bootptr);
	partp->relsect = getlong(&bootptr);
	partp->numsect = getlong(&bootptr);
#elif defined(i386)
	/*
	 * i386 platform:
	 *
	 * The fdisk table does not begin on a 4-byte boundary within
	 * the master boot record; so, we need to recopy its contents
	 * to another data structure to avoid an alignment exception.
	 */
	(void) bcopy(bootptr, partp, sizeof (struct ipart));
#else
#error  No Platform defined
#endif /* defined(sparc) */
}

/*
 * Get a correct byte/short/word routines for Sparc platform.
 */
#if defined(sparc)
getbyte(u_char **bp)
{
	int	b;

	b = **bp;
	*bp = *bp + 1;
	return (b);
}

getshort(u_char **bp)
{
	int	b;

	b = ((**bp) << 8) | *(*bp + 1);
	*bp += 2;
	return (b);
}

getlong(u_char **bp)
{
	int	b, bh, bl;

	bh = ((**bp) << 8) | *(*bp + 1);
	*bp += 2;
	bl = ((**bp) << 8) | *(*bp + 1);
	*bp += 2;

	b = (bh << 16) | bl;
	return (b);
}
#endif /* defined(sparc) */


/*
 * Convert cn[tn]dnsn to cn[tn]dnp0 path
 */
void
get_pname(char *name)
{
	char		buf[80];
	char		*devp = "/dev/dsk";
	char		*rdevp = "/dev/rdsk";
	char		np[32];
	char		*npt;

	/*
	 * If it is a full path /dev/[r]dsk/cn[tn]dnsn, use this path
	 */
	(void) strcpy(np, cur_disk->disk_name);
	if (strncmp(rdevp, cur_disk->disk_name, strlen(rdevp)) == 0 ||
	    strncmp(devp, cur_disk->disk_name, strlen(devp)) == 0) {
		/*
		 * Skip if the path is already included with pN
		 */
		if (strchr(np, 'p') == NULL) {
			npt = strrchr(np, 's');
			/* If sN is found, do not include it */
			if (isdigit(*++npt)) {
				*--npt = '\0';
			}
			sprintf(buf, "%sp0", np);
		} else {
			sprintf(buf, "%s", np);
		}
	} else {
		sprintf(buf, "/dev/rdsk/%sp0", np);
	}
	(void) strcpy(name, buf);
}

/*
 * Open file descriptor for current disk (cur_file)
 *	with "p0" path or cur_disk->disk_path
 */
void
open_cur_file(int mode)
{
	char	*dkpath;
	char	pbuf[256];

	switch (mode) {
	    case FD_USE_P0_PATH:
		(void) get_pname(&pbuf[0]);
		dkpath = pbuf;
		break;
	    case FD_USE_CUR_DISK_PATH:
		dkpath = cur_disk->disk_path;
		break;
	    default:
		err_print("Error: Invalid mode option for opening cur_file\n");
		fullabort();
	}

	/* Close previous cur_file */
	(void) close(cur_file);
	/* Open cur_file with the required path dkpath */
	if ((cur_file = open_disk(dkpath, O_RDWR | O_NDELAY)) < 0) {
		err_print(
		    "Error: can't open selected disk '%s'.\n", dkpath);
		fullabort();
	}
}


/*
 * This routine implements the 'fdisk' command.  It simply runs
 * the fdisk command on the current disk.
 * Use of this is restricted to interactive mode only.
 */
int
c_fdisk()
{

	char		buf[256];
	char		pbuf[256];
	struct stat	statbuf;

	/*
	 * We must be in interactive mode to use the fdisk command
	 */
	if (option_f != (char *)NULL || isatty(0) != 1 || isatty(1) != 1) {
		err_print("Fdisk command is for interactive use only!\n");
		return (-1);
	}

	/*
	 * There must be a current disk type and a current disk
	 */
	if (cur_dtype == NULL) {
		err_print("Current Disk Type is not set.\n");
		return (-1);
	}

	/*
	 * Before running the fdisk command, get file status of
	 *	/dev/rdsk/cn[tn]dnp0 path to see if this disk
	 *	supports fixed disk partition table.
	 */
	(void) get_pname(&pbuf[0]);
	if (stat(pbuf, (struct stat *)&statbuf) == -1) {
		err_print(
		"Disk does not support fixed disk partition table\n");
		return (0);
	}

	/*
	 * Run the fdisk program.
	 */
	(void) sprintf(buf, "fdisk %s\n", pbuf);
	system(buf);

	/*
	 * Open cur_file with "p0" path for accessing the fdisk table
	 */
	(void) open_cur_file(FD_USE_P0_PATH);

	/*
	 * Get solaris partition information in the fdisk partition table
	 */
	if ((get_solaris_part(cur_file, &cur_disk->fdisk_part)) == -1) {
		err_print("No fdisk solaris partition found\n");
		cur_disk->fdisk_part.numsect = 0;  /* No Solaris */
	}

	/*
	 * Restore cur_file with cur_disk->disk_path
	 */
	(void) open_cur_file(FD_USE_CUR_DISK_PATH);

	return (0);
}

/*
 * Fdisk has chagned the Solaris partition Update partition information
 *
 */


static void
update_cur_parts()
{

	int i;
	register struct partition_info *parts;

	for (i = 0; i < NDKMAP; i++) {
#if defined(_SUNOS_VTOC_16)
		if (cur_parts->vtoc.v_part[i].p_tag &&
			cur_parts->vtoc.v_part[i].p_tag != V_ALTSCTR) {
			cur_parts->vtoc.v_part[i].p_start = 0;
			cur_parts->vtoc.v_part[i].p_size = 0;

#endif
			cur_parts->pinfo_map[i].dkl_nblk = 0;
			cur_parts->pinfo_map[i].dkl_cylno = 0;
			cur_parts->vtoc.v_part[i].p_tag =
				default_vtoc_map[i].p_tag;
			cur_parts->vtoc.v_part[i].p_flag =
				default_vtoc_map[i].p_flag;
#if defined(_SUNOS_VTOC_16)
		}
#endif
	}
	cur_parts->pinfo_map[C_PARTITION].dkl_nblk = ncyl * spc();

#if defined(_SUNOS_VTOC_16)
	/*
	 * Adjust for the boot partitions
	 */
	cur_parts->pinfo_map[I_PARTITION].dkl_nblk = spc();
	cur_parts->pinfo_map[I_PARTITION].dkl_cylno = 0;
	cur_parts->vtoc.v_part[C_PARTITION].p_start =
		cur_parts->pinfo_map[C_PARTITION].dkl_cylno * nhead * nsect;
	cur_parts->vtoc.v_part[C_PARTITION].p_size =
		cur_parts->pinfo_map[C_PARTITION].dkl_nblk;

	cur_parts->vtoc.v_part[I_PARTITION].p_start =
			cur_parts->pinfo_map[I_PARTITION].dkl_cylno;
	cur_parts->vtoc.v_part[I_PARTITION].p_size =
			cur_parts->pinfo_map[I_PARTITION].dkl_nblk;

#endif	/* defined(_SUNOS_VTOC_16) */
	parts = cur_dtype->dtype_plist;
	cur_dtype->dtype_ncyl = ncyl;
	cur_dtype->dtype_plist = cur_parts;
	parts->pinfo_name = cur_parts->pinfo_name;
	cur_disk->disk_parts = cur_parts;
	cur_ctype->ctype_dlist = cur_dtype;

}

/*
 * XXX - we already have existing code that performs this functionality
 * in the driver!  Plan: get rid of this module, and replace with already
 * existing ioctl.
 */
int
get_solaris_part(int fd, struct ipart *ipart)
{
	int		i, error = 0;
	struct ipart	ip;
	int		status;
	char		*bootptr;

	lseek(fd, 0, 0);
	status = read(fd, (caddr_t)&boot_sec, NBPSCTR);

	if (status != NBPSCTR) {
		err_print("Bad read of fdisk partition. Status = %x\n", status);
		err_print("Cannot read fdisk partition information.\n");
		return (-1);
	}

	for (i = 0; i < 4; i++) {
		int	ipc;

		ipc = i * sizeof (struct ipart);

		/* Handling the alignment problem of struct ipart */
		bootptr = &boot_sec.parts[ipc];
		(void) fill_ipart(bootptr, &ip);



		if (ip.systid == SUNIXOS) {
			pcyl = lel(ip.numsect) / (nhead * nsect);
			xstart = lel(ip.relsect) / (nhead * nsect);
			solaris_offset = lel(ip.relsect);
			ncyl = pcyl - acyl;
			break;
		}
	}

	if (i == 4) {
		err_print("Solaris fdisk partition not found\n");
		return (-1);
	}

	if (bcmp(&ip, ipart, sizeof (struct ipart)) && !error) {
		printf("\nWARNING: Solaris fdisk partition changed - "
			"Please relabel the disk\n");
		bcopy(&ip, ipart, sizeof (struct ipart));
		update_cur_parts();
		return (error);
	}
	bcopy(&ip, ipart, sizeof (struct ipart));

	return (error);
}


int
copy_solaris_part(struct ipart *ipart)
{

	int		status, i, fd;
	struct mboot	mboot;
	struct ipart	ip;
	char		buf[80];
	char		*bootptr;
	struct stat	statbuf;


	(void) get_pname(&buf[0]);
	if (stat(buf, (struct stat *)&statbuf) == -1) {
		/*
		 * Make sure to reset solaris_offset to zero if it is
		 *	previously set by a selected disk that
		 *	supports the fdisk table.
		 */
		solaris_offset = 0;
		/* Return if this disk does not support fdisk table. */
		return (0);
	}

	if ((fd = open(buf, O_RDONLY)) < 0) {
		err_print("Error: can't open disk '%s'.\n", buf);
		return (-1);
	}

	status = read(fd, (caddr_t)&mboot, sizeof (struct mboot));

	if (status != sizeof (struct mboot)) {
		err_print("Bad read of fdisk partition.\n");
		close(fd);
		return (-1);
	}

	for (i = 0; i < 4; i++) {
		int	ipc;

		ipc = i * sizeof (struct ipart);

		/* Handling the alignment problem of struct ipart */
		bootptr = &mboot.parts[ipc];
		(void) fill_ipart(bootptr, &ip);


		if (ip.systid == SUNIXOS) {
			pcyl = lel(ip.numsect) / (nhead * nsect);
			ncyl = pcyl - acyl;
			solaris_offset = lel(ip.relsect);
			bcopy(&ip, ipart, sizeof (struct ipart));
			break;
		}
	}

	close(fd);
	return (0);

}

int
auto_solaris_part(struct dk_label *label)
{

	int		status, i, fd;
	struct mboot	mboot;
	struct ipart	ip;
	char		buf[80];
	char		*bootptr;

	sprintf(buf, "/dev/rdsk/%sp0", x86_devname);
	if ((fd = open(buf, O_RDONLY)) < 0) {
		err_print("Error: can't open selected disk '%s'.\n", buf);
		return (-1);
	}
	status = read(fd, (caddr_t)&mboot, sizeof (struct mboot));

	if (status != sizeof (struct mboot)) {
		err_print("Bad read of fdisk partition.\n");
		return (-1);
	}

	for (i = 0; i < 4; i++) {
		int	ipc;

		ipc = i * sizeof (struct ipart);

		/* Handling the alignment problem of struct ipart */
		bootptr = &mboot.parts[ipc];
		(void) fill_ipart(bootptr, &ip);


		if (ip.systid == SUNIXOS) {
			label->dkl_pcyl = lel(ip.numsect) /
			    (label->dkl_nhead * label->dkl_nsect);
			label->dkl_ncyl = label->dkl_pcyl - label->dkl_acyl;
			solaris_offset = lel(ip.relsect);
			break;
		}
	}

	close(fd);

	return (0);
}


int
good_fdisk()
{
	char		buf[80];
	struct stat	statbuf;

	(void) get_pname(&buf[0]);
	if (stat(buf, (struct stat *)&statbuf) == -1) {
		/* Return if this disk does not support fdisk table */
		return (1);
	}

	if (lel(cur_disk->fdisk_part.numsect) > 0)
		return (1);
	else
		return (0);
}
