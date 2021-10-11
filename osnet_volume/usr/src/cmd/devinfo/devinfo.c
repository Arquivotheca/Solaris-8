/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *	Two output fields under the -i option will always be
 *	output as zero, since they are not supported by Sun:
 *		Software version, and
 *		Drive id number.
 *	AT&T filled these 2 fields with data from their "pdsector",
 *	which Sun doesn't support per se.
 */

#pragma ident	"@(#)devinfo.c	1.7	97/05/05 SMI"	/* SVr4.0  1.3.1.1 */

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dkio.h>
#include <sys/vtoc.h>
#include <sys/mkdev.h>
#include <errno.h>

#define	DRERR	2
#define	OPENERR	2

/*
 * Standard I/O file descriptors.
 */
#define	STDOUT	1		/* Standard output */
#define	STDERR	2		/* Standard error */

static	void	partinfo(int fd, char *device);
static	void	devinfo(struct dk_geom *geom, int fd, char *device);
static	int	readgeom(int fd, char *name, struct dk_geom *geom);
static	int	readvtoc(int fd, char *name, struct vtoc *vtoc);
static	int	warn(char *what, char *why);
static	void	usage(void);

main(int argc, char **argv)
{
	struct dk_geom  geom;
	int geo;
	int errflg, iflg, pflg, fd, c;
	char *device;

	iflg = 0;
	pflg = 0;
	errflg = 0;
	while ((c = getopt(argc, argv, "i:p:")) != EOF) {
		switch (c) {
			case 'i':
				iflg++;
				device = argv[2];
				break;
			case 'p':
				pflg++;
				device = argv[2];
				break;
			case '?':
				errflg++;
				break;
			default:
				errflg++;
				break;
		}
		if (errflg)
			usage();
	}
	if ((optind > argc) || (optind == 1) || (pflg && iflg))
		usage();

	if ((fd = open(device, O_RDONLY)) < 0) {
		(void) fprintf(stderr, "devinfo: %s: %s\n",
			device, strerror(errno));
		exit(OPENERR);
	}

	geo = (readgeom(fd, device, &geom) == 0);
	if (!geo) {
		(void) close(fd);
		exit(DRERR);
	}
	if (iflg)
		devinfo(&geom, fd, device);
	if (pflg)
		partinfo(fd, device);
	(void) close(fd);
	return (0);
}

static void
partinfo(int fd, char *device)
{
	int i;
	unsigned int startblock, noblocks, flag, tag;
	int	slice;
	major_t maj;
	minor_t min;
	struct vtoc vtdata;
	struct stat statbuf;

	startblock = 0;
	noblocks = 0;

	if ((slice = readvtoc(fd, device, &vtdata)) < 0)
		exit(DRERR);
	i = stat(device, &statbuf);
	if (i < 0)
		exit(DRERR);
	maj = major(statbuf.st_rdev);
	min = minor(statbuf.st_rdev);
	startblock = vtdata.v_part[slice].p_start;
	noblocks = vtdata.v_part[slice].p_size;
	flag = vtdata.v_part[slice].p_flag;
	tag = vtdata.v_part[slice].p_tag;
	(void) printf("%s	%0lx	%0lx	%d	%d	%x	%x\n",
		device, maj, min, startblock, noblocks, flag, tag);
}

static void
devinfo(struct dk_geom *geom, int fd, char *device)
{
	int i;
	unsigned int nopartitions, sectorcyl, bytes;
	struct vtoc vtdata;
/*
	unsigned int version = 0;
	unsigned int driveid = 0;
*/

	nopartitions = 0;
	sectorcyl = 0;
	bytes = 0;

	if (readvtoc(fd, device, &vtdata) < 0)
		exit(DRERR);
	sectorcyl = geom->dkg_nhead  *  geom->dkg_nsect;
	bytes = vtdata.v_sectorsz;
/*
 *	these are not supported by Sun.
 *
	driveid = osect0->newsect0.pdinfo.driveid;
	version = osect0->newsect0.pdinfo.version;
 */
	for (i = 0; i < V_NUMPAR; i++)	{
		if (vtdata.v_part[i].p_size != 0x00)
			nopartitions++;
	}
/*
	(void) printf("%s	%0x	%0x	%d	%d	%d\n",
		device, version, driveid, sectorcyl, bytes, nopartitions);
*/
	(void) printf("%s	%0x	%0x	%d	%d	%d\n",
		device, 0, 0, sectorcyl, bytes, nopartitions);
}


/*
 * readgeom()
 *
 * Read the disk geometry.
 */
static int
readgeom(int fd, char *name, struct dk_geom *geom)
{
	if (ioctl(fd, DKIOCGGEOM, geom))
		return (warn(name, "Unable to read Disk geometry"));
	return (0);
}



/*
 * readvtoc()
 *
 * Read a partition map.
 */
static int
readvtoc(int fd, char *name, struct vtoc *vtoc)
{
	int	retval;

	retval = read_vtoc(fd, vtoc);

	switch (retval) {
		case (VT_ERROR):
			return (warn(name, strerror(errno)));
		case (VT_EIO):
			return (warn(name, "I/O error accessing VTOC"));
		case (VT_EINVAL):
			return (warn(name, "Invalid field in VTOC"));
		}

	return (retval);
}


/*
 * warn()
 *
 * Print an error message. Always returns -1.
 */
static int
warn(char *what, char *why)
{
	static char	myname[]  = "devinfo";
	static char	between[] = ": ";
	static char	after[]   = "\n";

	(void) write(STDERR, myname, (uint)strlen(myname));
	(void) write(STDERR, between, (uint)strlen(between));
	(void) write(STDERR, what, (uint)strlen(what));
	(void) write(STDERR, between, (uint)strlen(between));
	(void) write(STDERR, why, (uint)strlen(why));
	(void) write(STDERR, after, (uint)strlen(after));
	return (-1);
}

static void
usage(void)
{
	(void) fprintf(stderr, "Usage: devinfo -p device\n"
		"       devinfo -i device \n");
	exit(2);
}
