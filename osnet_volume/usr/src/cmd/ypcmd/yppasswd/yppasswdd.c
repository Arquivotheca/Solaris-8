/*
 * Copyright (c) 1985-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)yppasswdd.c	1.18	99/04/27 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>
#include <crypt.h>
#include <errno.h>
#include <tiuser.h>
#include <netdir.h>
#include <pwd.h>
#include <shadow.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpcsvc/yppasswd.h>
#include <netconfig.h>

/* must match sizes in passwd */
#define	STRSIZE 100

#define	DEFDIR "/etc/"
#define	MYPASSWD "passwd"
#define	MYSHADOW "shadow"

/* The guts are in there */
extern changepasswd(SVCXPRT *);

static void	boilerplate(struct svc_req *rqstp, SVCXPRT *transp);
static void	unlimit(int lim);
bool_t		validloginshell(struct passwd *pwd, char *arg);
int		validstr(char *str, size_t size);

static char  *getusershell(void);
static void   setusershell(void);
static void   endusershell(void);

int  Argc;
char **Argv;
int  mflag;			/* do a make */
int Mstart;
int single = 0;
int nogecos = 0;
int noshell = 0;
int nopw = 0;
int useadjunct = 0;
int useshadow = 0;

static char *defshell = "/bin/sh";

/* These are the various reasons we might exit. */
enum exitstat {
    Esuccess,
    EminusDandfiles,
    Emissingdir,
    Emissingadjunct,
    Eaccesspasswd,
    Eaccessshadow,
    Echdir,
    Egetnetconfigent,
    Et_open,
    Enetdir_rsvdport,
    Et_sync,
    Et_info,
    Esvc_create,
    Esvc_reg,
    Esvcrun_ret
};

static char err_usage[] =
"Usage:\n"
"        rpc.yppasswdd [-D directory | passwd [passwd.adjunct]]\n"
"                      [-nopw] [-nogecos]\n"
"                      [-noshell] [-m arg1 arg2 ...]\n"
"where\n"
"        directory is the directory where the passwd, shadow and/or\n"
"        passwd.adjunct files are found (/etc by default)\n"
"        It should match the setting of PWDIR in /var/yp/Makefile\n\n"
"        Alternatively, the old 4.1.x syntax is supported where\n"
"        passwd is the path to the passwd file\n"
"        passwd.adjunct is the patch to the passwd.adjunct file\n"
"        NOTES:\n"
"         1. The -D option and the passwd/passwd.adjunct arguments are\n"
"            mutually exclusive\n"
"         2. The old syntax deprecated and will be removed in a future\n"
"            release\n"
"         3. A shadow file found in the same directory as the passwd\n"
"            will be assumed to contain the password information\n\n"
"        arguments after -m are passed to make(1S) after password changes\n"
"        -nopw passwords may not be changed remotely using passwd\n"
"        -nogecos full name may not be changed remotely using passwd or chfn\n"
"        -noshell shell may not be changed remotely using passwd or chsh\n";

char passwd_file[FILENAME_MAX], shadow_file[FILENAME_MAX];
char lockfile[FILENAME_MAX], adjunct_file[FILENAME_MAX];

int
main(int argc, char **argv)
{
	SVCXPRT *transp4, *transp6, *transpl;
	struct netconfig *nconf4, *nconf6, *nconfl;
	int i, t, tli4, tli6;
	int errorflag;
	int dfexcl; /* -D or files, not both flag */
	enum exitstat exitstatus = Esuccess;
	int connmaxrec = RPC_MAXDATASIZE;

	strcpy(passwd_file, DEFDIR MYPASSWD);
	strcpy(shadow_file, DEFDIR MYSHADOW);
	strcpy(lockfile, DEFDIR ".pwd.lock");
	strcpy(adjunct_file, DEFDIR "security/passwd.adjunct");

	Argc = argc;
	Argv = argv;

	for (i = 1, errorflag = 0, dfexcl = 0; i < argc; i++) {
		if (argv[i][0] == '-' && argv[i][1] == 'm') {
		    if (access("/usr/ccs/bin/make", X_OK) < 0)
			fprintf(stderr,
				"%s: /usr/ccs/bin/make is not available, "
				"ignoring -m option",
				argv[0]);
		    else {
			mflag++;
			Mstart = i;
			break;
		    }
		} else if (argv[i][0] == '-' && argv[i][1] == 'D') {
		    switch (dfexcl) {
		    case 0:
			if (++i < argc) {
			    strcpy(passwd_file, argv[i]);
			    strcpy(shadow_file, argv[i]);
			    strcpy(adjunct_file, argv[i]);
			    strcpy(lockfile, argv[i]);
			    if (argv[i][strlen(argv[i]) - 1] == '/') {
				strcat(passwd_file, MYPASSWD);
				strcat(shadow_file, MYSHADOW);
				strcat(lockfile, ".pwd.lock");
				strcat(adjunct_file, "security/passwd.adjunct");
			    } else {
				strcat(passwd_file, "/" MYPASSWD);
				strcat(shadow_file, "/" MYSHADOW);
				strcat(lockfile, "/.pwd.lock");
				strcat(adjunct_file,
					"/security/passwd.adjunct");
			    }
			    dfexcl++;
			} else {
			    fprintf(stderr,
				"rpc.yppasswdd: -D option requires a "
				"directory argument\n");
			    errorflag++;
			    exitstatus = Emissingdir;
			}
			break;
		    case 1:
			fprintf(stderr,
				"rpc.yppasswdd: cannot specify passwd/"
				"passwd.adjunct pathnames AND use -D\n");
			errorflag++;
			dfexcl++;
			exitstatus = EminusDandfiles;
			break;
		    default:
			break;
		    }
	/* -single: Allow user to change only one of password,  */
	/*		shell, or full name at a time.  (WHY?)	*/
	/*	else if (strcmp(argv[i], "-single") == 0)	*/
	/*	    single = 1;					*/
	/*	else if (strcmp(argv[i], "-nosingle") == 0)	*/
	/*	    single = 0;					*/
		} else if (strcmp(argv[i], "-nogecos") == 0)
		    nogecos = 1;
		else if (strcmp(argv[i], "-nopw") == 0)
		    nopw = 1;
		else if (strcmp(argv[i], "-noshell") == 0)
		    noshell = 1;
		else if (argv[i][0] != '-') {
			/*
			 * If we find a shadow file, we warn that we're
			 * using it in addition to warning that the user
			 * it using a deprecated syntax.
			 */
		    errorflag++;
		    switch (dfexcl) {
		    case 0:
			strcpy(passwd_file, argv[i]);
			memset(shadow_file, 0, sizeof (shadow_file));
			strncpy(shadow_file, argv[i],
				strrchr(argv[i], '/') - argv[i] + 1);
			strcat(shadow_file, MYSHADOW);
			fprintf(stderr,
				"rpc.yppasswdd: specifying the password file"
				" on the command line is \n"
				"               obsolete, "
				"consider using the -D option instead.\n");
			if (access(shadow_file, F_OK) == 0) {
			    fprintf(stderr,
				    "rpc.yppasswdd: found a shadow file in "
				    "the same directory as %s\n"
				    "               It will be used.\n",
				    passwd_file);
			}
			if (i + 1 < argc && argv[i+1][0] != '-') {
			    strcpy(adjunct_file, argv[++i]);
			    if (access(adjunct_file, F_OK) != 0) {
				fprintf(stderr,
					"rpc.yppasswdd: adjunct file %s "
					"not found\n",
					adjunct_file);
				exitstatus = Emissingadjunct;
			    }
			}
			dfexcl++;
			break;
		    case 1:
			fprintf(stderr,
				"rpc.yppasswdd: cannot specify passwd/"
				"passwd.adjunct pathnames AND use -D\n");
			dfexcl++;
			exitstatus = EminusDandfiles;
			break;
		    default:
			break;
		    }
		} else {
		    errorflag++;
		    fprintf(stderr,
			    "rpc.yppasswdd: unrecognized option %s ignored\n",
			    argv[i]);
		}
	}

	if (errorflag)
		fprintf(stderr, err_usage);

	if (exitstatus)
		exit(exitstatus);

	if (access(passwd_file, W_OK) < 0) {
		fprintf(stderr, "rpc.yppasswdd: can't access %s\n",
			passwd_file);
		exitstatus = Eaccesspasswd;
	}
	if (access(shadow_file, W_OK) == 0) {
		useshadow = 1;
	} else {
		/* We don't demand a shadow file unless we're looking at /etc */
		if (strcmp(DEFDIR MYSHADOW, shadow_file) == 0) {
		    fprintf(stderr, "rpc.yppasswdd: can't access %s\n",
				shadow_file);
		    exitstatus = Eaccessshadow;
		}
	}
	if (access(adjunct_file, W_OK) == 0) {
		/* using an adjunct file */
		useadjunct = 1;
	}

	if (chdir("/var/yp") < 0) {
		fprintf(stderr, "rpc.yppasswdd: can't chdir to /var/yp\n");
		exitstatus = Echdir;
	}

	if (exitstatus)
		exit(exitstatus);

	if (errorflag)
		fprintf(stderr, "\nProceeding.\n");

#ifndef	DEBUG
	/* Close everything */
	for (t = sysconf(_SC_OPEN_MAX) - 1; t >= 0; t--)
		close(t);

	/* Wack umask that we inherited from parent */
	umask(0);

	/* Be a midwife to ourselves */
	if (fork())
		exit(Esuccess);

	/* Disassociation is hard to do, la la la */
	setpgrp();
	setsid();

	/* Ignore stuff */
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGWINCH, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);

	/*
	 * Just in case that wasn't enough, let's fork
	 * again.  (per Stevens).
	 */
	if (fork())
		exit(Esuccess);

	/*
	 * We need stdin, stdout, and stderr later when we
	 * fork a make(1).
	 */
	open("/dev/null", O_RDWR);	/* stdin */
	open("/dev/null", O_RDWR);	/* stdout */
	open("/dev/null", O_RDWR);	/* sterr */
#endif

	openlog("yppasswdd", LOG_CONS | LOG_PID, LOG_AUTH);
	unlimit(RLIMIT_CPU);
	unlimit(RLIMIT_FSIZE);

	/*
	 * Set non-blocking mode and maximum record size for
	 * connection oriented RPC transports.
	 */
	if (!rpc_control(RPC_SVC_CONNMAXREC_SET, &connmaxrec)) {
		syslog(LOG_INFO, "unable to set maximum RPC record size");
	}

	nconf4 = getnetconfigent("udp");
	nconf6 = getnetconfigent("udp6");
	if (nconf4 == 0 && nconf6 == 0) {
		syslog(LOG_ERR, "udp/udp6 transport not supported\n");
		exit(Egetnetconfigent);
	}

	tli4 = (nconf4 != 0) ? t_open(nconf4->nc_device, O_RDWR, NULL) : -1;
	tli6 = (nconf6 != 0) ? t_open(nconf6->nc_device, O_RDWR, NULL) : -1;

	if (tli4 == -1 && tli6 == -1) {
		syslog(LOG_ERR, "can\'t open TLI endpoint(s)\n");
		exit(Et_open);
	}

	if (tli4 != -1) {
		if (netdir_options(nconf4, ND_SET_RESERVEDPORT, tli4, NULL)) {
			syslog(LOG_ERR, "could not set reserved port: %s\n",
				netdir_sperror());
			exit(Enetdir_rsvdport);
		}
	}
	if (tli6 != -1) {
		if (netdir_options(nconf6, ND_SET_RESERVEDPORT, tli6, NULL)) {
			syslog(LOG_ERR, "could not set reserved port: %s\n",
				netdir_sperror());
			exit(Enetdir_rsvdport);
		}
	}
#ifdef	DEBUG
	{
		int i, tli[2];
		char *label[2] = {"udp", "udp6"};
		int state;
		struct t_info tinfo;

		tli[0] = tli4;
		tli[1] = tli6;

		for (i = 0; i < sizeof (tli)/sizeof (tli[0]); i++) {
			fprintf(stderr, "transport %s, fd = %d\n",
				tli[i], label[i]);
			if ((state = t_sync(tli[i])) < 0) {
				fprintf(stderr, "t_sync failed: %s\n",
					t_errlist[t_errno]);
				exit(Et_sync);
			}
			if (t_getinfo(tli[i], &tinfo) < 0) {
				fprintf(stderr, "t_getinfo failed: %s\n",
					t_errlist[t_errno]);
				exit(Et_info);
			}

			switch (state) {
			case T_UNBND:
				fprintf(stderr, "TLI is unbound\n");
				break;
			case T_IDLE:
				fprintf(stderr, "TLI is idle\n");
				break;
			case T_INREL:
				fprintf(stderr,
					"other side wants to release\n");
				break;
			case T_INCON:
				fprintf(stderr, "T_INCON\n");
				break;
			case T_DATAXFER:
				fprintf(stderr, "T_DATAXFER\n");
				break;
			default:
				fprintf(stderr, "no state info, state = %d\n",
					state);
			}
		}
	}
#endif
	if (tli4 != -1) {
		rpcb_unset((u_long)YPPASSWDPROG, (u_long)YPPASSWDVERS, nconf4);
		transp4 = svc_tli_create(tli4, nconf4, NULL, 0, 0);
	} else {
		transp4 = 0;
	}
	if (tli6 != -1) {
		rpcb_unset((u_long)YPPASSWDPROG, (u_long)YPPASSWDVERS, nconf6);
		transp6 = svc_tli_create(tli6, nconf6, NULL, 0, 0);
	} else {
		transp6 = 0;
	}
	if (transp4 == 0 && transp6 == 0) {
		syslog(LOG_ERR, "yppasswdd: couldn't create an RPC server\n");
		exit(Esvc_create);
	}
	if (transp4 && !svc_reg(transp4, (u_long)YPPASSWDPROG,
			(u_long)YPPASSWDVERS, boilerplate, nconf4)) {
		syslog(LOG_ERR, "yppasswdd: couldn't register yppasswdd\n");
		exit(Esvc_reg);
	}
	if (transp6 && !svc_reg(transp6, (u_long)YPPASSWDPROG,
			(u_long)YPPASSWDVERS, boilerplate, nconf6)) {
		syslog(LOG_ERR, "yppasswdd: couldn't register yppasswdd\n");
		exit(Esvc_reg);
	}

	/*
	 * Create a loopback RPC service for secure authentication of local
	 * principals -- we need this for accepting passwd updates from
	 * root on the master server.
	 */
	if ((nconfl = getnetconfigent("ticlts")) == NULL) {
	    syslog(LOG_ERR, "transport ticlts not supported\n");
	    exit(Egetnetconfigent);
	}
	rpcb_unset((u_long)YPPASSWDPROG, (u_long)YPPASSWDVERS, nconfl);
	transpl = svc_tli_create(RPC_ANYFD, nconfl, NULL, 0, 0);
	if (transpl == NULL) {
	    syslog(LOG_ERR,
		"yppasswdd: couldn't create an loopback RPC server\n");
	    exit(Esvc_create);
	}
	if (!svc_reg(transpl, (u_long)YPPASSWDPROG, (u_long)YPPASSWDVERS,
			boilerplate, nconfl)) {
	    syslog(LOG_ERR, "yppasswdd: couldn't register yppasswdd\n");
	    exit(Esvc_reg);
	}
	__rpc_negotiate_uid(transpl->xp_fd);
	freenetconfigent(nconf4);
	freenetconfigent(nconf6);
	freenetconfigent(nconfl);
	svc_run();
	syslog(LOG_ERR, "yppasswdd: svc_run shouldn't have returned\n");

	return (Esvcrun_ret);
	/* NOTREACHED */
}

static void
boilerplate(struct svc_req *rqstp, SVCXPRT *transp)
{
	switch (rqstp->rq_proc) {
	case NULLPROC:
		if (!svc_sendreply(transp, xdr_void, (char *)0))
		    syslog(LOG_WARNING,
			"yppasswdd: couldn't reply to RPC call\n");
		break;
	case YPPASSWDPROC_UPDATE:
		changepasswd(transp);
		break;
	}
	}

int
validstr(char *str, size_t size)
{
	char c;

	if (str == NULL || strlen(str) > size || strchr(str, ':'))
		return (0);
	while (c = *str++) {
		if (iscntrl(c))
		    return (0);
	}
	return (1);
}

bool_t
validloginshell(struct passwd *pwd, char *arg)
{
	static char newshell[STRSIZE];
	char *cp, *valid;

	if (pwd->pw_shell == 0 || *pwd->pw_shell == '\0')
		pwd->pw_shell = defshell;

	if (*(strrchr(pwd->pw_shell, '/') + 1) == 'r') {
		syslog(LOG_ERR,
			"yppasswdd: Cannot change from restricted shell %s\n",
			pwd->pw_shell);
		return (0);
	}

	for (valid = getusershell(); valid; valid = getusershell())
		if (strcmp(pwd->pw_shell, valid) == 0)
		    break;

	if (valid == NULL) {
		syslog(LOG_ERR, "yppasswdd: Current shell is not valid: %s\n",
			pwd->pw_shell);
		endusershell();
		return (0);
	}

	if (arg != 0) {
		strncpy(newshell, arg, sizeof (newshell) - 1);
		newshell[sizeof (newshell) - 1] = 0;
	} else {
		endusershell();
		return (0);
	}

	/*
	 * Allow user to give shell name w/o preceding pathname.
	 */
	setusershell();
	for (valid = getusershell(); valid; valid = getusershell()) {
		if (newshell[0] == '/') {
		    cp = valid;
		} else {
		    cp = strrchr(valid, '/');
		    if (cp == 0)
			cp = valid;
		    else
			cp++;
		}
		if (strcmp(newshell, cp) == 0)
		    break;
	}

	if (valid == 0) {
		syslog(LOG_WARNING,
			"%s is unacceptable as a new shell.\n", newshell);
		endusershell();
		return (0);
	}

	if (access(valid, X_OK) < 0) {
		syslog(LOG_WARNING, "%s is unavailable.\n", valid);
		endusershell();
		return (0);
	}

	pwd->pw_shell =  newshell;
	endusershell();
	return (1);
}

static void
unlimit(int lim)
{
	struct rlimit rlim;
	rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
	setrlimit(lim, &rlim);
}

static char *shelllist[] = {
	"/sbin/sh",
	"/sbin/jsh",
	"/bin/csh",
	"/usr/bin/csh",
	"/bin/jsh",
	"/usr/bin/jsh",
	"/bin/ksh",
	"/usr/bin/ksh",
	"/bin/rksh",
	"/usr/bin/rksh",
	"/bin/sh",
	"/usr/bin/sh",
	0
};
static FILE *usershellfp;
static char **curshell = shelllist;

static char *
getusershell(void)
{
	static char usershellbuffer[BUFSIZ];
	char *p;

	if (!usershellfp)
		if ((usershellfp = fopen("/etc/shells", "r")) == NULL) {
		    if (!*curshell) {
			curshell = shelllist;
			return (NULL);
		    }
		    return (*curshell++);
		}

	while (fgets(usershellbuffer, BUFSIZ, usershellfp)) {
		if ((p = strpbrk(usershellbuffer, "#")) != NULL)
		    *p = '\0';
		p = strtok(usershellbuffer, " \t\n");
		if (p != NULL)
		    return (p);
	}

	return (NULL);
}

static void
setusershell(void)
{
	if (usershellfp) {
		fclose(usershellfp);
		usershellfp = NULL;
	}

	curshell = shelllist;
}

static void
endusershell(void)
{
	if (usershellfp)
		fclose(usershellfp);

	usershellfp = NULL;
	curshell = shelllist;
}
