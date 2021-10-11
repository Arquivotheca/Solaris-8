/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)add_drv.c	1.33	99/11/19 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <wait.h>
#include <unistd.h>
#include <libintl.h>
#include <sys/modctl.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <locale.h>
#include <ftw.h>
#include <sys/sunddi.h>
#include <libdevinfo.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include "addrem.h"
#include "errmsg.h"

/*
 * globals needed for libdevinfo - there is no way to pass
 * private data to the find routine.
 */
struct dev_list {
	int clone;
	char *dev_name;
	char *driver_name;
	struct dev_list *next;
};

static char *new_drv;
static struct dev_list *conflict_lst = NULL;

static int module_not_found(char *, char *);
static int unique_driver_name(char *, char *, int *);
static int check_perm_opts(char *);
static int update_name_to_major(char *, major_t *, int);
static int fill_n2m_array(char *, char **, int *);
static int aliases_unique(char *);
static int unique_drv_alias(char *);
static int config_driver(char *, major_t, char *, char *, int, int, int);
static void usage();
static int update_minor_perm(char *, char *);
static int update_driver_classes(char *, char *);
static int update_driver_aliases(char *, char *);
static int do_the_update(char *, char *);
static void signal_rtn();
static int exec_command(char *, char **);

static int drv_name_conflict(di_node_t);
static int devfs_node(di_node_t node, void *arg);
static int drv_name_match(char *, int, char *, char *);
static void print_drv_conflict_info(int);
static void check_dev_dir(int);
static int dev_node(const char *, const struct stat *, int, struct FTW *);
static void free_conflict_list(struct dev_list *);
static int clone(di_node_t node);
static int check_space_within_quote(char *);

int
main(int argc, char *argv[])
{
	int opt;
	major_t major_num;
	char driver_name[FILENAME_MAX + 1];
	char *path_driver_name;
	char *perms = NULL;
	char *aliases = NULL;
	char *classes = NULL;
	int noload_flag = 0;
	int verbose_flag = 0;
	int force_flag = 0;
	int i_flag = 0;
	int c_flag = 0;
	int m_flag = 0;
	int cleanup_flag = 0;
	int server = 0;
	char *basedir = NULL;
	int is_unique;
	FILE *reconfig_fp;
	char basedir_rec[PATH_MAX + FILENAME_MAX + 1];
	char *slash;
	int pathlen;
	int x;
	int conflict;
	int fd;
	di_node_t root_node;	/* for device tree snapshot */

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

	while ((opt = getopt(argc, argv, "vfm:ni:b:c:")) != EOF) {
		switch (opt) {
		case 'm' :
			m_flag = 1;
			perms = calloc(strlen(optarg) + 1, 1);
			if (perms == NULL) {
				(void) fprintf(stderr, gettext(ERR_NO_MEM));
				exit(1);
			}
			(void) strcat(perms, optarg);
			break;
		case 'f':
			force_flag++;
			break;
		case 'v':
			verbose_flag++;
			break;
		case 'n':
			noload_flag++;
			break;
		case 'i' :
			i_flag = 1;
			aliases = calloc(strlen(optarg) + 1, 1);
			if (aliases == NULL) {
				(void) fprintf(stderr, gettext(ERR_NO_MEM));
				exit(1);
			}
			(void) strcat(aliases, optarg);
			if (check_space_within_quote(aliases) == ERROR) {
				(void) fprintf(stderr, gettext(ERR_NO_SPACE),
					aliases);
				exit(1);
			}
			break;
		case 'b' :
			server = 1;
			basedir = calloc(strlen(optarg) + 1, 1);
			if (basedir == NULL) {
				(void) fprintf(stderr, gettext(ERR_NO_MEM));
				exit(1);
			}
			(void) strcat(basedir, optarg);
			break;
		case 'c':
			c_flag = 1;
			classes = strdup(optarg);
			if (classes == NULL) {
				(void) fprintf(stderr, gettext(ERR_NO_MEM));
				exit(1);
			}
			break;
		case '?' :
		default:
			usage();
			exit(1);
		}
	}


	if (argv[optind] != NULL) {
		path_driver_name = calloc(strlen(argv[optind]) + 1, 1);
		if (path_driver_name == NULL) {
			(void) fprintf(stderr, gettext(ERR_NO_MEM));
			exit(1);

		}
		(void) strcat(path_driver_name, argv[optind]);
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

	/* get module name from path */

	/* if <path>/<driver> ends with slash; strip off slash/s */

	pathlen = strlen(path_driver_name);
	for (x = 1; ((path_driver_name[pathlen - x ] == '/') &&
	    (pathlen != 1)); x++) {
		path_driver_name[pathlen - x] = '\0';
	}

	slash = strrchr(path_driver_name, '/');

	if (slash == NULL) {
		(void) strcpy(driver_name, path_driver_name);

	} else {
		(void) strcpy(driver_name, ++slash);
		if (driver_name[0] == '\0') {
			(void) fprintf(stderr, gettext(ERR_NO_DRVNAME),
			    path_driver_name);
			usage();
			exit(1);
		}
	}
	new_drv = driver_name;

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
	}
	(void) close(fd);

	if ((some_checking(m_flag, i_flag)) == ERROR)
		err_exit();

	/*
	 * check validity of options
	 */
	if (m_flag) {
		if ((check_perm_opts(perms)) == ERROR)
			err_exit();
	}

	if (i_flag) {
		if (aliases != NULL)
			if ((aliases_unique(aliases)) == ERROR)
				err_exit();
	}


	if ((unique_driver_name(driver_name, name_to_major,
	    &is_unique)) == ERROR)
		err_exit();

	if (is_unique == NOT_UNIQUE) {
		(void) fprintf(stderr, gettext(ERR_NOT_UNIQUE), driver_name);
		err_exit();
	}

	if (!server) {
		if ((module_not_found(driver_name, path_driver_name))
		    == ERROR) {
			perror(NULL);
			(void) fprintf(stderr, gettext(ERR_NOMOD), driver_name);
			err_exit();
		}
		/*
		 * Check for a more specific driver conflict - see
		 * PSARC/1995/239
		 * Note that drv_name_conflict() can return -1 for error
		 * or 1 for a conflict.  Since the default is to fail unless
		 * the -f flag is specified, we don't bother to differentiate.
		 */
		if ((root_node = di_init("/", DINFOSUBTREE | DINFOMINOR))
		    == DI_NODE_NIL) {
			(void) fprintf(stderr, gettext(ERR_DEVTREE));
			conflict = -1;
		} else {
			conflict = drv_name_conflict(root_node);
			di_fini(root_node);
		}

		if (conflict) {
			/*
			 * if the force flag is not set, we fail here
			 */
			if (!force_flag) {
				(void) fprintf(stderr,
				    gettext(ERR_INSTALL_FAIL), driver_name);
				(void) fprintf(stderr, "Device managed by "
				    "another driver.\n");
				if (verbose_flag)
					print_drv_conflict_info(force_flag);
				err_exit();
			}
			/*
			 * The force flag was specified so we print warnings
			 * and install the driver anyways
			 */
			if (verbose_flag)
				print_drv_conflict_info(force_flag);
			free_conflict_list(conflict_lst);
		}
	}

	if ((update_name_to_major(driver_name, &major_num, server)) == ERROR) {
		err_exit();
	}

	cleanup_flag |= CLEAN_NAM_MAJ;


	if (m_flag) {
		if (update_minor_perm(driver_name, perms) == ERROR) {
			cleanup_flag |= CLEAN_MINOR_PERM;
			remove_entry(cleanup_flag, driver_name);
			err_exit();
		}
		cleanup_flag |= CLEAN_MINOR_PERM;
	}

	if (i_flag) {
		if (update_driver_aliases(driver_name, aliases) == ERROR) {
			cleanup_flag |= CLEAN_DRV_ALIAS;
			remove_entry(cleanup_flag, driver_name);
			err_exit();

		}
		cleanup_flag |= CLEAN_DRV_ALIAS;
	}

	if (c_flag) {
		if (update_driver_classes(driver_name, classes) == ERROR) {
			cleanup_flag |= CLEAN_DRV_CLASSES;
			remove_entry(cleanup_flag, driver_name);
			err_exit();

		}
		cleanup_flag |= CLEAN_DRV_CLASSES;
	}

	if (server) {
		(void) fprintf(stderr, gettext(BOOT_CLIENT));

		/*
		 * create /reconfigure file so system reconfigures
		 * on reboot
		 */
		(void) strcpy(basedir_rec, basedir);
		(void) strcat(basedir_rec, RECONFIGURE);
		reconfig_fp = fopen(basedir_rec, "a");
		(void) fclose(reconfig_fp);
	} else {
		/*
		 * paranoia - if we crash whilst configuring the driver
		 * this might avert possible file corruption.
		 */
		sync();

		if (config_driver(driver_name, major_num,
		    aliases, classes, cleanup_flag, noload_flag,
			verbose_flag) == ERROR) {
			err_exit();
		}
	}

	exit_unlock();

	if (verbose_flag) {
		(void) fprintf(stderr, gettext(DRIVER_INSTALLED), driver_name);
	}

	return (NOERR);
}

/*
 * ISA specific directories must be listed first. The
 * sequence defines the search order.
 */
static char *moddir[] = {
	"sparcv9",
	"",
};
static int nmodirs = sizeof (moddir) / sizeof (moddir[0]);

int
module_not_found(char *module_name, char *path_driver_name)
{
	char path[MAXPATHLEN + FILENAME_MAX + 1];
	struct stat buf;
	struct stat ukdbuf;
	char data [MAXMODPATHS];
	char usr_kernel_drv[FILENAME_MAX + 17];
	char *next = data;
	int i;

	/*
	 * if path
	 * 	if (path/module doesn't exist AND
	 *	 /usr/kernel/drv/<modir>/module doesn't exist)
	 *	error msg
	 *	exit add_drv
	 */
	if (strcmp(module_name, path_driver_name)) {
		if ((stat(path_driver_name, &buf) == 0) &&
		    ((buf.st_mode & S_IFMT) == S_IFREG)) {
			return (NOERR);
		}
		for (i = 0; i < nmodirs; i++) {
			(void) sprintf(usr_kernel_drv, "%s/%s/%s",
				"/usr/kernel/drv", moddir[i], module_name);

			if ((stat(usr_kernel_drv, &ukdbuf) == 0) &&
			    ((ukdbuf.st_mode & S_IFMT) == S_IFREG)) {
				return (NOERR);
			}
		}
	} else {
		/* no path */
		if (modctl(MODGETPATH, NULL, data) != 0) {
			(void) fprintf(stderr, gettext(ERR_MODPATH));
			return (ERROR);
		}

		next = strtok(data, MOD_SEP);
		while (next != NULL) {
			for (i = 0; i < nmodirs; i++) {
				(void) sprintf(path, "%s/drv/%s/%s",
					next, moddir[i], module_name);

				if ((stat(path, &buf) == 0) &&
				    ((buf.st_mode & S_IFMT) == S_IFREG)) {
					return (NOERR);
				}
			}
			next = strtok((char *)NULL, MOD_SEP);
		}
	}

	return (ERROR);
}

/*
 * search for driver_name in first field of file file_name
 * searching name_to_major and driver_aliases: name separated from rest of
 * line by blank
 * if there return
 * else return
 */
int
unique_driver_name(char *driver_name, char *file_name,
	int *is_unique)
{
	int ret;

	if ((ret = get_major_no(driver_name, file_name)) == ERROR) {
		(void) fprintf(stderr, gettext(ERR_CANT_ACCESS_FILE),
		    file_name);
	} else {
		/* XXX */
		/* check alias file for name collision */
		if (unique_drv_alias(driver_name) == ERROR) {
			ret = ERROR;
		} else {
			if (ret != UNIQUE)
				*is_unique = NOT_UNIQUE;
			else
				*is_unique = ret;
			ret = NOERR;
		}
	}
	return (ret);
}

/*
 * check each entry in perm_list for:
 *	4 arguments
 *	permission arg is in valid range
 * permlist entries separated by comma
 * return ERROR/NOERR
 */
int
check_perm_opts(char *perm_list)
{
	char *current_head;
	char *previous_head;
	char *one_entry;
	int i, len, scan_stat;
	char minor[FILENAME_MAX + 1];
	char perm[OPT_LEN + 1];
	char own[OPT_LEN + 1];
	char grp[OPT_LEN + 1];
	char dumb[OPT_LEN + 1];
	int status = NOERR;
	int intperm;

	len = strlen(perm_list);

	if (len == 0) {
		usage();
		return (ERROR);
	}

	one_entry = calloc(len + 1, 1);
	if (one_entry == NULL) {
		(void) fprintf(stderr, gettext(ERR_NO_MEM));
		return (ERROR);
	}

	previous_head = perm_list;
	current_head = perm_list;

	while (*current_head != '\0') {

		for (i = 0; i <= len; i++)
			one_entry[i] = 0;

		current_head = get_entry(previous_head, one_entry, ',');

		previous_head = current_head;
		scan_stat = sscanf(one_entry, "%s%s%s%s%s", minor, perm, own,
		    grp, dumb);

		if (scan_stat < 4) {
			(void) fprintf(stderr, gettext(ERR_MIS_TOK),
			    "-m", one_entry);
			status = ERROR;
		}
		if (scan_stat > 4) {
			(void) fprintf(stderr, gettext(ERR_TOO_MANY_ARGS),
			    "-m", one_entry);
			status = ERROR;
		}

		intperm = atoi(perm);
		if (intperm < 0000 || intperm > 4777) {
			(void) fprintf(stderr, gettext(ERR_BAD_MODE), perm);
			status = ERROR;
		}

	}

	free(one_entry);
	return (status);
}

static int
fill_n2m_array(char *filename, char **array, int *nelems)
{
	FILE *fp;
	char line[MAX_N2M_ALIAS_LINE + 1];
	char drv[FILENAME_MAX + 1];
	u_longlong_t dnum;
	major_t drv_majnum;

	/*
	 * Read through the file, marking each major number found
	 * order is not relevant
	 */
	if ((fp = fopen(filename, "r")) == NULL) {
		perror(NULL);
		(void) fprintf(stderr, gettext(ERR_CANT_ACCESS_FILE), filename);
		return (ERROR);
	}

	while (fgets(line, sizeof (line), fp) != 0) {

		if (sscanf(line, "%s %llu", drv, &dnum) != 2) {
			(void) fprintf(stderr, gettext(ERR_BAD_LINE),
			    filename, line);
			(void) fclose(fp);
			return (ERROR);
		}

		if (dnum > L_MAXMAJ32) {
			(void) fprintf(stderr, gettext(ERR_MAJ_TOOBIG), drv,
			    dnum, filename, L_MAXMAJ32);
			continue;
		}
		/*
		 * cast down to a major_t; we can be sure this is safe because
		 * of the above range-check.
		 */
		drv_majnum = (major_t)dnum;

		if (drv_majnum >= *nelems) {
			/*
			 * Allocate some more space, up to drv_majnum + 1 so
			 * we can accomodate 0 through drv_majnum.
			 *
			 * Note that in the failure case, we leak all of the
			 * old contents of array.  It's ok, since we just
			 * wind up exiting immediately anyway.
			 */
			*nelems = drv_majnum + 1;
			*array = realloc(*array, *nelems);
			if (*array == NULL) {
				(void) fprintf(stderr, gettext(ERR_NO_MEM));
				return (ERROR);
			}
		}
		(*array)[drv_majnum] = 1;
	}

	(void) fclose(fp);
	return (0);
}


/*
 * get major number
 * write driver_name major_num to name_to_major file
 * major_num returned in major_num
 * return success/failure
 */
int
update_name_to_major(char *driver_name, major_t *major_num, int server)
{
	char major[MAX_STR_MAJOR + 1];
	struct stat buf;
	char *num_list;
	char drv_majnum_str[MAX_STR_MAJOR + 1];
	int new_maj = -1;
	int i, tmp = 0, is_unique, have_rem_n2m = 0;
	int max_dev = 0;

	/*
	 * if driver_name already in rem_name_to_major
	 * 	delete entry from rem_nam_to_major
	 *	put entry into name_to_major
	 */

	if (stat(rem_name_to_major, &buf) == 0) {
		have_rem_n2m = 1;
	}

	if (have_rem_n2m) {
		if ((is_unique = get_major_no(driver_name, rem_name_to_major))
		    == ERROR)
			return (ERROR);

		/*
		 * found a match in rem_name_to_major
		 */
		if (is_unique != UNIQUE) {
			char scratch[FILENAME_MAX];

			/*
			 * If there is a match in /etc/rem_name_to_major then
			 * be paranoid: is that major number already in
			 * /etc/name_to_major (potentially under another name)?
			 */
			if (get_driver_name(is_unique, name_to_major,
			    scratch) != UNIQUE) {
				/*
				 * nuke the rem_name_to_major entry-- it
				 * isn't helpful.
				 */
				(void) delete_entry(rem_name_to_major,
				    driver_name, " ");
			} else {
				(void) snprintf(major, sizeof (major),
				    "%d", is_unique);

				if (append_to_file(driver_name, major,
				    name_to_major, ' ', " ") == ERROR) {
					(void) fprintf(stderr,
					    gettext(ERR_NO_UPDATE),
					    name_to_major);
					return (ERROR);
				}

				if (delete_entry(rem_name_to_major,
				    driver_name, " ") == ERROR) {
					(void) fprintf(stderr,
					    gettext(ERR_DEL_ENTRY), driver_name,
					    rem_name_to_major);
					return (ERROR);
				}

				/* found matching entry : no errors */
				*major_num = is_unique;
				return (NOERR);
			}
		}
	}

	/*
	 * Bugid: 1264079
	 * In a server case (with -b option), we can't use modctl() to find
	 *    the maximum major number, we need to dig thru client's
	 *    /etc/name_to_major and /etc/rem_name_to_major for the max_dev.
	 *
	 * if (server)
	 *    get maximum major number thru (rem_)name_to_major file on client
	 * else
	 *    get maximum major number allowable on current system using modctl
	 */
	if (server) {
		max_dev = 0;
		tmp = 0;

		max_dev = get_max_major(name_to_major);

		/* If rem_name_to_major exists, we need to check it too */
		if (have_rem_n2m) {
			tmp = get_max_major(rem_name_to_major);

			/*
			 * If name_to_major is missing, we can get max_dev from
			 * /etc/rem_name_to_major.  If both missing, bail out!
			 */
			if ((max_dev == ERROR) && (tmp == ERROR)) {
				(void) fprintf(stderr,
					gettext(ERR_CANT_ACCESS_FILE),
					name_to_major);
				return (ERROR);
			}

			/* guard against bigger maj_num in rem_name_to_major */
			if (tmp > max_dev)
				max_dev = tmp;
		} else {
			/*
			 * If we can't get major from name_to_major file
			 * and there is no /etc/rem_name_to_major file,
			 * then we don't have a max_dev, bail out quick!
			 */
			if (max_dev == ERROR)
				return (ERROR);
		}

		/*
		 * In case there is no more slack in current name_to_major
		 * table, provide at least 1 extra entry so the add_drv can
		 * succeed.  Since only one add_drv process is allowed at one
		 * time, and hence max_dev will be re-calculated each time
		 * add_drv is ran, we don't need to worry about adding more
		 * than 1 extra slot for max_dev.
		 */
		max_dev++;

	} else {
		if (modctl(MODRESERVED, NULL, &max_dev) < 0) {
			perror(NULL);
			(void) fprintf(stderr, gettext(ERR_MAX_MAJOR));
			return (ERROR);
		}
	}

	/*
	 * max_dev is really how many slots the kernel has allocated for
	 * devices... [0 , maxdev-1], not the largest available device num.
	 */
	if ((num_list = calloc(max_dev, 1)) == NULL) {
		(void) fprintf(stderr, gettext(ERR_NO_MEM));
		return (ERROR);
	}

	/*
	 * Populate the num_list array
	 */
	if (fill_n2m_array(name_to_major, &num_list, &max_dev) != 0) {
		return (ERROR);
	}
	if (have_rem_n2m) {
		if (fill_n2m_array(rem_name_to_major, &num_list, &max_dev) != 0)
			return (ERROR);
	}

	/* find first free major number */
	for (i = 0; i < max_dev; i++) {
		if (num_list[i] != 1) {
			new_maj = i;
			break;
		}
	}

	if (new_maj == -1) {
		(void) fprintf(stderr, gettext(ERR_NO_FREE_MAJOR));
		return (ERROR);
	}

	(void) sprintf(drv_majnum_str, "%d", new_maj);
	if (do_the_update(driver_name, drv_majnum_str) == ERROR) {
		return (ERROR);
	}

	*major_num = new_maj;
	return (NOERR);
}

static void
usage()
{
	(void) fprintf(stderr, gettext(USAGE));
}

/*
 * check each alias :
 *	alias list members separated by white space
 *	cannot exist as driver name in /etc/name_to_major
 *	cannot exist as driver or alias name in /etc/driver_aliases
 */
int
aliases_unique(char *aliases)
{
	char *current_head;
	char *previous_head;
	char *one_entry;
	int i, len;
	int is_unique;

	len = strlen(aliases);

	one_entry = calloc(len + 1, 1);
	if (one_entry == NULL) {
		(void) fprintf(stderr, gettext(ERR_NO_MEM));
		return (ERROR);
	}

	previous_head = aliases;

	do {
		for (i = 0; i <= len; i++)
			one_entry[i] = 0;

		current_head = get_entry(previous_head, one_entry, ' ');
		previous_head = current_head;

		if ((unique_driver_name(one_entry, name_to_major,
		    &is_unique)) == ERROR) {
			free(one_entry);
			return (ERROR);
		}

		if (is_unique != UNIQUE) {
			(void) fprintf(stderr, gettext(ERR_ALIAS_IN_NAM_MAJ),
			    one_entry);
			free(one_entry);
			return (ERROR);
		}

		if (unique_drv_alias(one_entry) != NOERR) {
			free(one_entry);
			return (ERROR);
		}

	} while (*current_head != '\0');

	free(one_entry);

	return (NOERR);

}

int
unique_drv_alias(char *drv_alias)
{
	FILE *fp;
	char drv[FILENAME_MAX + 1];
	char line[MAX_N2M_ALIAS_LINE + 1];
	char alias[FILENAME_MAX + 1];
	int status = NOERR;

	fp = fopen(driver_aliases, "r");

	if (fp != NULL) {
		while ((fgets(line, sizeof (line), fp) != 0) &&
		    status != ERROR) {
			if (sscanf(line, "%s %s", drv, alias) != 2)
				(void) fprintf(stderr, gettext(ERR_BAD_LINE),
				    driver_aliases, line);

			if ((strcmp(drv_alias, drv) == 0) ||
			    (strcmp(drv_alias, alias) == 0)) {
				(void) fprintf(stderr,
				    gettext(ERR_ALIAS_IN_USE),
				    drv_alias);
				status = ERROR;
			}
		}
		(void) fclose(fp);
		return (status);
	} else {
		perror(NULL);
		(void) fprintf(stderr, gettext(ERR_CANT_OPEN), driver_aliases);
		return (ERROR);
	}

}

/*
 * check that major_num doesn`t exceed maximum on this machine
 * do this here (again) to support add_drv on server for diskless clients
 */
int
config_driver(
	char *driver_name,
	major_t major_num,
	char *aliases,
	char *classes,
	int cleanup_flag,
	int noload_flag,
	int verbose_flag)
{
	int max_dev;
	int n = 0;
	char *cmdline[MAX_CMD_LINE];
	char maj_num[128];
	char *previous;
	char *current;
	int exec_status;
	int len;
	FILE *fp;

	if (modctl(MODRESERVED, NULL, &max_dev) < 0) {
		perror(NULL);
		(void) fprintf(stderr, gettext(ERR_MAX_MAJOR));
		return (ERROR);
	}

	if (major_num >= max_dev) {
		(void) fprintf(stderr, gettext(ERR_MAX_EXCEEDS),
		    major_num, max_dev);
		return (ERROR);
	}

	/* bind major number and driver name */

	/* build command line */
	cmdline[n++] = DRVCONFIG;
	if (verbose_flag) {
		cmdline[n++] = "-v";
	}
	cmdline[n++] = "-b";
	if (classes) {
		cmdline[n++] = "-c";
		cmdline[n++] = classes;
	}
	cmdline[n++] = "-i";
	cmdline[n++] = driver_name;
	cmdline[n++] = "-m";
	(void) sprintf(maj_num, "%lu", major_num);
	cmdline[n++] = maj_num;

	if (aliases != NULL) {
		len = strlen(aliases);
		previous = aliases;
		do {
			cmdline[n++] = "-a";
			cmdline[n] = calloc(len + 1, 1);
			if (cmdline[n] == NULL) {
				(void) fprintf(stderr,
				    gettext(ERR_NO_MEM));
				return (ERROR);
			}
			current = get_entry(previous,
			    cmdline[n++], ' ');
			previous = current;

		} while (*current != '\0');

	}
	cmdline[n] = (char *)0;

	exec_status = exec_command(DRVCONFIG_PATH, cmdline);

	if (exec_status != NOERR) {
		perror(NULL);
		remove_entry(cleanup_flag, driver_name);
		return (ERROR);
	}


	/*
	 * now that we have the name to major number bound,
	 * config the driver
	 */

	/*
	 * create /reconfigure file so system reconfigures
	 * on reboot if we're actually loading the driver
	 * now
	 */
	if (!noload_flag) {
		fp = fopen(RECONFIGURE, "a");
		(void) fclose(fp);
	}

	/* build command line */

	if (!noload_flag) {
		n = 0;
		cmdline[n++] = DEVFSADM;
		if (verbose_flag) {
			cmdline[n++] = "-v";
		}
		cmdline[n++] = "-i";
		cmdline[n++] = driver_name;
		cmdline[n] = (char *)0;

		exec_status = exec_command(DEVFSADM_PATH, cmdline);

		if (exec_status != NOERR) {
			/* no clean : name and major number are bound */
			(void) fprintf(stderr, gettext(ERR_CONFIG),
				driver_name);
		}
	}

	return (NOERR);
}

static int
update_driver_classes(
	char *driver_name,
	char *classes)
{
	/* make call to update the classes file */
	return (append_to_file(driver_name, classes, driver_classes,
	    ' ', "\t"));
}

static int
update_driver_aliases(
	char *driver_name,
	char *aliases)
{
	/* make call to update the aliases file */
	return (append_to_file(driver_name, aliases, driver_aliases, ' ', " "));

}

static int
update_minor_perm(
	char *driver_name,
	char *perm_list)
{
	return (append_to_file(driver_name, perm_list, minor_perm, ',', ":"));
}

static int
do_the_update(
	char *driver_name,
	char *major_number)
{

	return (append_to_file(driver_name, major_number, name_to_major,
	    ' ', " "));
}

static void
signal_rtn()
{
	exit_unlock();
}

static int
exec_command(
	char *path,
	char *cmdline[MAX_CMD_LINE])
{
	pid_t pid;
	uint_t stat_loc;
	int waitstat;
	int exit_status;

	/* child */
	if ((pid = fork()) == 0) {

		(void) execv(path, cmdline);
		perror(NULL);
		return (ERROR);
	} else if (pid == -1) {
		/* fork failed */
		perror(NULL);
		(void) fprintf(stderr, gettext(ERR_FORK_FAIL), cmdline);
		return (ERROR);
	} else {
		/* parent */
		do {
			waitstat = waitpid(pid, (int *)&stat_loc, 0);

		} while ((!WIFEXITED(stat_loc) &&
			!WIFSIGNALED(stat_loc)) || (waitstat == 0));

		exit_status = WEXITSTATUS(stat_loc);

		return (exit_status);
	}
}

/*
 * Check to see if the driver we are adding is a more specific
 * driver for a device already attached to a less specific driver.
 * In other words, see if this driver comes earlier on the compatible
 * list of a device already attached to another driver.
 * If so, the new node will not be created (since the device is
 * already attached) but when the system reboots, it will attach to
 * the new driver but not have a node - we need to warn the user
 * if this is the case.
 */
static int
drv_name_conflict(di_node_t root_node)
{
	/*
	 * walk the device tree checking each node
	 */
	if (di_walk_node(root_node, DI_WALK_SIBFIRST, NULL, devfs_node) == -1) {
		free_conflict_list(conflict_lst);
		conflict_lst = (struct dev_list *)NULL;
		(void) fprintf(stderr, gettext(ERR_DEVTREE));
		return (-1);
	}

	if (conflict_lst == NULL)
		/* no conflicts found */
		return (0);
	else
		/* conflicts! */
		return (1);
}

/*
 * called via di_walk_node().
 * called for each node in the device tree.  We skip nodes that:
 *	1. are not hw nodes (since they cannot have generic names)
 *	2. that do not have a compatible property
 *	3. whose node name = binding name.
 *	4. nexus nodes - the name of a generic nexus node would
 *	not be affected by a driver change.
 * Otherwise, we parse the compatible property, if we find a
 * match with the new driver before we find a match with the
 * current driver, then we have a conflict and we save the
 * node away.
 */
/*ARGSUSED*/
static int
devfs_node(di_node_t node, void *arg)
{
	char *binding_name, *node_name, *compat_names, *devfsnm;
	struct dev_list *new_entry;
	char strbuf[MAXPATHLEN];
	int n_names;

	/*
	 * if there is no compatible property, we don't
	 * have to worry about any conflicts.
	 */
	if ((n_names = di_compatible_names(node, &compat_names)) <= 0)
		return (DI_WALK_CONTINUE);

	/*
	 * if the binding name and the node name match, then
	 * either no driver existed that could be bound to this node,
	 * or the driver name is the same as the node name.
	 */
	binding_name = di_binding_name(node);
	node_name = di_node_name(node);
	if ((binding_name == NULL) || (strcmp(node_name, binding_name) == 0))
		return (DI_WALK_CONTINUE);

	/*
	 * we can skip nexus drivers since they do not
	 * have major/minor number info encoded in their
	 * /devices name and therefore won't change.
	 */
	if (di_driver_ops(node) & DI_BUS_OPS)
		return (DI_WALK_CONTINUE);

	/*
	 * check for conflicts
	 * If we do find that the new driver is a more specific driver
	 * than the driver already attached to the device, we'll save
	 * away the node name for processing later.
	 */
	if (drv_name_match(compat_names, n_names, binding_name, new_drv)) {
		devfsnm = di_devfs_path(node);
		(void) sprintf(strbuf, "%s%s", DEVFS_ROOT, devfsnm);
		di_devfs_path_free(devfsnm);
		new_entry = (struct dev_list *)calloc(1,
		    sizeof (struct dev_list));
		if (new_entry == (struct dev_list *)NULL) {
			(void) fprintf(stderr, gettext(ERR_NO_MEM));
			err_exit();
		}
		/* save the /devices name */
		if ((new_entry->dev_name = strdup(strbuf)) == NULL) {
			(void) fprintf(stderr, gettext(ERR_NO_MEM));
			free(new_entry);
			err_exit();
		}
		/* save the driver name */
		if ((new_entry->driver_name = strdup(di_driver_name(node)))
		    == NULL) {
			(void) fprintf(stderr, gettext(ERR_NO_MEM));
			free(new_entry->dev_name);
			free(new_entry);
			err_exit();
		}
		/* check to see if this is a clone device */
		if (clone(node))
			new_entry->clone = 1;

		/* add it to the list */
		new_entry->next = conflict_lst;
		conflict_lst = new_entry;
	}

	return (DI_WALK_CONTINUE);
}

static int
clone(di_node_t node)
{
	di_minor_t minor = DI_MINOR_NIL;

	while ((minor = di_minor_next(node, minor)) != DI_MINOR_NIL) {
		if (di_minor_type(minor) == DDM_ALIAS)
			return (1);
	}
	return (0);
}
/*
 * check to see if the new_name shows up on the compat list before
 * the cur_name (driver currently attached to the device).
 */
static int
drv_name_match(char *compat_names, int n_names, char *cur_name, char *new_name)
{
	int i, ret = 0;

	if (strcmp(cur_name, new_name) == 0)
		return (0);

	/* parse the coompatible list */
	for (i = 0; i < n_names; i++) {
		if (strcmp(compat_names, new_name) == 0) {
			ret = 1;
			break;
		}
		if (strcmp(compat_names, cur_name) == 0) {
			break;
		}
		compat_names += strlen(compat_names) + 1;
	}
	return (ret);
}

/*
 * A more specific driver is being added for a device already attached
 * to a less specific driver.  Print out a general warning and if
 * the force flag was passed in, give the user a hint as to what
 * nodes may be affected in /devices and /dev
 */
static void
print_drv_conflict_info(int force)
{
	struct dev_list *ptr;

	if (conflict_lst == NULL)
		return;
	if (force) {
		(void) fprintf(stderr,
		    "\nA reconfiguration boot must be performed to "
		    "complete the\n");
		(void) fprintf(stderr, "installation of this driver.\n");
	}

	if (force) {
		(void) fprintf(stderr,
		    "\nThe following entries in /devices will be "
		    "affected:\n\n");
	} else {
		(void) fprintf(stderr,
		    "\nDriver installation failed because the following\n");
		(void) fprintf(stderr,
		    "entries in /devices would be affected:\n\n");
	}

	ptr = conflict_lst;
	while (ptr != NULL) {
		(void) fprintf(stderr, "\t%s", ptr->dev_name);
		if (ptr->clone)
			(void) fprintf(stderr, " (clone device)\n");
		else
			(void) fprintf(stderr, "[:*]\n");
		(void) fprintf(stderr, "\t(Device currently managed by driver "
		    "\"%s\")\n\n", ptr->driver_name);
		ptr = ptr->next;
	}
	check_dev_dir(force);
}

/*
 * use nftw to walk through /dev looking for links that match
 * an entry in the conflict list.
 */
static void
check_dev_dir(int force)
{
	int  walk_flags = FTW_PHYS | FTW_MOUNT;
	int ft_depth = 15;

	if (force) {
		(void) fprintf(stderr, "\nThe following entries in /dev will "
		    "be affected:\n\n");
	} else {
		(void) fprintf(stderr, "\nThe following entries in /dev would "
		    "be affected:\n\n");
	}

	(void) nftw("/dev", dev_node, ft_depth, walk_flags);

	(void) fprintf(stderr, "\n");
}

/*
 * checks a /dev link to see if it matches any of the conlficting
 * /devices nodes in conflict_lst.
 */
/*ARGSUSED1*/
static int
dev_node(const char *node, const struct stat *node_stat, int flags,
	struct FTW *ftw_info)
{
	char linkbuf[MAXPATHLEN];
	struct dev_list *ptr;

	if (readlink(node, linkbuf, MAXPATHLEN) == -1)
		return (0);

	ptr = conflict_lst;

	while (ptr != NULL) {
		if (strstr(linkbuf, ptr->dev_name) != NULL)
			(void) fprintf(stderr, "\t%s\n", node);
		ptr = ptr->next;
	}
	return (0);
}


static void
free_conflict_list(struct dev_list *list)
{
	struct dev_list *save;

	/* free up any dev_list structs we allocated. */
	while (list != NULL) {
		save = list;
		list = list->next;
		free(save->dev_name);
		free(save);
	}
}

static int
check_space_within_quote(char *str)
{
	register int i;
	register int len;
	int quoted = 0;

	len = strlen(str);
	for (i = 0; i < len; i++, str++) {
		if (*str == '"') {
			if (quoted == 0)
				quoted++;
			else
				quoted--;
		} else if (*str == ' ' && quoted)
			return (ERROR);
	}

	return (0);
}
