
#pragma	ident	"@(#)auto_sense.c	1.20	98/03/06 SMI"

/*
 * Copyright (c) 1993-1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * This file contains functions to implement automatic configuration
 * of scsi disks.
 */
#include "global.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "misc.h"
#include "param.h"
#include "ctlr_scsi.h"
#include "auto_sense.h"
#include "partition.h"
#include "label.h"
#include "startup.h"
#include "analyze.h"
#include "io.h"
#include "hardware_structs.h"


#define	DISK_NAME_MAX		256

extern	int			nctypes;
extern	struct	ctlr_type	ctlr_types[];


/*
 * Marker for free hog partition
 */
#define	HOG		(-1)



/*
 * Default partition tables
 *
 *	Disk capacity		root	swap	usr
 *	-------------		----	----	---
 *	0mb to 64mb		0	0	remainder
 *	64mb to 180mb		16mb	16mb	remainder
 *	180mb to 280mb		16mb	32mb	remainder
 *	280mb to 380mb		24mb	32mb	remainder
 *	380mb to 600mb		32mb	32mb	remainder
 *	600mb to 1gb		32mb	64mb	remainder
 *	1gb to 2gb		64mb	128mb	remainder
 *	2gb on up		128mb	128mb	remainder
 */
struct part_table {
	int	partitions[NDKMAP];
};

static struct part_table part_table_64mb = {
	{ 0,	0,	0,	0,	0,	0,	HOG,	0}
};

static struct part_table part_table_180mb = {
	{ 16,	16,	0,	0,	0,	0,	HOG,	0}
};

static struct part_table part_table_280mb = {
	{ 16,	32,	0,	0,	0,	0,	HOG,	0}
};

static struct part_table part_table_380mb = {
	{ 24,	32,	0,	0,	0,	0,	HOG,	0}
};

static struct part_table part_table_600mb = {
	{ 32,	32,	0,	0,	0,	0,	HOG,	0}
};

static struct part_table part_table_1gb = {
	{ 32,	64,	0,	0,	0,	0,	HOG,	0}
};

static struct part_table part_table_2gb = {
	{ 64,	128,	0,	0,	0,	0,	HOG,	0}
};

static struct part_table part_table_infinity = {
	{ 128,	128,	0,	0,	0,	0,	HOG,	0}
};


static struct default_partitions {
	long			min_capacity;
	long			max_capacity;
	struct part_table	*part_table;
} default_partitions[] = {
	{ 0,	64,		&part_table_64mb },	/* 0 to 64 mb */
	{ 64,	180,		&part_table_180mb },	/* 64 to 180 mb */
	{ 180,	280,		&part_table_280mb },	/* 180 to 280 mb */
	{ 280,	380,		&part_table_380mb },	/* 280 to 380 mb */
	{ 380,	600,		&part_table_600mb },	/* 380 to 600 mb */
	{ 600,	1024,		&part_table_1gb },	/* 600 to 1 gb */
	{ 1024,	2048,		&part_table_2gb },	/* 1 to 2 gb */
	{ 2048,	INFINITY,	&part_table_infinity },	/* 2 gb on up */
};

#define	DEFAULT_PARTITION_TABLE_SIZE	\
	(sizeof (default_partitions) / sizeof (struct default_partitions))

/*
 * msgs for check()
 */
#define	FORMAT_MSG	"Auto configuration via format.dat"
#define	GENERIC_MSG	"Auto configuration via generic SCSI-2"

/*
 * Disks on symbios(Hardwire raid controller) return a fixed number
 * of heads(64)/cylinders(64) and adjust the cylinders depending
 * capacity of the configured lun.
 * In such a case we get number of physical cylinders < 3 which
 * is the minimum required by solaris(2 reserved + 1 data cylinders).
 * Hence try to adjust the cylinders by reducing the "nsect/nhead".
 *
 */
/*
 * assuming a minimum of 32 block cylinders.
 */
#define	MINIMUM_NO_HEADS	2
#define	MINIMUM_NO_SECTORS	16

#define	MINIMUM_NO_CYLINDERS	128

#if defined(_SUNOS_VTOC_8)

/* These are 16-bit fields */
#define	MAXIMUM_NO_HEADS	65535
#define	MAXIMUM_NO_SECTORS	65535
#define	MAXIMUM_NO_CYLINDERS	65535

#endif	/* defined(_SUNOS_VTOC_8) */

/*
 * minimum number of cylinders required by Solaris.
 */
#define	SUN_MIN_CYL		3



/*
 * ANSI prototypes for local static functions
 */
static struct disk_type	*generic_disk_sense(
				int		fd,
				int		can_prompt,
				struct dk_label	*label,
				struct scsi_inquiry *inquiry,
				struct scsi_capacity *capacity,
				char		*disk_name);
static int		use_existing_disk_type(
				int		fd,
				int		can_prompt,
				struct dk_label	*label,
				struct scsi_inquiry *inquiry,
				struct disk_type *disk_type,
				struct scsi_capacity *capacity);
int			build_default_partition(struct dk_label *label);
static struct disk_type	*find_scsi_disk_type(
				char		*disk_name,
				struct dk_label	*label);
static struct disk_type	*find_scsi_disk_by_name(
				char		*disk_name);
static struct ctlr_type	*find_scsi_ctlr_type(void);
static struct ctlr_info	*find_scsi_ctlr_info(
				struct dk_cinfo	*dkinfo);
static struct disk_type	*new_scsi_disk_type(
				int		fd,
				char		*disk_name,
				struct dk_label	*label);
static struct disk_info	*find_scsi_disk_info(
				struct dk_cinfo	*dkinfo);
static char		*get_sun_disk_name(
				char		*disk_name,
				struct scsi_inquiry *inquiry);
static char		*get_generic_disk_name(
				char		*disk_name,
				struct scsi_inquiry *inquiry);
static int		force_blocksize(int fd);
static int		raw_format(int fd);
static char		*strcopy(
				char	*dst,
				char	*src,
				int	n);
static	int		adjust_disk_geometry(int capacity, int *cyl,
						int *nsect, int *nhead);
#if defined(_SUNOS_VTOC_8)
static int square_box(
			int capacity,
			int *dim1, int lim1,
			int *dim2, int lim2,
			int *dim3, int lim3);
#endif	/* defined(_SUNOS_VTOC_8) */




/*
 * Auto-sense a scsi disk configuration, ie get the information
 * necessary to construct a label.  We have two different
 * ways to auto-sense a scsi disk:
 *	- format.dat override, via inquiry name
 *	- generic scsi, via standard mode sense and inquiry
 * Depending on how and when we are called, and/or
 * change geometry and reformat.
 */
struct disk_type *
auto_sense(
	int		fd,
	int		can_prompt,
	struct dk_label	*label)
{
	struct scsi_inquiry		inquiry;
	struct scsi_capacity		capacity;
	struct disk_type		*disk_type;
	char				disk_name[DISK_NAME_MAX];
	int				force_format_dat = 0;
	int				force_generic = 0;
	u_ioparam_t			ioparam;
	int				deflt;

	/*
	 * First, if expert mode, find out if the user
	 * wants to override any of the standard methods.
	 */
	if (can_prompt && expert_mode) {
		deflt = 1;
		ioparam.io_charlist = confirm_list;
		if (input(FIO_MSTR, FORMAT_MSG, '?', &ioparam,
				&deflt, DATA_INPUT) == 0) {
			force_format_dat = 1;
		} else if (input(FIO_MSTR, GENERIC_MSG, '?', &ioparam,
				&deflt, DATA_INPUT) == 0) {
			force_generic = 1;
		}
	}

	/*
	 * Get the Inquiry data.  If this fails, there's
	 * no hope for this disk, so give up.
	 */
	if (uscsi_inquiry(fd, (char *)&inquiry, sizeof (inquiry))) {
		return ((struct disk_type *)NULL);
	}
	if (option_msg && diag_msg) {
		err_print("Product id: ");
		print_buf(inquiry.inq_pid, sizeof (inquiry.inq_pid));
		err_print("\n");
	}

	/*
	 * Get the Read Capacity
	 */
	if (uscsi_read_capacity(fd, &capacity)) {
		return ((struct disk_type *)NULL);
	}
	if (option_msg && diag_msg) {
		err_print("blocks:  %ld (0x%x)\n",
			capacity.capacity, capacity.capacity);
		err_print("blksize: %ld\n", capacity.lbasize);
	}

	/*
	 * Extract the disk name for the format.dat override
	 */
	(void) get_sun_disk_name(disk_name, &inquiry);
	if (option_msg && diag_msg) {
		err_print("disk name:  `%s`\n", disk_name);
	}

	/*
	 * Figure out which method we use for auto sense.
	 * If a particular method fails, we fall back to
	 * the next possibility.
	 */

	if (force_generic) {
		return (generic_disk_sense(fd, can_prompt, label,
			&inquiry, &capacity, disk_name));
	}

	/*
	 * Try for an existing format.dat first
	 */
	if ((disk_type = find_scsi_disk_by_name(disk_name)) != NULL) {
		if (use_existing_disk_type(fd, can_prompt, label,
				&inquiry, disk_type, &capacity)) {
			return (disk_type);
		}
		if (force_format_dat) {
			return (NULL);
		}
	}

	/*
	 * Otherwise, try using generic SCSI-2 sense and inquiry.
	 */

	return (generic_disk_sense(fd, can_prompt, label,
			&inquiry, &capacity, disk_name));
}



/*ARGSUSED*/
static struct disk_type *
generic_disk_sense(
	int			fd,
	int			can_prompt,
	struct dk_label		*label,
	struct scsi_inquiry	*inquiry,
	struct scsi_capacity	*capacity,
	char			*disk_name)
{
	struct disk_type		*disk;
	int				pcyl;
	int				ncyl;
	int				acyl;
	int				nhead;
	int				nsect;
	int				rpm;
	long				nblocks;
	union {
		struct mode_format	page3;
		u_char			buf3[MAX_MODE_SENSE_SIZE];
	} u_page3;
	union {
		struct mode_geometry	page4;
		u_char			buf4[MAX_MODE_SENSE_SIZE];
	} u_page4;
	struct scsi_capacity		new_capacity;
	struct mode_format		*page3 = &u_page3.page3;
	struct mode_geometry		*page4 = &u_page4.page4;
	struct scsi_ms_header		header;

	/*
	 * If the name of this disk appears to be "SUN", use it,
	 * otherwise construct a name out of the generic
	 * Inquiry info.  If it turns out that we already
	 * have a SUN disk type of this name that differs
	 * in geometry, we will revert to the generic name
	 * anyway.
	 */
	if (memcmp(disk_name, "SUN", strlen("SUN")) != 0) {
		(void) get_generic_disk_name(disk_name, inquiry);
	}

	/*
	 * If the device's block size is not 512, we have to
	 * change block size, reformat, and then sense the
	 * geometry.  To do this, we must be able to prompt
	 * the user.
	 */
	if (capacity->lbasize != DEV_BSIZE) {
		if (!can_prompt) {
			return (NULL);
		}
		if (force_blocksize(fd)) {
			goto err;
		}

		/*
		 * Get the capacity again, since this has changed
		 */
		if (uscsi_read_capacity(fd, &new_capacity)) {
			goto err;
		}
		if (option_msg && diag_msg) {
			err_print("blocks:  %ld (0x%x)\n",
				new_capacity.capacity, new_capacity.capacity);
			err_print("blksize: %ld\n", new_capacity.lbasize);
		}
		capacity = &new_capacity;
		if (capacity->lbasize != DEV_BSIZE) {
			goto err;
		}
	}

	/*
	 * Get current Page 3 - Format Parameters page
	 */
	if (uscsi_mode_sense(fd, DAD_MODE_FORMAT, MODE_SENSE_PC_CURRENT,
			(caddr_t)&u_page3, MAX_MODE_SENSE_SIZE, &header)) {
		goto err;
	}

	/*
	 * Get current Page 4 - Drive Geometry page
	 */
	if (uscsi_mode_sense(fd, DAD_MODE_GEOMETRY, MODE_SENSE_PC_CURRENT,
			(caddr_t)&u_page4, MAX_MODE_SENSE_SIZE, &header)) {
		goto err;
	}

#if defined(_LITTLE_ENDIAN)
	/*
	 * Correct for byte order if necessary
	 */
	page4->rpm = ntohs(page4->rpm);
	page4->step_rate = ntohs(page4->step_rate);
	page3->tracks_per_zone = ntohs(page3->tracks_per_zone);
	page3->alt_sect_zone = ntohs(page3->alt_sect_zone);
	page3->alt_tracks_zone = ntohs(page3->alt_tracks_zone);
	page3->alt_tracks_vol = ntohs(page3->alt_tracks_vol);
	page3->sect_track = ntohs(page3->sect_track);
	page3->data_bytes_sect = ntohs(page3->data_bytes_sect);
	page3->interleave = ntohs(page3->interleave);
	page3->track_skew = ntohs(page3->track_skew);
	page3->cylinder_skew = ntohs(page3->cylinder_skew);
#endif		/* defined(_LITTLE_ENDIAN) */


	/*
	 * Construct a new label out of the sense data,
	 * Inquiry and Capacity.
	 */
	pcyl = (page4->cyl_ub << 16) + (page4->cyl_mb << 8) + page4->cyl_lb;
	nhead = page4->heads;
	nsect = page3->sect_track;
	rpm = page4->rpm;

	/*
	 * If the number of physical cylinders reported is less
	 * the SUN_MIN_CYL(3) then try to adjust the geometry so that
	 * we have atleast SUN_MIN_CYL cylinders.
	 */
	if (pcyl < SUN_MIN_CYL) {
		if (adjust_disk_geometry(capacity->capacity + 1, &pcyl,
						&nhead, &nsect)) {
			goto err;
		}
	}

	/*
	 * The sd driver reserves 2 cylinders the backup disk label and
	 * the deviceid.  Set the number of data cylinders to pcyl-acyl.
	 */
	acyl = DK_ACYL;
	ncyl = pcyl - acyl;

	if (option_msg && diag_msg) {
		err_print("Geometry:\n");
		err_print("    pcyl:    %d\n", pcyl);
		err_print("    ncyl:    %d\n", ncyl);
		err_print("    heads:   %d\n", nhead);
		err_print("    nsects:  %d\n", nsect);
		err_print("    acyl:    %d\n", acyl);

#if defined(_SUNOS_VTOC_16)
		err_print("    bcyl:    %d\n", bcyl);
#endif			/* defined(_SUNOS_VTOC_16) */

		err_print("    rpm:     %d\n", rpm);
	}

	/*
	 * Some drives report 0 for page4->rpm, adjust it to AVG_RPM, 3600.
	 */
	if (rpm < MIN_RPM || rpm > MAX_RPM) {
		err_print("Mode sense page(4) reports rpm value as %d,"
			" adjusting it to %d\n", rpm, AVG_RPM);
		rpm = AVG_RPM;
	}

	/*
	 * Get the number of blocks from Read Capacity data. Note that
	 * the logical block address range from 0 to capacity->capacity.
	 */
	nblocks = capacity->capacity + 1;

	/*
	 * Some drives report 0 for nsect (page 3, byte 10 and 11) if they
	 * have variable number of sectors per track. So adjust nsect.
	 * Also the value is defined as vendor specific, hence check if
	 * it is in a tolerable range. The values (32 and 4 below) are
	 * chosen so that this change below does not generate a different
	 * geometry for currently supported sun disks.
	 */
	if ((nsect <= 0) ||
	    (pcyl * nhead * nsect) < (nblocks - nblocks/32) ||
	    (pcyl * nhead * nsect) > (nblocks + nblocks/4)) {
		err_print("Mode sense page(3) reports nsect value as %d, "
		    "adjusting it to %d\n", nsect, nblocks / (pcyl * nhead));
		nsect = nblocks / (pcyl * nhead);
	}

	/*
	 * Some drives report their physical geometry such that
	 * it is greater than the actual capacity.  Adjust the
	 * geometry to allow for this, so we don't run off
	 * the end of the disk.
	 */
	if ((pcyl * nhead * nsect) > nblocks) {
		int	p = pcyl;
		if (option_msg && diag_msg) {
			err_print("Computed capacity (%ld) exceeds actual "
				"disk capacity (%ld)\n",
				pcyl * nhead * nsect, nblocks);
		}
		do {
			pcyl--;
		} while ((pcyl * nhead * nsect) > nblocks);

		if (can_prompt && expert_mode && !option_f) {
			/*
			 * Try to adjust nsect instead of pcyl to see if we
			 * can optimize. For compatability reasons do this
			 * only in expert mode (refer to bug 1144812).
			 */
			int	n = nsect;
			do {
				n--;
			} while ((p * nhead * n) > nblocks);
			if ((p * nhead * n) > (pcyl * nhead * nsect)) {
				u_ioparam_t	ioparam;
				int		deflt = 1;
				/*
				 * Ask the user for a choice here.
				 */
				ioparam.io_bounds.lower = 1;
				ioparam.io_bounds.upper = 2;
				err_print("1. Capacity = %d, with pcyl = %d "
					"nhead = %d nsect = %d\n",
					(pcyl * nhead * nsect),
					pcyl, nhead, nsect);
				err_print("2. Capacity = %d, with pcyl = %d "
					"nhead = %d nsect = %d\n",
					(p * nhead * n),
					p, nhead, n);
				if (input(FIO_INT, "Select one of the above "
				    "choices ", ':', &ioparam,
					&deflt, DATA_INPUT) == 2) {
					pcyl = p;
					nsect = n;
				}
			}
		}
	}

#if defined(_SUNOS_VTOC_8)
	/*
	 * Finally, we need to make sure we don't overflow any of the
	 * fields in our disk label.  To do this we need to `square
	 * the box' so to speak.  We will lose bits here.
	 */

	if ((pcyl > MAXIMUM_NO_CYLINDERS &&
		((nsect > MAXIMUM_NO_SECTORS) ||
		(nhead > MAXIMUM_NO_HEADS))) ||
		((nsect > MAXIMUM_NO_SECTORS) &&
		(nhead > MAXIMUM_NO_HEADS))) {
		err_print("This disk is too big to label. "
			" You will lose some blocks.\n");
	}
	if ((pcyl > MAXIMUM_NO_CYLINDERS) ||
		(nsect > MAXIMUM_NO_SECTORS) ||
		(nhead > MAXIMUM_NO_HEADS)) {
		u_ioparam_t	ioparam;
		int		order;
		char		msg[256];

		order = ((ncyl > nhead)<<2) |
			((ncyl > nsect)<<1) |
			(nhead > nsect);
		switch (order) {
		case 0x7: /* ncyl > nhead > nsect */
			nblocks =
				square_box(nblocks,
					&pcyl, MAXIMUM_NO_CYLINDERS,
					&nhead, MAXIMUM_NO_HEADS,
					&nsect, MAXIMUM_NO_SECTORS);
			break;
		case 0x6: /* ncyl > nsect > nhead */
			nblocks =
				square_box(nblocks,
					&pcyl, MAXIMUM_NO_CYLINDERS,
					&nsect, MAXIMUM_NO_SECTORS,
					&nhead, MAXIMUM_NO_HEADS);
			break;
		case 0x4: /* nsect > ncyl > nhead */
			nblocks =
				square_box(nblocks,
					&nsect, MAXIMUM_NO_SECTORS,
					&pcyl, MAXIMUM_NO_CYLINDERS,
					&nhead, MAXIMUM_NO_HEADS);
			break;
		case 0x0: /* nsect > nhead > ncyl */
			nblocks =
				square_box(nblocks,
					&nsect, MAXIMUM_NO_SECTORS,
					&nhead, MAXIMUM_NO_HEADS,
					&pcyl, MAXIMUM_NO_CYLINDERS);
			break;
		case 0x3: /* nhead > ncyl > nsect */
			nblocks =
				square_box(nblocks,
					&nhead, MAXIMUM_NO_HEADS,
					&pcyl, MAXIMUM_NO_CYLINDERS,
					&nsect, MAXIMUM_NO_SECTORS);
			break;
		case 0x1: /* nhead > nsect > ncyl */
			nblocks =
				square_box(nblocks,
					&nhead, MAXIMUM_NO_HEADS,
					&nsect, MAXIMUM_NO_SECTORS,
					&pcyl, MAXIMUM_NO_CYLINDERS);
			break;
		default:
			/* How did we get here? */
			impossible("label overflow adjustment");

			/* Do something useful */
			nblocks =
				square_box(nblocks,
					&nhead, MAXIMUM_NO_HEADS,
					&nsect, MAXIMUM_NO_SECTORS,
					&pcyl, MAXIMUM_NO_CYLINDERS);
			break;
		}
		if (option_msg && diag_msg &&
			(capacity->capacity + 1 != nblocks)) {
			err_print("After adjusting geometry you lost"
				" %d of %d blocks.\n",
				(capacity->capacity + 1 - nblocks),
				capacity->capacity + 1);
		}
		while (can_prompt && expert_mode && !option_f) {
			int				deflt = 1;

			/*
			 * Allow user to modify this by hand if desired.
			 */
			sprintf(msg, "\nGeometry: %d heads, %d sectors %d "
				" cylinders result in %d out of %d blocks.\n"
				"Do you want to modify the device geometry",
				nhead, nsect, pcyl,
				(int)nblocks, (int)capacity->capacity + 1);

			ioparam.io_charlist = confirm_list;
			if (input(FIO_MSTR, msg, '?', &ioparam,
				&deflt, DATA_INPUT) != 0)
				break;

			ioparam.io_bounds.lower = MINIMUM_NO_HEADS;
			ioparam.io_bounds.upper = MAXIMUM_NO_HEADS;
			nhead = input(FIO_INT, "Number of heads", ':',
				&ioparam, &nhead, DATA_INPUT);
			ioparam.io_bounds.lower = MINIMUM_NO_SECTORS;
			ioparam.io_bounds.upper = MAXIMUM_NO_SECTORS;
			nsect = input(FIO_INT,
				"Number of sectors per track",
				':', &ioparam, &nsect, DATA_INPUT);
			ioparam.io_bounds.lower = SUN_MIN_CYL;
			ioparam.io_bounds.upper = MAXIMUM_NO_CYLINDERS;
			pcyl = input(FIO_INT, "Number of cylinders",
				':', &ioparam, &pcyl, DATA_INPUT);
			nblocks = nhead * nsect * pcyl;
			if (nblocks > capacity->capacity + 1) {
				err_print("Warning: %d blocks exceeds "
					"disk capacity of %d blocks\n",
					nblocks,
					capacity->capacity + 1);
			}
		}
	}
#endif		/* defined(_SUNOS_VTOC_8) */

	ncyl = pcyl - acyl;

	if (option_msg && diag_msg) {
		err_print("\nGeometry after adjusting for capacity:\n");
		err_print("    pcyl:    %d\n", pcyl);
		err_print("    ncyl:    %d\n", ncyl);
		err_print("    heads:   %d\n", nhead);
		err_print("    nsects:  %d\n", nsect);
		err_print("    acyl:    %d\n", acyl);
		err_print("    rpm:     %d\n", rpm);
	}

	(void) memset((char *)label, 0, sizeof (struct dk_label));

	label->dkl_magic = DKL_MAGIC;

	(void) sprintf(label->dkl_asciilabel,
		"%s cyl %d alt %d hd %d sec %d",
		disk_name, ncyl, acyl, nhead, nsect);

	label->dkl_pcyl = pcyl;
	label->dkl_ncyl = ncyl;
	label->dkl_acyl = acyl;
	label->dkl_nhead = nhead;
	label->dkl_nsect = nsect;
	label->dkl_apc = 0;
	label->dkl_intrlv = 1;
	label->dkl_rpm = rpm;

#if defined(_FIRMWARE_NEEDS_FDISK)
	(void) auto_solaris_part(label);
	ncyl = label->dkl_ncyl;
#endif		/* defined(_FIRMWARE_NEEDS_FDISK) */


	if (!build_default_partition(label)) {
		goto err;
	}

	(void) checksum(label, CK_MAKESUM);

	/*
	 * Find an existing disk type defined for this disk.
	 * For this to work, both the name and geometry must
	 * match.  If there is no such type, but there already
	 * is a disk defined with that name, but with a different
	 * geometry, construct a new generic disk name out of
	 * the inquiry information.  Whatever name we're
	 * finally using, if there's no such disk type defined,
	 * build a new disk definition.
	 */
	if ((disk = find_scsi_disk_type(disk_name, label)) == NULL) {
		if (find_scsi_disk_by_name(disk_name) != NULL) {
			char	old_name[DISK_NAME_MAX];
			strcpy(old_name, disk_name);
			(void) get_generic_disk_name(disk_name,
				inquiry);
			if (option_msg && diag_msg) {
				err_print(
"Changing disk type name from '%s' to '%s'\n", old_name, disk_name);
			}
			(void) sprintf(label->dkl_asciilabel,
				"%s cyl %d alt %d hd %d sec %d",
				disk_name, ncyl, acyl, nhead, nsect);
			(void) checksum(label, CK_MAKESUM);
			disk = find_scsi_disk_type(disk_name, label);
		}
		if (disk == NULL) {
			disk = new_scsi_disk_type(fd, disk_name, label);
			if (disk == NULL)
				goto err;
		}
	}

	return (disk);

err:
	if (option_msg && diag_msg) {
		err_print(
		"Configuration via generic SCSI-2 information failed\n");
	}
	return (NULL);
}


/*ARGSUSED*/
static int
use_existing_disk_type(
	int			fd,
	int			can_prompt,
	struct dk_label		*label,
	struct scsi_inquiry	*inquiry,
	struct disk_type	*disk_type,
	struct scsi_capacity	*capacity)
{
	struct scsi_capacity	new_capacity;
	int			pcyl;
	int			acyl;
	int			nhead;
	int			nsect;
	int			rpm;

	/*
	 * If the device's block size is not 512, we have to
	 * change block size, reformat, and then sense the
	 * geometry.  To do this, we must be able to prompt
	 * the user.
	 */
	if (capacity->lbasize != DEV_BSIZE) {
		if (!can_prompt) {
			return (0);
		}
		if (force_blocksize(fd)) {
			goto err;
		}

		/*
		 * Get the capacity again, since this has changed
		 */
		if (uscsi_read_capacity(fd, &new_capacity)) {
			goto err;
		}

		if (option_msg && diag_msg) {
			err_print("blocks:  %ld (0x%x)\n",
				new_capacity.capacity, new_capacity.capacity);
			err_print("blksize: %ld\n", new_capacity.lbasize);
		}

		capacity = &new_capacity;
		if (capacity->lbasize != DEV_BSIZE) {
			goto err;
		}
	}

	/*
	 * Construct a new label out of the format.dat
	 */
	pcyl = disk_type->dtype_pcyl;
	acyl = disk_type->dtype_acyl;
	ncyl = disk_type->dtype_ncyl;
	nhead = disk_type->dtype_nhead;
	nsect = disk_type->dtype_nsect;
	rpm = disk_type->dtype_rpm;

	if (option_msg && diag_msg) {
		err_print("Format.dat geometry:\n");
		err_print("    pcyl:    %ld\n", pcyl);
		err_print("    heads:   %ld\n", nhead);
		err_print("    nsects:  %ld\n", nsect);
		err_print("    acyl:    %ld\n", acyl);
		err_print("    rpm:     %ld\n", rpm);
	}

	(void) memset((char *)label, 0, sizeof (struct dk_label));

	label->dkl_magic = DKL_MAGIC;

	(void) sprintf(label->dkl_asciilabel,
		"%s cyl %d alt %d hd %d sec %d",
		disk_type->dtype_asciilabel,
		ncyl, acyl, nhead, nsect);

	label->dkl_pcyl = pcyl;
	label->dkl_ncyl = ncyl;
	label->dkl_acyl = acyl;
	label->dkl_nhead = nhead;
	label->dkl_nsect = nsect;
	label->dkl_apc = 0;
	label->dkl_intrlv = 1;
	label->dkl_rpm = rpm;

	if (!build_default_partition(label)) {
		goto err;
	}

	(void) checksum(label, CK_MAKESUM);
	return (1);

err:
	if (option_msg && diag_msg) {
		err_print(
			"Configuration via format.dat geometry failed\n");
	}
	return (0);
}

int
build_default_partition(
	struct dk_label			*label)
{
	int				i;
	int				ncyls[NDKMAP];
	int				nblks;
	int				cyl;
	struct dk_vtoc			*vtoc;
	struct part_table		*pt;
	struct default_partitions	*dpt;
	long				capacity;
	int				freecyls;
	int				blks_per_cyl;
	int				ncyl;

	/*
	 * Install a default vtoc
	 */
	vtoc = &label->dkl_vtoc;
	vtoc->v_version = V_VERSION;
	vtoc->v_nparts = NDKMAP;
	vtoc->v_sanity = VTOC_SANE;

	for (i = 0; i < NDKMAP; i++) {
		vtoc->v_part[i].p_tag = default_vtoc_map[i].p_tag;
		vtoc->v_part[i].p_flag = default_vtoc_map[i].p_flag;
	}

	/*
	 * Find a partition that matches this disk.  Capacity
	 * is in integral number of megabytes.
	 */
	capacity = (long)(label->dkl_ncyl * label->dkl_nhead *
		label->dkl_nsect) / (long)((1024 * 1024) / DEV_BSIZE);
	dpt = default_partitions;
	for (i = 0; i < DEFAULT_PARTITION_TABLE_SIZE; i++, dpt++) {
		if (capacity >= dpt->min_capacity &&
				capacity < dpt->max_capacity) {
			break;
		}
	}
	if (i == DEFAULT_PARTITION_TABLE_SIZE) {
		if (option_msg && diag_msg) {
			err_print("No matching default partition (%d)\n",
				capacity);
		}
		return (0);
	}
	pt = dpt->part_table;

	/*
	 * Go through default partition table, finding fixed
	 * sized entries.
	 */
	freecyls = label->dkl_ncyl;
	blks_per_cyl = label->dkl_nhead * label->dkl_nsect;
	for (i = 0; i < NDKMAP; i++) {
		if (pt->partitions[i] == HOG || pt->partitions[i] == 0) {
			ncyls[i] = 0;
		} else {
			/*
			 * Calculate number of cylinders necessary
			 * for specified size, rounding up to
			 * the next greatest integral number of
			 * cylinders.  Always give what they
			 * asked or more, never less.
			 */
			nblks = pt->partitions[i] * ((1024*1024)/DEV_BSIZE);
			nblks += (blks_per_cyl - 1);
			ncyls[i] = nblks / blks_per_cyl;
			freecyls -= ncyls[i];
		}
	}

	if (freecyls < 0) {
		if (option_msg && diag_msg) {
			for (i = 0; i < NDKMAP; i++) {
				if (ncyls[i] == 0)
					continue;
				err_print("Partition %d: %d cyls\n",
					i, ncyls[i]);
			}
			err_print("Free cylinders exhausted (%d)\n",
				freecyls);
		}
		return (0);
	}

#if defined(i386)
	/*
	 * Set the default boot partition to 1 cylinder
	 */
	ncyls[8] = 1;
	freecyls -= 1;

	/*
	 * Set the default alternates partition to 2 cylinders
	 */
	ncyls[9] = 2;
	freecyls -= 2;
#endif			/* defined(i386) */

	/*
	 * Set the free hog partition to whatever space remains.
	 * It's an error to have more than one HOG partition,
	 * but we don't verify that here.
	 */
	for (i = 0; i < NDKMAP; i++) {
		if (pt->partitions[i] == HOG) {
			assert(ncyls[i] == 0);
			ncyls[i] = freecyls;
			break;
		}
	}

	/*
	 * Error checking
	 */
	ncyl = 0;
	for (i = 0; i < NDKMAP; i++) {
		ncyl += ncyls[i];
	}
	assert(ncyl == (label->dkl_ncyl));

	/*
	 * Finally, install the partition in the label.
	 */
	cyl = 0;

#if defined(_SUNOS_VTOC_16)
	for (i = NDKMAP/2; i < NDKMAP; i++) {
		if (i == 2 || ncyls[i] == 0)
			continue;
		label->dkl_vtoc.v_part[i].p_start = cyl * blks_per_cyl;
		label->dkl_vtoc.v_part[i].p_size = ncyls[i] * blks_per_cyl;
		cyl += ncyls[i];
	}
	for (i = 0; i < NDKMAP/2; i++) {

#elif defined(_SUNOS_VTOC_8)
	for (i = 0; i < NDKMAP; i++) {

#else
#error No VTOC format defined.
#endif				/* defined(_SUNOS_VTOC_16) */

		if (i == 2 || ncyls[i] == 0)
			continue;
#if defined(_SUNOS_VTOC_8)
		label->dkl_map[i].dkl_cylno = cyl;
		label->dkl_map[i].dkl_nblk = ncyls[i] * blks_per_cyl;

#elif defined(_SUNOS_VTOC_16)
		label->dkl_vtoc.v_part[i].p_start = cyl * blks_per_cyl;
		label->dkl_vtoc.v_part[i].p_size = ncyls[i] * blks_per_cyl;

#else
#error No VTOC format defined.
#endif				/* defined(_SUNOS_VTOC_8) */

		cyl += ncyls[i];
	}

	/*
	 * Set the whole disk partition
	 */
#if defined(_SUNOS_VTOC_8)
	label->dkl_map[2].dkl_cylno = 0;
	label->dkl_map[2].dkl_nblk =
		label->dkl_ncyl * label->dkl_nhead * label->dkl_nsect;

#elif defined(_SUNOS_VTOC_16)
	label->dkl_vtoc.v_part[2].p_start = 0;
	label->dkl_vtoc.v_part[2].p_size =
		(label->dkl_ncyl + label->dkl_acyl) * label->dkl_nhead *
			label->dkl_nsect;
#else
#error No VTOC format defined.
#endif				/* defined(_SUNOS_VTOC_8) */


	if (option_msg && diag_msg) {
		float	scaled;
		err_print("\n");
		for (i = 0; i < NDKMAP; i++) {
#if defined(_SUNOS_VTOC_8)
			if (label->dkl_map[i].dkl_nblk == 0)

#elif defined(_SUNOS_VTOC_16)
			if (label->dkl_vtoc.v_part[i].p_size == 0)

#else
#error No VTOC format defined.
#endif				/* defined(_SUNOS_VTOC_8) */

				continue;
			err_print("Partition %d:   ", i);
#if defined(_SUNOS_VTOC_8)
			scaled = bn2mb(label->dkl_map[i].dkl_nblk);

#elif defined(_SUNOS_VTOC_16)

			scaled = bn2mb(label->dkl_vtoc.v_part[i].p_size);
#else
#error No VTOC format defined.
#endif				/* defined(_SUNOS_VTOC_8) */

			if (scaled > 1024.0) {
				err_print("%6.2fGB  ", scaled/1024.0);
			} else {
				err_print("%6.2fMB  ", scaled);
			}
			err_print(" %6d cylinders\n",
#if defined(_SUNOS_VTOC_8)
			    label->dkl_map[i].dkl_nblk/blks_per_cyl);

#elif defined(_SUNOS_VTOC_16)
			    label->dkl_vtoc.v_part[i].p_size/blks_per_cyl);

#else
#error No VTOC format defined.
#endif				/* defined(_SUNOS_VTOC_8) */

		}
		err_print("\n");
	}

	return (1);
}



/*
 * Find an existing scsi disk definition by this name,
 * if possible.
 */
static struct disk_type *
find_scsi_disk_type(
	char			*disk_name,
	struct dk_label		*label)
{
	struct ctlr_type	*ctlr;
	struct disk_type	*dp;

	ctlr = find_scsi_ctlr_type();
	for (dp = ctlr->ctype_dlist; dp != NULL; dp = dp->dtype_next) {
		if ((strcmp(dp->dtype_asciilabel, disk_name) == 0) &&
				dp->dtype_pcyl == label->dkl_pcyl &&
				dp->dtype_ncyl == label->dkl_ncyl &&
				dp->dtype_acyl == label->dkl_acyl &&
				dp->dtype_nhead == label->dkl_nhead &&
				dp->dtype_nsect == label->dkl_nsect) {
			return (dp);
		}
	}

	return ((struct disk_type *)NULL);
}


/*
 * Find an existing scsi disk definition by this name,
 * if possible.
 */
static struct disk_type *
find_scsi_disk_by_name(
	char			*disk_name)
{
	struct ctlr_type	*ctlr;
	struct disk_type	*dp;

	ctlr = find_scsi_ctlr_type();
	for (dp = ctlr->ctype_dlist; dp != NULL; dp = dp->dtype_next) {
		if ((strcmp(dp->dtype_asciilabel, disk_name) == 0)) {
			return (dp);
		}
	}

	return ((struct disk_type *)NULL);
}


/*
 * Return a pointer to the ctlr_type structure for SCSI
 * disks.  This list is built into the program, so there's
 * no chance of not being able to find it, unless someone
 * totally mangles the code.
 */
static struct ctlr_type *
find_scsi_ctlr_type()
{
	struct	mctlr_list	*mlp;

	mlp = controlp;

	while (mlp != NULL) {
		if (mlp->ctlr_type->ctype_ctype == DKC_SCSI_CCS) {
			return (mlp->ctlr_type);
		}
	mlp = mlp->next;
	}

	impossible("no SCSI controller type");
	/*NOTREACHED*/
}



/*
 * Return a pointer to the scsi ctlr_info structure.  This
 * structure is allocated the first time format sees a
 * disk on this controller, so it must be present.
 */
static struct ctlr_info *
find_scsi_ctlr_info(
	struct dk_cinfo		*dkinfo)
{
	struct ctlr_info	*ctlr;

	if (dkinfo->dki_ctype != DKC_SCSI_CCS) {
		return (NULL);
	}

	for (ctlr = ctlr_list; ctlr != NULL; ctlr = ctlr->ctlr_next) {
		if (ctlr->ctlr_addr == dkinfo->dki_addr &&
			ctlr->ctlr_space == dkinfo->dki_space &&
				ctlr->ctlr_ctype->ctype_ctype ==
					DKC_SCSI_CCS) {
			return (ctlr);
		}
	}

	impossible("no SCSI controller info");
	/*NOTREACHED*/
}



static struct disk_type *
new_scsi_disk_type(
	int		fd,
	char		*disk_name,
	struct dk_label	*label)
{
	struct disk_type	*dp;
	struct disk_type	*disk;
	struct ctlr_info	*ctlr;
	struct dk_cinfo		dkinfo;
	struct partition_info	*part;
	struct partition_info	*pt;
	struct disk_info	*disk_info;
	int			i;

	/*
	 * Get the disk controller info for this disk
	 */
	if (ioctl(fd, DKIOCINFO, &dkinfo) == -1) {
		if (option_msg && diag_msg) {
			err_print("DKIOCINFO failed\n");
		}
		return (NULL);
	}

	/*
	 * Find the ctlr_info for this disk.
	 */
	ctlr = find_scsi_ctlr_info(&dkinfo);

	/*
	 * Allocate a new disk type for the SCSI controller.
	 */
	disk = (struct disk_type *)zalloc(sizeof (struct disk_type));

	/*
	 * Find the disk_info instance for this disk.
	 */
	disk_info = find_scsi_disk_info(&dkinfo);

	/*
	 * The controller and the disk should match.
	 */
	assert(disk_info->disk_ctlr == ctlr);

	/*
	 * Link the disk into the list of disks
	 */
	dp = ctlr->ctlr_ctype->ctype_dlist;
	if (dp == NULL) {
		ctlr->ctlr_ctype->ctype_dlist = dp;
	} else {
		while (dp->dtype_next != NULL) {
			dp = dp->dtype_next;
		}
		dp->dtype_next = disk;
	}
	disk->dtype_next = NULL;

	/*
	 * Allocate and initialize the disk name.
	 */
	disk->dtype_asciilabel = alloc_string(disk_name);

	/*
	 * Initialize disk geometry info
	 */
	disk->dtype_pcyl = label->dkl_pcyl;
	disk->dtype_ncyl = label->dkl_ncyl;
	disk->dtype_acyl = label->dkl_acyl;
	disk->dtype_nhead = label->dkl_nhead;
	disk->dtype_nsect = label->dkl_nsect;
	disk->dtype_rpm = label->dkl_rpm;

	/*
	 * Attempt to match the partition map in the label
	 * with a know partition for this disk type.
	 */
	for (part = disk->dtype_plist; part; part = part->pinfo_next) {
		if (parts_match(label, part)) {
			break;
		}
	}

	/*
	 * If no match was made, we need to create a partition
	 * map for this disk.
	 */
	if (part == NULL) {
		part = (struct partition_info *)
			zalloc(sizeof (struct partition_info));
		pt = disk->dtype_plist;
		if (pt == NULL) {
			disk->dtype_plist = part;
		} else {
			while (pt->pinfo_next != NULL) {
				pt = pt->pinfo_next;
			}
			pt->pinfo_next = part;
		}
		part->pinfo_next = NULL;

		/*
		 * Set up the partition name
		 */
		part->pinfo_name = alloc_string("default");

		/*
		 * Fill in the partition info from the label
		 */
		for (i = 0; i < NDKMAP; i++) {

#if defined(_SUNOS_VTOC_8)
			part->pinfo_map[i] = label->dkl_map[i];

#elif defined(_SUNOS_VTOC_16)
			part->pinfo_map[i].dkl_cylno =
				label->dkl_vtoc.v_part[i].p_start /
					((int)(disk->dtype_nhead *
						disk->dtype_nsect - apc));
			part->pinfo_map[i].dkl_nblk =
				label->dkl_vtoc.v_part[i].p_size;
#else
#error No VTOC format defined.
#endif				/* defined(_SUNOS_VTOC_8) */

		}
	}


	/*
	 * Use the VTOC if valid, or install a default
	 */
	if (label->dkl_vtoc.v_version == V_VERSION) {
		(void) memcpy(disk_info->v_volume, label->dkl_vtoc.v_volume,
			LEN_DKL_VVOL);
		part->vtoc = label->dkl_vtoc;
	} else {
		(void) memset(disk_info->v_volume, 0, LEN_DKL_VVOL);
		set_vtoc_defaults(part);
	}

	/*
	 * Link the disk to the partition map
	 */
	disk_info->disk_parts = part;

	return (disk);
}


static struct disk_info *
find_scsi_disk_info(
	struct dk_cinfo		*dkinfo)
{
	struct disk_info	*disk;
	struct dk_cinfo		*dp;

	for (disk = disk_list; disk != NULL; disk = disk->disk_next) {
		assert(dkinfo->dki_ctype == DKC_SCSI_CCS);
		dp = &disk->disk_dkinfo;
		if (dp->dki_ctype == dkinfo->dki_ctype &&
			dp->dki_cnum == dkinfo->dki_cnum &&
			dp->dki_unit == dkinfo->dki_unit &&
			strcmp(dp->dki_dname, dkinfo->dki_dname) == 0) {
			return (disk);
		}
	}

	impossible("No SCSI disk info instance\n");
	/*NOTREACHED*/
}


static char *
get_sun_disk_name(
	char			*disk_name,
	struct scsi_inquiry	*inquiry)
{
	/*
	 * Extract the sun name of the disk
	 */
	(void) memset(disk_name, 0, DISK_NAME_MAX);
	(void) memcpy(disk_name, (char *)&inquiry->inq_pid[9], 7);

	return (disk_name);
}


static char *
get_generic_disk_name(
	char			*disk_name,
	struct scsi_inquiry	*inquiry)
{
	char	*p;

	(void) memset(disk_name, 0, DISK_NAME_MAX);
	p = strcopy(disk_name, inquiry->inq_vid,
		sizeof (inquiry->inq_vid));
	*p++ = '-';
	p = strcopy(p, inquiry->inq_pid, sizeof (inquiry->inq_pid));
	*p++ = '-';
	p = strcopy(p, inquiry->inq_revision,
		sizeof (inquiry->inq_revision));

	return (disk_name);
}



static int
force_blocksize(
	int	fd)
{
	union {
		struct mode_format	page3;
		u_char			buf3[MAX_MODE_SENSE_SIZE];
	} u_page3;
	struct mode_format		*page3 = &u_page3.page3;
	struct scsi_ms_header		header;

	if (check("\
Must reformat device to 512-byte blocksize.  Continue") == 0) {

		/*
		 * Get current Page 3 - Format Parameters page
		 */
		if (uscsi_mode_sense(fd, DAD_MODE_FORMAT,
			MODE_SENSE_PC_CURRENT, (caddr_t)&u_page3,
			MAX_MODE_SENSE_SIZE, &header)) {
			goto err;
		}

		/*
		 * Make our changes to the geometry
		 */
		header.mode_header.length = 0;
		header.mode_header.device_specific = 0;
		page3->mode_page.ps = 0;
		page3->data_bytes_sect = DEV_BSIZE;

		/*
		 * make sure that logical block size is of
		 * DEV_BSIZE.
		 */
		header.block_descriptor.blksize_hi = (DEV_BSIZE >> 16);
		header.block_descriptor.blksize_mid = (DEV_BSIZE >> 8);
		header.block_descriptor.blksize_lo = (char)(DEV_BSIZE);
		/*
		 * Select current Page 3 - Format Parameters page
		 */
		if (uscsi_mode_select(fd, DAD_MODE_FORMAT,
			MODE_SELECT_PF, (caddr_t)&u_page3,
			MODESENSE_PAGE_LEN(&u_page3), &header)) {
			goto err;
		}

		/*
		 * Now reformat the device
		 */
		if (raw_format(fd)) {
			goto err;
		}
		return (0);
	}

err:
	if (option_msg && diag_msg) {
		err_print(
			"Reformat device to 512-byte blocksize failed\n");
	}
	return (1);
}

static int
raw_format(
	int	fd)
{
	union scsi_cdb			cdb;
	struct uscsi_cmd		ucmd;
	struct scsi_defect_hdr		defect_hdr;

	(void) memset((char *)&ucmd, 0, sizeof (ucmd));
	(void) memset((char *)&cdb, 0, sizeof (union scsi_cdb));
	(void) memset((char *)&defect_hdr, 0, sizeof (defect_hdr));
	cdb.scc_cmd = SCMD_FORMAT;
	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP0;
	ucmd.uscsi_bufaddr = (caddr_t)&defect_hdr;
	ucmd.uscsi_buflen = sizeof (defect_hdr);
	cdb.cdb_opaque[1] = FPB_DATA;

	/*
	 * Issue the format ioctl
	 */
	fmt_print("Formatting...\n");
	(void) fflush(stdout);
	if (uscsi_cmd(fd, &ucmd,
		(option_msg && diag_msg) ? F_NORMAL : F_SILENT)) {
		return (1);
	}
	return (0);
}

/*
 * Copy a string of characters from src to dst, for at
 * most n bytes.  Strip all leading and trailing spaces,
 * and stop if there are any non-printable characters.
 * Return ptr to the next character to be filled.
 */
static char *
strcopy(
	char	*dst,
	char	*src,
	int	n)
{
	int	i;

	while (*src == ' ' && n > 0) {
		src++;
		n--;
	}

	for (i = 0; n-- > 0 && isascii(*src) && isprint(*src); src++) {
		if (*src == ' ') {
			i++;
		} else {
			while (i-- > 0)
				*dst++ = ' ';
			*dst++ = *src;
		}
	}

	*dst = 0;
	return (dst);
}

/*
 * adjust disk geometry.
 * This is used when disk reports a disk geometry page having
 * no of physical cylinders is < 3 which is the minimum required
 * by Solaris (2 for storing labels and at least one as a data
 * cylinder )
 */
adjust_disk_geometry(int capacity, int *cyl, int *nhead, int *nsect)
{
	int	lcyl = *cyl;
	int	lnhead = *nhead;
	int	lnsect = *nsect;

	assert(lcyl < SUN_MIN_CYL);

	/*
	 * reduce nsect by 2 for each iteration  and re-calculate
	 * the number of cylinders.
	 */
	while (lnsect > MINIMUM_NO_SECTORS &&
			lcyl < MINIMUM_NO_CYLINDERS) {
		/*
		 * make sure that we do not go below MINIMUM_NO_SECTORS.
		 */
		lnsect = max(MINIMUM_NO_SECTORS, lnsect / 2);
		lcyl   = (capacity) / (lnhead * lnsect);
	}
	/*
	 * If the geometry still does not satisfy
	 * MINIMUM_NO_CYLINDERS then try to reduce the
	 * no of heads.
	 */
	while (lnhead > MINIMUM_NO_HEADS &&
			(lcyl < MINIMUM_NO_CYLINDERS)) {
		lnhead = max(MINIMUM_NO_HEADS, lnhead / 2);
		lcyl =  (capacity) / (lnhead * lnsect);
	}
	/*
	 * now we should have atleast SUN_MIN_CYL cylinders.
	 * If we still do not get SUN_MIN_CYL with MINIMUM_NO_HEADS
	 * and MINIMUM_NO_HEADS then return error.
	 */
	if (lcyl < SUN_MIN_CYL)
		return (1);
	else {
		*cyl = lcyl;
		*nhead = lnhead;
		*nsect = lnsect;
		return (0);
	}
}

/*
 * Reduce the size of one dimention below a specified
 * limit with a minimum loss of volume.  Dimenstions are
 * assumed to be passed in form the largest value (the one
 * that needs to be reduced) to the smallest value.  The
 * values will be twiddled until they are all less than or
 * equal to their limit.  Returns the number in the new geometry.
 */
static int
square_box(
		int capacity,
		int *dim1, int lim1,
		int *dim2, int lim2,
		int *dim3, int lim3)
{
	int	i;

	/*
	 * Although the routine should work with any ordering of
	 * parameters, it's most efficient if they are passed in
	 * in decreasing magnitude.
	 */
	assert(*dim1 >= *dim2);
	assert(*dim2 >= *dim3);

	/*
	 * This is done in a very arbitrary manner.  We could try to
	 * find better values but I can't come up with a method that
	 * would run in a reasonable amount of time.  That could take
	 * approximately 65535 * 65535 iterations of a dozen flops each
	 * or well over 4G flops.
	 *
	 * First:
	 *
	 * Let's see how far we can go with bitshifts w/o losing
	 * any blocks.
	 */

	for (i = 0; (((*dim1)>>i)&1) == 0 && ((*dim1)>>i) > lim1; i++);
	if (i) {
		*dim1 = ((*dim1)>>i);
		*dim3 = ((*dim3)<<i);
	}

	if (((*dim1) > lim1) || ((*dim2) > lim2) || ((*dim3) > lim3)) {
		double 	d[4];

		/*
		 * Second:
		 *
		 * Set the highest value at its limit then calculate errors,
		 * adjusting the 2nd highest value (we get better resolution
		 * that way).
		 */
		d[1] = lim1;
		d[3] = *dim3;
		d[2] = (double)capacity/(d[1]*d[3]);

		/*
		 * If we overflowed the middle term, set it to its limit and
		 * chose a new low term.
		 */
		if (d[2] > lim2) {
			d[2] = lim2;
			d[3] = (double)capacity/(d[1]*d[2]);
		}
		/*
		 * Convert to integers.
		 */
		*dim1 = d[1];
		*dim2 = d[2];
		*dim3 = d[3];
	}
	/*
	 * Fixup any other possible problems.
	 * If this happens, we need a new disklabel format.
	 */
	if (*dim1 > lim1) *dim1 = lim1;
	if (*dim2 > lim2) *dim2 = lim2;
	if (*dim3 > lim3) *dim3 = lim3;
	return (*dim1 * *dim2 * *dim3);
}
