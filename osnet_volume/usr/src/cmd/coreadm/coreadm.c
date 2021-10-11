#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <libintl.h>
#include <locale.h>
#include <sys/stat.h>
#include <sys/corectl.h>

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)coreadm.c	1.2	99/06/02 SMI"

/*
 * Usage:
 * 	coreadm [ -g pattern ] [ -i pattern ]
 * 		[ -e {global | process | global-setid | proc-setid | log} ]
 * 		[ -d {global | process | global-setid | proc-setid | log} ]
 * 	coreadm [ -p pattern ] [ pid ... ]
 * 	coreadm -u
 */

#define	E_SUCCESS	0		/* Exit status for success */
#define	E_ERROR		1		/* Exit status for error */
#define	E_USAGE		2		/* Exit status for usage error */

static	const	char	PATH_CONFIG[] = "/etc/coreadm.conf";
#define	CF_OWNER	0				/* Uid 0 (root) */
#define	CF_GROUP	1				/* Gid 1 (other) */
#define	CF_PERM	(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)	/* Mode 0644 */

static	char	*command;
static	char	*glob_pattern;
static	size_t	glob_size;
static	char	*init_pattern;
static	size_t	init_size;
static	char	*proc_pattern;
static	size_t	proc_size;
static	int	enable;
static	int	disable;

static	int	report_settings(void);
static	int	do_processes(int argc, char **argv);
static	int	do_modify(void);
static	int	do_update(void);
static	int	write_config(void);

static void
usage()
{
	(void) fprintf(stderr, gettext(
"usage:\n"));
	(void) fprintf(stderr, gettext(
"    %s [ -g pattern ] [ -i pattern ]\n"), command);
	(void) fprintf(stderr, gettext(
"            [ -e {global | process | global-setid | proc-setid | log} ]\n"));
	(void) fprintf(stderr, gettext(
"            [ -d {global | process | global-setid | proc-setid | log} ]\n"));
	(void) fprintf(stderr, gettext(
"    %s [ -p pattern ] [ pid ... ]\n"), command);
	(void) fprintf(stderr, gettext(
"    %s -u\n"), command);
	exit(E_USAGE);
}

int
main(int argc, char **argv)
{
	int rc;
	int flag;
	int opt;
	int modify;
	int update = 0;
	int error = 0;

	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);

	/* command name (e.g., "coreadm") */
	if ((command = strrchr(argv[0], '/')) != NULL)
		command++;
	else
		command = argv[0];

	while ((opt = getopt(argc, argv, "g:i:p:e:d:u?")) != EOF) {
		switch (opt) {
		case 'g':
			glob_pattern = optarg;
			glob_size = strlen(glob_pattern) + 1;
			break;
		case 'i':
			init_pattern = optarg;
			init_size = strlen(init_pattern) + 1;
			break;
		case 'p':
			proc_pattern = optarg;
			proc_size = strlen(proc_pattern) + 1;
			break;
		case 'e':
		case 'd':
			if (strcmp(optarg, "global") == 0)
				flag = CC_GLOBAL_PATH;
			else if (strcmp(optarg, "process") == 0)
				flag = CC_PROCESS_PATH;
			else if (strcmp(optarg, "global-setid") == 0)
				flag = CC_GLOBAL_SETID;
			else if (strcmp(optarg, "proc-setid") == 0)
				flag = CC_PROCESS_SETID;
			else if (strcmp(optarg, "log") == 0)
				flag = CC_GLOBAL_LOG;
			else {
				flag = 0;
				error = 1;
			}
			if (opt == 'e') {
				enable |= flag;
				disable &= ~flag;
			} else {
				disable |= flag;
				enable &= ~flag;
			}
			break;
		case 'u':
			update = 1;
			break;
		case '?':
		default:
			error = 1;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (error)
		usage();

	/*
	 * If 'modify' is true, we must modify the system settings
	 * and update the configuration file with the new parameters.
	 */
	modify = glob_pattern != NULL ||
		init_pattern != NULL ||
		(enable|disable) != 0;

	if ((modify|update) && geteuid() != 0) {
		(void) fprintf(stderr,
		    gettext(
		    "%s: you must be root to exercise the -[giedu] options\n"),
		    command);
		return (E_USAGE);
	}
	if (update && (modify || proc_pattern != NULL || argc != 0)) {
		(void) fprintf(stderr,
		    gettext(
		    "%s: the -u option must stand alone\n"),
		    command);
		usage();
	}
	if (modify && proc_pattern != NULL) {
		(void) fprintf(stderr,
		    gettext(
		    "%s: -[gied] and -p options are mutually exclusive\n"),
		    command);
		usage();
	}
	if (modify && argc != 0) {
		(void) fprintf(stderr,
		    gettext(
		    "%s: -[gied] options cannot have a process-id list\n"),
		    command);
		usage();
	}
	if (proc_pattern != NULL && argc == 0) {
		(void) fprintf(stderr,
		    gettext(
		    "%s: -p option requires a list of process-ids\n"),
		    command);
		usage();
	}

	if (update)
		rc = do_update();
	else if (modify)
		rc = do_modify();
	else if (argc != 0)
		rc = do_processes(argc, argv);
	else
		rc = report_settings();

	return (rc);
}

static int
report_settings()
{
	int options;
	char global_path[PATH_MAX];
	char init_path[PATH_MAX];

	if ((options = core_get_options()) == -1) {
		perror("core_get_options()");
		return (E_ERROR);
	}
	if (core_get_global_path(global_path, PATH_MAX) != 0) {
		perror("core_get_global_path()");
		return (E_ERROR);
	}
	if (core_get_process_path(init_path, PATH_MAX, 1)) {
		perror("core_get_process_path()");
		return (E_ERROR);
	}
	(void) printf(gettext("     global core file pattern: %s\n"),
		global_path);
	(void) printf(gettext("       init core file pattern: %s\n"),
		init_path);
	(void) printf(gettext("            global core dumps: %s\n"),
		(options & CC_GLOBAL_PATH)? "enabled" : "disabled");
	(void) printf(gettext("       per-process core dumps: %s\n"),
		(options & CC_PROCESS_PATH)? "enabled" : "disabled");
	(void) printf(gettext("      global setid core dumps: %s\n"),
		(options & CC_GLOBAL_SETID)? "enabled" : "disabled");
	(void) printf(gettext(" per-process setid core dumps: %s\n"),
		(options & CC_PROCESS_SETID)? "enabled" : "disabled");
	(void) printf(gettext("     global core dump logging: %s\n"),
		(options & CC_GLOBAL_LOG)? "enabled" : "disabled");
	return (0);
}

static int
do_processes(int argc, char **argv)
{
	char process_path[PATH_MAX];
	pid_t pid;
	char *next;
	int rc = 0;

	if (proc_pattern == NULL) {
		while (argc-- > 0) {
			pid = strtol(*argv, &next, 10);
			if (*next != '\0' || !isdigit(**argv)) {
				(void) fprintf(stderr,
					gettext("%s: invalid process-id\n"),
					*argv);
				rc = E_USAGE;
			} else if (core_get_process_path(process_path,
			    PATH_MAX, pid)) {
				perror(*argv);
				rc = E_USAGE;
			} else {
				(void) printf(gettext("%s:\t%s\n"),
					*argv, process_path);
			}
			argv++;
		}
	} else {
		while (argc-- > 0) {
			pid = strtol(*argv, &next, 10);
			if (*next != '\0') {
				(void) fprintf(stderr,
					gettext("%s: invalid process-id\n"),
					*argv);
				rc = E_USAGE;
			} else if (core_set_process_path(proc_pattern,
			    proc_size, pid)) {
				perror(*argv);
				rc = E_USAGE;
			}
			argv++;
		}
	}

	return (rc);
}

static int
do_modify()
{
	int options;

	if ((options = core_get_options()) == -1) {
		perror("core_get_options()");
		return (E_ERROR);
	}
	options |= enable;
	options &= ~disable;
	if (core_set_options(options) != 0) {
		perror("core_set_options()");
		return (E_ERROR);
	}
	if (glob_pattern != NULL &&
	    core_set_global_path(glob_pattern, glob_size) != 0) {
		perror("core_set_global_path()");
		return (E_ERROR);
	}
	if (init_pattern != NULL &&
	    core_set_process_path(init_pattern, init_size, 1) != 0) {
		perror("core_set_process_path()");
		return (E_ERROR);
	}
	return (write_config());
}

/*
 * BUFSIZE must be large enough to contain the longest path plus some more.
 */
#define	BUFSIZE	(PATH_MAX + 80)

static int
yes(char *name, char *value, int line)
{
	if (strcmp(value, "yes") == 0)
		return (1);
	if (strcmp(value, "no") == 0)
		return (0);
	(void) fprintf(stderr,
		gettext(
		"\"%s\", line %d: warning: value must be yes or no: %s=%s\n"),
		PATH_CONFIG, line, name, value);
	return (0);
}

static int
do_update()
{
	FILE *fp;
	int line;
	int options;
	char gpattern[PATH_MAX];
	char ipattern[PATH_MAX];
	char buf[BUFSIZE];
	char name[BUFSIZE], value[BUFSIZE];
	int n;
	int len;

	/* defaults */
	options = CC_PROCESS_PATH;
	gpattern[0] = '\0';
	(void) strcpy(ipattern, "core");

	if ((fp = fopen(PATH_CONFIG, "r")) == NULL) {
		/*
		 * No config file, just accept the current settings.
		 */
		return (write_config());
	}

	for (line = 1; fgets(buf, BUFSIZE, fp) != NULL; line++) {
		/*
		 * Skip comment lines and empty lines.
		 */
		if (buf[0] == '#' || buf[0] == '\n')
			continue;
		/*
		 * Look for "name=value", with optional whitespace on either
		 * side, terminated by a newline, and consuming the whole line.
		 */
		n = sscanf(buf, " %[^=]=%s \n%n", name, value, &len);
		if (n >= 1 && name[0] != '\0' &&
		    (n == 1 || len == strlen(buf))) {
			if (n == 1)
				value[0] = '\0';
			if (strcmp(name, "COREADM_GLOB_PATTERN") == 0) {
				(void) strcpy(gpattern, value);
				continue;
			}
			if (strcmp(name, "COREADM_INIT_PATTERN") == 0) {
				(void) strcpy(ipattern, value);
				continue;
			}
			if (strcmp(name, "COREADM_GLOB_ENABLED") == 0) {
				if (yes(name, value, line))
					options |= CC_GLOBAL_PATH;
				continue;
			}
			if (strcmp(name, "COREADM_PROC_ENABLED") == 0) {
				if (yes(name, value, line))
					options |= CC_PROCESS_PATH;
				continue;
			}
			if (strcmp(name, "COREADM_GLOB_SETID_ENABLED") == 0) {
				if (yes(name, value, line))
					options |= CC_GLOBAL_SETID;
				continue;
			}
			if (strcmp(name, "COREADM_PROC_SETID_ENABLED") == 0) {
				if (yes(name, value, line))
					options |= CC_PROCESS_SETID;
				continue;
			}
			if (strcmp(name, "COREADM_GLOB_LOG_ENABLED") == 0) {
				if (yes(name, value, line))
					options |= CC_GLOBAL_LOG;
				continue;
			}
			(void) fprintf(stderr,
				gettext(
			"\"%s\", line %d: warning: invalid token: %s\n"),
				PATH_CONFIG, line, name);
		} else {
			(void) fprintf(stderr,
				gettext("\"%s\", line %d: syntax error\n"),
				PATH_CONFIG, line);
		}
	}
	(void) fclose(fp);
	if (core_set_options(options) != 0) {
		perror("core_set_options()");
		return (E_ERROR);
	}
	if (core_set_global_path(gpattern, strlen(gpattern)+1) != 0) {
		perror("core_set_global_path()");
		return (E_ERROR);
	}
	if (core_set_process_path(ipattern, strlen(ipattern)+1, 1) != 0) {
		perror("core_set_process_path()");
		return (E_ERROR);
	}
	return (write_config());
}

static int
write_config()
{
	int fd;
	FILE *fp;
	int options;
	char global_path[PATH_MAX];
	char init_path[PATH_MAX];

	if ((options = core_get_options()) == -1) {
		perror("core_get_options()");
		return (E_ERROR);
	}
	if (core_get_global_path(global_path, PATH_MAX) != 0) {
		perror("core_get_global_path()");
		return (E_ERROR);
	}
	if (core_get_process_path(init_path, PATH_MAX, 1)) {
		perror("core_get_process_path()");
		return (E_ERROR);
	}
	if ((fd = open(PATH_CONFIG, O_WRONLY|O_CREAT|O_TRUNC, CF_PERM)) == -1) {
		(void) fprintf(stderr,
			gettext("failed to open %s"), PATH_CONFIG);
		return (E_ERROR);
	}
	if ((fp = fdopen(fd, "w")) == NULL) {
		(void) fprintf(stderr,
			gettext("failed to open stream for %s"), PATH_CONFIG);
		return (E_ERROR);
	}
	(void) fputs(
		"#\n"
		"# coreadm.conf\n"
		"#\n"
		"# Parameters for system core file configuration.\n"
		"# Do NOT edit this file by hand -- use coreadm(1) instead.\n"
		"#\n",
		fp);

	(void) fprintf(fp, "COREADM_GLOB_PATTERN=%s\n",
		global_path);
	(void) fprintf(fp, "COREADM_INIT_PATTERN=%s\n",
		init_path);
	(void) fprintf(fp, "COREADM_GLOB_ENABLED=%s\n",
		(options & CC_GLOBAL_PATH)? "yes" : "no");
	(void) fprintf(fp, "COREADM_PROC_ENABLED=%s\n",
		(options & CC_PROCESS_PATH)? "yes" : "no");
	(void) fprintf(fp, "COREADM_GLOB_SETID_ENABLED=%s\n",
		(options & CC_GLOBAL_SETID)? "yes" : "no");
	(void) fprintf(fp, "COREADM_PROC_SETID_ENABLED=%s\n",
		(options & CC_PROCESS_SETID)? "yes" : "no");
	(void) fprintf(fp, "COREADM_GLOB_LOG_ENABLED=%s\n",
		(options & CC_GLOBAL_LOG)? "yes" : "no");

	(void) fflush(fp);
	(void) fsync(fd);
	(void) fchmod(fd, CF_PERM);
	(void) fchown(fd, CF_OWNER, CF_GROUP);
	(void) fclose(fp);

	return (0);
}
