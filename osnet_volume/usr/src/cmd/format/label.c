
/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)label.c	1.26	99/11/06 SMI"

/*
 * This file contains the code relating to label manipulation.
 */

#include "global.h"
#include "label.h"
#include "misc.h"
#include "main.h"
#include "partition.h"
#include <string.h>
#include <memory.h>
#include <sys/isa_defs.h>
#include <sys/vtoc.h>

#if defined(_FIRMWARE_NEEDS_FDISK)
#include <sys/dktp/fdisk.h>
#include "menu_fdisk.h"
#endif		/* defined(_FIRMWARE_NEEDS_FDISK) */


extern int	read_vtoc(int, struct vtoc *);
extern int	write_vtoc(int, struct vtoc *);


/*
 * This routine checks the given label to see if it is valid.
 */
int
checklabel(label)
	register struct dk_label *label;
{

	/*
	 * Check the magic number.
	 */
	if (label->dkl_magic != DKL_MAGIC)
		return (0);
	/*
	 * Check the checksum.
	 */
	if (checksum(label, CK_CHECKSUM) != 0)
		return (0);
	return (1);
}

/*
 * This routine checks or calculates the label checksum, depending on
 * the mode it is called in.
 */
int
checksum(label, mode)
	struct	dk_label *label;
	int	mode;
{
	register short *sp, sum = 0;
	register short count = (sizeof (struct dk_label)) / (sizeof (short));

	/*
	 * If we are generating a checksum, don't include the checksum
	 * in the rolling xor.
	 */
	if (mode == CK_MAKESUM)
		count -= 1;
	sp = (short *)label;
	/*
	 * Take the xor of all the half-words in the label.
	 */
	while (count--) {
		sum ^= *sp++;
	}
	/*
	 * If we are checking the checksum, the total will be zero for
	 * a correct checksum, so we can just return the sum.
	 */
	if (mode == CK_CHECKSUM)
		return (sum);
	/*
	 * If we are generating the checksum, fill it in.
	 */
	else {
		label->dkl_cksum = sum;
		return (0);
	}
}

/*
 * This routine is used to extract the id string from the string stored
 * in a disk label.  The problem is that the string in the label has
 * the physical characteristics of the drive appended to it.  The approach
 * is to find the beginning of the physical attributes portion of the string
 * and truncate it there.
 */
int
trim_id(id)
	char	*id;
{
	register char *c;

	/*
	 * Start at the end of the string.  When we match the word ' cyl',
	 * we are at the beginning of the attributes.
	 */
	for (c = id + strlen(id); c >= id; c--) {
		if (strncmp(c, " cyl", strlen(" cyl")) == 0) {
			/*
			 * Remove any white space.
			 */
			for (; (((*(c - 1) == ' ') || (*(c - 1) == '\t')) &&
				(c >= id)); c--);
			break;
		}
	}
	/*
	 * If we ran off the beginning of the string, something is wrong.
	 */
	if (c < id)
		return (-1);
	/*
	 * Truncate the string.
	 */
	*c = '\0';
	return (0);
}

/*
 * This routine constructs and writes a label on the disk.  It writes both
 * the primary and backup labels.  It assumes that there is a current
 * partition map already defined.  It also notifies the SunOS kernel of
 * the label and partition information it has written on the disk.
 */
int
write_label()
{
	int	error = 0, head, sec;
	struct dk_label label;
	struct dk_label new_label;
	struct vtoc	vtoc;
	struct dk_geom	geom;
	int		nbackups;

#if defined(_SUNOS_VTOC_8)
	int i;
#endif		/* defined(_SUNOS_VTOC_8) */

	/*
	 * Fill in a label structure with the geometry information.
	 */
	(void) memset((char *)&label, 0, sizeof (struct dk_label));
	(void) memset((char *)&new_label, 0, sizeof (struct dk_label));
	label.dkl_pcyl = pcyl;
	label.dkl_ncyl = ncyl;
	label.dkl_acyl = acyl;

#if defined(_SUNOS_VTOC_16)
	label.dkl_bcyl = bcyl;
#endif			/* defined(_SUNOC_VTOC_16) */

	label.dkl_nhead = nhead;
	label.dkl_nsect = nsect;
	label.dkl_apc = apc;
	label.dkl_intrlv = 1;
	label.dkl_rpm = cur_dtype->dtype_rpm;

#if defined(_SUNOS_VTOC_8)
	/*
	 * Also fill in the current partition information.
	 */
	for (i = 0; i < NDKMAP; i++) {
		label.dkl_map[i] = cur_parts->pinfo_map[i];
	}
#endif			/* defined(_SUNOS_VTOC_8) */

	label.dkl_magic = DKL_MAGIC;

	/*
	 * Fill in the vtoc information
	 */
	label.dkl_vtoc = cur_parts->vtoc;

	/*
	 * Use the current label
	 */
	bcopy(cur_disk->v_volume, label.dkl_vtoc.v_volume, LEN_DKL_VVOL);

	/*
	 * Put asciilabel in; on x86 it's in the vtoc, not the label.
	 */
	(void) sprintf(label.dkl_asciilabel, "%s cyl %d alt %d hd %d sec %d",
	    cur_dtype->dtype_asciilabel, ncyl, acyl, nhead, nsect);

#if defined(_SUNOS_VTOC_16)
	/*
	 * Also add in v_sectorsz, as the driver will.  Everyone
	 * else is assuming DEV_BSIZE, so we do the same.
	 */
	label.dkl_vtoc.v_sectorsz = DEV_BSIZE;
#endif			/* defined(_SUNOS_VTOC_16) */

	/*
	 * Generate the correct checksum.
	 */
	(void) checksum(&label, CK_MAKESUM);
	/*
	 * Convert the label into a vtoc
	 */
	if (label_to_vtoc(&vtoc, &label) == -1) {
		return (-1);
	}
	/*
	 * Fill in the geometry info.  This is critical that
	 * we do this before writing the vtoc.
	 */
	bzero((caddr_t)&geom, sizeof (struct dk_geom));
	geom.dkg_ncyl = ncyl;
	geom.dkg_acyl = acyl;

#if defined(_SUNOS_VTOC_16)
	geom.dkg_bcyl = bcyl;
#endif			/* defined(_SUNOS_VTOC_16) */

	geom.dkg_nhead = nhead;
	geom.dkg_nsect = nsect;
	geom.dkg_intrlv = 1;
	geom.dkg_apc = apc;
	geom.dkg_rpm = cur_dtype->dtype_rpm;
	geom.dkg_pcyl = pcyl;
	/*
	 * Lock out interrupts so we do things in sync.
	 */
	enter_critical();
	/*
	 * Do the ioctl to tell the kernel the geometry.
	 */
	if (ioctl(cur_file, DKIOCSGEOM, &geom) == -1) {
		err_print("Warning: error setting drive geometry.\n");
		error = -1;
	}
	/*
	 * Write the vtoc.  At the time of this writing, our
	 * drivers convert the vtoc back to a label, and
	 * then write both the primary and backup labels.
	 * This is not a requirement, however, as we
	 * always use an ioctl to read the vtoc from the
	 * driver, so it can do as it likes.
	 */
	if (write_vtoc(cur_file, &vtoc) != 0) {
		err_print("Warning: error writing VTOC.\n");
		error = -1;
	}

	/*
	 * Calculate where the backup labels went.  They are always on
	 * the last alternate cylinder, but some older drives put them
	 * on head 2 instead of the last head.  They are always on the
	 * first 5 odd sectors of the appropriate track.
	 */
	if (cur_ctype->ctype_flags & CF_BLABEL)
		head  = 2;
	else
		head = nhead - 1;
	/*
	 * Read and verify the backup labels.
	 */
	nbackups = 0;
	for (sec = 1; sec < BAD_LISTCNT * 2 + 1; sec += 2) {
		if ((*cur_ops->op_rdwr)(DIR_READ, cur_file,
			((chs2bn(ncyl + acyl - 1, head, sec)) + solaris_offset),
			1, (caddr_t)&new_label, F_NORMAL, NULL)) {
			err_print("Warning: error reading backup label.\n");
			error = -1;
		} else {
			if (bcmp((char *)&label, (char *)&new_label,
				sizeof (struct dk_label)) == 0) {
					nbackups++;
			}
		}
	}
	if (nbackups != BAD_LISTCNT) {
		err_print("Warning: %s\n", nbackups == 0 ?
			"no backup labels" :
			"some backup labels incorrect");
	}
	/*
	 * Mark the current disk as labelled and notify the kernel of what
	 * has happened.
	 */
	cur_disk->disk_flags |= DSK_LABEL;

	exit_critical();
	return (error);
}


/*
 * Read the label from the disk.
 * Do this via the read_vtoc() library routine, then convert it to a label.
 * We also need a DKIOCGGEOM ioctl to get the disk's geometry.
 */
int
read_label(int fd, struct dk_label *label)
{
	struct vtoc	vtoc;
	struct dk_geom	geom;

	if (read_vtoc(fd, &vtoc) < 0 || ioctl(fd, DKIOCGGEOM, &geom) == -1) {
		return (-1);
	}
	return (vtoc_to_label(label, &vtoc, &geom));
}


/*
 * Convert vtoc/geom to label.
 */
int
vtoc_to_label(struct dk_label *label, struct vtoc *vtoc, struct dk_geom *geom)
{
#if defined(_SUNOS_VTOC_8)
	struct dk_map		*lmap;

#elif defined(_SUNOS_VTOC_16)
	struct dkl_partition	*lmap;

#else
#error No VTOC format defined.
#endif			/* defined(_SUNOS_VTOC_8) */

	struct partition	*vpart;
	long			nblks;
	int			i;

	(void) memset((char *)label, 0, sizeof (struct dk_label));

	/*
	 * Sanity-check the vtoc
	 */
	if (vtoc->v_sanity != VTOC_SANE || vtoc->v_sectorsz != DEV_BSIZE ||
			vtoc->v_nparts != V_NUMPAR) {
		return (-1);
	}

	/*
	 * Sanity check of geometry
	 */
	if (geom->dkg_ncyl == 0 || geom->dkg_nhead == 0 ||
			geom->dkg_nsect == 0) {
		return (-1);
	}

	label->dkl_magic = DKL_MAGIC;

	/*
	 * Copy necessary portions of the geometry information
	 */
	label->dkl_rpm = geom->dkg_rpm;
	label->dkl_pcyl = geom->dkg_pcyl;
	label->dkl_apc = geom->dkg_apc;
	label->dkl_intrlv = geom->dkg_intrlv;
	label->dkl_ncyl = geom->dkg_ncyl;
	label->dkl_acyl = geom->dkg_acyl;

#if defined(_SUNOS_VTOC_16)
	label->dkl_bcyl = geom->dkg_bcyl;
#endif			/* defined(_SUNOS_VTOC_16) */

	label->dkl_nhead = geom->dkg_nhead;
	label->dkl_nsect = geom->dkg_nsect;

#if defined(_SUNOS_VTOC_8)
	label->dkl_obs1 = geom->dkg_obs1;
	label->dkl_obs2 = geom->dkg_obs2;
	label->dkl_obs3 = geom->dkg_obs3;
#endif			/* defined(_SUNOS_VTOC_8) */

	label->dkl_write_reinstruct = geom->dkg_write_reinstruct;
	label->dkl_read_reinstruct = geom->dkg_read_reinstruct;

	/*
	 * Copy vtoc structure fields into the disk label dk_vtoc
	 */
	label->dkl_vtoc.v_sanity = vtoc->v_sanity;
	label->dkl_vtoc.v_nparts = vtoc->v_nparts;
	label->dkl_vtoc.v_version = vtoc->v_version;

	(void) memcpy(label->dkl_vtoc.v_volume, vtoc->v_volume,
		LEN_DKL_VVOL);
	for (i = 0; i < V_NUMPAR; i++) {
		label->dkl_vtoc.v_part[i].p_tag = vtoc->v_part[i].p_tag;
		label->dkl_vtoc.v_part[i].p_flag = vtoc->v_part[i].p_flag;
	}
	(void) memcpy((char *)label->dkl_vtoc.v_bootinfo,
		(char *)vtoc->v_bootinfo, sizeof (vtoc->v_bootinfo));
	(void) memcpy((char *)label->dkl_vtoc.v_reserved,
		(char *)vtoc->v_reserved, sizeof (vtoc->v_reserved));
	(void) memcpy((char *)label->dkl_vtoc.v_timestamp,
		(char *)vtoc->timestamp, sizeof (vtoc->timestamp));

	(void) memcpy(label->dkl_asciilabel, vtoc->v_asciilabel,
		LEN_DKL_ASCII);

	/*
	 * Note the conversion from starting sector number
	 * to starting cylinder number.
	 * Return error if division results in a remainder.
	 */
#if defined(_SUNOS_VTOC_8)
	lmap = label->dkl_map;

#elif defined(_SUNOS_VTOC_16)
	lmap = label->dkl_vtoc.v_part;
#else
#error No VTOC format defined.
#endif			/* defined(_SUNOS_VTOC_8) */

	vpart = vtoc->v_part;

	nblks = (int)label->dkl_nsect * (int)label->dkl_nhead;

	for (i = 0; i < NDKMAP; i++, lmap++, vpart++) {
		if ((vpart->p_start % nblks) != 0 ||
				(vpart->p_size % nblks) != 0) {
			return (-1);
		}
#if defined(_SUNOS_VTOC_8)
		lmap->dkl_cylno = vpart->p_start / nblks;
		lmap->dkl_nblk = vpart->p_size;

#elif defined(_SUNOS_VTOC_16)
		lmap->p_start = vpart->p_start;
		lmap->p_size = vpart->p_size;
#else
#error No VTOC format defined.
#endif			/* defined(_SUNOS_VTOC_8) */
	}

	/*
	 * Finally, make a checksum
	 */
	(void) checksum(label, CK_MAKESUM);

	return (0);
}



/*
 * Extract a vtoc structure out of a valid label
 */
int
label_to_vtoc(struct vtoc *vtoc, struct dk_label *label)
{
#if defined(_SUNOS_VTOC_8)
	struct dk_map2		*lpart;
	struct dk_map		*lmap;
	long			nblks;

#elif defined(_SUNOS_VTOC_16)
	struct dkl_partition	*lpart;
#else
#error No VTOC format defined.
#endif				/* defined(_SUNOS_VTOC_8) */

	struct partition	*vpart;
	int			i;

	(void) memset((char *)vtoc, 0, sizeof (struct vtoc));

	switch (label->dkl_vtoc.v_version) {
	case 0:
		/*
		 * No valid vtoc information in the label.
		 * Construct default p_flags and p_tags.
		 */
		vpart = vtoc->v_part;
		for (i = 0; i < V_NUMPAR; i++, vpart++) {
			vpart->p_tag = default_vtoc_map[i].p_tag;
			vpart->p_flag = default_vtoc_map[i].p_flag;
		}
		break;

	case V_VERSION:
		vpart = vtoc->v_part;
		lpart = label->dkl_vtoc.v_part;
		for (i = 0; i < V_NUMPAR; i++, vpart++, lpart++) {
			vpart->p_tag = lpart->p_tag;
			vpart->p_flag = lpart->p_flag;

#if defined(_SUNOS_VTOC_16)
			vpart->p_start = lpart->p_start;
			vpart->p_size = lpart->p_size;
#endif	/* defined(_SUNOS_VTOC_16) */
		}
		(void) memcpy(vtoc->v_volume, label->dkl_vtoc.v_volume,
			LEN_DKL_VVOL);
		(void) memcpy((char *)vtoc->v_bootinfo,
			(char *)label->dkl_vtoc.v_bootinfo,
				sizeof (vtoc->v_bootinfo));
		(void) memcpy((char *)vtoc->v_reserved,
			(char *)label->dkl_vtoc.v_reserved,
				sizeof (vtoc->v_reserved));
		(void) memcpy((char *)vtoc->timestamp,
			(char *)label->dkl_vtoc.v_timestamp,
				sizeof (vtoc->timestamp));
		break;

	default:
		return (-1);
	}

	/*
	 * XXX - this looks wrong to me....
	 * why are these values hardwired, rather than returned from
	 * the real disk label?
	 */
	vtoc->v_sanity = VTOC_SANE;
	vtoc->v_version = V_VERSION;
	vtoc->v_sectorsz = DEV_BSIZE;
	vtoc->v_nparts = V_NUMPAR;

	(void) memcpy(vtoc->v_asciilabel, label->dkl_asciilabel,
		LEN_DKL_ASCII);

#if defined(_SUNOS_VTOC_8)
	/*
	 * Convert partitioning information.
	 * Note the conversion from starting cylinder number
	 * to starting sector number.
	 */
	lmap = label->dkl_map;
	vpart = vtoc->v_part;
	nblks = label->dkl_nsect * label->dkl_nhead;
	for (i = 0; i < V_NUMPAR; i++, vpart++, lmap++) {
		vpart->p_start = lmap->dkl_cylno * nblks;
		vpart->p_size = lmap->dkl_nblk;
	}
#endif			/* defined(_SUNOS_VTOC_8) */

	return (0);
}


#ifdef	FOR_DEBUGGING_ONLY
int
dump_label(label)
	struct dk_label	*label;
{
	int		i;

	fmt_print("%s\n", label->dkl_asciilabel);

	fmt_print("version:  %d\n", label->dkl_vtoc.v_version);
	fmt_print("volume:   ");
	for (i = 0; i < LEN_DKL_VVOL; i++) {
		if (label->dkl_vtoc.v_volume[i] == 0)
			break;
		fmt_print("%c", label->dkl_vtoc.v_volume[i]);
	}
	fmt_print("\n");
	fmt_print("v_nparts: %d\n", label->dkl_vtoc.v_nparts);
	fmt_print("v_sanity: %lx\n", label->dkl_vtoc.v_sanity);

#if defined(_SUNOS_VTOC_8)
	fmt_print("rpm:      %d\n", label->dkl_rpm);
	fmt_print("pcyl:     %d\n", label->dkl_pcyl);
	fmt_print("apc:      %d\n", label->dkl_apc);
	fmt_print("obs1:     %d\n", label->dkl_obs1);
	fmt_print("obs2:     %d\n", label->dkl_obs2);
	fmt_print("intrlv:   %d\n", label->dkl_intrlv);
	fmt_print("ncyl:     %d\n", label->dkl_ncyl);
	fmt_print("acyl:     %d\n", label->dkl_acyl);
	fmt_print("nhead:    %d\n", label->dkl_nhead);
	fmt_print("nsect:    %d\n", label->dkl_nsect);
	fmt_print("obs3:     %d\n", label->dkl_obs3);
	fmt_print("obs4:     %d\n", label->dkl_obs4);

#elif defined(_SUNOS_VTOC_16)
	fmt_print("rpm:      %d\n", label->dkl_rpm);
	fmt_print("pcyl:     %d\n", label->dkl_pcyl);
	fmt_print("apc:      %d\n", label->dkl_apc);
	fmt_print("intrlv:   %d\n", label->dkl_intrlv);
	fmt_print("ncyl:     %d\n", label->dkl_ncyl);
	fmt_print("acyl:     %d\n", label->dkl_acyl);
	fmt_print("nhead:    %d\n", label->dkl_nhead);
	fmt_print("nsect:    %d\n", label->dkl_nsect);
	fmt_print("bcyl:     %d\n", label->dkl_bcyl);
	fmt_print("skew:     %d\n", label->dkl_skew);
#else
#error No VTOC format defined.
#endif				/* defined(_SUNOS_VTOC_8) */
	fmt_print("magic:    %0x\n", label->dkl_magic);
	fmt_print("cksum:    %0x\n", label->dkl_cksum);

	for (i = 0; i < NDKMAP; i++) {

#if defined(_SUNOS_VTOC_8)
		fmt_print("%c:        cyl=%d, blocks=%d", i+'a',
			label->dkl_map[i].dkl_cylno,
			label->dkl_map[i].dkl_nblk);

#elif defined(_SUNOS_VTOC_16)
		fmt_print("%c:        start=%d, blocks=%d", i+'a',
		    label->dkl_vtoc.v_part[i].p_start,
		    label->dkl_vtoc.v_part[i].p_size);
#else
#error No VTOC format defined.
#endif				/* defined(_SUNOS_VTOC_8) */

		fmt_print(",  tag=%d,  flag=%d",
			label->dkl_vtoc.v_part[i].p_tag,
			label->dkl_vtoc.v_part[i].p_flag);
		fmt_print("\n");
	}

	fmt_print("read_reinstruct:  %d\n", label->dkl_read_reinstruct);
	fmt_print("write_reinstruct: %d\n", label->dkl_write_reinstruct);

	fmt_print("bootinfo: ");
	for (i = 0; i < 3; i++) {
		fmt_print("0x%x ", label->dkl_vtoc.v_bootinfo[i]);
	}
	fmt_print("\n");

	fmt_print("reserved: ");
	for (i = 0; i < 10; i++) {
		if ((i % 4) == 3)
			fmt_print("\n");
		fmt_print("0x%x ", label->dkl_vtoc.v_reserved[i]);
	}
	fmt_print("\n");

	fmt_print("timestamp:\n");
	for (i = 0; i < NDKMAP; i++) {
		if ((i % 4) == 3)
			fmt_print("\n");
		fmt_print("0x%x ", label->dkl_vtoc.v_timestamp[i]);
	}
	fmt_print("\n");

	fmt_print("pad:\n");
	dump("", label->dkl_pad, LEN_DKL_PAD, HEX_ONLY);

	fmt_print("\n\n");
}
#endif	FOR_DEBUGGING_ONLY
