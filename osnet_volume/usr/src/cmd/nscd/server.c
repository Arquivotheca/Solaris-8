/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)server.c	1.25	99/05/25 SMI"

/*
 * Simple doors name server cache daemon
 */

#include <stdio.h>
#include <signal.h>
#include <sys/door.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
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
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <sys/door.h>
#include "getxby_door.h"
#include "server_door.h"
#include "nscd.h"
/* Includes for filenames of databases */
#include <shadow.h>
#include <userdefs.h>
#include <netdb.h>
#include <exec_attr.h>
#include <prof_attr.h>
#include <user_attr.h>

extern int 	optind;
extern int 	opterr;
extern int 	optopt;
extern char * 	optarg;

static void detachfromtty();


admin_t	current_admin;
int will_become_server;

nsc_stat_t *
getcacheptr(char * s)
{
	static const char *caches[7] = {"passwd", "group", "hosts", "ipnodes",
	    "exec_attr", "prof_attr", "user_attr" };

	if (strncmp(caches[0], s, strlen(caches[0])) == 0)
		return (&current_admin.passwd);

	if (strncmp(caches[1], s, strlen(caches[1])) == 0)
		return (&current_admin.group);

	if (strncmp(caches[2], s, strlen(caches[3])) == 0)
		return (&current_admin.host);

	if (strncmp(caches[3], s, strlen(caches[2])) == 0)
		return (&current_admin.node);

	if (strncmp(caches[4], s, strlen(caches[3])) == 0)
		return (&current_admin.exec);

	if (strncmp(caches[5], s, strlen(caches[4])) == 0)
		return (&current_admin.prof);

	if (strncmp(caches[6], s, strlen(caches[5])) == 0)
		return (&current_admin.user);

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
 *  routine to check if server is already running
 */

int
nsc_ping()
{
	nsc_data_t data;
	nsc_data_t * dptr;
	int ndata;
	int adata;

	data.nsc_call.nsc_callnumber = NULLCALL;
	ndata = sizeof (data);
	adata = sizeof (data);
	dptr = &data;
	return (_nsc_trydoorcall(&dptr, &ndata, &adata));
}

static void dozip()
{
	/* not much here */
}

/*
 *  This is here to prevent disaster if we get a panic
 *  in libthread.  This overrides the libthread definition
 *  and forces the process to exit immediately.
 */
void
_panic(char * s)
{
	_exit(1);
}

keep_open_dns_socket()
{
	_res.options |= RES_STAYOPEN; /* just keep this udp socket open */
}

/*
 * declaring this causes the files backend to use hashing
 * this is of course an utter hack, but provides a nice
 * quiet back door to enable this feature for only the nscd.
 */
void
__nss_use_files_hash()
{

}
/*
 *
 *  The allocation of resources for cache lookups is an interesting
 *  problem, and one that has caused several bugs in the beta release
 *  of 2.5.  In particular, the introduction of a thottle to prevent
 *  the creation of excessive numbers of LWPs in the case of a failed
 *  name service has led to a denial of service problem when the
 *  name service request rate exceeds the name service's ability
 *  to respond.  As a result, I'm implementing the following
 *  algorithm:
 *
 *  1) We cap the number of total threads.
 *  2) We save CACHE_THREADS of those for cache lookups only.
 *  3) We use a common pool of 2/3 of the remain threads that are used first
 *  4) We save the remainder and allocate 1/3 of it for table specific lookups
 *
 *  The intent is to prevent the failure of a single name service from
 *  causing denial of service, and to always have threads available for
 *  cached lookups.  If a request comes in and the answer isn't in the
 *  cache and we cannot get a thread, we simply return NOSERVER, forcing
 *  the client to lookup the
 *  data itself.  This will prevent the types of starvation seen
 *  at UNC due to a single threaded DNS backend, and allows the cache
 *  to eventually become filled.
 *
 */

/* 7 tables: passwd, group, hosts, ipnodes, exec_attr, prof_attr, user_attr */
#define	NSCD_TABLES		7
#define	TABLE_THREADS		10
#define	COMMON_THREADS		20
#define	CACHE_MISS_THREADS	(COMMON_THREADS + NSCD_TABLES * TABLE_THREADS)
#define	CACHE_HIT_THREADS	20
#define	MAX_SERVER_THREADS	(CACHE_HIT_THREADS + CACHE_MISS_THREADS)

static sema_t common_sema;
static sema_t passwd_sema;
static sema_t hosts_sema;
static sema_t nodes_sema;
static sema_t group_sema;
static sema_t exec_sema;
static sema_t prof_sema;
static sema_t user_sema;
static thread_key_t lookup_state_key;

void
initialize_lookup_clearance()
{
	thr_keycreate(&lookup_state_key, NULL);
	sema_init(&common_sema, COMMON_THREADS, USYNC_THREAD, 0);
	sema_init(&passwd_sema, TABLE_THREADS, USYNC_THREAD, 0);
	sema_init(&hosts_sema, TABLE_THREADS, USYNC_THREAD, 0);
	sema_init(&nodes_sema, TABLE_THREADS, USYNC_THREAD, 0);
	sema_init(&group_sema, TABLE_THREADS, USYNC_THREAD, 0);
	sema_init(&exec_sema, TABLE_THREADS, USYNC_THREAD, 0);
	sema_init(&prof_sema, TABLE_THREADS, USYNC_THREAD, 0);
	sema_init(&user_sema, TABLE_THREADS, USYNC_THREAD, 0);
}

int
get_clearance(int callnumber)
{
	int res;

	sema_t * table_sema = NULL;
	char * tab;

	if ((res = sema_trywait(&common_sema)) == 0) {
		thr_setspecific(lookup_state_key, NULL);
		return (0);
	}

	switch (MASKUPDATEBIT(callnumber)) {

	case GETPWUID:
	case GETPWNAM:
		tab = "passwd";
		table_sema = &passwd_sema;
		break;

	case GETGRNAM:
	case GETGRGID:
		tab = "group";
		table_sema = &group_sema;
		break;

	case GETHOSTBYNAME:
	case GETHOSTBYADDR:
		tab = "hosts";
		table_sema = &hosts_sema;
		break;

	case GETIPNODEBYNAME:
	case GETIPNODEBYADDR:
		tab = "ipnodes";
		table_sema = &nodes_sema;
		break;
	case GETEXECID:
		tab = "exec_attr";
		table_sema = &exec_sema;
		break;

	case GETPROFNAM:
		tab = "prof_attr";
		table_sema = &prof_sema;
		break;

	case GETUSERNAM:
		tab = "user_attr";
		table_sema = &user_sema;
		break;

	}

	if ((res = sema_trywait(table_sema)) == 0) {
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
	int res;
	int which;

	sema_t * table_sema = NULL;

	thr_getspecific(lookup_state_key, (void**)&which);

	if (which == 0) /* from common pool */ {
		sema_post(&common_sema);
		return (0);
	}

	switch (MASKUPDATEBIT(callnumber)) {

	case GETPWUID:
	case GETPWNAM:
		table_sema = &passwd_sema;
		break;

	case GETGRNAM:
	case GETGRGID:
		table_sema = &group_sema;
		break;

	case GETHOSTBYNAME:
	case GETHOSTBYADDR:
		table_sema = &hosts_sema;
		break;

	case GETIPNODEBYNAME:
	case GETIPNODEBYADDR:
		table_sema = &nodes_sema;
		break;

	case GETEXECID:
		table_sema = &exec_sema;
		break;

	case GETPROFNAM:
		table_sema = &prof_sema;
		break;

	case GETUSERNAM:
		table_sema = &user_sema;
		break;
	}

	sema_post(table_sema);
	return (0);
}


static mutex_t		create_lock;
int			nscd_max_servers = MAX_SERVER_THREADS;
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
	static void *value = 0;

	/* disable cancellation to avoid hangs if server threads disappear */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	thr_setspecific(server_key, value);
	door_return(NULL, 0, NULL, 0);
}

/*
 * Server threads are created here.
 */
/*ARGSUSED*/
static void
server_create(door_info_t *dip)
{
	mutex_lock(&create_lock);
	if (++num_servers > nscd_max_servers) {
		num_servers--;
		mutex_unlock(&create_lock);
		return;
	}
	mutex_unlock(&create_lock);
	thr_create(NULL, 0, server_tsd_bind, NULL, THR_BOUND|THR_DETACHED,
	    NULL);
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
	int did;
	int opt;
	int errflg = 0;
	int showstats = 0;
	int doset = 0;
	int loaded_config_file = 0;
	struct stat buf;
	sigset_t myset;
	struct sigaction action;

	/*
	 *  Special case non-root user  here - he can just print stats
	 */

	if (geteuid()) {
		if (argc != 2 || strcmp(argv[1], "-g")) {
			fprintf(stderr,
			    "Must be root to use any option other than "\
			    "-g.\n\n");
			usage(argv[0]);
		}

		if ((nsc_ping() != SUCCESS) ||
		    (client_getadmin(&current_admin) != 0)) {
			fprintf(stderr, "%s doesn't appear to be running.\n",
				argv[0]);
			exit(1);
		}
		(void) client_showstats(&current_admin);
		exit(0);
	}



	/*
	 *  Determine if there is already a daemon running
	 */

	will_become_server = (nsc_ping() != SUCCESS);

	/*
	 *	process usual options
	 */

	/*
	 *  load normal config file
	 */

	if (will_become_server) {
		static const nsc_stat_t defaults = {
			0,	/* stats */
			0,	/* stats */
			0,	/* stats */
			0,	/* stats */
			0,	/* stats */
			0,	/* stats */
			211,	/* suggested size */
			1,	/* enabled */
			0,	/* invalidate cmd */
			600,	/* positive ttl */
			10, 	/* netative ttl */
			20,	/* keep hot */
			0,	/* old data not ok */
			1 };	/* check files */

		current_admin.passwd = defaults;
		current_admin.group  = defaults;
		current_admin.host   = defaults;
		current_admin.node   = defaults;
		current_admin.exec   = defaults;
		current_admin.prof   = defaults;
		current_admin.user   = defaults;

		current_admin.logfile[0] = '\0';

		if (access("/etc/nscd.conf", R_OK) == 0) {
			if (nscd_parse(argv[0], "/etc/nscd.conf") < 0) {
				exit(1);
			}
			loaded_config_file++;
		}
	}

	else {
		if (client_getadmin(&current_admin)) {
			fprintf(stderr, "Cannot contact nscd properly(?)\n");
			exit(1);
		}

		current_admin.logfile[0] = '\0';
	}

	while ((opt = getopt(argc, argv,
	    "S:Kf:c:ge:p:n:i:l:d:s:h:o:")) != EOF) {
		nsc_stat_t * cache;
		char * cacheopt;

		switch (opt) {

		case 'S':		/* undocumented feature */
			doset++;
			cache = getcacheptr(optarg);
			cacheopt = getcacheopt(optarg);
			if (!cache || !cacheopt) {
				errflg++;
				break;
			}
			if (strcmp(cacheopt, "yes") == 0)
			    cache->nsc_secure_mode = 1;
			else if (strcmp(cacheopt, "no") == 0)
			    cache->nsc_secure_mode = 0;
			else
			    errflg++;
			break;

		case 'K':		/* undocumented feature */
			client_killserver();
			exit(0);
			break;

		case 'f':
			doset++;
			loaded_config_file++;
			if (nscd_parse(argv[0], optarg) < 0) {
				exit(1);
			}
			break;

		case 'g':
			showstats++;
			break;

		case 'p':
			doset++;
			cache = getcacheptr(optarg);
			cacheopt = getcacheopt(optarg);
			if (!cache || !cacheopt) {
				errflg++;
				break;
			}
			cache->nsc_pos_ttl = atoi(cacheopt);
			break;

		case 'n':
			doset++;
			cache = getcacheptr(optarg);
			cacheopt = getcacheopt(optarg);
			if (!cache || !cacheopt) {
				errflg++;
				break;
			}
			cache->nsc_neg_ttl = atoi(cacheopt);
			break;

		case 'c':
			doset++;
			cache = getcacheptr(optarg);
			cacheopt = getcacheopt(optarg);
			if (!cache || !cacheopt) {
				errflg++;
				break;
			}

			if (strcmp(cacheopt, "yes") == 0)
			    cache->nsc_check_files = 1;
			else if (strcmp(cacheopt, "no") == 0)
			    cache->nsc_check_files = 0;
			else
			    errflg++;
			break;


		case 'i':
			doset++;
			cache = getcacheptr(optarg);
			if (!cache) {
				errflg++;
				break;
			}
			cache->nsc_invalidate = 1;
			break;

		case 'l':
			doset++;
			strcpy(current_admin.logfile, optarg);
			break;

		case 'd':

			doset++;
			current_admin.debug_level = atoi(optarg);
			break;

		case 's':
			doset++;
			cache = getcacheptr(optarg);
			cacheopt = getcacheopt(optarg);
			if (!cache || !cacheopt) {
				errflg++;
				break;
			}

			cache->nsc_suggestedsize = atoi(cacheopt);

			break;

		case 'h':
			doset++;
			cache = getcacheptr(optarg);
			cacheopt = getcacheopt(optarg);
			if (!cache || !cacheopt) {
				errflg++;
				break;
			}
			cache->nsc_keephot = atoi(cacheopt);
			break;

		case 'o':
			doset++;
			cache = getcacheptr(optarg);
			cacheopt = getcacheopt(optarg);
			if (!cache || !cacheopt) {
				errflg++;
				break;
			}
			if (strcmp(cacheopt, "yes") == 0)
			    cache->nsc_old_data_ok = 1;
			else if (strcmp(cacheopt, "no") == 0)
			    cache->nsc_old_data_ok = 0;
			else
			    errflg++;
			break;

		case 'e':
			doset++;
			cache = getcacheptr(optarg);
			cacheopt = getcacheopt(optarg);
			if (!cache || !cacheopt) {
				errflg++;
				break;
			}
			if (strcmp(cacheopt, "yes") == 0)
			    cache->nsc_enabled = 1;
			else if (strcmp(cacheopt, "no") == 0)
			    cache->nsc_enabled = 0;
			else
			    errflg++;
			break;

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
					"Error during admin call\n");
				exit(1);
			}
		}
		if (!showstats && !doset) {
			fprintf(stderr,
				"%s already running.... no admin specified\n",
				argv[0]);
		}
		exit(0);
	}

	/*
	 *   daemon from here ou
	 */

	if (!loaded_config_file) {
		fprintf(stderr,
			"No configuration file specifed and /etc/nscd.conf" \
			"not present\n");
		exit(1);
	}

	if (current_admin.debug_level) {
		/* we're debugging... */
		if (strlen(current_admin.logfile) == 0)
		/* no specified log file */
			strcpy(current_admin.logfile, "stderr");
		else
			nscd_set_lf(&current_admin, current_admin.logfile);
	} else {
		if (strlen(current_admin.logfile) == 0)
			strcpy(current_admin.logfile, "/dev/null");
		nscd_set_lf(&current_admin, current_admin.logfile);
		detachfromtty();
	}

	/* perform some initialization */
	initialize_lookup_clearance();
	keep_open_dns_socket();
	getpw_init();
	getgr_init();
	gethost_init();
	getnode_init();
	getexec_init();
	getprof_init();
	getuser_init();

	/* Establish our own server thread pool */

	door_server_create(server_create);
	if (thr_keycreate(&server_key, server_destroy) != 0) {
		perror("thr_keycreate");
		exit(-1);
	}

	/* Create a door */

	if ((did =  door_create(switcher,
				NAME_SERVICE_DOOR_COOKIE,
				DOOR_UNREF)) < 0) {
		perror("door_create");
		exit(-1);
	}

	/* bind to file system */

	if (stat(NAME_SERVICE_DOOR, &buf) < 0) {
		int newfd;
		if ((newfd = creat(NAME_SERVICE_DOOR, 0444)) < 0) {
			logit("Cannot create %s:%s\n",
				NAME_SERVICE_DOOR,
				strerror(errno));
			exit(1);
		}
		close(newfd);
	}

	if (fattach(did, NAME_SERVICE_DOOR) < 0) {
		if ((errno != EBUSY) ||
		    (fdetach(NAME_SERVICE_DOOR) <  0) ||
		    (fattach(did, NAME_SERVICE_DOOR) < 0)) {
			perror("door_attach");
			exit(2);
		}
	}

	action.sa_handler = dozip;
	action.sa_flags = 0;
	sigemptyset(&action.sa_mask);
	sigemptyset(&myset);
	sigaddset(&myset, SIGHUP);

	if (sigaction(SIGHUP, &action, NULL) < 0) {
		perror("sigaction");
		exit(1);
	}

	if (thr_sigsetmask(SIG_BLOCK, &myset, NULL) < 0) {
		perror("thr_sigsetmask");
		exit(1);
	}


	/*
	 *  kick off revalidate threads
	 */

	if (thr_create(NULL, NULL,
		(void *(*)(void *))getpw_revalidate, 0, 0, NULL) != 0) {
		perror("thr_create");
		exit(1);
	}

	if (thr_create(NULL, NULL,
		(void *(*)(void *))gethost_revalidate, 0, 0, NULL) != 0) {
		perror("thr_create");
		exit(1);
	}

	if (thr_create(NULL, NULL,
		(void *(*)(void*))getnode_revalidate, 0, 0, NULL) != 0) {
		perror("thr_create");
		exit(1);
	}

	if (thr_create(NULL, NULL,
		(void *(*)(void*))getgr_revalidate, 0, 0, NULL) != 0) {
		perror("thr_create");
		exit(1);
	}

	if (thr_create(NULL, NULL,
	    (void *(*)(void*))getexec_revalidate, 0, 0, NULL) != 0) {
		perror("thr_create");
		exit(1);
	}

	if (thr_create(NULL, NULL,
	    (void *(*)(void*))getprof_revalidate, 0, 0, NULL) != 0) {
		perror("thr_create");
		exit(1);
	}

	if (thr_create(NULL, NULL,
	    (void *(*)(void*))getuser_revalidate, 0, 0, NULL) != 0) {
		perror("thr_create");
		exit(1);
	}

	if (thr_sigsetmask(SIG_UNBLOCK, &myset, NULL) < 0) {
		perror("thr_sigsetmask");
		exit(1);
	}

	/*CONSTCOND*/
	while (1) {
		pause();
		logit("Reloading /etc/nscd.conf\n");
		nscd_parse(argv[0], "/etc/nscd.conf");
	}
}


/*ARGSUSED*/
void switcher(void *cookie, char *argp, size_t arg_size, door_desc_t *dp,
    uint_t n_desc)
{
	union {
		nsc_data_t	data;
		char		space[8192];
	} u;

	time_t now;

	static time_t last_nsswitch_check;
	static time_t last_nsswitch_modified;
	static time_t last_resolv_modified;

	static mutex_t nsswitch_lock;

	nsc_call_t * ptr = (nsc_call_t *)argp;

	if (argp == DOOR_UNREF_DATA) {
		printf("Door Slam... exiting\n");
		exit(0);
	}

	if (ptr == NULL) { /* empty door call */
		(void) door_return(NULL, 0, 0, 0); /* return the favor */
	}

	now = time(NULL);

	/*
	 *  just in case check
	 */

	mutex_lock(&nsswitch_lock);

	if (now - last_nsswitch_check > 10) {
		struct stat nss_buf;
		struct stat res_buf;

		last_nsswitch_check = now;

		mutex_unlock(&nsswitch_lock); /* let others continue */

		/*
		 *  This code keeps us from statting resolv.conf
		 *  if it doesn't exist, yet prevents us from ignoring
		 *  it if it happens to disappear later on for a bit.
		 */

		if (last_resolv_modified >= 0) {
			if (stat("/etc/resolv.conf", &res_buf) < 0) {
				if (last_resolv_modified == 0)
				    last_resolv_modified = -1;
				else
				    res_buf.st_mtime = last_resolv_modified;
			} else if (last_resolv_modified == 0) {
			    last_resolv_modified = res_buf.st_mtime;
			}
		}

		if (stat("/etc/nsswitch.conf", &nss_buf) < 0) {

			/*EMPTY*/;

		} else if (last_nsswitch_modified == 0) {

			last_nsswitch_modified = nss_buf.st_mtime;

		} else if ((last_nsswitch_modified < nss_buf.st_mtime) ||
			((last_resolv_modified > 0) &&
			(last_resolv_modified < res_buf.st_mtime))) {

			/* time for restart */
			logit("nscd restart due to /etc/nsswitch.conf or "\
				"resolv.conf change\n");

			execl("/etc/init.d/nscd", "nscd", "start", NULL);
			perror("execl failed");
			exit(1);
		}

	} else
	    mutex_unlock(&nsswitch_lock);

	switch (ptr->nsc_callnumber) {

	case NULLCALL:
		u.data.nsc_ret.nsc_return_code = SUCCESS;
		u.data.nsc_ret.nsc_bufferbytesused = sizeof (nsc_return_t);
		break;


	case GETPWNAM:
		*(argp + arg_size - 1) = 0; /* FALLTHROUGH */
	case GETPWUID:
		getpw_lookup(&u.data.nsc_ret, sizeof (u), ptr, now);
		break;

	case GETGRNAM:
		*(argp + arg_size - 1) = 0; /* FALLTHROUGH */
	case GETGRGID:
		getgr_lookup(&u.data.nsc_ret, sizeof (u), ptr, now);
		break;

	case GETHOSTBYNAME:
		*(argp + arg_size - 1) = 0; /* FALLTHROUGH */
	case GETHOSTBYADDR:
		gethost_lookup(&u.data.nsc_ret, sizeof (u), ptr, now);
		break;

	case GETIPNODEBYNAME:
		*(argp + arg_size - 1) = 0; /* FALLTHROUGH */
	case GETIPNODEBYADDR:
		getnode_lookup(&u.data.nsc_ret, sizeof (u), ptr, now);
		break;

	case GETEXECID:
		*(argp + arg_size - 1) = 0;
		getexec_lookup(&u.data.nsc_ret, sizeof (u), ptr, now);
		break;

	case GETPROFNAM:
		*(argp + arg_size - 1) = 0;
		getprof_lookup(&u.data.nsc_ret, sizeof (u), ptr, now);
		break;

	case GETUSERNAM:
		*(argp + arg_size - 1) = 0;
		getuser_lookup(&u.data.nsc_ret, sizeof (u), ptr, now);
		break;

	case GETADMIN:
		getadmin(&u.data.nsc_ret, sizeof (u), ptr);
		break;

	case SETADMIN:
	case KILLSERVER: {

		door_cred_t dc;

		if (_door_cred(&dc) < 0) {
			perror("door_cred");
		}

		if (dc.dc_euid != 0) {
			logit("SETADMIN call failed(cred): caller pid %d, "\
				"uid %d, euid %d\n",
				dc.dc_pid, dc.dc_ruid, dc.dc_euid);
			u.data.nsc_ret.nsc_return_code = NOTFOUND;
			break;
		}

		if (ptr->nsc_callnumber == KILLSERVER) {
			logit("Nscd received KILLSERVER cmd from pid %d, "\
				"uid %d, euid %d\n",
				dc.dc_pid, dc.dc_ruid, dc.dc_euid);
			exit(0);
		}
		else
		    setadmin(&u.data.nsc_ret, sizeof (u), ptr);
		break;
	}

	default:
		logit("Unknown name service door call op %d\n",
		    ptr->nsc_callnumber);
		u.data.nsc_ret.nsc_return_code = -1;
		u.data.nsc_ret.nsc_bufferbytesused = sizeof (nsc_return_t);
		break;

	}
	door_return(&u.data, u.data.nsc_ret.nsc_bufferbytesused, NULL, 0);
}

void
usage(char * s)
{
	fprintf(stderr, "Usage: %s [-d debug_level] [-l logfilename]\n", s);
	fprintf(stderr,
		"	[-p cachename,positive_time_to_live]\n");
	fprintf(stderr,
		"	[-n cachename,negative_time_to_live]\n");
	fprintf(stderr,
		"	[-i cachename] [-s cachename,suggestedsize]\n");

	fprintf(stderr,
		"	[-h cachename,keep_hot_count] "\
		"[-o cachename,\"yes\"|\"no\"]\n");

	fprintf(stderr,
		"	[-e cachename,\"yes\"|\"no\"] [-g] " \
		"[-c cachename,\"yes\"|\"no\"]\n");

	fprintf(stderr,
		"	[-f configfilename] \n");

	fprintf(stderr,
		"\n	Supported caches: passwd, group, hosts, ipnodes\n");

	fprintf(stderr,
		"         exec_attr, prof_attr, and user_attr.\n");

	exit(1);

}


static int logfd = 2;

nscd_set_lf(admin_t * ptr, char * s)
{
	int newlogfd;

	/*
	 *  we don't really want to try and open the log file
	 *  /dev/null since that will fail w/ our security fixes
	 */

	if (*s == 0) {
		/* ignore empty log file specs */
		;
	} else if (s == NULL || strcmp(s, "/dev/null") == 0) {
		strcpy(current_admin.logfile, "/dev/null");
		close(logfd);
		logfd = -1;
	} else {
		/*
		* bug 4007235 - mode is -1(Dave!)
		*
		* In order to open this file securely, we'll try a few tricks
		*/

		if ((newlogfd = open(s, O_EXCL|O_WRONLY|O_CREAT, 0644)) < 0) {
			/*
			* File already exists... now we need to get cute
			* since opening a file in a world-writeable directory
			* safely is hard = it could be a hard link or a
			* symbolic link to a system file.
			*/
			struct stat before;

			if (lstat(s, &before) < 0) {
				logit("Cannot open new logfile \"%s\": %sn",
					s, strerror(errno));
				return (-1);
			}

			if (S_ISREG(before.st_mode) && /* no symbolic links */
				(before.st_nlink == 1) && /* no hard links */
				(before.st_uid == 0)) {   /* owned by root */
				if ((newlogfd =
				    open(s, O_APPEND|O_WRONLY, 0644)) < 0) {
					logit("Cannot open new "\
					    "logfile \"%s\": %s\n", s,
					    strerror(errno));
					return (-1);
				}
			} else {
				logit("Cannot use specified logfile \"%s\": "\
				    "file is/has links or isn't owned by "\
				    "root\n", s);
				return (-1);
			}
		}

		strcpy(ptr->logfile, s);
		close(logfd);
		logfd = newlogfd;
		logit("Start of new logfile %s\n", s);
	}
	return (0);
}

int
logit(char * format, ...)
{
	static mutex_t loglock;
	struct timeval tv;

	char buffer[1024];

	va_list ap;
	va_start(ap, format);

	if (logfd >= 0) {
		int safechars;
		gettimeofday(&tv, NULL);

		ctime_r(&tv.tv_sec, buffer, 1024);
		sprintf(buffer+19, ".%.4d	", tv.tv_usec/100);
		safechars = sizeof (buffer) - 30;
		if (vsnprintf(buffer+25, safechars, format, ap) > safechars)
			strcat(buffer, "...\n");

		mutex_lock(&loglock);
		write(logfd, buffer, strlen(buffer));
		mutex_unlock(&loglock);
	}

	va_end(ap);

	return (0);
}

void
do_update(nsc_call_t * in)
{
	union {
		nsc_data_t	data;
		char		space[8192];
	} u;

	time_t now = time(NULL);

	switch (MASKUPDATEBIT(in->nsc_callnumber)) {

	case GETPWUID:
	case GETPWNAM:
		getpw_lookup(&u.data.nsc_ret, sizeof (u), in, now);
		break;

	case GETGRNAM:
	case GETGRGID:
		getgr_lookup(&u.data.nsc_ret, sizeof (u), in, now);
		break;

	case GETHOSTBYNAME:
	case GETHOSTBYADDR:
		gethost_lookup(&u.data.nsc_ret, sizeof (u), in, now);
		break;

	case GETIPNODEBYNAME:
	case GETIPNODEBYADDR:
		getnode_lookup(&u.data.nsc_ret, sizeof (u), in, now);
		break;

	case GETEXECID:
		getexec_lookup(&u.data.nsc_ret, sizeof (u), in, now);
		break;

	case GETPROFNAM:
		getprof_lookup(&u.data.nsc_ret, sizeof (u), in, now);
		break;

	case GETUSERNAM:
		getuser_lookup(&u.data.nsc_ret, sizeof (u), in, now);
		break;

	default:
		assert(0);
		break;
	}

	free(in);
}

int
launch_update(nsc_call_t * in)
{
	nsc_call_t * c;

	int l = nsc_calllen(in);

	in->nsc_callnumber |= UPDATEBIT;

	memcpy(c = malloc(l), in, l);

	if (current_admin.debug_level >= DBG_ALL) {
		logit("launching update\n");
	}

	if (thr_create(NULL,
	    NULL,
	    (void *(*)(void*))do_update,
	    c,
	    0|THR_DETACHED, NULL) != 0) {
		logit("thread create failed\n");
		exit(1);
	}

	return (0);
}

int
nsc_calllen(nsc_call_t * in)
{
	switch (MASKUPDATEBIT(in->nsc_callnumber)) {

	case GETPWUID:
	case GETGRGID:
	case NULLCALL:
		return (sizeof (*in));

	case GETPWNAM:
	case GETGRNAM:
	case GETHOSTBYNAME:
	case GETIPNODEBYNAME:
		return (sizeof (*in) + strlen(in->nsc_u.name));

	case GETHOSTBYADDR:
	case GETIPNODEBYADDR:
		return (sizeof (*in) + in->nsc_u.addr.a_length);

	case GETEXECID:
	case GETPROFNAM:
	case GETUSERNAM:

		return (sizeof (*in) + strlen(in->nsc_u.name));
	}

	return (0);
}

int
client_getadmin(admin_t * ptr)
{
	union {
		nsc_data_t data;
		char space[8192];
	} u;

	nsc_data_t * dptr;
	int ndata;
	int adata;

	u.data.nsc_call.nsc_callnumber = GETADMIN;
	ndata = sizeof (u);
	adata = sizeof (u.data);
	dptr = &u.data;

	if (_nsc_trydoorcall(&dptr, &ndata, &adata) != SUCCESS) {
		return (-1);
	}

	memcpy(ptr, dptr->nsc_ret.nsc_u.buff, sizeof (*ptr));
	return (0);
}

/*ARGSUSED*/
int
getadmin(nsc_return_t * out, int size, nsc_call_t * ptr)
{
	out->nsc_return_code = SUCCESS;
	out->nsc_bufferbytesused = sizeof (current_admin);
	memcpy(out->nsc_u.buff, &current_admin, sizeof (current_admin));
	return (0);
}

/*ARGSUSED*/
int
setadmin(nsc_return_t * out, int size, nsc_call_t * ptr)
{
	admin_t * new;

	out->nsc_return_code = SUCCESS;
	out->nsc_bufferbytesused = sizeof (nsc_return_t);

	new = (admin_t *) ptr->nsc_u.name;


	/*
	 *  global admin stuff
	 */

	if ((nscd_set_lf(&current_admin, new->logfile) < 0) ||
	    nscd_set_dl(&current_admin, new->debug_level) < 0) {
		out->nsc_return_code = NOTFOUND;
		return (-1);
	}

	/*
	 * per cache items
	 */

	if (new->passwd.nsc_invalidate) {
		logit("Invalidating passwd cache\n");
		getpw_invalidate();
	}

	if (new->group.nsc_invalidate) {
		logit("Invalidating group cache\n");
		getgr_invalidate();
	}

	if (new->host.nsc_invalidate) {
		logit("Invalidating host cache\n");
		gethost_invalidate();
	}

	if (new->node.nsc_invalidate) {
		logit("Invalidating ipnodes cache\n");
		getnode_invalidate();
	}

	if (nscd_set_ttl_positive(&current_admin.passwd,
			"passwd",
			new->passwd.nsc_pos_ttl) < 0		||
	    nscd_set_ttl_negative(&current_admin.passwd,
			"passwd",
			new->passwd.nsc_neg_ttl) < 0		||
	    nscd_set_khc(&current_admin.passwd,
			"passwd",
			new->passwd.nsc_keephot) < 0		||
	    nscd_set_odo(&current_admin.passwd,
			"passwd",
			new->passwd.nsc_old_data_ok) < 0	||
	    nscd_set_ec(&current_admin.passwd,
			"passwd",
			new->passwd.nsc_enabled) < 0		||
	    nscd_set_ss(&current_admin.passwd,
			"passwd",
			new->passwd.nsc_suggestedsize) < 0	   ||

	    nscd_set_ttl_positive(&current_admin.group,
			"group",
			new->group.nsc_pos_ttl) < 0		||
	    nscd_set_ttl_negative(&current_admin.group,
			"group",
			new->group.nsc_neg_ttl) < 0		||
	    nscd_set_khc(&current_admin.group,
			"group",
			new->group.nsc_keephot) < 0		||
	    nscd_set_odo(&current_admin.group,
			"group",
			new->group.nsc_old_data_ok) < 0		||
	    nscd_set_ec(&current_admin.group,
			"group",
			new->group.nsc_enabled) < 0		||
	    nscd_set_ss(&current_admin.group,
			"group",
			new->group.nsc_suggestedsize) < 0	||

	    nscd_set_ttl_positive(&current_admin.node,
			"ipnodes",
			new->node.nsc_pos_ttl) < 0		||
	    nscd_set_ttl_negative(&current_admin.node,
			"ipnodes",
			new->node.nsc_neg_ttl) < 0		||
	    nscd_set_khc(&current_admin.node,
			"ipnodes",
			new->node.nsc_keephot) < 0		||
	    nscd_set_odo(&current_admin.node,
			"ipnodes",
			new->node.nsc_old_data_ok) < 0		||
	    nscd_set_ec(&current_admin.node,
			"ipnodes",
			new->node.nsc_enabled) < 0		||
	    nscd_set_ss(&current_admin.node,
			"ipnodes",
			new->node.nsc_suggestedsize) < 0	||

	    nscd_set_ttl_positive(&current_admin.host,
			"hosts",
			new->host.nsc_pos_ttl) < 0		||
	    nscd_set_ttl_negative(&current_admin.host,
			"hosts",
			new->host.nsc_neg_ttl) < 0		||
	    nscd_set_khc(&current_admin.host,
			"hosts",
			new->host.nsc_keephot) < 0		||
	    nscd_set_odo(&current_admin.host,
			"hosts",
			new->host.nsc_old_data_ok) < 0		||
	    nscd_set_ec(&current_admin.host,
			"hosts",
			new->host.nsc_enabled) < 0		||
	    nscd_set_ss(&current_admin.host,
			"hosts",
			new->host.nsc_suggestedsize) < 0) {
		out->nsc_return_code = NOTFOUND;
		return (-1);
	}
	out->nsc_return_code = SUCCESS;
	return (0);
}

client_killserver()
{
	union {
		nsc_data_t data;
		char space[8192];
	} u;

	nsc_data_t * dptr;
	int ndata;
	int adata;

	u.data.nsc_call.nsc_callnumber = KILLSERVER;

	ndata = sizeof (u);
	adata = sizeof (nsc_call_t);

	dptr = &u.data;

	_nsc_trydoorcall(&dptr, &ndata, &adata);
}


int
client_setadmin(admin_t * ptr)
{
	union {
		nsc_data_t data;
		char space[8192];
	} u;

	nsc_data_t * dptr;
	int ndata;
	int adata;

	u.data.nsc_call.nsc_callnumber = SETADMIN;

	memcpy(u.data.nsc_call.nsc_u.name, ptr, sizeof (*ptr));

	ndata = sizeof (u);
	adata = sizeof (*ptr);

	dptr = &u.data;

	if (_nsc_trydoorcall(&dptr, &ndata, &adata) != SUCCESS) {
		return (-1);
	}

	return (0);
}

static void
dump_stat(nsc_stat_t * ptr)
{
	int hitrate;
	printf("%10s  cache is enabled\n",
		(ptr->nsc_enabled?"Yes":"No"));
	printf("%10d  cache hits on positive entries\n",
		ptr->nsc_pos_cache_hits);
	printf("%10d  cache hits on negative entries\n",
		ptr->nsc_neg_cache_hits);
	printf("%10d  cache misses on positive entries\n",
		ptr->nsc_pos_cache_misses);
	printf("%10d  cache misses on negative entries\n",
		ptr->nsc_neg_cache_misses);
	hitrate = ptr->nsc_pos_cache_misses + ptr->nsc_neg_cache_misses +
		ptr->nsc_pos_cache_hits + ptr->nsc_neg_cache_hits;

	if (hitrate)
	    hitrate = (100*(ptr->nsc_pos_cache_hits +
		ptr->nsc_neg_cache_hits))/hitrate;

	printf("%10d%% cache hit rate\n",  hitrate);
	printf("%10d  queries deferred\n", ptr->nsc_throttle_count);
	printf("%10d  total entries\n", ptr->nsc_entries);
	printf("%10d  suggested size\n", ptr->nsc_suggestedsize);
	printf("%10d  seconds time to live for positive entries\n",
		ptr->nsc_pos_ttl);
	printf("%10d  seconds time to live for negative entries\n",
		ptr->nsc_neg_ttl);
	printf("%10d  most active entries to be kept valid\n",
		ptr->nsc_keephot);
	printf("%10s  check /etc/{passwd, group, hosts, inet/ipnodes} file "\
		"for changes\n",
		(ptr->nsc_check_files?"Yes":"No"));

	printf("%10s  use possibly stale data rather than waiting for "\
		"refresh\n",
		(ptr->nsc_old_data_ok?"Yes":"No"));
}

int
client_showstats(admin_t * ptr)
{

	printf("nscd configuration:\n\n");
	printf("%10d  server debug level\n", ptr->debug_level);
	printf("\"%s\"  is server log file\n", ptr->logfile);

	printf("\npasswd cache:\n\n");
	dump_stat(&(ptr->passwd));
	printf("\ngroup cache:\n\n");
	dump_stat(&(ptr->group));
	printf("\nhosts cache:\n\n");
	dump_stat(&(ptr->host));
	printf("\nipnodes cache:\n\n");
	dump_stat(&(ptr->node));
	printf("\nexec_attr cache:\n\n");
	dump_stat(&(ptr->exec));
	printf("\nprof_attr cache:\n\n");
	dump_stat(&(ptr->prof));
	printf("\nuser_attr cache:\n\n");
	dump_stat(&(ptr->user));

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
		break;
	case 0:
		break;
	default:
		exit(0);
	}
	setsid();
	(void) open("/dev/null", O_RDWR, 0);
	dup(0);
	dup(0);
}
