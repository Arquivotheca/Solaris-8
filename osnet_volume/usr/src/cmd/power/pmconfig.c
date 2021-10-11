/*
 * Copyright (c) 1994 - 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)pmconfig.c	1.30	99/11/11 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/pm.h>
#include <syslog.h>
#include <limits.h>
#include <libintl.h>
#include <utmpx.h>
#include <pwd.h>
#include <deflt.h>
#include <locale.h>
#include <sys/stat.h>
#include <sys/cpr.h>
#ifdef sparc
#include <sys/mnttab.h>
#include <sys/openpromio.h>
#include <string.h>
#endif
#include "powerd.h"

#define	OK2UPDATE	1
#define	NOTOK		0

#define	DEFAULTFILE	"/etc/default/power"
#define	CONSOLE		"/dev/console"
static int has_cpr_perm	= 0;
static int has_pm_perm = 0;

/*
 * The ctype isspace() accepts too many other chars as well
 */
#define	isspace(ch)	((ch) == ' ' || (ch) == '\t')
#define	iseol(ch)	((ch) == '\n' || (ch) == '\r' || (ch) == '\f')
#define	isdelimiter(ch)	(isspace(ch) || iseol(ch) || (ch) == ')' || (scaled && \
	isscale(ch)))
#define	isscale(ch)	((ch) == 'H' || (ch) == 'h' || (ch) == 'M' || \
	(ch) == 'm' || (ch) == 'S' || (ch) == 's')

typedef enum {
	NAME, NUMBER, EOL, TIME, EOFTOK, OPAREN, CPAREN, ERROR, SCALED
} token_t;

static token_t	lex(FILE *, char *, int);
static void	find_eol(FILE *);
static void	process_device(FILE *, int fd, char *);
static void	process_old_device(FILE *, int fd, char *);
static int	other_valid_tokens(FILE *, char *);
static int	is_default_device(char *);
static int	launch(char *, char *[]);
#ifdef sparc
static void	process_statefile(FILE *, char *);
static int	parse_statefile_path(char *, struct cprconfig *);
static char	*dup_msg;
#endif
static int	process_autopm(FILE *, int, char *);
static int	getscale(FILE *, char *);
static int	collect_comp(FILE *, char *, int **, size_t *, int);
static int	process_path(char **);
static void	check_perms();
static int	has_cnowner_perms();
static int	has_userlist_perms(char *, char *);
static int	redo_powerd();
static void	get_line(FILE *, char *);
static int	update_cprconfig();
static int	write_pwrcfg(FILE *, FILE *);
static int	isblank(char *);
static struct utmpx	utmp;
#define	NMAX	(sizeof (utmp.ut_name))

#define	ERR_MSG_LEN	MAXNAMELEN+64
#define	CONF_FILE	"/etc/power.conf"
#define	USAGE		"Usage:\t%s [ -r ] [ -f file ]\n"
#define	OPENPROM	"/dev/openprom"
#define	PROM_BUFSZ	1024

#define	ESTAR_V2_PROP	"energystar-v2"
#define	ESTAR_V3_PROP	"energystar-v3"
typedef enum {no_estar, estar_v2, estar_v3} estar_version_t;
static estar_version_t estar_vers;
static estar_version_t find_estarv();

static struct cprconfig	old_cprcfg;
static struct cprconfig cur_cprcfg;
static int	ok_to_update_cpr = OK2UPDATE;
			/* default OK until caught fatal syntax error */
static int	ok_to_update_pm = OK2UPDATE;
			/* default OK until caught fatal syntax error */

static int	lineno;
static char	me[MAXNAMELEN];		/* name of this program */

static	uid_t	user_uid;

static char	*header_str[] = {
"#",
"# Copyright (c) 1996 - 1999 by Sun Microsystems, Inc.",
"# All rights reserved.",
"#",
"#pragma ident	\"@(#)power.conf	1.14	99/10/18 SMI\"",
"#",
"# Power Management Configuration File",
"#",
"# NOTE: The entry below is only used when no windowing environment",
"# is running.  When running windowing environment, monitor power",
"# management is controlled by the window system.",
};

static char	as_cmt[]	=
		"# Auto-Shutdown\tIdle(min)\tStart/Finish(hh:mm)\tBehavior";
static char	statefile_cmt[]	= "# Statefile\tPath";

static pid_t	*daemon_pid = (pid_t *)-1;

int
main(int argc, char *argv[])
{
	FILE		*file;
	FILE		*ofile, *nfile;
	struct stat	st;
	char		buf[MAXNAMELEN];
	char		errmsg[ERR_MSG_LEN];
	int		fd, cfd;
	token_t		token;
	key_t		key;
	int		shmid;
	struct shmid_ds	shm_stat;
	int		sys_idle;
	int		autopm_detected = 0;
	char		newconf[MAXNAMELEN];
	int		exitcode = EXIT_SUCCESS;
#ifdef sparc
	static int	found_statefile;

	dup_msg =
	    gettext("%s (line %d of %s): Ignoring redundant statefile "
	    "entry.\n");
#endif

#ifdef lint
	utmp = utmp;
#endif

	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);

	(void) snprintf(me, MAXNAMELEN, "%s", argv[0]);
	if ((argc > 3) ||
	    ((argc == 3) && (strcmp(argv[1], "-f") != 0)) ||
	    ((argc == 2) && (strcmp(argv[1], "-r") != 0))) {
		(void) fprintf(stderr, USAGE, me);
		exit(EXIT_FAILURE);
	}

	check_perms();

	if (! has_cpr_perm && ! has_pm_perm) {
		(void) fprintf(stderr,
			gettext("%s: Permission denied. \n"), me);
		exit(EXIT_FAILURE);
	}

	if ((argc == 2) && ! (has_cpr_perm && has_pm_perm)) {
		(void) fprintf(stderr, gettext("%s -r: "
				"Permission denied.\n"), me);
		exit(EXIT_FAILURE);
	}

	if (argc == 3) {
		(void) strcpy(newconf, argv[2]);
		if ((nfile = fopen(newconf, "r")) == NULL) {
			(void) fprintf(stderr,
				gettext("%s: Can't open %s for read: "),
				me, newconf);
			perror(NULL);
			exit(EXIT_FAILURE);
		}
	}

	(void *) memset((void *) &old_cprcfg, 0, sizeof (old_cprcfg));
	(void *) memset((void *) &cur_cprcfg, 0, sizeof (cur_cprcfg));

	/*
	 * Initialize /etc/.cpr_config
	 *
	 * The following fields always get updated to provide correct
	 * system nature as far as pm and cpr concerned regardless
	 * how 'pmconfig' is invoked.  dtpower relys on these fields.
	 *	is_cpr_default
	 *	is_autopm_default
	 *	is_cpr_capable
	 *	is_autowakeup_capable
	 *
	 * If 'pmconfig' or 'pmconfig -f file' is invoked, the rest of the
	 * fields in /etc/.cpr_config gets updated depend on what permission
	 * user has.  With only PM permission, the "apm_behavior" will be
	 * updated, with only CPR permission, then all, except "apm_behavior"
	 * field will be updated.
	 *
	 * If 'pmconfig -f file' is invoked with only PM or CPR permission,
	 * only the PM or CPR commands from 'file' will get activated and
	 * upon success, the PM or CPR commands from the  existing
	 * power.conf file will be replaced by the PM or CPR commands from
	 * 'file'.
	 *
	 * User must have both PM and CPR permission to run 'pmconfig -r'.
	 */

#ifdef sparc
	estar_vers = find_estarv();

	if (estar_vers == estar_v2) {
		cur_cprcfg.is_cpr_default = 1;
	} else if (estar_vers == estar_v3) {
		cur_cprcfg.is_autopm_default = 1;
	}

	if (uadmin(A_FREEZE, AD_CHECK, 0) == 0)
		cur_cprcfg.is_cpr_capable = 1;
#else
	estar_vers = no_estar;
#endif sparc

	if ((fd = open("/dev/tod", O_RDONLY)) > 0) {
		cur_cprcfg.is_autowakeup_capable = 1;
		close(fd);
	}

	if ((key = ftok("/dev/pm", 'P')) < 0) {
		(void) fprintf(stderr, gettext("%s: Unable to access /dev/pm"),
		    me);
		exit(EXIT_FAILURE);
	}
	shmid = shmget(key, (int)sizeof (pid_t), SHM_RDONLY);

	if ((shmid != -1) && ((daemon_pid =
		(pid_t *)shmat(shmid, NULL, SHM_RDONLY)) != (pid_t *)(-1))) {

		/*
		 * Check creator's uid. It should be root.
		 * This is for fixing bug 4252989.
		 */
		if (shmctl(shmid, IPC_STAT, &shm_stat) < 0) {
			fprintf(stderr, gettext("%s: Can't do IPC_STAT: "), me);
			perror(NULL);
			(void) shmdt((void *)daemon_pid);
			exit(EXIT_FAILURE);
		}

		if (shm_stat.shm_perm.cuid != 0) {
			if (getuid() == 0) {
				(void) shmdt((void *)daemon_pid);
				shmctl(shmid, IPC_RMID, NULL);
				daemon_pid = (pid_t *)-1;
			} else {
				(void) fprintf(stderr, gettext("%s: A non root"
				" process is attached to key of "
				"/usr/lib/power/powerd.\n  Please run "
				"/usr/sbin/pmconfig as root to fix the problem."
				"\n"), me);
				(void) shmdt((void *)daemon_pid);
				exit(EXIT_FAILURE);
			}
		}
	}

	if (((cfd = open(CPR_CONFIG, O_RDONLY)) < 0) ||
	    (read(cfd, &old_cprcfg, sizeof (old_cprcfg))
		!= sizeof (old_cprcfg))) {
		if (user_uid == 0) {
			unlink(CPR_CONFIG);
			(void) memset((void *)&old_cprcfg, 0,
				sizeof (old_cprcfg));
		} else {
			fprintf(stderr,
			gettext("%s: %s file corrupted. Please run %s as\n"
			"root to fix the problem.\n "), me, CPR_CONFIG, me);
			(void) close(cfd);
			exit(EXIT_FAILURE);
		}
	}
	if (cfd > 0)
		close(cfd);

	if (has_pm_perm) {
		/*
		 * unconfigure pm
		 */
		if ((fd = open("/dev/pm", O_RDWR)) == -1) {
			(void) fprintf(stderr,
				gettext("%s: Can't open \"/dev/pm\": "), me);
			perror(NULL);
			exit(EXIT_FAILURE);
		}

		if (ioctl(fd, PM_REM_DEVICES, 0) < 0) {
			fprintf(stderr,
				gettext("%s: Can't unconfigure devices: "), me);
			perror(NULL);
			(void) close(fd);
			exit(EXIT_FAILURE);
		}
		if (ioctl(fd, PM_RESET_PM, 0) < 0) {
			fprintf(stderr,
				gettext("%s: Can't reset pm state: "), me);
			perror(NULL);
			(void) close(fd);
			exit(EXIT_FAILURE);
		}
	}

	/*
	 * "pmconfig -r":
	 */
	if (argc == 2) {
		cur_cprcfg.cf_magic = old_cprcfg.cf_magic;
		cur_cprcfg.cf_type = old_cprcfg.cf_type;
		(void) strcpy(cur_cprcfg.cf_path, old_cprcfg.cf_path);
		(void) strcpy(cur_cprcfg.cf_fs, old_cprcfg.cf_fs);
		(void) strcpy(cur_cprcfg.cf_devfs, old_cprcfg.cf_devfs);
		(void) strcpy(cur_cprcfg.cf_dev_prom, old_cprcfg.cf_dev_prom);

		if (update_cprconfig() < 0 || redo_powerd() < 0) {
			(void) fprintf(stderr,
				gettext("%s: Can't unconfigure cpr\n"),
				me);
			exitcode = EXIT_FAILURE;
		}
		if (daemon_pid != (pid_t *)-1)
			(void) shmdt((void *)daemon_pid);
		(void) close(fd);
		exit(exitcode);
	}

	/*
	 * while not -r, initialize loadaverage to default value
	 */
	cur_cprcfg.loadaverage_thold = 0.04;

	if (!(ofile = fopen(CONF_FILE, "r"))) {
		(void) fprintf(stderr,
		    gettext("%s: Can't open \"%s\"\n"), me, CONF_FILE);
		(void) close(fd);
		exit(EXIT_FAILURE);
	}

	/*
	 * activating pm and/or cpr changes from ether /etc/power.conf
	 * file or 'newconf' file (with -f option).
	 * Only those changes that user has permission to make would be
	 * activated.
	 */
	if (argc == 1)
		file = ofile;
	else
		file = nfile;

	lineno = 1;
	while ((token = lex(file, buf, 0)) != EOFTOK) {
		if (token == EOL)
			continue;
		if (token != NAME) {	/* Want a name first */
			(void) fprintf(stderr,
			    gettext("%s (line %d of %s): Must begin with a "
			    "name\n"), me, lineno, CONF_FILE);
			find_eol(file);
			continue;
		}
		if (other_valid_tokens(file, buf)) {
			continue;
		} else if (strcmp(buf, "autopm") == 0) {
			if (has_pm_perm) {
				if (process_autopm(file, fd, errmsg) != 0)
					(void) fprintf(stderr,
						gettext("%s(line %d of %s):"
						"autopm: %s\n"),
						me, lineno, CONF_FILE, errmsg);
				else
					autopm_detected = 1;
			}
			find_eol(file);
			continue;
#ifdef sparc
		} else if (strcmp(buf, "statefile") == 0) {
			if (++found_statefile > 1)
				(void) fprintf(stderr,
					dup_msg, me, lineno);
			else if (has_cpr_perm)
				process_statefile(file, buf);

			find_eol(file);
#endif
		} else if (has_pm_perm)
			process_device(file, fd, buf);
		else
			find_eol(file);
	}

	/*
	 * "pmconfig"
	 */
	if (argc == 1) {
		if ((update_cprconfig() < 0) || (redo_powerd() < 0)) {
			exitcode = EXIT_FAILURE;
		}
		if (daemon_pid != (pid_t *)-1)
			(void) shmdt((void *)daemon_pid);
		(void) close(fd);

		fclose(ofile);
		exit(exitcode);
	}

	/*
	 *  "pmconfig -f file"
	 */
	if ((write_pwrcfg(ofile, nfile) < 0) ||
	    (update_cprconfig() < 0) ||
	    (redo_powerd() < 0)) {
		exitcode = EXIT_FAILURE;
	}

	fclose(nfile);
	fclose(ofile);
	if (daemon_pid != (pid_t *)-1)
		(void) shmdt((void *)daemon_pid);
	(void) close(fd);

	return (exitcode);
}


#ifdef sparc
/*
 * Parse the "statefile" line.  The 2nd argument is the path selected
 * by the user to contain the cpr statefile.  Verify that either:
 * -it resides on a writeable, local, UFS filesystem and that if it
 *  already exists and is not a directory, or:
 * -it is a character special file upon which no file system is mounted
 *
 * Then create the hidden config file in root used
 * by both the cpr kernel module and the booter program.
 */
static void
process_statefile(FILE *file, char *buf)
{
	typedef union {
		char	buf[PROM_BUFSZ];
		struct	openpromio opp;
	} Oppbuf;
	Oppbuf oppbuf;
	int	fdprom;
	struct openpromio *opp = &(oppbuf.opp);
	int	fd;

	if (uadmin(A_FREEZE, AD_CHECK, 0) != 0) {
		if (errno != ENOTSUP) {
			ok_to_update_cpr = NOTOK;
			fprintf(stderr, "%s: uadmin(A_FREEZE, A_CHECK, 0): ", me);
			perror(NULL);
		}
		return;
	}

	if (lex(file, buf, 0) != NAME || *buf != '/') {
		fprintf(stderr, gettext("%s: %s line (%d) \"statefile\" "
		    "requires pathname argument.\n"), me, CONF_FILE, lineno);

		ok_to_update_cpr = NOTOK;
		return;
	}

	if ((fdprom = open(OPENPROM, O_RDONLY)) == -1) {
		fprintf(stderr, gettext("%s: %s line (%d) Cannot open %s: "),
			me, CONF_FILE, lineno, OPENPROM);
		perror(NULL);
		ok_to_update_cpr = NOTOK;
		return;
	}

	if (parse_statefile_path(buf, &cur_cprcfg)) {
	    ok_to_update_cpr = NOTOK;
	    return;
	}

	/*
	 * Convert the device special file representing the filesystem
	 * containing the cpr statefile to a full device path.
	 */
	opp->oprom_size = PROM_BUFSZ;
	strcpy(opp->oprom_array, cur_cprcfg.cf_devfs);

	if (ioctl(fdprom, OPROMDEV2PROMNAME, opp) == -1) {
		openlog(NULL, 0, LOG_DAEMON);
		syslog(LOG_NOTICE, gettext("%s: %s line (%d) failed to convert"
		    " mount point %s to prom name"),
		    me, CONF_FILE, lineno, opp->oprom_array);
		closelog();
		ok_to_update_cpr = NOTOK;
		return;
	}
	strcpy(cur_cprcfg.cf_dev_prom, opp->oprom_array);
	cur_cprcfg.cf_magic = CPR_CONFIG_MAGIC;
}

/*
 * Parse the statefile path and populate the cprconfig structure with
 * the results.
 */
int
parse_statefile_path(char *path, struct cprconfig *cf)
{
	FILE	*fp;
	static struct mnttab mtab_ref = { NULL, NULL, "ufs", NULL, NULL };
	struct	mnttab mtab;
	struct stat st;
	char	*slash;
	int	longest = 0;
	int	error;
	dev_t	dev;

	slash = strrchr(path, '/'); /* Caller guarantees initial slash */

	/*
	 * First check for the character special device
	 */
	error = stat(path, &st);

	if (error != -1 && (st.st_mode & S_IFMT) == S_IFCHR) {
		fprintf(stderr, gettext("%s: %s line (%d) -- statefile \"%s\" "
		    "is a character special file. Only block special or regular"
		    " files are supported.\n"), me, CONF_FILE, lineno, path);
		return (-1);
	}

	if (error != -1 && (st.st_mode & S_IFMT) == S_IFBLK) {
		cf->cf_type = CFT_SPEC;
		strcpy(cf->cf_devfs, path);
		dev = st.st_rdev;
	} else {
		cf->cf_type = CFT_UFS;

		/*
		 * Make sure penultimate component of path is a directory.  Only
		 * need to make this test for non-root.
		 */
		if (slash != path) {
			*slash = '\0';		/* Put the slash back later! */
			if (stat(path, &st) == -1 ||
			    (st.st_mode & S_IFMT) != S_IFDIR) {
				fprintf(stderr, gettext("%s: %s line (%d) "
				    "statefile directory %s not found.\n"),
				    me, CONF_FILE, lineno, path);
				*slash = '/';
				return (-1);
			}
			*slash = '/';
		}

		/*
		 * If the path refers to existing file, it must be a regular
		 * file.
		 */
		if ((error =
		    stat(path, &st)) == 0 && (st.st_mode & S_IFMT) != S_IFREG) {
			fprintf(stderr, gettext("%s: %s line (%d) -- "
			    "statefile \"%s\" is not a regular file.\n"),
			    me, CONF_FILE, lineno, path);
			return (-1);
		}

		/*
		 * If unable to stat the full path, "not found" is the
		 * only excuse.
		 */
		if (error == -1 && errno != ENOENT) {
			fprintf(stderr, gettext("%s: %s line (%d) -- "
				"cannot access statefile \"%s\": "),
				me, CONF_FILE, lineno, path);
			perror(NULL);
			return (-1);
		}
	}

	if ((fp = fopen(MNTTAB, "r")) == NULL) {
		fprintf(stderr, gettext("%s: %s line (%d) error opening %s: "),
			me, CONF_FILE, lineno, MNTTAB);
		perror(NULL);

		return (-1);
	}

	/*
	 * Read all the ufs entries in mnttab.  For UFS statefile, try to
	 * match each mountpoint path with the start of the full path.  The
	 * longest such match is the mountpoint of the statefile fs.
	 * For SPEC statefile, look up the mount point and see if it is the
	 * same device as the statefile, (not allowed).
	 */
	while ((error = getmntany(fp, &mtab, &mtab_ref)) != -1) {
		int	len;

		if (error > 0) {
			fprintf(stderr,
				gettext("%s: %s line (%d) error reading %s.\n"),
				me, CONF_FILE, lineno, MNTTAB);
			longest = -1;
			break;
		}

		switch (cf->cf_type) {
		case CFT_UFS:
			len = strlen(mtab.mnt_mountp);
			if (strncmp(mtab.mnt_mountp, path, len) == 0 &&
				path[len == 1 ? 0: len ] == '/')
				if (len > longest) {
					longest = len;
					strcpy(cf->cf_fs, mtab.mnt_mountp);
					strcpy(cf->cf_devfs, mtab.mnt_special);
					strcpy(cf->cf_path,
					    longest == 1 ?
					    path + 1 : path + longest + 1);
				}
			break;
		case CFT_SPEC:
			if (stat(mtab.mnt_special, &st) != -1 &&
			    (st.st_rdev == dev)) {
				/* fs mounted on statefile! */
				fprintf(stderr, gettext("%s: %s line (%d) -- "
				    "statefile \"%s\" has a file system mounted"
				    " on it.\n"), me, CONF_FILE, lineno, path);
				return (-1);
			}
		}
	}
	fclose(fp);

	return (longest == -1 ? longest : 0);
}
#endif

static void
process_device(FILE *file, int fd, char *buf1)
{
	char		*ch, buf2[MAXNAMELEN];
	pm_req_t	req;
	token_t		token;
	int		value, ret;
	int		neg = 1;
	char 		*type;
	int		cmd;

	req.physpath = buf1;
	ret = -1;

	/*
	 * If new syntax for setting thresholds
	 */
	if (strcmp(buf1, "device-thresholds") == 0) {
		token = lex(file, buf2, 0);
		type = "device";
		if (token != NAME) {
			(void) fprintf(stderr, gettext("%s (line %d of %s): "
			    "Must specify a %s name or path\n"), me, lineno,
			    CONF_FILE, type);
			find_eol(file);
			ok_to_update_pm = NOTOK;
			return;
		}
		for (ch = buf2; !isspace(*ch); )
			ch++;
		req.physpath = malloc((size_t)(ch - buf2 + 1));
		if (req.physpath == NULL) {
			fprintf(stderr, gettext("%s (line %d of %s): Out of "
			    "memory\n"), me, lineno, CONF_FILE);
			find_eol(file);
			ok_to_update_pm = NOTOK;
			return;
		}
		strncpy(req.physpath, buf2, ch - buf2);
		req.physpath[ch - buf2] = '\0';
		if (process_path(&req.physpath)) {	/* prints error */
			find_eol(file);
			return;
		}
		/* have finished dev_path, look for thresholds */
		token = lex(file, buf2, 1);
		switch (token) {
		case NUMBER:		/* PM_SET_DEVICE_THRESHOLD */
		{
			cmd = PM_SET_DEVICE_THRESHOLD;
			value = 0;
			ch = buf2;
			if (*ch == '-') {
				neg = -1;
				ch++;
			}
			while (*ch != '\0')
				value = value * 10 + *ch++ - '0';
			value *= neg;
			value *= getscale(file, buf2);
			req.value = value;
			break;
		}

		case OPAREN:		/* PM_SET_COMPONENT_THRESHOLD */
		{
			size_t datasize = 0;
			int *dataptr = NULL;
			int indx = 0;		/* index of current count */
			extern int collect_comp(FILE *, char *, int **,
			    size_t *, int);

			do {
				datasize += sizeof (int);	/* for count */
				dataptr = realloc(dataptr, datasize);
				if (dataptr == NULL) {
					(void) fprintf(stderr, gettext("%s "
					    "(line %d of %s): Out of memory\n"),
					    me, lineno, CONF_FILE);
					find_eol(file);
					ok_to_update_pm = NOTOK;
					return;
				}
				if ((ret = collect_comp(file, buf2, &dataptr,
				    &datasize, indx)) == 0)
					/* freed by collect_comp */
					return;
				indx += ret + 1;
			} while ((token = lex(file, buf2, 1)) == OPAREN);

			if (token == EOL) {
				/* now we need a terminating 0 count */
				datasize += sizeof (int);
				dataptr = realloc(dataptr, datasize);
				dataptr[indx] = 0;
			} else {
				(void) fprintf(stderr,
				    gettext("%s (line %d of %s): "), me, lineno,
				    CONF_FILE);
				(void) fprintf(stderr,
				    gettext("Threshold value error\n"));
				free(dataptr);
				ok_to_update_pm = NOTOK;
				find_eol(file);
				return;
			}
			cmd = PM_SET_COMPONENT_THRESHOLDS;
			req.data = dataptr;
			req.datasize = datasize;
			break;
		}

		case NAME:	/* always-on -> PM_SET_DEVICE_THRESHOLD */
			if (strcmp(buf2, "always-on") != 0) {
				(void) fprintf(stderr,
				    gettext("%s (line %d of %s): Unrecognized "
				    "entry '%s'\n"), me, lineno, CONF_FILE,
				    buf2);
				find_eol(file);
				ok_to_update_pm = NOTOK;
				return;
			} else {
				cmd = PM_SET_DEVICE_THRESHOLD;
				req.value = INT_MAX;
			}
			break;

		default:
			(void) fprintf(stderr,
			    gettext("%s (line %d of %s): "), me, lineno,
			    CONF_FILE);
			(void) fprintf(stderr,
			    gettext("Can't find threshold value.\n"));
			ok_to_update_pm = NOTOK;
			find_eol(file);
			return;
		}
		ret = ioctl(fd, cmd, &req);
		if (cmd == PM_SET_COMPONENT_THRESHOLDS)
			free(req.data);
		if (ret < 0) {
			fflush(stderr);
			find_eol(file);
			perror(req.physpath);
			ok_to_update_pm = NOTOK;
		}
		return;
	} else if (strcmp(buf1, "device-dependency") == 0) {
		int count = 0;
		int kept = 0;
		int keeper = 0;
		token = lex(file, buf2, 0);
		type = "device";
		if (token != NAME) {
			(void) fprintf(stderr, gettext("%s (line %d of %s): "
			    "Must specify a %s name or path\n"), me, lineno,
			    CONF_FILE, type);
			find_eol(file);
			ok_to_update_pm = NOTOK;
			return;
		}
		for (ch = buf2; !isspace(*ch); )
			ch++;
		req.physpath = malloc((size_t)(ch - buf2 + 1));
		if (req.physpath == NULL) {
			fprintf(stderr, gettext("%s (line %d of %s): Out of "
			    "memory\n"), me, lineno, CONF_FILE);
			find_eol(file);
			ok_to_update_pm = NOTOK;
			return;
		}
		strncpy(req.physpath, buf2, ch - buf2);
		req.physpath[ch - buf2] = '\0';
		/*
		 * Because we ship power.conf with a
		 * device-dependency /dev/fb /dev/kbd
		 * line, we want to suppress any error messages from this line
		 * but process_path changes the path, so we need to remember
		 * whether the old one was from this line
		 */
		kept = (strncmp(req.physpath, "/dev/fb", strlen("/dev/fb")) ==
		    0);
		if (process_path(&req.physpath)) {	/* prints error */
			find_eol(file);
			return;
		}
		/* have finished dev_path, look for dependents */
		while ((token = lex(file, buf2, 1)) == NAME) {
			count++;
			for (ch = buf2; !isspace(*ch); )
				ch++;
			req.data = (void *)malloc((size_t)(ch - buf2 + 1));
			if (req.data == NULL) {
				fprintf(stderr, gettext("%s (line %d of %s): "
				    "Out of memory\n"), me, lineno, CONF_FILE);
				find_eol(file);
				ok_to_update_pm = NOTOK;
				return;
			}
			strncpy((char *)req.data, buf2, ch - buf2);
			((char *)req.data)[ch - buf2] = '\0';
			keeper = (count == 1 &&
			    (strcmp((char *)req.data, "/dev/kbd") == 0));
			if (process_path((char **)&req.data)) {
				find_eol(file);
				return;
			}
			ret = ioctl(fd, PM_ADD_DEPENDENT, &req);
			if (ret < 0 && !(kept && keeper)) {
				/*
				 * XXX really should save this and make the test
				 * only if we don't find another entry on the
				 * line, or we should just make tese tuples only
				 */
				(void) fprintf(stderr,
	gettext("%s: (line %d of %s): Cannot set %s dependency between "
		"\"%s\" and \"%s\": "),
	me, lineno, CONF_FILE, type, req.physpath, (char *)req.data);
				perror(NULL);
				free(req.physpath);
				free(req.data);
				find_eol(file);
				ok_to_update_pm = NOTOK;
				return;
			}
		}
		free(req.physpath);
		if (count)
			free(req.data);
		if (token != EOL) {
			(void) fprintf(stderr,
			    gettext("%s (line %d of %s): Unrecognizable "
			    "dependent path \"%s\"\n"), me, lineno, CONF_FILE,
			    buf2);
			find_eol(file);
			ok_to_update_pm = NOTOK;
			return;
		} else {
			if (count == 0) {
				(void) fprintf(stderr, gettext("%s (line %d of "
				    "%s): Must specify at least one dependent "
				    "%s path.\n"), me, lineno, CONF_FILE, type);
				ok_to_update_pm = NOTOK;
				return;
			}
		}
	} else if (strcmp(buf1, "system-threshold") == 0) {
		cmd = PM_SET_SYSTEM_THRESHOLD;
		type = "system";
		token = lex(file, buf2, 1);
		if (token != NUMBER) {
			if (strcmp(buf2, "always-on") != 0) {
				(void) fprintf(stderr,
				    gettext("%s (line %d of %s): Must specify a"
				    " %s threshold number or \"always-on\"\n"),
				    me, lineno, CONF_FILE, type);
				ok_to_update_pm = NOTOK;
				find_eol(file);
				return;
			} else {
				value = INT_MAX;
			}
		} else {
			value = 0;
			ch = buf2;
			if (*ch == '-') {
				neg = -1;
				ch++;
			}
			while (*ch != '\0')
				value = value * 10 + *ch++ - '0';
			value *= neg;
			if (value < 0) {
				(void) fprintf(stderr,
				    gettext("%s (line %d of %s): %s "
				    "Threshold must be positive integer\n"),
				    me, lineno, CONF_FILE, type);
				find_eol(file);
				ok_to_update_pm = NOTOK;
				return;
			}
			value *= getscale(file, buf2);
		}
		/* we ignore the rest of the line */
		(void) ioctl(fd, cmd, value);
		find_eol(file);
		return;
	} else {
		/*
		 * Backwards compatibility mode:
		 */
		process_old_device(file, fd, buf1);
	}
}

static int
other_valid_tokens(FILE *file, char *name)
{
	char	lbuf[1024];
	int	ret = 0;

	if (! has_cpr_perm) {
		ret = ((strcmp(name, "autoshutdown") == 0 ||
			strcmp(name, "ttychars") == 0 ||
			strcmp(name, "loadaverage") == 0 ||
			strcmp(name, "diskreads") == 0 ||
			strcmp(name, "nfsreqs") == 0 ||
			strcmp(name, "idlecheck") == 0) ? 1 : 0);
		if (ret)
			find_eol(file);
		return (ret);
	}

	if (strcmp(name, "ttychars") == 0) {
		get_line(file, lbuf);
		if (sscanf(lbuf, "%d", &cur_cprcfg.ttychars_thold) != 1) {
			(void) fprintf(stderr,
				gettext("%s (line %d): illegal \"ttychars\" "
					"entry \n"), me, (lineno-1));
			ok_to_update_cpr = NOTOK;
		}
		ret++;
	} else if (strcmp(name, "loadaverage") == 0) {
		get_line(file, lbuf);
		if (sscanf(lbuf, "%f", &cur_cprcfg.loadaverage_thold) != 1) {
			(void) fprintf(stderr,
				gettext("%s (line %d): illegal \"loadaverage\""
					"  entry\n"), me, (lineno-1));
			ok_to_update_cpr = NOTOK;
		}
		ret++;
	} else if (strcmp(name, "nfsreqs") == 0) {
		get_line(file, lbuf);
		if (sscanf(lbuf, "%d", &cur_cprcfg.nfsreqs_thold) != 1) {
			(void) fprintf(stderr,
				gettext("%s (line %d): illegal \"nfsreqs\" "
					"entry\n"), me, (lineno-1));
			ok_to_update_cpr = NOTOK;
		}
		ret++;
	} else if (strcmp(name, "diskreads") == 0) {
		get_line(file, lbuf);
		if (sscanf(lbuf, "%d", &cur_cprcfg.diskreads_thold) != 1) {
			(void) fprintf(stderr,
				gettext("%s (line %d): illegal \"diskreads\" "
					"entry \n"), me, (lineno-1));
			ok_to_update_cpr = NOTOK;
		}
		ret++;
	} else if (strcmp(name, "idlecheck") == 0) {
		get_line(file, lbuf);
		if (sscanf(lbuf, "%s", cur_cprcfg.idlecheck_path) != 1) {
			(void) fprintf(stderr,
				gettext("%s (line %d): illegal \"idlecheck\" "
				"entry \n"), me, (lineno-1));
			ok_to_update_cpr = NOTOK;
		}
		ret++;
	} else if (strcmp(name, "autoshutdown") == 0) {
		get_line(file, lbuf);
		if (sscanf(lbuf, "%d%d:%d%d:%d%s",
			&cur_cprcfg.as_idle, &cur_cprcfg.as_sh,
			&cur_cprcfg.as_sm, &cur_cprcfg.as_fh,
			&cur_cprcfg.as_fm, cur_cprcfg.as_behavior) != 6) {
			(void) fprintf(stderr,
				gettext("%s (line %d): illegal \"autoshutdown\""
					"entry\n"), me, (lineno-1));
			ok_to_update_cpr = NOTOK;
		}
		ret++;
	}

	return (ret);
}

static int tokensaved = 0;
static int saved_token = 0;
static token_t
lex(FILE *file, char *cp, int scaled)
{
	register int	ch;
	register int	neg = 0;

	if (tokensaved) {
		tokensaved = 0;	/* set by getscale, cleared here and find_eol */
		return (saved_token);
	}
	while ((ch  = getc(file)) != EOF) {
		if (isspace(ch))
			continue;
		if (ch == '\\') {
			ch = (char)getc(file);
			if (iseol(ch)) {
				lineno++;
				continue;
			} else {
				(void) ungetc(ch, file);
				ch = '\\';
				break;
			}
		}
		break;
	}

	*cp++ = ch;
	if (ch == EOF)
		return (EOFTOK);
	if (iseol(ch)) {
		lineno++;
		return (EOL);
	}
	if (ch == '#') {
		find_eol(file);
		return (EOL);
	}
	if (ch == '(') {
		return (OPAREN);
	}
	if (ch == ')') {
		return (CPAREN);
	}
	if ((ch >= '0' && ch <= '9') || ch == '-') {
		if (ch == '-')
			neg = 1;
		while ((ch = (char)getc(file)) >= '0' && ch <= '9')
			*cp++ = ch;
		if (isdelimiter(ch) || ch == EOF) {
			*cp = '\0';
			(void) ungetc(ch, file);
			return (NUMBER);
		}
		if (ch == ':' && !neg) {
			*cp++ = ch;
			while ((ch = (char)getc(file)) >= '0' && ch <= '9')
				*cp++ = ch;
			if (isdelimiter(ch) || ch == EOF) {
				*cp = '\0';
				(void) ungetc(ch, file);
				return (TIME);
			}
		}
		while ((ch = (char)getc(file)) != EOF && !isdelimiter(ch));
		(void) ungetc(ch, file);
		return (ERROR);
	} else {
		scaled = 0;	/* only applies to numbers */
	}
	while ((ch = (char)getc(file)) != EOF && !isdelimiter(ch))
		*cp++ = ch;
	*cp = '\0';
	(void) ungetc(ch, file);
	return (NAME);
}

static void
find_eol(FILE *file)
{
	register int ch, last = '\0';

	/*
	 * If getscale ate the EOL
	 */
	if (tokensaved && (saved_token == EOL)) {
		tokensaved = 0;
		return;
	}
	do {
		while ((ch = (char)getc(file)) != EOF && !iseol(ch))
			last = ch;
		lineno++;
	} while (last == '\\' && ch != EOF);
	tokensaved = 0;
}

static void
get_line(FILE *file, char *lbuf)
{
	register int ch, last = '\0';
	char	*cp;

	cp = lbuf;
	do {
		while ((ch = (char)getc(file)) != EOF && !iseol(ch)) {
			last = ch;
			*cp++ = (char)ch;
		}
		lineno++;
	} while ((last == '\\' && ch != EOF) && cp--);
	*cp = '\0';
}


/*
 * We ship with certain devices named in the power.conf file.  If the named
 * device does not exist it will generally be beyond the power of the user
 * to do anything about it (e.g. x86 and ppc keyboard/mouse drivers don't
 * support PM yet).
 * So we filter out these, sincd we *do* want to complain about any that the
 * user has added.
 */
static char *default_devices[] = {
"/dev/kbd",
"/dev/mouse",
"/dev/fb",
0
};

static int
is_default_device(char *name)
{
	char **dp = default_devices;

	while (*dp) {
		if (strcmp(*dp++, name) == 0)
			return (1);
	}
	return (0);
}

/*
 * Launch the specified program and wait for it to complete.  We use this
 * to launch powerd since it's faster than system(3s) and we don't need
 * to use any shell syntax.
 */

static int
launch(char *file, char *argv[])
{
	char *fallback_argv[2];
	pid_t pid, p;
	int status;

	if (argv == NULL) {
		fallback_argv[0] = file;
		fallback_argv[1] = NULL;
		argv = fallback_argv;
	}

	if ((pid = fork()) == 0) {
		(void) execv(file, argv);
		exit(EXIT_FAILURE);
		/*NOTREACHED*/
	} else if (pid == -1)
		return (-1);

	do {
		p = waitpid(pid, &status, 0);
	} while (p == -1 && errno == EINTR);

	return ((p == -1) ? -1 : status);
}

static void
process_old_device(FILE *file, int fd, char *buf1)
{
	char		*ch, buf2[MAXNAMELEN];
	pm_request	req;
	token_t		token;
	int		value, ret, cmpt, dep;
	int		neg = 1;

	req.who = buf1;
	req.dependent = buf2;
	ret = -1;

	for (cmpt = 0; (token = lex(file, buf2, 0)) == NUMBER; cmpt++) {
		value = 0;
		ch = buf2;
		if (*ch == '-') {
			neg = -1;
			ch++;
		}
		while (*ch != '\0')
			value = value * 10 + *ch++ - '0';
		req.select = cmpt;
		req.level = value * neg;
		req.size = strlen(req.dependent) + 1;
		if ((ret = ioctl(fd, PM_SET_THRESHOLD, &req)) < 0) {
			break;
		}
	}
	if (ret < 0) {
		if (token != NUMBER && cmpt == 0) {
			(void) fprintf(stderr,
			    gettext("%s (line %d of %s): "), me, lineno,
			    CONF_FILE);
			(void) fprintf(stderr,
			    gettext("Can't find threshold value.\n"));
			ok_to_update_pm = NOTOK;
		} else if ((errno == EINVAL) &&
				(! is_default_device(req.who))) {
			(void) fprintf(stderr,
			    gettext("%s (line %d of %s): "), me, lineno,
			    CONF_FILE);
			(void) fprintf(stderr,
			    gettext("Threshold value error\n"));
			ok_to_update_pm = NOTOK;
		} else {
			/*
			 * Don't complain about the default devices,
			 * as the user can't be expected to do anything
			 * about their absence
			 */
			if (errno != ENODEV && !is_default_device(req.who)) {
				(void) fprintf(stderr,
				    gettext("%s (line %d of %s): "), me, lineno,
				    CONF_FILE);
				(void) fprintf(stderr, gettext("%s: \"%s\": "),
						me, req.who);
				perror(NULL);
				ok_to_update_pm = NOTOK;
			}
		}
		find_eol(file);
		return;
	}
	for (dep = 0; token != EOL && token != EOFTOK; dep++) {
		if (token != NAME) {
			(void) fprintf(stderr,
			    gettext("%s (line %d of %s): Unrecognizable "
			    "dependent name \"%s\"\n"), me, lineno, CONF_FILE,
			    buf2);
			fflush(stderr);
			ok_to_update_pm = NOTOK;
		} else {
			req.size = strlen(req.dependent) + 1;
			if ((ret = ioctl(fd, PM_ADD_DEP, &req)) < 0) {
				fflush(stderr);
				perror(req.who);
				/*
				 * Don't complain about the default devices,
				 * as the user can't be expected to do anything
				 * about their absence
				 */
				if (errno != ENODEV &&
				    !is_default_device(buf2)) {
					(void) fprintf(stderr,
					gettext("%s (line %d of %s): \"%s\": "),
					me, lineno, CONF_FILE, buf2);
					perror(NULL);
					ok_to_update_pm = NOTOK;
				}
			}
		}
		token = lex(file, buf2, 0);
	}
}

static int
process_autopm(FILE *file, int pm_fd, char *errmsg)
{
	char behavior[MAXNAMELEN];
#ifdef sparc
	int prom_fd;
	union {
		char buf[PROM_BUFSZ + sizeof (uint_t)];
		struct openpromio opp;
	} oppbuf;
	register struct openpromio *opp = &(oppbuf.opp);
#endif

	if (lex(file, behavior, 0) != NAME) {
		strcpy(errmsg, gettext("Entry should start with \"autopm\"."));
		return (-1);
	}

	if (strcmp(behavior, "enable") == 0) {
		if ((ioctl(pm_fd, PM_START_PM, NULL) < 0) &&
		    (errno != EBUSY)) {
			strcpy(errmsg, strerror(errno));
			ok_to_update_pm = NOTOK;
			return (-1);
		}
		strcpy(cur_cprcfg.apm_behavior, "enable");
	} else if (strcmp(behavior, "disable") == 0) {
		if ((ioctl(pm_fd, PM_STOP_PM, NULL) < 0) &&
		    (errno != EINVAL)) {
			strcpy(errmsg, strerror(errno));
			ok_to_update_pm = NOTOK;
			return (-1);
		}
		strcpy(cur_cprcfg.apm_behavior, "disable");
	} else if (strcmp(behavior, "default") == 0) {
#ifdef sparc
		if (estar_vers == estar_v3) {
			if ((ioctl(pm_fd, PM_START_PM, NULL) < 0) &&
			    (errno != EBUSY)) {
				sprintf(errmsg, gettext("PM_START_PM failed: "
				    "%s"), strerror(errno));
				ok_to_update_pm = NOTOK;
				return (-1);
			}
		}
#endif
		strcpy(cur_cprcfg.apm_behavior, behavior);
	} else {
		sprintf(errmsg, gettext("Invalid \"%s\" behavior."), behavior);
		ok_to_update_pm = NOTOK;
		return (-1);
	}

	return (0);
}

static int
getscale(FILE *file, char *cp)
{
	int token;
	int scale;

	/*
	 * This is a horrible hack, but so is having your own lex
	 * This function uses global variables to influence the behavior of
	 * lex(), as does find_eol()
	 */
	token = lex(file, cp, 0);
	if (token == NAME) {
		switch (*cp) {
		case 'h':
		case 'H':
			scale = 3600;
			break;
		case 'm':
		case 'M':
			scale = 60;
			break;
		case 's':
		case 'S':
			scale = 1;
			break;
		}
		tokensaved = 0;
	} else {
		scale = 1;
		tokensaved = 1;
		saved_token = token;
	}
	return (scale);
}

/*
 * Reads a ')' terminated list of scaled numbers for "device-thresholds".
 * Returns the numbe of threshold entries found, or 0 for failure.
 * reallocs the buffer to be big enough to hold all the entries and inserts
 * them starting at the indicated index + 1, storing the number of entries
 * at the index location
 */
static int
collect_comp(FILE *file, char *cp, int **dp, size_t *ds, int indx)
{
	int nthresh = 0;
	int value;
	char *ch;
	int token;
	int neg = 1;

	for (nthresh = 0; (token = lex(file, cp, 1)) == NUMBER; nthresh++) {
		value = 0;
		ch = cp;
		if (*ch == '-') {
			neg = -1;
			ch++;
		}
		while (*ch != '\0')
			value = value * 10 + *ch++ - '0';
		value *= neg;
		value *= getscale(file, cp);

		*ds += sizeof (int);			/* buffer size */
		*dp = realloc(*dp, *ds);
		if (*dp == NULL) {
			(void) fprintf(stderr, gettext("%s (line %d of %s): "
			    "Out of memory\n"), me, lineno, CONF_FILE);
			free(*dp);
			find_eol(file);
			return (0);
		}
		(*dp)[indx + nthresh + 1] = value;
	}
	if (token != CPAREN) {
		(void) fprintf(stderr, gettext("%s (line %d of %s): Can't find "
		    "threshold value.\n"), me, lineno, CONF_FILE);
		free(*dp);
		find_eol(file);
		return (0);
	}
	(*dp)[indx] = nthresh;
	return (nthresh);
}

/*
 * Convert a symbolic link into /devices into the appropriate physical path,
 * and trim leading /devices and trailing minor string from resolved path.
 * If not a symbolic link into /devices, fail if there is a minor string in
 * paths provided.  Returns non-zero if an error occurs.  Caller must dispose
 * of the rest of the line, etc.
 */
static int
process_path(char **pp)
{
	char pathbuf[PATH_MAX+1];
	char *rp, *cp;
	struct stat st;

	st.st_rdev = 0;
	/* if we have a link to a device file */
	if (stat(*pp, &st) == 0 && st.st_rdev != 0) {
		if (realpath(*pp, pathbuf) == NULL) {
			/* this sequence can't happen */
			(void) fprintf(stderr, gettext("%s: (line %d of %s): "
				"Cannot convert %s to real path."),
				me, lineno, CONF_FILE, *pp);
			perror(*pp);
			return (1);
		}
		if (strncmp("/devices", pathbuf, strlen("/devices")) == 0) {
			cp = pathbuf + strlen("/devices");
			if ((rp = strchr(cp, ':')) != NULL)
				*rp = '\0';
			*pp = realloc(*pp, strlen(cp) + 1);
			if (*pp == NULL) {
				(void) fprintf(stderr,
				    gettext("%s (line %d of %s): Out of "
				    "memory\n"), me, lineno, CONF_FILE);
				return (1);
			}
			(void) strcpy(*pp, cp);
			return (0);
		}
	} else {
		if ((cp = strchr(*pp, ':')) != NULL) {
			(void) fprintf(stderr, gettext("%s (line %d of %s): "
			    "Physical path may not contain minor string "
			    "(%s)\n"), me, lineno, CONF_FILE, cp);
			return (1);
		} else {
			return (0);	/* use what was passed in */
		}
	}
}


static int
isblank(char *lbuf)
{
	int	SPACE = (int)' ';
	char	*cp;
	int	ret = 1;

	cp = lbuf;
	while (*cp != '\0') {
		if ((int)*cp > SPACE) {
			ret = 0;
			break;
		}
		cp++;
	}

	return (ret);
}


/*
 * check_perm() reads /etc/default/power file to determine whether
 * and what permission user has.  It sets the global variable
 * has_pm_perm/has_cpr_perm to 1 if user has pm/cpr permission,
 * 0 if user does not have pm/cpr permission.
 */
static void
check_perms()
{
	char	*perms_str;
	char	user[NMAX];

	user_uid = getuid();
	(void) strncpy(user, (getpwuid(user_uid))->pw_name, NMAX);
	if (user_uid == 0) {
		has_cpr_perm = 1;
		has_pm_perm = 1;
		return;
	}

	/*
	 * possible PMCHANGEPERM  and CPRCHANGEPERM entries are:
	 *    all			(all users + root)
	 *    -				(none + root)
	 *    <user1<delimeter>...>	(list users + root)
	 *    console-owner		(console onwer + root. default)
	 * Any error in reading/parsing the file allows only root
	 * to use the program
	 */

	if (defopen(DEFAULTFILE) == 0) {
		if ((perms_str = defread("PMCHANGEPERM=")) != NULL) {

			if (strcmp("all", perms_str) == 0) {
				has_pm_perm = 1;
			} else if (strcmp("console-owner", perms_str) == 0) {
				has_pm_perm = has_cnowner_perms(user_uid)
					? 1 : 0;
			} else if (has_userlist_perms(user, perms_str)) {
				has_pm_perm = 1;
			}
		}

		if ((perms_str = defread("CPRCHANGEPERM=")) != NULL) {

			if (strcmp("all", perms_str) == 0) {
				has_cpr_perm = 1;
			} else if (strcmp("console-owner", perms_str) == 0) {
				has_cpr_perm = has_cnowner_perms(user_uid)
					? 1 : 0;
			} else if (has_userlist_perms(user, perms_str)) {
				has_cpr_perm = 1;
			}
		}

		(void) defopen((char *)NULL);
		return;
	} else {
		has_cpr_perm = has_pm_perm = 0;
	}
}


static int
has_cnowner_perms(uid_t user_uid)
{
	struct	stat	sbuf;

	if (stat(CONSOLE, &sbuf)) {
		return (0);
	} else {
		return (user_uid == sbuf.st_uid);
	}
}

#define	BEGIN_TOKEN	'<'
#define	END_TOKEN	'>'

static int
has_userlist_perms(char *user, char *list)
{
	char	*tokenp;

	if (*list == BEGIN_TOKEN) {
		list = list + 1;
		if ((tokenp = strrchr(list, END_TOKEN)) != NULL) {
			*tokenp = '\0';
			while ((tokenp = strtok(list, ", ")) != NULL) {
				list = NULL;
				if (strcmp(user, tokenp) == 0) {
					return (1);
				}
			}
		}
	}
	return (0);
}


/*
 * find_estarv() returns the Energy Star version information that's
 * stored in bootprom.
 * Note: A better way to do is by calling libdevinfo functions to
 * extract the information from a snapshot of the device tree.
 *
 * Return value:
 *   no_estar - not Energy Star compliant,
 *   estar_v2 - Energy Star V2 compliant,
 *   estar_v3 - Energy Star V3 compliant.
 */
#ifdef sparc
static estar_version_t
find_estarv()
{
	int prom_fd;
	union {
		char buf[PROM_BUFSZ + sizeof (uint_t)];
		struct openpromio opp;
	} oppbuf;
	register struct openpromio *opp;
	estar_version_t vers = no_estar;

	opp = &(oppbuf.opp);
	if ((prom_fd = open(OPENPROM, O_RDONLY)) < 0) {
		fprintf(stderr, gettext("%s: Can't open %s: "), me, OPENPROM);
		perror(NULL);
		return (vers);
	}

	(void *) memset((void *)&oppbuf, 0, (PROM_BUFSZ + sizeof (uint_t)));
	opp->oprom_size = sizeof (int);
	if (ioctl(prom_fd, OPROMNEXT, opp) < 0) {
		fprintf(stderr, gettext("%s: Can't find prom's root: "), me);
		perror(NULL);
		close(prom_fd);
		return (vers);
	}

	(void *) memset((void *)&oppbuf, 0, (PROM_BUFSZ + sizeof (uint_t)));
	do {
		opp->oprom_size = PROM_BUFSZ;
		if (ioctl(prom_fd, OPROMNXTPROP, opp) < 0) {
			fprintf(stderr, gettext("%s: ioctl OPROMNXTPROP: "),
				me);
			perror(NULL);
			close(prom_fd);
			return (vers);
		}
		if (strcmp(opp->oprom_array, ESTAR_V2_PROP) == 0) {
			vers = estar_v2;
			break;
		} else if (strcmp(opp->oprom_array, ESTAR_V3_PROP) == 0) {
			vers = estar_v3;
			break;
		}
	} while (opp->oprom_size > 0);

	close(prom_fd);
	return (vers);
}
#endif sparc

/*
 * update_cprconfig() updates the pm and/or cpr part of /etc/.cpr_config
 * file depending on what permssion user has.
 *
 * Return value:
 *    0 - Upon success,
 *   -1 - Upon failure.
 */
static int
update_cprconfig()
{
	int	fd;
	struct cprconfig *cf;

	cf = &cur_cprcfg;

	/*
	 * if only has CPRCHANGEPERM, there's only one entry that shouldn't
	 * get updated
	 */
	if (! has_pm_perm) {
		if (ok_to_update_cpr) {
			if (strcmp(cur_cprcfg.apm_behavior,
				old_cprcfg.apm_behavior))
			(void) strcpy(cur_cprcfg.apm_behavior,
				old_cprcfg.apm_behavior);
		}
		else
			return (-1);
	}
	/*
	 * if only has PMCHANGEPERM, there's only one entry to update
	 */
	else if (! has_cpr_perm) {
		if (ok_to_update_pm) {
			if (strcmp(cur_cprcfg.apm_behavior,
				old_cprcfg.apm_behavior))
			(void) strcpy(old_cprcfg.apm_behavior,
				cur_cprcfg.apm_behavior);
			cf = &old_cprcfg;
		}
		else
			return (-1);
	} else if (! (ok_to_update_pm && ok_to_update_cpr))
		return (-1);

	if ((fd = open(CPR_CONFIG, O_CREAT | O_WRONLY, 0644)) == -1) {
		fprintf(stderr, gettext("%s: Can't open %s to update "
			"changes: "), me, CPR_CONFIG);
		perror(NULL);
		return (-1);
	}

	if (write(fd, cf, sizeof (struct cprconfig)) !=
		sizeof (struct cprconfig)) {
		fprintf(stderr, gettext("%s: error writing %s: "),
			me, CPR_CONFIG);
		perror(NULL);
		close(fd);
		return (-1);
	}

	close(fd);
	return (0);
}

/*
 * redo_powerd() makes 'powerd' daemon process re-read the updated
 * /etc/.cpr_config file by sending a SIGHUP signal to the process.
 * If there isn't a 'powerd' running, it'll start one.
 *
 * Return value:
 *   0 - Upon success,
 *  -1 - Upon failure.
 */
static int
redo_powerd()
{
	if (daemon_pid == (pid_t *)-1) {
		if (launch("/usr/lib/power/powerd", NULL) == -1) {
			(void) fprintf(stderr,
				gettext("%s: Failed to start powerd: "), me);
			perror(NULL);
			return (-1);
		}

	} else if (sigsend(P_PID, *daemon_pid, SIGHUP)) {
		if (errno == ESRCH) {
			if (launch("/usr/lib/power/powerd",
			    NULL) == -1) {
				(void) fprintf(stderr,
					gettext("%s: Failed to start powerd: "),
					me);
				perror(NULL);
				return (-1);
			}
		} else {
			(void) fprintf(stderr,
				gettext("%s: Error sending signal to powerd: "),
				me);
			perror(NULL);
			return (-1);
		}
	}
	return (0);
}

/*
 * write_pwrcfg(FILE *ofile, FILE *nfile) applies only to "-f file"
 * option, where 'ofile' points to current /etc/power.conf file, and
 * 'nfile' points to user specified new file.  This procedure
 * writes a subset of command lines from 'nfile' upon which user has
 * proper permission to change to 'ofile', while reserves the rest
 * of the command lines in 'ofile' that user doesn't have permission
 * to change.
 * Upon successful write, all comments in current power.conf file will
 * be lost. The procedure only intends to save the original power.conf
 * file that comes with system installation for user's reference.
 *
 * Return value:
 *   0 - Upon success,
 *  -1 - Upon failure.
 */
static int
write_pwrcfg(FILE *ofile, FILE *nfile)
{
	FILE	*savfile;
	FILE	*tfile, *pmfile, *cprfile;
	char	tname[64];
	char	nmtok[MAXNAMELEN];
	char	lbuf[2048];
	char	ch;
	char	*cp, *l;
	struct stat st;
	int i;

	if ((has_cpr_perm && ! ok_to_update_cpr) ||
	    (has_pm_perm && ! ok_to_update_pm))
		return (-1);

	/*
	 * Attempt to save the power.conf file that comes from OS
	 * installation
	 */
	if ((stat("/etc/power.conf-Orig", &st) < 0) || (st.st_size == 0)) {
		if ((savfile = fopen("/etc/power.conf-Orig", "w+")) != NULL) {
			while ((ch = (char)getc(ofile)) != EOF)
				putc(ch, savfile);
			fflush(savfile);
			fclose(savfile);
		}
	}

	sprintf(tname, "/etc/tmpwrcf.%d", getpid());

	if ((tfile = fopen(tname, "w+")) == NULL) {
		fprintf(stderr, "%s: Can't open %s for write: ", me, tname);
		perror(NULL);
		return (-1);
	}

	pmfile = nfile;
	cprfile = nfile;
	if (! has_pm_perm)
		pmfile = ofile;
	if (! has_cpr_perm)
		cprfile = ofile;

	for (i = 0; i < (sizeof (header_str) / sizeof (char *)); i++)
		(void) fprintf(tfile, "%s\n", header_str[i]);

	rewind(cprfile);
	/* write as portion to new power.conf file */
	while ((ch = getc(cprfile)) != EOF) {
		if (ch == '#') {
			find_eol(cprfile);
			continue;
		}

		(void) ungetc(ch, cprfile);
		get_line(cprfile, lbuf);

		if (isblank(lbuf))
			continue;

		l = lbuf;
		cp = nmtok;

		while (!isspace(*l) && !iseol(*l))
			*cp++ = *l++;
		*cp = '\0';

		if (strcmp(nmtok, "autoshutdown") == 0) {
			(void) fprintf(tfile, "\n%s\n", as_cmt);
			(void) fprintf(tfile, "%s\n\n", lbuf);
			continue;
		} else if (strcmp(nmtok, "statefile") == 0) {
			(void) fprintf(tfile, "\n%s\n", statefile_cmt);
			(void) fprintf(tfile, "%s\n\n", lbuf);
			continue;
		} else if (strcmp(nmtok, "diskreads") == 0 ||
			strcmp(nmtok, "idlecheck") == 0 ||
			strcmp(nmtok, "loadaverage") == 0 ||
			strcmp(nmtok, "nfsreqs") == 0 ||
			strcmp(nmtok, "ttychars") == 0) {
			;
		}
		else
			continue;

		(void) fprintf(tfile, "%s\n", lbuf);
	}

	rewind(pmfile);
	/* write pm portion to new power.conf file */
	while ((ch = getc(pmfile)) != EOF) {
		if (ch == '#') {
			find_eol(pmfile);
			continue;
		}

		(void) ungetc(ch, pmfile);
		get_line(pmfile, lbuf);

		if (isblank(lbuf))
			continue;

		l = lbuf;
		cp = nmtok;

		while (!isspace(*l) && !iseol(*l))
			*cp++ = *l++;
		*cp = '\0';

		if (strcmp(nmtok, "autopm") == 0 ||
			strcmp(nmtok, "device-thresholds") == 0 ||
			strcmp(nmtok, "device-dependency") == 0 ||
			strcmp(nmtok, "system-threshold") == 0) {
			(void) fprintf(tfile, "%s\n\n", lbuf);
			continue;
		} else if (strcmp(nmtok, "autoshutdown") == 0 ||
			strcmp(nmtok, "statefile") == 0 ||
			strcmp(nmtok, "diskreads") == 0 ||
			strcmp(nmtok, "idlecheck") == 0 ||
			strcmp(nmtok, "loadaverage") == 0 ||
			strcmp(nmtok, "nfsreqs") == 0 ||
			strcmp(nmtok, "ttychars") == 0) {

			continue;
		}
		/*
		 * accept the rest
		 */

		(void) fprintf(tfile, "%s\n", lbuf);

	} /* pm while */

	fflush(tfile);
	fclose(tfile);

	if (rename(tname, "/etc/power.conf") < 0) {
		fprintf(stderr, "%s: Can't rename %s to /etc/power.conf: ",
			me, tname);
		perror(NULL);
		unlink(tname);
		return (-1);
	}

	return (0);
}
