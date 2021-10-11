/*
 * Copyright 1999 Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * I18N message number ranges
 *  This file: 21000 - 21499
 *  Shared common messages: 1 - 1999
 */

#pragma ident	"@(#)qlgcupdate.c	1.5	99/11/05 SMI"

/*
 * Functions to support the download of FCode to PCI HBAs
 * Qlogic ISP21XX/22XX boards: FC100/P single port, FC100/2P dual port
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <limits.h>
#include <signal.h>
#include <dirent.h>
#include <nl_types.h>
#include <utmpx.h>
#include <sys/mnttab.h>
#include <sys/file.h>
#include <sys/mtio.h>
#include <sys/scsi/impl/uscsi.h>
#include <sys/fc4/fcal_linkapp.h>
#include <stgcom.h>
#include "ifpio.h"	/* sys/scsi/adapters/ifpio.h (from ifp package) */
#include "luxadm.h"

#define	HBA_MAX	128
#define	FCODE_HDR 200

/*	Global variables	*/
static char	fc_trans[] = "SUNW,ifp";	/* fibre channel transport */
static char	qlgc2100[] = "FC100/P";		/* product name for 2100   */
static char	qlgc2200[] = "FC100/2P";	/* product name for 2200   */
static char	pcibus_list[HBA_MAX][PATH_MAX];
/*	Internal functions	*/
static int	q_load_file(int, char *);
static int	q_getbootdev(uchar_t *);
static int	q_getdevctlpath(char *, int *);
static int	q_warn(int);
static int	q_findversion(int, int, uchar_t *, uint16_t *);
static int 	q_findfileversion(char *, uchar_t *, uint16_t *);

/*
 * Searches for and updates the cards.  This is the "main" function
 * and will give the output to the user by calling the subfunctions.
 * args: FCode file; if NULL only the current FCode version is printed
 */
int
q_qlgc_update(unsigned int verbose, char *file)
/*ARGSUSED*/
{
	int fd, fcode_fd, errnum = 0, devcnt = 0;
	uint_t i, fflag = 0, vflag = 0;
	uint16_t chip_id = 0, file_id = 0;
	uchar_t fcode_buf[FCODE_HDR];
	static uchar_t	bootpath[PATH_MAX];
	static uchar_t	version[MAXNAMELEN], version_file[MAXNAMELEN];
	char devpath[PATH_MAX];
	void	(*sigint)(); /* to store default SIGTERM setting */
	static struct	utmpx *utmpp = NULL; /* pointer for getutxent() */

	if (!file) {
		vflag++;
	} else {
		fflag++;

		/* check for a valid file */
		if ((fcode_fd = open(file, O_RDONLY)) < 0) {
			(void) fprintf(stderr,
			    MSGSTR(21000, "Error: Could not open %s\n"), file);
			return (1);
		}
		if (read(fcode_fd, fcode_buf, FCODE_HDR) != FCODE_HDR) {
			perror(MSGSTR(21001, "read"));
			(void) close(fcode_fd);
			return (1);
		}
		/*
		 * FCode header check - make sure it's PCI FCode
		 * Structure of FCode header (byte# refers to byte numbering
		 * in FCode spec, not the byte# of our fcode_buf buffer):
		 * header	byte 00    0x55  prom signature byte one
		 *		byte 01    0xaa  prom signature byte two
		 * data		byte 00-03 P C I R
		 */
		if ((fcode_buf[0x20] != 0x55) ||
		    (fcode_buf[0x21] != 0xaa) ||
		    (fcode_buf[0x3c] != 'P') ||
		    (fcode_buf[0x3d] != 'C') ||
		    (fcode_buf[0x3e] != 'I') ||
		    (fcode_buf[0x3f] != 'R')) {
			(void) fprintf(stderr, MSGSTR(21002,
		"Error: %s is not a valid FC100/P, FC100/2P FCode file.\n"),
			    file);
			(void) close(fcode_fd);
			return (1);
		}
		/* check for single user mode */
		while ((utmpp = getutxent()) != NULL) {
			if (strstr(utmpp->ut_line, "run-level") &&
			    (strcmp(utmpp->ut_line, "run-level S") &&
				strcmp(utmpp->ut_line, "run-level 1"))) {
				if (q_warn(1)) {
					(void) endutxent();
					(void) close(fcode_fd);
					return (0);
				}
				break;
			}
		}
		(void) endutxent();

		/* get bootpath */
		if (!q_getbootdev((uchar_t *)&bootpath[0]) &&
		    getenv("_LUX_D_DEBUG") != NULL) {
			(void) fprintf(stdout, "  Bootpath: %s\n", bootpath);
		}
	}
	/*
	 * Get count of, and names of PCI slots with ifp device control
	 * (devctl) nodes.  Search /devices.
	 */
	(void) strcpy(devpath, "/devices");
	if (q_getdevctlpath(devpath, (int *)&devcnt) == 0) {
		(void) fprintf(stdout, MSGSTR(21003,
		"\n  Found Path to %d FC100/P, FC100/2P Devices\n"), devcnt);
	} else {
		(void) fprintf(stderr, MSGSTR(21004,
	"Error: Could not get /devices path to FC100/P, FC100/2P Cards.\n"));
	}

	for (i = 0; i < devcnt; i++) {

		if (fflag && (strcmp(&pcibus_list[i][0],
		    (char *)bootpath) == 0)) {
			(void) fprintf(stderr,
			    MSGSTR(21005, "Ignoring %s (bootpath)\n"),
			    &pcibus_list[i][0]);
			continue;
		}

		(void) fprintf(stdout,
		MSGSTR(21006, "\n  Opening Device: %s\n"), &pcibus_list[i][0]);

		if ((fd = open(&pcibus_list[i][0], O_RDWR)) < 0) {
			(void) fprintf(stderr,
			    MSGSTR(21000, "Error: Could not open %s\n"),
			    &pcibus_list[i][0]);
			continue;
		}
		/*
		 * Check FCode version present on the adapter (at last boot)
		 */
		if (q_findversion(verbose, i, (uchar_t *)&version[0],
		    &chip_id) == 0) {
			if (strlen((char *)version) == 0) {
				(void) fprintf(stdout, MSGSTR(21007,
	"  Detected FCode Version:\tNo version available for this FCode\n"));
			} else {
			(void) fprintf(stdout, MSGSTR(21008,
			    "  Detected FCode Version:\t%s\n"), version);
			}
		} else {
			chip_id = 0x0;
		}

		if (fflag) {
			/*
			 * Check version of the supplied FCode file (once)
			 */
			if ((file_id != 0 && version_file != NULL) ||
			    (q_findfileversion((char *)
			    &fcode_buf[0], (uchar_t *)
			    &version_file[0], &file_id) == 0)) {
				(void) fprintf(stdout, MSGSTR(21009,
				    "  New FCode Version:\t\t%s\n"),
				    version_file);
			} else {
				return (1);
			}

			/*
			 * Load the New FCode
			 * Give warning if file doesn't appear to be correct
			 *
			 */
			if (chip_id == 0) {
				errnum = 2; /* can't get chip_id */
			} else if (chip_id - file_id != 0) {
				errnum = 3; /* file/card mismatch */
			} else {
				errnum = 0; /* everything is ok */
			}

			if (!q_warn(errnum)) {
				/* Disable user-interrupt Control-C */
				sigint =
				    (void (*)(int)) signal(SIGINT, SIG_IGN);

				/* Load FCode */
				(void) fprintf(stdout, MSGSTR(21010,
				    "  Loading FCode: %s\n"), file);

				if (q_load_file(fcode_fd,
				    &pcibus_list[i][0]) == 0) {
					(void) fprintf(stdout, MSGSTR(21011,
				"  Successful FCode download on %s\n"),
					    &pcibus_list[i][0]);
				} else {
					(void) fprintf(stderr, MSGSTR(21012,
				"Error: FCode download failed on %s\n"),
					    &pcibus_list[i][0]);
				}
				/* Restore SIGINT (user interrupt) setting */
				(void) signal(SIGINT, sigint);
			}
		}
			(void) close(fd);
	}
	(void) fprintf(stdout, "  ");
	(void) fprintf(stdout, MSGSTR(125, "Complete\n"));
	(void) close(fcode_fd);
	return (0);
}


/*
 * Retrieve the version banner from the card
 *    uses ioctl: FCIO_FCODE_MCODE_VERSION  	FCode revision
 */
static int
q_findversion(int verbose, int index, uchar_t *version, uint16_t *chip_id)
/*ARGSUSED*/
{
	int fd;
	struct 	ifp_fm_version *version_buffer = NULL;
	char	prom_ver[100] = {NULL};
	char	mcode_ver[100] = {NULL};


	if ((fd = open(&pcibus_list[index][0], O_RDWR)) < 0) {
		(void) fprintf(stderr,
		    MSGSTR(21000, "Error: Could not open %s\n"),
		    &pcibus_list[index][0]);
		return (1);
	}

	if ((version_buffer = (struct ifp_fm_version *)malloc(
		sizeof (struct ifp_fm_version))) == NULL) {
			(void) fprintf(stderr,
			    MSGSTR(21013, "Error: Memory allocation failed\n"));
		(void) close(fd);
		return (1);
	}

	version_buffer->fcode_ver = (char *)version;
	version_buffer->mcode_ver = mcode_ver;
	version_buffer->prom_ver = prom_ver;
	version_buffer->fcode_ver_len = MAXNAMELEN - 1;
	version_buffer->mcode_ver_len = 100;
	version_buffer->prom_ver_len = 100;

	if (ioctl(fd, FCIO_FCODE_MCODE_VERSION, version_buffer) < 0) {
		(void) fprintf(stderr, MSGSTR(21014,
		"Error: Driver interface FCIO_FCODE_MCODE_VERSION failed\n"));
		free(version_buffer);
		(void) close(fd);
		return (1);
	}

	version[version_buffer->fcode_ver_len] = '\0';

	/* Get type of card from product name in FCode version banner */
	if (strstr((char *)version, qlgc2100)) {
		*chip_id = 0x2100;
	} else if (strstr((char *)version, qlgc2200)) {
		*chip_id = 0x2200;
	} else {
		*chip_id = 0x0;
	}

	/* Need a way to get card MCODE (firmware) to track certain HW bugs */
	if (getenv("_LUX_D_DEBUG") != NULL) {
		(void) fprintf(stdout, "  Device %i: QLGC chip_id %x\n",
		    index+1, *chip_id);
		(void) fprintf(stdout, "  FCode:%s\n  MCODE:%s\n  PROM:%s\n",
		    (char *)version, mcode_ver, prom_ver);
	}

	free(version_buffer);
	(void) close(fd);
	return (0);
}

/*
 * Retrieve the version banner and file type (2100 or 2200) from the file
 */
static int
q_findfileversion(char *dl_fcode, uchar_t *version_file, uint16_t *file_id)
{
	int mark;
	char temp[4] = {NULL};

	/* Get file version from FCode, 0x2100 or 0x2200 */
	*file_id = dl_fcode[0x42] & 0xff;
	*file_id |= (dl_fcode[0x43] << 8) & 0xff00;

	/* Banner length varies; grab banner to end of date marker yr/mo/da */
	version_file[0] = '\0';
	for (mark = 111; mark < 191; mark++) {
		(void) strncpy(temp, (char *)&dl_fcode[mark], 4);
		if ((strncmp(&temp[0], "/", 1) == 0) &&
		    (strncmp(&temp[3], "/", 1) == 0)) {
			(void) strncat((char *)version_file,
			    (char *)&dl_fcode[mark], 6);
			break;
		}
		(void) strncat((char *)version_file, temp, 1);
	}
	return (0);
}


/*
 * Build a list of all the devctl entries for all the 2100/2200 based adapters
 */
static int
q_getdevctlpath(char *devpath, int *devcnt)
{
	struct stat	statbuf;
	struct dirent	*dirp = NULL;
	DIR		*dp = NULL;
	char		*ptr = NULL;
	int		err;

	if (lstat(devpath, &statbuf) < 0) {
		(void) fprintf(stderr,
		    MSGSTR(21016, "Error: %s lstat() error\n"), devpath);
		return (1);
	}

	if (strstr(devpath, fc_trans) && strstr(devpath, "devctl")) {
		(void) strcpy(pcibus_list[*devcnt], devpath);
		*devcnt += 1;
		return (0);
	}

	if (S_ISDIR(statbuf.st_mode) == 0) {
		/*
		 * not a directory so
		 * we don't care about it - return
		 */
		return (0);
	}

	/*
	 * It's a directory. Call ourself to
	 * traverse the path(s)
	 */
	ptr = devpath + strlen(devpath);
	*ptr++ = '/';
	*ptr = 0;

	/* Forget the /devices/pseudo/ directory */
	if (strcmp(devpath, "/devices/pseudo/") == 0) {
		return (0);
	}

	if ((dp = opendir(devpath)) == NULL) {
		(void) fprintf(stderr,
		    MSGSTR(21017, "Error: %s Can't read directory\n"), devpath);
		return (1);
	}

	while ((dirp = readdir(dp)) != NULL) {

		if (strcmp(dirp->d_name, ".") == 0 ||
		    strcmp(dirp->d_name, "..") == 0) {
			continue;
		}
		(void) strcpy(ptr, dirp->d_name); /* append name */
		err = q_getdevctlpath(devpath, devcnt);
	}

	if (closedir(dp) < 0) {
		(void) fprintf(stderr,
		MSGSTR(21018, "Error: Can't close directory %s\n"), devpath);
		return (1);
	}
	return (err);
}

/*
 * Get the boot device.  Cannot load FCode to current boot device.
 * Boot devices under volume management will prompt a warning.
 */
static int
q_getbootdev(uchar_t *bootpath)
{
	struct mnttab mp;
	struct mnttab mpref;
	FILE *fp = NULL;
	static char buf[BUFSIZ];
	char *p = NULL, *p1 = NULL;  /* p = full device, p1 = chunk to rm */
	char *slot = ":devctl";
	char *root = "/";

	if ((fp = fopen(MNTTAB, "r")) == NULL) {
		(void) fprintf(stderr,
		    MSGSTR(21000, "Error: Could not open %s\n"), MNTTAB);
		return (1);
	}

	mntnull(&mpref);
	mpref.mnt_mountp = (char *)root;

	if (getmntany(fp, &mp, &mpref) != 0 ||
	    mpref.mnt_mountp == NULL) {
		(void) fprintf(stderr, MSGSTR(21019,
		    "Error: Cannot get boot device, check %s.\n"), MNTTAB);
		(void) fclose(fp);
		return (1);
	}
	(void) fclose(fp);

	/*
	 * If we can't get a link, we may be dealing with a volume mgr
	 * so give a warning.  If a colon is present, we likely have a
	 * non-local disk or cd-rom, so no warning is necessary.
	 * e.g. /devices/pci@1f,4000/scsi@3/sd@6,0:b (cdrom, no link) or
	 * 	storage-e4:/blah/blah remote boot server
	 */
	if (readlink(mp.mnt_special, buf, BUFSIZ) < 0) {
		if (strstr(mp.mnt_special, ":") == NULL) {
			(void) fprintf(stderr, MSGSTR(21020,
	"\nWarning: Cannot read boot device link, check %s.\n"), MNTTAB);
			(void) fprintf(stderr, MSGSTR(21021,
	"Do not upgrade FCode on adapters controlling the boot device.\n"));
		}
		return (1);
	}
	/*
	 * Copy boot device path to bootpath.  First remove leading
	 * path junk (../../..) then if it's an ifp device, chop off
	 * the disk and add the devctl to the end of the path.
	 */
	if (p = strstr(buf, "/devices")) {
		if (strstr(buf, fc_trans) != NULL) {
			p1 = strrchr(p, '/');
			*p1 = '\0';
		}
	}
	(void) strcpy((char *)bootpath, (char *)p);
	if (p1) {
		(void) strcat((char *)bootpath, slot);
	}
	return (0);
}

/*
 * Load FCode to card.
 *    uses ioctl: IFPIO_FCODE_DOWNLOAD
 */
static int
q_load_file(int fcode_fd, char *device)
{
	static int	dev_fd, fcode_size;
	struct stat	stat;
	ifp_download_t	*download_p = NULL;
	uint16_t file_id = 0;

	if (lseek(fcode_fd, 0, SEEK_SET) == -1) {
		perror(MSGSTR(21022, "seek"));
		(void) close(fcode_fd);
		return (1);
	}
	if (fstat(fcode_fd, &stat) == -1) {
		perror(MSGSTR(21023, "fstat"));
		(void) close(fcode_fd);
		return (1);
	}

	fcode_size = stat.st_size;

	if ((download_p = (ifp_download_t *)malloc(
		sizeof (ifp_download_t) + fcode_size)) == NULL) {
		(void) fprintf(stderr,
		    MSGSTR(21013, "Error: Memory allocation failed\n"));
		(void) close(fcode_fd);
		return (1);
	}
	if (read(fcode_fd, download_p->dl_fcode, fcode_size) != fcode_size) {
		perror(MSGSTR(21001, "read"));
		free(download_p);
		(void) close(fcode_fd);
		return (1);
	}

	download_p->dl_fcode_len = fcode_size;

	file_id = download_p->dl_fcode[0x42] & 0xff;
	file_id |= (download_p->dl_fcode[0x43] << 8) & 0xff00;
	download_p->dl_chip_id = file_id;

	if ((dev_fd = open(device, O_RDWR)) < 0) {
		(void) fprintf(stderr,
		    MSGSTR(21000, "Error: Could not open %s\n"), device);
		free(download_p);
		return (1);
	}

	if (ioctl(dev_fd, IFPIO_FCODE_DOWNLOAD, download_p) < 0) {
		(void) fprintf(stderr, MSGSTR(21024,
		    "Error: Driver interface IFPIO_FCODE_DOWNLOAD failed\n"));
		free(download_p);
		(void) close(dev_fd);
		return (1);
	}
	free(download_p);
	(void) close(dev_fd);
	return (0);
}

/*
 * Issue warning strings and loop for Yes/No user interaction
 *    err# 0 -- we're ok, warn for pending FCode load
 *         1 -- not in single user mode
 *         2 -- can't get chip_id
 *         3 -- card and file have same type (2100/2200)
 */
static int
q_warn(int errnum)
{
	char input[1024];
	input[0] = '\0';

	if (errnum == 1) {
		(void) fprintf(stderr, MSGSTR(21025,
		    "\nWarning: System is not in single-user mode.\n"));
		(void) fprintf(stderr, MSGSTR(21026,
	"Loading FCode will reset the adapter and terminate I/O activity\n"));
	} else {
		if (errnum == 2) {
		(void) fprintf(stderr, MSGSTR(21027,
		"\nWarning: Installed FCode has a blank or unrecognized"
		" version banner.\n"));
		} else if (errnum == 3) {
		(void) fprintf(stderr, MSGSTR(21028,
		"\nWarning: New FCode file version does not match this"
		" board type.\n"));
		}
		(void) fprintf(stderr, MSGSTR(21029,
		"\nWARNING!! This program will update the FCode in this"
		" FC100/PCI device.\n"));
		(void) fprintf(stderr, MSGSTR(21030,
		"This may take a few (5) minutes. Please be patient.\n"));
	}

loop1:
	(void) fprintf(stderr, MSGSTR(21031,
		"Do you wish to continue ? (y/n) "));

	(void) gets(input);

	if ((strcmp(input, MSGSTR(21032, "y")) == 0) ||
			(strcmp(input, MSGSTR(40, "yes")) == 0)) {
		return (0);
	} else if ((strcmp(input, MSGSTR(21033, "n")) == 0) ||
			(strcmp(input, MSGSTR(45, "no")) == 0)) {
		(void) fprintf(stderr,
		    MSGSTR(21034, "Not Downloading FCode\n"));
		return (1);
	} else {
		(void) fprintf(stderr, MSGSTR(21035, "Invalid input\n"));
		goto loop1;
	}
}
