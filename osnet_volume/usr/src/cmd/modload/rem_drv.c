/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rem_drv.c	1.13	99/06/04 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <libintl.h>
#include <string.h>
#include <fcntl.h>
#include <sys/buf.h>
#include <sys/stat.h>
#include <sys/modctl.h>
#include <sys/wait.h>
#include <limits.h>
#include <signal.h>
#include <malloc.h>
#include <locale.h>
#include <ftw.h>
#include <sys/types.h>
#include <sys/mkdev.h>
#include "addrem.h"
#include "errmsg.h"

#define	FT_DEPTH	15	/* device tree depth for nftw() */

static void get_mod(char *, int *);
static int devfs_clean(void);
static void usage(void);
static int check_node(const char *, const struct stat *,
	int, struct FTW *);
static void signal_rtn();

static	major_t drv_major_no;	/* major number of driver being removed */

main(int argc, char *argv[])
{
	int opt;
	char *basedir = NULL, *driver_name = NULL;
	int server = 0;
	FILE *fp, *reconfig_fp;
	struct stat buf;
	int modid, found, fd;
	char maj_num[MAX_STR_MAJOR + 1];

	(void) setlocale(LC_ALL, "");
#if	!defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	/*  must be run by root */

	if (getuid() != 0) {
		(void) fprintf(stderr, gettext(ERR_NOT_ROOT));
		exit(1);
	}

	while ((opt = getopt(argc, argv, "b:")) != -1) {
		switch (opt) {
		case 'b' :
			server = 1;
			basedir = calloc(strlen(optarg) + 1, 1);
			if (basedir == NULL) {
				(void) fprintf(stderr, gettext(ERR_NO_MEM));
				exit(1);
			}
			(void) strcat(basedir, optarg);
			break;
		case '?' :
			usage();
			exit(1);
		}
	}

	if (argv[optind] != NULL) {
		driver_name = calloc(strlen(argv[optind]) + 1, 1);
		if (driver_name == NULL) {
			(void) fprintf(stderr, gettext(ERR_NO_MEM));
			exit(1);

		}
		(void) strcat(driver_name, argv[optind]);
		/*
		 * check for extra args
		 */
		if ((optind + 1) != argc) {
			usage();
			exit(1);
		}

	} else {
		usage();
		exit(1);
	}

	/* set up add_drv filenames */
	if ((build_filenames(basedir)) == ERROR) {
		exit(1);
	}

	(void) sigset(SIGINT, signal_rtn);
	(void) sigset(SIGHUP, signal_rtn);
	(void) sigset(SIGTERM, signal_rtn);

	/* must be only running version of add_drv/rem_drv */
	if ((fd = open(add_rem_lock, O_CREAT | O_EXCL | O_WRONLY,
	    S_IRUSR | S_IWUSR)) == -1) {
		if (errno == EEXIST) {
			(void) fprintf(stderr, gettext(ERR_PROG_IN_USE));
		} else {
			perror(gettext(ERR_LOCKFILE));
		}
		exit(1);
	} else {
		(void) close(fd);
	}

	if ((some_checking(1, 1)) == ERROR)
		err_exit();

	if (!server) {
		/* get the module id for this driver */
		get_mod(driver_name, &modid);

		/* module is installed */
		if (modid != -1) {
			if (modctl(MODUNLOAD, modid) < 0) {
				perror(NULL);
				(void) fprintf(stderr, gettext(ERR_MODUN),
				driver_name);
			}
		}
	}

	/* look up the major number of the driver being removed. */
	if ((found = get_major_no(driver_name, name_to_major)) == ERROR) {
		(void) fprintf(stderr, gettext(ERR_MAX_MAJOR), name_to_major);
		err_exit();
	}
	if (found == UNIQUE) {
		(void) fprintf(stderr, gettext(ERR_NOT_INSTALLED),
		    driver_name);
		err_exit();
	}

	/* this is passed to check_node() later */
	drv_major_no = (major_t)found;

	/* clean up /devices */
	if (devfs_clean() == ERROR) {
		(void) fprintf(stderr,
		    gettext(ERR_DEVFSCLEAN), driver_name);
	}

	/*
	 * add driver to rem_name_to_major; if this fails, don`t
	 * delete from name_to_major
	 */
	(void) sprintf(maj_num, "%d", found);

	if (append_to_file(driver_name, maj_num,
	    rem_name_to_major, ' ', " ") == ERROR) {
		(void) fprintf(stderr, gettext(ERR_NO_UPDATE),
		    rem_name_to_major);
		err_exit();
	}


	/*
	 * delete references to driver in add_drv/rem_drv database
	 */

	remove_entry(CLEAN_ALL, driver_name);

	exit_unlock();

	/*
	 * Create reconfigure file; want the driver removed
	 * from kernel data structures upon reboot
	 */
	reconfig_fp = fopen(RECONFIGURE, "a");
	(void) fclose(reconfig_fp);

	return (NOERR);
}

void
get_mod(char *driver_name, int *mod)
{
	struct modinfo	modinfo;

	modinfo.mi_id = -1;
	modinfo.mi_info = MI_INFO_ALL;
	do {
		/*
		 * If we are at the end of the list of loaded modules
		 * then set *mod = -1 and return
		 */
		if (modctl(MODINFO, 0, &modinfo) < 0) {
			*mod = -1;
			return;
		}

		*mod = modinfo.mi_id;
	} while (strcmp(driver_name, modinfo.mi_name) != 0);
}

/*
 * walk /devices looking for things to delete :-)
 */
int
devfs_clean()
{
	int  walk_flags = FTW_PHYS | FTW_MOUNT;

	if (nftw(devfs_root, check_node, FT_DEPTH, walk_flags) == -1) {
		perror("rem_drv: nftw");
		return (ERROR);
	} else
		return (NOERR);
}

/*
 * called by nftw() for each node encountered in /devices.
 * check to make sure if it is a device file and can be stat'd.
 * Try to match the major number of the node with the major number
 * of the driver being removed. If this fails, and the node
 * we are inspecting is the clone device, then check the minor
 * number of the clone node to see if it matches the major
 * number of the driver being deleted.  Unlink the node if
 * either of these two cases is true.
 */
/*ARGSUSED3*/
static int
check_node(const char *node, const struct stat *node_stat, int flags,
	struct FTW *ftw_info)
{
	major_t	dev_major_no;
	static major_t clone_major_no;
	char *clone = "clone";
	static int found_clone = -1;
	int unlink_node = 0;

	/* get the clone driver major number if we haven't already */
	if (found_clone == -1) {
		found_clone = get_major_no(clone, name_to_major);
		if ((found_clone == ERROR) || (found_clone == UNIQUE)) {
			fprintf(stderr, gettext(ERR_BAD_LINE),
			    name_to_major, clone);
			found_clone = 0;
		} else
			clone_major_no = (major_t)found_clone;
	}

	/* make sure this is a device file and can be stat'd */
	if ((flags != FTW_F) ||
	    (flags == FTW_NS) ||
	    (!(S_ISCHR(node_stat->st_mode) || S_ISBLK(node_stat->st_mode))))
		return (0);

	/* get the major number of this entry */
	dev_major_no = major(node_stat->st_rdev);

	if (dev_major_no == drv_major_no)
		unlink_node = 1;	/* major numbers match */
	else if ((found_clone) &&
	    (clone_major_no == dev_major_no) &&
	    (minor(node_stat->st_rdev) == drv_major_no)) {
		/*
		 * This is a clone device and the minor number of the
		 * /devices entry matches the major number of the driver
		 * being removed.
		 */
		unlink_node = 1;
	}

	if (unlink_node) {
		if (unlink(node)) {
			perror(NULL);
			(void) fprintf(stderr, gettext(ERR_UNLINK), node);
		}
	}
	return (0);
}

static void
usage()
{
	(void) fprintf(stderr, gettext(REM_USAGE1));
}

static void
signal_rtn()
{
	exit_unlock();
}
