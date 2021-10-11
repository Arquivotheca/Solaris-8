/*
 *   diskscan:
 *   performs a verification pass over a device specified on command line;
 *   display progress on stdout, and print bad sector numbers to stderr
 */


/*
 * Copyrighted as an unpublished work.
 * (c) Copyright 1989 Sun Microsystems, Inc.
 * All rights reserved.
 *
 * RESTRICTED RIGHTS
 *
 * These programs are supplied under a license.  They may be used,
 * disclosed, and/or copied only as permitted under such license
 * agreement.  Any copy must contain the above copyright notice and
 * this restricted rights notice.  Use, copying, and/or disclosure
 * of the programs is strictly prohibited unless otherwise provided
 * in the license agreement.
 */

#ident "@(#)diskscan.c	1.6	92/12/17 SMI"


#include <stdio.h>
#include <fcntl.h>
#include <memory.h>
#include <ctype.h>
#include <malloc.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/vtoc.h>
#include <sys/dkio.h>

void	verexit();	/* signal handler and exit routine	*/
void	report();	   /* tell user how we're getting on */

#define TRUE		1
#define FALSE		0
#define VER_WRITE	1
#define VER_READ	2

char	*progname;

struct  dk_geom	dkg;		/* physical device boot info */
char	*buf;			/* buffer used to read in disk structs. */

char	replybuf[64];		/* used for user replies to questions */

daddr_t unix_base;		  /* first sector of UNIX System partition */
daddr_t unix_size;		  /* # sectors in UNIX System partition */

long	numbadrd = 0;	   /* number of bad sectors on read */
long	numbadwr = 0;	   /* number of bad sectors on write */
char	eol = '\n';		 /* end-of-line char (if -n, we set to '\n') */
int	print_warn = 1;	/* should the warning message be printed? */

int do_scan = VER_READ;

void main(argc,argv)
int argc;
char *argv[];
{
	extern char	 *optarg;
	extern int	  optind;
	int		devfd;			/* device file descriptor */
	struct stat 	statbuf;
	struct part_info	part_info;
	int		c;
	int		errflag = 0;
	char		*device;

	progname = argv[0];

	/* Don't buffer stdout - we don't want to see bursts */

	setbuf(stdout, NULL);

	while ((c = getopt(argc, argv, "Wny")) != -1)
	{
		switch (c)
		{
		case 'W':
			do_scan = VER_WRITE;
			break;

		case 'n':
			eol = '\r';
			break;

		case 'y':
			print_warn = 0;
			break;

		default:
			++errflag;
			break;
		}
	}

	if ((argc - optind) < 1)
		errflag++;

	if (errflag)
	{
		fprintf(stderr, "\nUsage: %s [-W] [-n] [-y] <phys_device_name>\n", progname);
		exit(1);
	}

	device = argv[optind];

	if (stat(device, &statbuf)) {
		fprintf(stderr, "%s: invalid device %s, stat failed\n", progname, device);
		perror("");
		exit(4);
	}
	if ((statbuf.st_mode & S_IFMT) != S_IFCHR) {
		fprintf(stderr, "%s: device %s is not character special\n", progname, device);
		exit(5);
	}
	if ((devfd=open(device, O_RDWR)) == -1) {
		fprintf(stderr, "%s: open of %s failed\n", progname ,device);
		perror("");
		exit(8);
	}

	if ((ioctl (devfd, DKIOCGGEOM, &dkg)) == -1) {
		fprintf(stderr, "%s: unable to get disk geometry.\n", progname);
		perror("");
		exit(9);
	}
	if ((ioctl (devfd, DKIOCPARTINFO, &part_info)) == -1) {
		fprintf(stderr, "%s: unable to get partition info.\n",
				progname);
		perror("");
		exit(9);
	}

	unix_base = part_info.p_start;
	unix_size = part_info.p_length;

	scandisk(device, devfd, do_scan);
	exit(0);
}

/*
 *  scandisk:
 *	  attempt to read every sector of the drive;
 *	  display bad sectors found on stderr
 */

scandisk(device, devfd, writeflag)
char	*device;
int	devfd;
int	writeflag;
{
	int	 trksiz = NBPSCTR * dkg.dkg_nsect;
	char	*verbuf;
	daddr_t cursec;
	int	 cylsiz =  dkg.dkg_nsect * dkg.dkg_nhead;
	int	 i;
	char	*rptr;
	long	tmpend = 0;
	long	tmpsec = 0;

/* #define LIBMALLOC */

#ifdef LIBMALLOC

	extern int  mallopt();

	/* This adds 5k to the binary, but it's a lot prettier */

	if ( mallopt(M_GRAIN, 0x200) ) /* make track buffer sector aligned */
	{
		perror("mallopt");
		exit(1);
	}
	if ( (verbuf=malloc(NBPSCTR * dkg.dkg_nsect))==(char *)NULL)
	{
		perror("malloc");
		exit(1);
	}

#else

	if ( (verbuf=malloc(0x200 + NBPSCTR * dkg.dkg_nsect))==(char *)NULL)
	{
		perror("malloc");
		exit(1);
	}
	verbuf = (char *)( ((unsigned long)verbuf + 0x00000200) & 0xfffffe00);

#endif

	/* write pattern in track buffer */

	for (i = 0; i < trksiz; i++)
	verbuf[i] = 0xe5;

	/* Turn off retry, and set trap to turn them on again */

	signal(SIGINT, verexit);
	signal(SIGQUIT, verexit);

	if (writeflag == VER_READ)
		goto do_readonly;

	/*
	 *   display warning only if -n arg not passed
	 *   (otherwise the UI system will take care of it)
	 */

	if (print_warn == 1)
	{
		printf("\nCAUTION: ABOUT TO DO DESTRUCTIVE WRITE ON %s\n",
					device);
		printf("	 THIS WILL DESTROY ANY DATA YOU HAVE ON\n");
		printf("	 THAT PARTITION OR SLICE.\n");
		printf("Do you want to continue (y/n)? ");

		rptr = gets(replybuf);
		if (!rptr ||  !((replybuf[0] == 'Y') || (replybuf[0] == 'y')))
			exit (10);
	}

	for (cursec = 0; cursec < unix_size; cursec +=  dkg.dkg_nsect)
	{
		if (lseek(devfd, (long) cursec * NBPSCTR , 0) == -1)
		{
		fprintf(stderr, "Error seeking sector %ld Cylinder %ld\n", cursec, cursec / cylsiz);
			verexit(1);
		}

		/*
		 *  verify sector at a time only when the whole track write fails;
		 *  (if we write a sector at a time, it takes forever)
		 */

	report("Writing", cursec);

		if (write(devfd, verbuf, trksiz) != trksiz)
		{
			tmpend = cursec +  dkg.dkg_nsect;
			for (tmpsec = cursec; tmpsec < tmpend; tmpsec++)
			{
				/*
				 *  try writing to it once; if this fails,
				 *  then announce the sector bad on stderr
				 */

				if (lseek(devfd,(long)tmpsec * NBPSCTR ,0) == -1)
				{
			fprintf(stderr, "Error seeking sector %ld Cylinder %ld\n",
					 tmpsec, cursec / cylsiz);
					verexit(1);
				}

		report("Writing", tmpsec);

				if (write(devfd,verbuf,NBPSCTR ) != NBPSCTR )
				{
					fprintf(stderr, "%ld\n", tmpsec + unix_base);
					numbadwr++;
				}
			}
		}
	}

	putchar(eol);

do_readonly:

	for (cursec = 0; cursec < unix_size; cursec +=  dkg.dkg_nsect)
	{
	if (lseek(devfd, (long) cursec * NBPSCTR , 0) == -1)
	{
		fprintf(stderr, "Error seeking sector %ld   Cylinder %ld\n",
				cursec, cursec / cylsiz);
		verexit(1);
	}

	/*
	 *  read a sector at a time only when the whole track write fails;
	 *  (if we do a sector at a time read, it takes forever)
	 */

	report("Reading", cursec);
	if (read(devfd, verbuf, trksiz) != trksiz)
	{
		tmpend = cursec +  dkg.dkg_nsect;
		for (tmpsec = cursec; tmpsec < tmpend; tmpsec++)
		{
		if (lseek(devfd,(long)tmpsec * NBPSCTR ,0) == -1)
		{
			fprintf(stderr, "Error seeking sector %ld Cylinder %ld\n",
					 tmpsec, cursec / cylsiz);
			verexit(1);
		}
		report("Reading", tmpsec);
		if (read(devfd,verbuf,NBPSCTR ) != NBPSCTR )
		{
			fprintf(stderr, "%ld\n", tmpsec + unix_base);
			numbadrd++;
		}
		}
	}

	}
	printf("%c%c======== Diskscan complete ========%c", eol, eol, eol);


	if ((numbadrd > 0) || (numbadwr > 0))
	{
	printf("%cFound %d bad sector(s) on read, %d bad sector(s) on write%c",
		eol, numbadrd, numbadwr, eol);
	}
}

void verexit(code)
int code;
{
	printf("\n");
	exit ( code );
}


/*
 *   report where we are...
 */

void report(what, sector)
char *what;
int sector;
{
	printf("%s sector %-7ld of %-7ld%c", what, sector, unix_size, eol);
}
