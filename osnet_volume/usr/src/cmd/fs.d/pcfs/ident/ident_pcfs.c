/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ident_pcfs.c 1.8	96/05/13 SMI"

#include	<stdio.h>
#include	<fcntl.h>
#include	<unistd.h>
#include	<rpc/types.h>
#include	<sys/types.h>
#include	<sys/fs/pc_label.h>
#if	defined(_FIRMWARE_NEEDS_FDISK)
#include	<sys/dktp/fdisk.h>
#endif
#include	<rmmount.h>
#ifdef	DEBUG
#include	<errno.h>
#endif


#define	DOS_READLEN	(PC_SECSIZE * 4)


/*
 * We call it a pcfs file system iff:
 *	The "media type" descriptor in the label == the media type
 *		descriptor that's supposed to be the first byte
 *		of the FAT.
 *	The second byte of the FAT is 0xff.
 *	The third byte of the FAT is 0xff.
 *
 *	Assumptions:
 *
 *	1.	I don't really know how safe this is, but it is
 *	mentioned as a way to tell whether you have a dos disk
 *	in my book (Advanced MSDOS Programming, Microsoft Press).
 *	Actually it calls it an "IBM-compatible disk" but that's
 *	good enough for me.
 *
 * 	2.	The FAT is right after the reserved sector(s), and both
 *	the sector size and number of reserved sectors must be gotten
 *	from the boot sector.
 */
/*ARGSUSED*/
bool_t
ident_fs(int fd, char *rawpath, bool_t *clean, bool_t verbose)
{
	static bool_t	ident_fs_offset(int, char *, bool_t *, bool_t, off_t);
#if	defined(_FIRMWARE_NEEDS_FDISK)
	static bool_t	find_dos_part_offset(int, off_t *);
#endif
	off_t		offset = 0L;
	bool_t		res = FALSE;


#ifdef	DEBUG
	(void) fprintf(stderr,
	    "DEBUG: pcfs ident_fs(%d, \"%s\", ...): entering\n",
	    fd, rawpath);
#endif

	/* try no offset first (i.e. a floppy?) */
	res = ident_fs_offset(fd, rawpath, clean, verbose, offset);
#if	defined(_FIRMWARE_NEEDS_FDISK)
	if (!res) {
		if (find_dos_part_offset(fd, &offset)) {
			res = ident_fs_offset(fd, rawpath, clean, verbose,
			    offset);
		}
	}
#endif	/* _FIRMWARE_NEEDS_FDISK */

#ifdef	DEBUG
	(void) fprintf(stderr, "DEBUG: pcfs ident_fs(): returning %s\n",
	    res ? "TRUE" : "FALSE");
#endif
	return (res);
}


#if	defined(_FIRMWARE_NEEDS_FDISK)
/*
 * find DOS partitions offset *iff* there's no Solaris partition *at all*
 */
static bool_t
find_dos_part_offset(int fd, off_t *offsetp)
{
	bool_t		res = FALSE;
	char		mbr[DOS_READLEN];	/* master boot record buf */
	struct mboot	*mbp;
	int		i;
	struct ipart	*ipp;
	int		dos_index = -1;





#ifdef	DEBUG
	(void) fprintf(stderr,
	    "DEBUG: find_dos_part_offset(%d, ...): entering\n", fd);
#endif

	/* seek to start of disk (assume it works) */
	if (lseek(fd, 0L, SEEK_SET) != 0L) {
		perror("pcfs seek");	/* should be able to seek to 0 */
		goto dun;
	}

	/* try to read */
	if (read(fd, mbr, DOS_READLEN) != DOS_READLEN) {
#ifdef	DEBUG
		(void) fprintf(stderr,
		    "DEBUG: read of FDISK table failed; %d\n", errno);
#else
		perror("pcfs read");
#endif
		goto dun;
	}

	/* get pointer to master boot struct and validate */
	mbp = (struct mboot *)mbr;
	if (mbp->signature != MBB_MAGIC) {
		/* not an FDISK table */
#ifdef	DEBUG
		(void) fprintf(stderr, "DEBUG: FDISK magic number wrong\n");
#endif
		goto dun;
	}

	/* scan fdisk entries, looking for first BIG-DOS/DOSOS16 entry */
	for (i = 0; i < FD_NUMPART; i++) {
		ipp = (struct ipart *)&(mbp->parts[i * sizeof (struct ipart)]);
		if ((ipp->systid == DOSOS16) ||
		    (ipp->systid == DOSHUGE)) {
			dos_index = i;
		}
		if (ipp->systid == SUNIXOS) {
			/* oh oh -- not suposed to be solaris here! */
#ifdef	DEBUG
			(void) fprintf(stderr,
			    "DEBUG: Solaris found but not wanted!\n");
#endif
			goto dun;
		}
	}

	/* see if we found a match */
	if (dos_index >= 0) {
		res = TRUE;
		*offsetp = ipp->relsect * PC_SECSIZE;
	}

dun:
#ifdef	DEBUG
	(void) fprintf(stderr,
	    "DEBUG: find_dos_parts_offset(): returning %s\n",
	    res ? "TRUE" : "FALSE");
#endif
	return (res);
}
#endif	/* _FIRMWARE_NEEDS_FDISK */


static bool_t
ident_fs_offset(int fd, char *rawpath, bool_t *clean, bool_t verbose,
    off_t offset)
{
	u_char	pc_stuff[DOS_READLEN];
	uint_t	fat_off;
	bool_t	res = FALSE;


#ifdef	DEBUG
	(void) fprintf(stderr,
	    "DEBUG: pcfs ident_fs_offset(%d, \"%s\", ..., %ld): entering\n",
	    fd, rawpath, offset);
#endif

	/*
	 * pcfs is always clean... at least there's no way to tell if
	 * it isn't!
	 */
	*clean = TRUE;

	/* go to start of image */
	if (lseek(fd, offset, SEEK_SET) != offset) {
		perror("pcfs seek");	/* should be able to seek to 0 */
		goto dun;
	}

	/* read the boot sector (plus some) */
	if (read(fd, pc_stuff, DOS_READLEN) != DOS_READLEN) {
#ifdef	DEBUG
		(void) fprintf(stderr, "DEBUG: read of MBR failed: %d\n",
		    errno);
#else
		perror("pcfs read");	/* should be able to read 4 sectors */
#endif
		goto dun;
	}

	/* no need to go farther if magic# is wrong */
	if ((*pc_stuff != (uchar_t)DOS_ID1) &&
	    (*pc_stuff != (uchar_t)DOS_ID2a)) {
#ifdef	DEBUG
		(void) fprintf(stderr, "DEBUG: DOS_ID1 or DOS_ID2a wrong\n");
#endif
		goto dun;	/* magic# wrong */
	}

	/* calculate where FAT starts */
	fat_off = ltohs(pc_stuff[PCB_BPSEC]) * ltohs(pc_stuff[PCB_RESSEC]);

	/* if offset is too large we probably have garbage */
	if (fat_off >= sizeof (pc_stuff)) {
#ifdef	DEBUG
		(void) fprintf(stderr, "DEBUG: FAT offset out of range\n");
#endif
		goto dun;	/* FAT offset out of range */
	}

	if ((pc_stuff[PCB_MEDIA] == pc_stuff[fat_off]) &&
	    ((uchar_t)0xff == pc_stuff[fat_off + 1]) &&
	    ((uchar_t)0xff == pc_stuff[fat_off + 2])) {
		res = TRUE;
	}

dun:
#ifdef	DEBUG
	(void) fprintf(stderr, "DEBUG: pcfs ident_fs_offset(): returning %s\n",
	    res ? "TRUE" : "FALSE");
#endif
	return (res);
}
