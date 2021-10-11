/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)setboot.c 1.5     99/06/04 SMI"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/mnttab.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/openpromio.h>



/*
 * 128 is the size of the largest (currently) property name
 * 8192 - MAXPROPSIZE - sizeof (int) is the size of the largest
 * (currently) property value, viz. nvramrc.
 * the sizeof(u_int) is from struct openpromio
 */
#define	MAXPROPSIZE		128
#define	MAXVALSIZE		(8192 - MAXPROPSIZE - sizeof (u_int))

#define	BOOTDEV_PROP_NAME	"boot-device"

static int getbootdevname(char *, char *);
static int setprom(unsigned, unsigned, char *);
extern int devfs_dev_to_prom_name(char *, char *);

/*
 * Call getbootdevname() to get the absolute pathname of boot device
 * and call setprom() to set the boot-device variable.
 */
int
setboot(unsigned int yes, unsigned int verbose, char *fname)
{
	char	bdev[MAXPATHLEN];
	extern int errno;

	if (!getbootdevname(fname, bdev)) {
		(void) fprintf(stderr, "Cannot determine device name for %s\n",
			fname);
		return (errno);
	}

	return (setprom(yes, verbose, bdev));
}

/*
 * Read the mnttab and resolve the special device of the fs we are
 * interested in, into an absolute pathname
 */
static int
getbootdevname(char *bootfs, char *bdev)
{
	FILE *f;
	char *fname;
	char *devname;
	struct mnttab m;
	struct stat sbuf;
	int mountpt = 0;
	int found = 0;

	devname = bootfs;

	if (stat(bootfs, &sbuf) < 0) {
		perror("stat");
		return (0);
	}

	switch (sbuf.st_mode & S_IFMT) {
		case S_IFBLK:
			break;
		default:
			mountpt = 1;
			break;
	}

	if (mountpt) {
		fname = MNTTAB;
		f = fopen(fname, "r");
		if (f == NULL) {
			perror(fname);
			return (0);
		}

		while (getmntent(f, &m) == 0) {
			if (strcmp(m.mnt_mountp, bootfs))
				continue;
			else {
				found = 1;
				break;
			}
		}

		(void) fclose(f);

		if (!found) {
			return (0);
		}
		devname = m.mnt_special;
	}

	if (devfs_dev_to_prom_name(devname, bdev) != 0) {
		perror(devname);
		return (0);
	}

	return (1);
}

/*
 * setprom() - use /dev/openprom to read the "boot_device" variable and set
 * it to the new value.
 */
static int
setprom(unsigned yes, unsigned verbose, char *bdev)
{
	struct openpromio	*pio;
	int			fd;
	extern int 		errno;
	char			save_bootdev[MAXVALSIZE];

	if ((fd = open("/dev/openprom", O_RDWR)) < 0) {
		perror("Could not open openprom dev");
		return (errno);
	}

	pio = (struct openpromio *)malloc(sizeof (struct openpromio) +
					MAXVALSIZE + MAXPROPSIZE);

	if (pio == (struct openpromio *)NULL) {
		perror("Could not malloc memory for property name");
		return (errno);
	}

	pio->oprom_size = MAXVALSIZE;
	(void) strcpy(pio->oprom_array, BOOTDEV_PROP_NAME);

	if (ioctl(fd, OPROMGETOPT, pio) < 0) {
		perror("openprom getopt ioctl");
		return (errno);
	}

	/*
	 * save the existing boot-device, so we can use it if setting
	 * to new value fails.
	 */
	(void) strcpy(save_bootdev, pio->oprom_array);

	if (verbose) {
		(void) fprintf(stdout,
				"Current boot-device = %s\n", pio->oprom_array);
		(void) fprintf(stdout, "New boot-device = %s\n", bdev);
	}

	if (!yes) {
		(void) fprintf(stdout, "Do you want to change boot-device "
			"to the new setting? (y/n) ");
		switch (getchar()) {
			case 'Y':
			case 'y':
				break;
			default:
				return (0);
		}
	}

	/* set the new value for boot-device */

	pio->oprom_size = (int)strlen(BOOTDEV_PROP_NAME) + 1 +
				(int)strlen(bdev);

	(void) strcpy(pio->oprom_array, BOOTDEV_PROP_NAME);
	(void) strcpy(pio->oprom_array + (int)strlen(BOOTDEV_PROP_NAME) + 1,
					bdev);

	if (ioctl(fd, OPROMSETOPT, pio) < 0) {
		perror("openprom setopt ioctl");
		return (errno);
	}

	/* read back the value that was set */

	pio->oprom_size = MAXVALSIZE;
	(void) strcpy(pio->oprom_array, BOOTDEV_PROP_NAME);

	if (ioctl(fd, OPROMGETOPT, pio) < 0) {
		perror("openprom getopt ioctl");
		return (errno);
	}

	if (strcmp(bdev, pio->oprom_array)) {

		/* could not  set the new device name, set the old one back */

		perror("Could not set boot-device, reverting to old value");
		pio->oprom_size = (int)strlen(BOOTDEV_PROP_NAME) + 1 +
			(int)strlen(save_bootdev);

		(void) strcpy(pio->oprom_array, BOOTDEV_PROP_NAME);
			(void) strcpy(pio->oprom_array +
				(int)strlen(BOOTDEV_PROP_NAME) + 1,
				save_bootdev);

		if (ioctl(fd, OPROMSETOPT, pio) < 0) {
			perror("openprom setopt ioctl");
			return (errno);
		}

	}

	(void) close(fd);

	return (0);
}
