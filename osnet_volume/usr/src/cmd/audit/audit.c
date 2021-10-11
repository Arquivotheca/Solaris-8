/*
#ifndef lintdded SID
static	char cmw_sccsid[] = "@(#)audit.c 1.9 92/01/30 SMI; SunOS CMW";
static  char bsm_sccsid[] = "@(#)audit.c 4.5.1.1 91/09/08 SMI; BSM Module
";
static  char mls_sccsid[] = "@(#)audit.c 3.4 90/11/03 SMI; SunOS MLS";
#endif
*/
#ident	"@(#)audit.c	1.9	97/11/14	SMI"	/* BSM */

#include <stdlib.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#include <bsm/audit.h>
#include <bsm/libbsm.h>
#include <locale.h>

#if !defined(TEXT_DOMAIN)
#define TEXT_DOMAIN "SUNW_OST_OSCMD"
#endif

extern int	kill();

/* GLOBALS */
static char	*auditdatafile = AUDITDATAFILE;
static char	*progname = "audit";
static char	*usage = "audit [-n] | [-s] | [-t]";
static int	silent = 0;

static int	get_auditd_pid();


/*
 * audit() - This program serves as a general administrator's interface to
 *	the audit trail.
 *
 * input:
 *	audit -d username(s)
 *		- change kernel version of event flags for "username{s}"
 *		- based on passwd adjunct and audit_control file. (OBSOLETE)
 *	audit -s
 *		- signal audit daemon to read audit_control file.
 *	audit -n
 *		- signal audit daemon to use next audit_control audit directory.
 *	audit -u username auditflags
 *		- change kernel version of users audit flags.  (OBSOLETE)
 *	audit -t
 *		- signal audit daemon to disable auditing.
 *	audit -T
 *		- signal audit daemon to disable auditing report no errors.
 *
 * output:
 *
 * returns:	0 - command successful
 *		>0 - command failed
 */

main(argc, argv)
int	argc;
char	*argv[];
{
	pid_t pid; /* process id of auditd read from auditdatafile */
	int	sig; /* signal to send auditd */

	/* Internationalization */
	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);

	if (getuid() != 0) {
		(void) fprintf(stderr, gettext("%s: not super-user\n"),
			progname);
		exit(2);
	}

	if (argc != 2 || argv[1][0] != '-' || argv[1][2] != '\0') {
		(void) fprintf(stderr, gettext("usage: %s\n"), usage);
		exit(3);
	}

	switch (argv[1][1]) {
	case 'n':
		sig = SIGUSR1;
		break;
	case 's':
		sig = SIGHUP;
		break;
	case 't':
		sig = SIGTERM;
		break;
	case 'T':
		sig = SIGTERM;
		silent = 1;
		break;
	case 'd':
	case 'u':
		(void) fprintf(stderr, "%s: %s\n", progname, gettext(
			"-d and -u are obsolete, use auditconfig(1m)"));
		exit(5);
		break;
	default:
		(void) fprintf(stderr, gettext("usage: %s\n"), usage);
		exit(6);
	}

	if (get_auditd_pid(&pid) != 0) {
		if (silent)
			exit(0);
		else {
			(void) fprintf(stderr, "%s: %s\n", progname, gettext(
			"can't get process id of auditd from audit_data(4)"));
			exit(4);
		}
	}

	if (kill(pid, sig) != 0) {
		if (silent)
			exit(0);
		else {
			perror(progname);
			(void) fprintf(stderr,
			    gettext("%s: cannot signal auditd\n"), progname);
			exit(1);
		}
	}

	exit(0);
}


/*
 * get_auditd_pid():
 *
 * returns:	0 - successful
 *		1 - error
 */

static int
get_auditd_pid(p_pid)
pid_t *p_pid;		/* id of audit daemon */
{
	FILE * adp;		/* audit_data file pointer */
	int	retstat;

	if ((adp = fopen(auditdatafile, "r")) == NULL) {
		if (!silent)
			perror(progname);
		return (1);
	}
	retstat = fscanf(adp, "%d", p_pid) != 1;
	(void) fclose(adp);
	return (retstat);
}
