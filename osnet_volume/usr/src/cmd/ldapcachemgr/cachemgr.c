/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)cachemgr.c 1.1     99/07/07 SMI"

/*
 * Simple doors ldap cache daemon
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <door.h>
#include <time.h>
#include <string.h>
#include <libintl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <thread.h>
#include <stdarg.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <memory.h>
#include <sys/types.h>
#include <syslog.h>
#include "cachemgr.h"

static void	detachfromtty();
admin_t		current_admin;
static int	will_become_server;

static void switcher(void *cookie, char *argp, size_t arg_size,
			door_desc_t *dp, uint_t n_desc);
static void usage(char * s);
static int cachemgr_set_lf(admin_t * ptr, char * logfile);
static int client_getadmin(admin_t * ptr);
static int getadmin(ldap_return_t * out);
static int setadmin(ldap_return_t * out, ldap_call_t * ptr);
static  int client_setadmin(admin_t * ptr);
static int client_showstats(admin_t * ptr);

#ifdef SLP
int			use_slp = 0;
static unsigned int	refresh = 10800;	/* dynamic discovery interval */
#endif SLP

static ldap_stat_t *
getcacheptr(char * s)
{
	static const char *caches[1] = {"ldap"};

	if (strncmp(caches[0], s, strlen(caches[0])) == 0)
		return (&current_admin.ldap_stat);

	return (NULL);
}

char *
getcacheopt(char * s)
{
	while (*s && *s != ',')
		s++;
	return ((*s == ',') ? (s + 1) : NULL);
}

/*
 *  This is here to prevent disaster if we get a panic
 *  in libthread.  This overrides the libthread definition
 *  and forces the process to exit immediately.
 */

void
_panic(char * s)
{
	if (s != NULL)
		logit("%s\n", s);
	logit("ldap_cachemgr: _panic'ed\n");
	syslog(LOG_ERR, "ldap_cachemgr: _panic'ed");

	_exit(1);
}

#define	LDAP_TABLES		1	/* ldap */
#define	TABLE_THREADS		10
#define	COMMON_THREADS		20
#define	CACHE_MISS_THREADS	(COMMON_THREADS + LDAP_TABLES * TABLE_THREADS)
#define	CACHE_HIT_THREADS	20
#define	MAX_SERVER_THREADS	(CACHE_HIT_THREADS + CACHE_MISS_THREADS)

static sema_t common_sema;
static sema_t ldap_sema;
static thread_key_t lookup_state_key;

static void
initialize_lookup_clearance()
{
	thr_keycreate(&lookup_state_key, NULL);
	sema_init(&common_sema, COMMON_THREADS, USYNC_THREAD, 0);
	sema_init(&ldap_sema, TABLE_THREADS, USYNC_THREAD, 0);
}

int
get_clearance(int callnumber)
{
	sema_t	*table_sema = NULL;
	char	*tab;

	if (sema_trywait(&common_sema) == 0) {
		thr_setspecific(lookup_state_key, NULL);
		return (0);
	}

	switch (callnumber) {
		case GETLDAPCONFIG:
			tab = "ldap";
			table_sema = &ldap_sema;
			break;
		default:
			logit("Internal Error: get_clearance\n");
			break;
	}

	if (sema_trywait(table_sema) == 0) {
		thr_setspecific(lookup_state_key, (void*)1);
		return (0);
	}

	if (current_admin.debug_level >= DBG_CANT_FIND) {
		logit("get_clearance: throttling load for %s table\n", tab);
	}

	return (-1);
}

int
release_clearance(int callnumber)
{
	int	which;
	sema_t	*table_sema = NULL;

	thr_getspecific(lookup_state_key, (void**)&which);
	if (which == 0) /* from common pool */ {
		sema_post(&common_sema);
		return (0);
	}

	switch (callnumber) {
		case GETLDAPCONFIG:
			table_sema = &ldap_sema;
			break;
		default:
			logit("Internal Error: release_clearance\n");
			break;
	}
	sema_post(table_sema);

	return (0);
}


static mutex_t		create_lock;
static int		num_servers = 0;
static thread_key_t	server_key;


/*
 * Bind a TSD value to a server thread. This enables the destructor to
 * be called if/when this thread exits.  This would be a programming error,
 * but better safe than sorry.
 */

/*ARGSUSED*/
static void *
server_tsd_bind(void *arg)
{
	static void	*value = 0;

	/*
	 * disable cancellation to prevent hangs when server
	 * threads disappear
	 */

	thr_setspecific(server_key, value);
	door_return(NULL, 0, NULL, 0);

	return (value);
}

/*
 * Server threads are created here.
 */

/*ARGSUSED*/
static void
server_create(door_info_t *dip)
{
	mutex_lock(&create_lock);
	if (++num_servers > MAX_SERVER_THREADS) {
		num_servers--;
		mutex_unlock(&create_lock);
		return;
	}
	mutex_unlock(&create_lock);
	thr_create(NULL, 0, server_tsd_bind, NULL,
		THR_BOUND|THR_DETACHED, NULL);
}

/*
 * Server thread are destroyed here
 */

/*ARGSUSED*/
static void
server_destroy(void *arg)
{
	mutex_lock(&create_lock);
	num_servers--;
	mutex_unlock(&create_lock);
}

void
main(int argc, char ** argv)
{
	int			did;
	int			opt;
	int			errflg = 0;
	int			showstats = 0;
	int			doset = 0;
	struct stat		buf;
	sigset_t		myset;
	struct sigaction	sighupaction;
	static void		client_killserver();

	if (chdir(NSLDAPDIRECTORY) < 0) {
		fprintf(stderr, gettext("chdir(\"%s\") failed: %s\n"),
			NSLDAPDIRECTORY, strerror(errno));
	}

	/*
	 *  Special case non-root user here -	he/she/they/it can just print
	 *					stats
	 */

	if (geteuid()) {
		if (argc != 2 || strcmp(argv[1], "-g")) {
			fprintf(stderr,
			    gettext("Must be root to use any option "
			    "other than -g.\n\n"));
			usage(argv[0]);
		}

		if ((__ns_ldap_cache_ping() != SUCCESS) ||
		    (client_getadmin(&current_admin) != 0)) {
			fprintf(stderr,
				gettext("%s doesn't appear to be running.\n"),
				argv[0]);
			exit(1);
		}
		(void) client_showstats(&current_admin);
		exit(0);
	}



	/*
	 *  Determine if there is already a daemon running
	 */

	will_become_server = (__ns_ldap_cache_ping() != SUCCESS);

	/*
	 *  load normal config file
	 */

	if (will_become_server) {
		static const ldap_stat_t defaults = {
			0,		/* stat */
			DEFAULTTTL};	/* ttl */

		current_admin.ldap_stat = defaults;
		strcpy(current_admin.logfile, LOGFILE);
	} else {
		if (client_getadmin(&current_admin)) {
			fprintf(stderr, gettext("Cannot contact %s "
				"properly(?)\n"), argv[0]);
			exit(1);
		}
		strcpy(current_admin.logfile, LOGFILE);
	}

#ifndef SLP
	while ((opt = getopt(argc, argv, "Kgl:r:d:")) != EOF) {
#else
	while ((opt = getopt(argc, argv, "Kgs:l:r:d:")) != EOF) {
#endif SLP
		ldap_stat_t	*cache;

		switch (opt) {
		case 'K':
			client_killserver();
			exit(0);
			break;
		case 'g':
			showstats++;
			break;
		case 'r':
			doset++;
			cache = getcacheptr("ldap");
			if (!optarg) {
				errflg++;
				break;
			}
			cache->ldap_ttl = atoi(optarg);
			break;
		case 'l':
			doset++;
			strcpy(current_admin.logfile, optarg);
			break;
		case 'd':
			doset++;
			current_admin.debug_level = atoi(optarg);
			break;
#ifdef SLP
		case 's':	/* undocumented: use dynamic (SLP) config */
			use_slp = 1;
			break;
#endif SLP
		default:
			errflg++;
			break;
		}
	}

	if (errflg)
	    usage(argv[0]);

	if (!will_become_server) {
		if (showstats) {
			client_showstats(&current_admin);
		}
		if (doset) {
			if (client_setadmin(&current_admin) < 0) {
				fprintf(stderr,
					gettext("Error during admin call\n"));
				exit(1);
			}
		}
		if (!showstats && !doset) {
			fprintf(stderr,
			gettext("%s already running....use '%s "
				"-K' to stop\n"), argv[0], argv[0]);
		}
		exit(0);
	}

	/*
	 *   daemon from here on
	 */

	if (current_admin.debug_level) {
		/*
		 * we're debugging...
		 */
		if (strlen(current_admin.logfile) == 0)
			/*
			 * no specified log file
			 */
			strcpy(current_admin.logfile, LOGFILE);
		else
			cachemgr_set_lf(&current_admin,
				current_admin.logfile);
	} else {
		if (strlen(current_admin.logfile) == 0)
			strcpy(current_admin.logfile, "/dev/null");
			cachemgr_set_lf(&current_admin, current_admin.logfile);
			detachfromtty();
	}

	/*
	 * perform some initialization
	 */

	initialize_lookup_clearance();

	if (getldap_init() != 0)
		exit(-1);

	/*
	 * Establish our own server thread pool
	 */

	door_server_create(server_create);
	if (thr_keycreate(&server_key, server_destroy) != 0) {
		logit("thr_keycreate() call failed\n");
		syslog(LOG_ERR, "ldap_cachemgr: thr_keycreate() call failed");
		perror("thr_keycreate");
		exit(-1);
	}

	/*
	 * Create a door
	 */

	if ((did =  door_create(switcher,
				LDAP_CACHE_DOOR_COOKIE,
				DOOR_UNREF)) < 0) {
		logit("door_create() call failed\n");
		syslog(LOG_ERR, "ldap_cachemgr: door_create() call failed");
		perror("door_create");
		exit(-1);
	}

	/*
	 * bind to file system
	 */

	if (stat(LDAP_CACHE_DOOR, &buf) < 0) {
		int	newfd;

		if ((newfd = creat(LDAP_CACHE_DOOR, 0444)) < 0) {
			logit("Cannot create %s:%s\n",
				LDAP_CACHE_DOOR,
				strerror(errno));
			exit(1);
		}
		close(newfd);
	}

	if (fattach(did, LDAP_CACHE_DOOR) < 0) {
		if ((errno != EBUSY) ||
		    (fdetach(LDAP_CACHE_DOOR) <  0) ||
		    (fattach(did, LDAP_CACHE_DOOR) < 0)) {
			logit("fattach() call failed\n");
			syslog(LOG_ERR, "ldap_cachemgr: fattach() call failed");
			perror("fattach");
			exit(2);
		}
	}

	/* catch SIGHUP revalid signals */
	sighupaction.sa_handler = getldap_revalidate;
	sighupaction.sa_flags = 0;
	sigemptyset(&sighupaction.sa_mask);
	sigemptyset(&myset);
	sigaddset(&myset, SIGHUP);

	if (sigaction(SIGHUP, &sighupaction, NULL) < 0) {
		logit("sigaction() call failed\n");
		syslog(LOG_ERR, "ldap_cachemgr: sigaction() call failed");
		perror("sigaction");
		exit(1);
	}

	if (thr_sigsetmask(SIG_BLOCK, &myset, NULL) < 0) {
		logit("thr_sigsetmask() call failed\n");
		syslog(LOG_ERR, "ldap_cachemgr: thr_sigsetmask() call failed");
		perror("thr_sigsetmask");
		exit(1);
	}

	/*
	 *  kick off revalidate threads only if ttl != 0
	 */

	if (thr_create(NULL, NULL, (void *(*)(void*))getldap_refresh,
		0, 0, NULL) != 0) {
		logit("thr_create() call failed\n");
		syslog(LOG_ERR, "ldap_cachemgr: thr_create() call failed");
		perror("thr_create");
		exit(1);
	}

#ifdef SLP
	if (use_slp) {
		/* kick off SLP discovery thread */
		if (thr_create(NULL, NULL, (void *(*)(void *))discover,
			(void *)&refresh, 0, NULL) != 0) {
			logit("thr_create() call failed\n");
			syslog(LOG_ERR, "ldap_cachemgr: thr_create() "
				"call failed");
			perror("thr_create");
			exit(1);
		}
	}
#endif SLP

	if (thr_sigsetmask(SIG_UNBLOCK, &myset, NULL) < 0) {
		logit("thr_sigsetmask() call failed\n");
		syslog(LOG_ERR, "ldap_cachemgr: the_sigsetmask() call failed");
		perror("thr_sigsetmask");
		exit(1);
	}

	/*CONSTCOND*/
	while (1) {
		pause();
	}
}


/*ARGSUSED*/
static void
switcher(void *cookie, char *argp, size_t arg_size,
		door_desc_t *dp, uint_t n_desc)
{
	dataunion		u;
	ldap_call_t	*ptr = (ldap_call_t *)argp;
	door_cred_t	dc;

	if (argp == DOOR_UNREF_DATA) {
		logit("Door Slam... invalid door param\n");
		syslog(LOG_ERR, "ldap_cachemgr: Door Slam... "
			"invalid door param");
		printf(gettext("Door Slam... invalid door param\n"));
		exit(0);
	}

	if (ptr == NULL) { /* empty door call */
		(void) door_return(NULL, 0, 0, 0); /* return the favor */
	}

	switch (ptr->ldap_callnumber) {
	case NULLCALL:
		u.data.ldap_ret.ldap_return_code = SUCCESS;
		u.data.ldap_ret.ldap_bufferbytesused = sizeof (ldap_return_t);
		break;
	case GETLDAPCONFIG:
		getldap_lookup(&u.data.ldap_ret, ptr);
		current_admin.ldap_stat.ldap_numbercalls++;
		break;
	case GETADMIN:
		getadmin(&u.data.ldap_ret);
		break;
	case SETADMIN:
	case KILLSERVER:
		if (door_cred(&dc) < 0) {
			logit("door_cred() call failed\n");
			syslog(LOG_ERR, "ldap_cachemgr: door_cred() "
				"call failed");
			perror("door_cred");
			break;
		}
		if (dc.dc_euid != 0 && ptr->ldap_callnumber == SETADMIN) {
			logit("SETADMIN call failed (cred): caller "
			    "pid %d, uid %d, euid %d\n",
			    dc.dc_pid, dc.dc_ruid, dc.dc_euid);
			u.data.ldap_ret.ldap_return_code = NOTFOUND;
			break;
		}
		if (ptr->ldap_callnumber == KILLSERVER) {
			logit("ldap_cachemgr received KILLSERVER cmd from "
			    "pid %d, uid %d, euid %d\n",
			    dc.dc_pid, dc.dc_ruid, dc.dc_euid);
			exit(0);
		} else {
		    setadmin(&u.data.ldap_ret, ptr);
		}
		break;
	default:
		logit("Unknown ldap service door call op %d\n",
		    ptr->ldap_callnumber);
		u.data.ldap_ret.ldap_return_code = -1;
		u.data.ldap_ret.ldap_bufferbytesused = sizeof (ldap_return_t);
		break;
	}
	door_return((char *)&u.data,
		u.data.ldap_ret.ldap_bufferbytesused, NULL, 0);
}

static void
usage(char * s)
{
	fprintf(stderr,
		gettext("Usage: %s [-d debug_level] [-l logfilename]\n"), s);
	fprintf(stderr, gettext("	[-K] [-r revalidate_interval] "));
#ifndef SLP
	fprintf(stderr, gettext("	[-g]\n"));
#else
	fprintf(stderr, gettext("	[-g] [-s]\n"));
#endif SLP
	exit(1);
}


static int	logfd = -1;

static int
cachemgr_set_lf(admin_t * ptr, char *logfile)
{
	int	newlogfd;

	/*
	 *  we don't really want to try and open the log file
	 *  /dev/null since that will fail w/ our security fixes
	 */

	if (logfile == NULL || *logfile == 0) {
		/*EMPTY*/;
	} else if (strcmp(logfile, "/dev/null") == 0) {
		strcpy(current_admin.logfile, "/dev/null");
		close(logfd);
		logfd = -1;
	} else {
		if ((newlogfd =
			open(logfile, O_EXCL|O_WRONLY|O_CREAT, 0644)) < 0) {
			/*
			 * File already exists... now we need to get cute
			 * since opening a file in a world-writeable directory
			 * safely is hard = it could be a hard link or a
			 * symbolic link to a system file.
			 *
			 */
			struct stat	before;

			if (lstat(logfile, &before) < 0) {
				logit("Cannot open new logfile \"%s\": %sn",
					logfile, strerror(errno));
				return (-1);
			}
			if (S_ISREG(before.st_mode) &&	/* no symbolic links */
			    (before.st_nlink == 1) &&	/* no hard links */
			    (before.st_uid == 0)) {	/* owned by root */
				if ((newlogfd =
				    open(logfile,
				    O_APPEND|O_WRONLY, 0644)) < 0) {
					logit("Cannot open new logfile "
						"\"%s\": %s\n",
						logfile, strerror(errno));
					return (-1);
				}
			} else {
				logit("Cannot use specified logfile "
				    "\"%s\": file is/has links or isn't "
				    "owned by root\n", logfile);
				return (-1);
			}
		}
		strcpy(ptr->logfile, logfile);
		close(logfd);
		logfd = newlogfd;
		logit("Starting ldap_cachemgr, logfile %s\n", logfile);
	}
	return (0);
}

void
logit(char * format, ...)
{
	static mutex_t	loglock;
	struct timeval	tv;
	char		buffer[BUFSIZ];
	va_list		ap;

	va_start(ap, ...);

	if (logfd >= 0) {
		int	safechars;

		gettimeofday(&tv, NULL);
		ctime_r(&tv.tv_sec, buffer, BUFSIZ);
		snprintf(buffer+19, BUFSIZE, ".%.4ld	", tv.tv_usec/100);
		safechars = sizeof (buffer) - 30;
		if (vsnprintf(buffer+25, safechars, format, ap) > safechars)
			strcat(buffer, "...\n");
		mutex_lock(&loglock);
		write(logfd, buffer, strlen(buffer));
		mutex_unlock(&loglock);
	}
	va_end(ap);
}


void
do_update(ldap_call_t * in)
{
	dataunion		u;

	switch (in->ldap_callnumber) {
	case GETLDAPCONFIG:
		getldap_lookup(&u.data.ldap_ret, in);
		break;
	default:
		assert(0);
		break;
	}

	free(in);
}


static int
client_getadmin(admin_t * ptr)
{
	dataunion		u;
	ldap_data_t	*dptr;
	int		ndata;
	int		adata;

	u.data.ldap_call.ldap_callnumber = GETADMIN;
	ndata = sizeof (u);
	adata = sizeof (u.data);
	dptr = &u.data;

	if (__ns_ldap_trydoorcall(&dptr, &ndata, &adata) != SUCCESS) {
		return (-1);
	}
	memcpy(ptr, dptr->ldap_ret.ldap_u.buff, sizeof (*ptr));

	return (0);
}


static int
getadmin(ldap_return_t * out)
{
	out->ldap_return_code = SUCCESS;
	out->ldap_bufferbytesused = sizeof (current_admin);
	memcpy(out->ldap_u.buff, &current_admin, sizeof (current_admin));

	return (0);
}


static int
setadmin(ldap_return_t * out, ldap_call_t * ptr)
{
	admin_t	*new;

	out->ldap_return_code = SUCCESS;
	out->ldap_bufferbytesused = sizeof (ldap_return_t);
	new = (admin_t *) ptr->ldap_u.domainname;

	/*
	 *  global admin stuff
	 */

	if ((cachemgr_set_lf(&current_admin, new->logfile) < 0) ||
	    cachemgr_set_dl(&current_admin, new->debug_level) < 0) {
		out->ldap_return_code = NOTFOUND;
		return (-1);
	}

	if (cachemgr_set_ttl(&current_admin.ldap_stat,
			"ldap",
			new->ldap_stat.ldap_ttl) < 0) {
		out->ldap_return_code = NOTFOUND;
		return (-1);
	}
	out->ldap_return_code = SUCCESS;

	return (0);
}


static void
client_killserver()
{
	dataunion		u;
	ldap_data_t		*dptr;
	int			ndata;
	int			adata;

	u.data.ldap_call.ldap_callnumber = KILLSERVER;
	ndata = sizeof (u);
	adata = sizeof (ldap_call_t);
	dptr = &u.data;

	__ns_ldap_trydoorcall(&dptr, &ndata, &adata);
}


static int
client_setadmin(admin_t * ptr)
{
	dataunion		u;
	ldap_data_t		*dptr;
	int			ndata;
	int			adata;

	u.data.ldap_call.ldap_callnumber = SETADMIN;
	memcpy(u.data.ldap_call.ldap_u.domainname, ptr, sizeof (*ptr));
	ndata = sizeof (u);
	adata = sizeof (*ptr);
	dptr = &u.data;

	if (__ns_ldap_trydoorcall(&dptr, &ndata, &adata) != SUCCESS) {
		return (-1);
	}

	return (0);
}


static int
client_showstats(admin_t * ptr)
{

	printf(gettext("\ncachemgr configuration:\n"));
	printf(gettext("server debug level %10d\n"), ptr->debug_level);
	printf(gettext("server log file\t\"%s\"\n"), ptr->logfile);
	printf(gettext("number of calls to ldapcachemgr %10d\n"),
		ptr->ldap_stat.ldap_numbercalls);
	printf(gettext("seconds time to live for positive entries %10d\n"),
		ptr->ldap_stat.ldap_ttl);

	return (0);
}


/*
 * detach from tty
 */
static void
detachfromtty()
{
	close(0);
	close(1);
	close(2);
	switch (fork1()) {
		case (pid_t)-1:
			perror("fork");
			logit("fork1() call failed\n");
			syslog(LOG_ERR, "ldap_cachemgr: fork1() call failed");
			break;
		case 0:
			break;
		default:
			exit(0);
	}
	setsid();
	if (open("/dev/null", O_RDWR, 0) != -1) {
		dup(0);
		dup(0);
	}
}
