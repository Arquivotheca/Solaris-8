/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988,1989,1999  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *			All rights reserved.
 *
 */

#ident	"@(#)fstyp.c	1.5	99/02/26 SMI"

/*
 * fstyp
 *
 * Designed to work with block devices (e.g., /dev/diskette), not
 * raw devices (e.g., /dev/rdiskette)
 */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/mntent.h>
#include <sys/errno.h>
#include <sys/fs/pc_fs.h>
#include <sys/fs/pc_label.h>
#include <sys/stat.h>
#include <sys/vfs.h>

#include <stdio.h>
#include <sys/mnttab.h>

#include <locale.h>

extern offset_t llseek();


#define	PC_BPSEC(a)	((a[12]<<8)+a[11])	/* bytes per sector */
#define	PC_SPC(a)	(a[13])			/* sectors per cluster */
#define	PC_RESSEC(a)	((a[15]<<8)+a[14])	/* reserved sectors */
#define	PC_NFAT(a)	(a[16])			/* number of fats */
#define	PC_NROOTENT(a)	((a[18]<<8)+a[17])	/* number of root dir entries */
#define	PC_MEDIA(a)	(a[21])			/* media descriptor */
#define	PC_SPT(a)	((a[25]<<8)+a[24])	/* sectors per track */
#define	PC_NHEAD(a)	((a[27]<<8)+a[26])	/* number of heads */
#define	PC_HIDSEC(a)	((a[29]<<8)+a[28])	/* number of hidden sectors */
#define	PC_DRVNUM(a)	(a[36])			/* drive number */
#define	PC_LAB_ADDR(a)	(&a[43])		/* addr of volume label */
#define	LABEL_SIZE 11				/* size of volume label */

/*
 * Parse out all the following forms as unsigned quantities. It would
 * not be unusual for them to be large enough that the high bit would be
 * set and they would then be misinterpreted as negative quantities.
 *
 * The values are:
 *	number of sectors
 *	big number of sectors (MSDOS4.0 and later)
 *	sectors/FAT
 *	big sectors/FAT (FAT32 only)
 */
#define	PC_NSEC(a)	((unsigned short)((a[20]<<8)+a[19]))
#define	PC_BIGNSEC(a)	((unsigned)((a[35]<<24)+(a[34]<<16)+(a[33]<<8)+a[32]))
#define	PC_SPF(a)	((unsigned short)(a[23]<<8)+a[22])
#define	PC_BIGSPF(a)	((unsigned)((a[39]<<24)+(a[38]<<16)+(a[37]<<8)+a[36]))

/*
 * Boolean macros
 */
#define	PC_EXTSIG(a)	(a[38] == 0x29)	/* do we have an extended BPB? */
#define	PC_EXTSIG32(a)	(a[66] == 0x29)	/* do we have FAT32 w/ extended BPB? */

int	vflag = 0;		/* verbose output */
int	errflag = 0;
extern	int	optind;
extern	int	errno;
char	*cbasename;
char	*special;
char	*fstype;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int	c;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	cbasename = argv[0];
	while ((c = getopt(argc, argv, "v")) != EOF) {
		switch (c) {

		case 'v':		/* dump super block */
			vflag++;
			break;

		case '?':
			errflag++;
		}
	}
	if (errflag) {
		usage();
		exit(31+1);
	}
	if (argc < optind) {
		usage();
		exit(31+1);
	}
	special = argv[optind];
	dumpfs(special);

	/* NOTREACHED */
}


usage()
{
	(void) fprintf(stderr, gettext("pcfs usage: fstyp [-v] special\n"));
}


dumpfs(name)
	char *name;
{
	char	buf[DEV_BSIZE];
	char	label[LABEL_SIZE+1];
	unsigned char	media;

	close(0);
	if (open(name, 0) != 0) {
		perror(name);
		exit(1);
	}
	llseek(0, (offset_t)0, 0);
	if (read(0, buf, sizeof (buf)) != sizeof (buf)) {

		/*
		 * As an fstyp command, this can get called for a device from
		 * any filesystem (or for that matter, a bogus device). The
		 * read() returns EINVAL when it's not a pcfs. In this case,
		 * don't print an error message.
		 */
		if (errno != EINVAL) {
			perror(name);
		}
		exit(1);
	}

	media = (unsigned char)PC_MEDIA(buf);
	if (!valid_media(media))
		exit(31+1);
	if (!well_formed(buf))
		exit(31+1);
	printf("%s\n", "pcfs");

	if (!vflag)
		exit(0);

	printf("Bytes Per Sector  %d\t\tSectors Per Cluster    %d\n",
		(unsigned short)PC_BPSEC(buf), (unsigned char)PC_SPC(buf));
	printf("Reserved Sectors  %d\t\tNumber of FATs         %d\n",
		(unsigned short)PC_RESSEC(buf), (unsigned char)PC_NFAT(buf));
	printf("Root Dir Entries  %d\t\tNumber of Sectors      %d\n",
		(unsigned short)PC_NROOTENT(buf), (unsigned short)PC_NSEC(buf));
	printf("Sectors Per FAT   %d\t\tSectors Per Track      %d\n",
		(unsigned short)PC_SPF(buf), (unsigned short)PC_SPT(buf));
	printf("Number of Heads   %d\t\tNumber Hidden Sectors  %d\n",
		(unsigned short)PC_NHEAD(buf), (unsigned short)PC_HIDSEC(buf));
	strncpy(label, PC_LAB_ADDR(buf), LABEL_SIZE);
	label[LABEL_SIZE+1] = 0;
	printf("Volume Label: %s\n", label);
	printf("Drive Number: 0x%x\n", (unsigned char)PC_DRVNUM(buf));
	printf("Media Type: 0x%x   ", media);

	switch (media) {
	case MD_FIXED:
		printf("\"Fixed\" Disk\n");
		break;
	case SS8SPT:
		printf("Single Sided, 8 Sectors Per Track\n");
		break;
	case DS8SPT:
		printf("Double Sided, 8 Sectors Per Track\n");
		break;
	case SS9SPT:
		printf("Single Sided, 9 Sectors Per Track\n");
		break;
	case DS9SPT:
		printf("Double Sided, 9 Sectors Per Track\n");
		break;
	case DS18SPT:
		printf("Double Sided, 18 Sectors Per Track\n");
		break;
	case DS9_15SPT:
		printf("Double Sided, 9-15 Sectors Per Track\n");
		break;
	default:
		printf("Unknown Media Type\n");
	}

	close(0);
	exit(0);
}

valid_media(media_type)
unsigned char media_type;
{
	switch (media_type) {

	case MD_FIXED:
	case SS8SPT:
	case DS8SPT:
	case SS9SPT:
	case DS9SPT:
	case DS18SPT:
	case DS9_15SPT:
		return (1);
	default:
		return (0);
	}
}

well_formed(char bs[])
{
	int fatmatch;

	if (PC_EXTSIG(bs)) {
		fatmatch = ((bs[PCFS_TYPESTRING_OFFSET16] == 'F' &&
			bs[PCFS_TYPESTRING_OFFSET16 + 1] == 'A' &&
			bs[PCFS_TYPESTRING_OFFSET16 + 2] == 'T') &&
			(PC_SPF(bs) > 0) &&
			((PC_NSEC(bs) == 0 && PC_BIGNSEC(bs) > 0) ||
			    PC_NSEC(bs) > 0));
	} else if (PC_EXTSIG32(bs)) {
		fatmatch = ((bs[PCFS_TYPESTRING_OFFSET32] == 'F' &&
			bs[PCFS_TYPESTRING_OFFSET32 + 1] == 'A' &&
			bs[PCFS_TYPESTRING_OFFSET32 + 2] == 'T') &&
			(PC_SPF(bs) == 0 && PC_BIGSPF(bs) > 0) &&
			((PC_NSEC(bs) == 0 && PC_BIGNSEC(bs) > 0) ||
			    PC_NSEC(bs) > 0));
	} else {
		fatmatch = (PC_NSEC(bs) > 0 && PC_SPF(bs) > 0);
	}

	return (fatmatch && PC_BPSEC(bs) > 0 && PC_BPSEC(bs) % 512 == 0 &&
		PC_SPC(bs) > 0 && PC_RESSEC(bs) >= 1 && PC_NFAT(bs) > 0);
}
