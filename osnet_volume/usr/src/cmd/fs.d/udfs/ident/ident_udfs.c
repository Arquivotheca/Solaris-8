/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ident_udfs.c	1.5	99/10/14 SMI"

#include	<stdio.h>
#include	<fcntl.h>
#include	<unistd.h>
#include	<rmmount.h>
#include	<rpc/types.h>
#include	<sys/types.h>
#include	<sys/cdio.h>
#include	<sys/dkio.h>
#include	<sys/fs/udf_volume.h>

static bool_t udfs_check_avds(int32_t, uint8_t *,
		int32_t, uint32_t *, uint32_t *);
static bool_t udfs_getsector(int32_t, uint8_t *, int32_t, int32_t);
/*
 * We call it a udfs file system iff:
 *	The File system is a valid udfs
 *
 */

#ifdef	STANDALONE
/*
 * Compile using cc -DSTANDALONE ident_udfs.c
 * if needed to run this standalone for testing
 */
int32_t
main(int32_t argc, char *argv[])
{
	int32_t			clean;
	int32_t			fd;
	int32_t			ret;

	if (argc != 2) {
		(void) printf("Usage : %s device_name\n", argv[0]);
		return (1);
	}

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror(argv[0]);
		return (1);
	}

	ret = ident_fs(fd, "", &clean, 0);
	(void) printf("return value of ident_fs is %s clean flag "
		"is set to %d\n", (ret == TRUE) ? "TRUE" : "FALSE", clean);

	close(fd);
	return (0);
}
#endif

/*
 * As sun scsi cdrom drives return block size of different
 * values 512, 1024, 2048 so we still need to check the
 * different blocksizes on the device. But on the other
 * hand ATAPI cdrom and DVD-ROM
 * drives will return the blocksize as 2048 which is
 * the most probable block size of UDFS on a CD/DVD media
 * for this reason we issue the ioctl at the begining of
 * the code. The code also handles the situation when
 * a a image is created on a Hard Disk and copied to a CD-ROM.
 */
/* ARGSUSED */
int32_t
ident_fs(int32_t fd, char *rawpath, int32_t *clean, int32_t verbose)
{
	int32_t			ssize = 0;
	int32_t			count = 0;
	int32_t			index = 0;
	int32_t			ret = FALSE;
	int32_t			bsizes[] = {0, 512, 1024, 2048};
	uint32_t		loc = 0;
	uint32_t		len = 0;
	struct	log_vol_desc	*lvd = NULL;
	struct	log_vol_int_desc	*lvid = NULL;
	uint8_t			*read_buf = NULL;
	uint32_t		buf[2048/4];
				/* size match with the biggest bsizes */
	struct	dk_minfo	dkminfo;

	read_buf = (uint8_t *)buf;

	/*
	 * Try to get the physical
	 * block size of the device
	 */
	if (ioctl(fd, CDROMGBLKMODE, &bsizes[0]) < 0) {
		/*
		 * Not a CDROM so issue DKIOCGMEDIAINFO
		 */
		if (ioctl(fd, DKIOCGMEDIAINFO, &dkminfo) == 0) {
			bsizes[0] = dkminfo.dki_lbsize;
		} else {
			bsizes[0] = 512;
		}
	}

	/* Read AVD */
	count = sizeof (bsizes) / sizeof (int32_t);
	for (index = 0; index < count; index++) {
		if ((index > 0) && (bsizes[index] == bsizes[0])) {
			continue;
		}
		ret = udfs_check_avds(fd, read_buf, bsizes[index], &loc, &len);
		if (ret == TRUE) {
			break;
		}
	}

	/*
	 * Return FALSE if there is no Anchor Volume Descriptor
	 */
	if (ret == FALSE) {
		return (FALSE);
	}

	ssize = bsizes[index];

	/*
	 * read mvds and figure out the location
	 * of the lvid
	 */
	count = len / ssize;
	for (index = 0; index < count; index++) {
		if (udfs_getsector(fd, read_buf, loc + index, ssize) == FALSE) {
			return (FALSE);
		}
		/* LINTED */
		lvd = (struct log_vol_desc *)read_buf;
		if (SWAP_16(lvd->lvd_tag.tag_id) == UD_LOG_VOL_DESC) {
			loc = SWAP_32(lvd->lvd_int_seq_ext.ext_loc);
			len = SWAP_32(lvd->lvd_int_seq_ext.ext_len);
			break;
		}
	}
	if (index == count) {
		return (FALSE);
	}

	/*
	 * See if the lvid is closed
	 * or open integrity
	 */
	count = len / ssize;
	for (index = 0; index < count; index++) {
		if (udfs_getsector(fd, read_buf, loc + index, ssize) == FALSE) {
			return (FALSE);
		}
		/* LINTED */
		lvid = (struct log_vol_int_desc *)read_buf;
		if (SWAP_16(lvid->lvid_tag.tag_id) == UD_LOG_VOL_INT) {
			if (SWAP_32(lvid->lvid_int_type) == LOG_VOL_OPEN_INT) {
				*clean = FALSE;
			} else {
				*clean = TRUE;
			}
			return (TRUE);
		}
	}
	return (FALSE);
}

static bool_t
udfs_check_avds(int32_t fd, uint8_t *read_buf, int32_t ssize,
		uint32_t *mvds_loc, uint32_t *mvds_size)
{
	struct	anch_vol_desc_ptr	*avd = NULL;

	if (udfs_getsector(fd, read_buf, ANCHOR_VOL_DESC_LOC, ssize) ==
			TRUE) {
		/* LINTED */
		avd = (struct anch_vol_desc_ptr *)read_buf;
		if (SWAP_16(avd->avd_tag.tag_id) == UD_ANCH_VOL_DESC) {
			*mvds_loc = SWAP_32(avd->avd_main_vdse.ext_loc);
			*mvds_size = SWAP_32(avd->avd_main_vdse.ext_len);
			return (TRUE);
		}
	}
	return (FALSE);
}

static bool_t
udfs_getsector(int32_t fd, uint8_t *buf, int32_t secno, int32_t ssize)
{
	if (llseek(fd, (offset_t)(secno * ssize), SEEK_SET) < 0L) {
		return (FALSE);
	}

	if (read(fd, buf, ssize) != ssize) {
		return (FALSE);
	}

	/* all went well */
	return (TRUE);
}
