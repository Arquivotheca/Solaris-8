/*
 *	Copyright (c) 1996,1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)cachemgr.c	1.5	99/03/16 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <netdir.h>
#include <rpcsvc/nis.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "nis_cache.h"

int mgr_verbose;
int alarm_rang = 0;
int got_sighup = 0;

extern int __nis_debuglevel;
extern void cacheprog_2(struct svc_req *, SVCXPRT *);
extern nis_error __nis_CacheMgrInit_discard(int);

static char *create_ti_server();
static void my_svc_run();
static void do_timers();
static void do_config();

void
hangup_handler(int sig)
{
	got_sighup = 1;
}

void
cache_purge_handler(int sig)
{
	alarm_rang = 1;
}

/*
 *  Cleanup, set signal handler to default, and send ourselves
 *  the signal again.
 */
void
cache_cleanup_handler(int sig)
{
	__nis_CacheMgrCleanup();
	sigset(sig, SIG_DFL);
	kill(getpid(), sig);
}

static void usage(name)
	char *name;
{
	printf("\tusage: %s\n", name);
	printf("\t\t-d  [debug_level]\n");
	printf("\t\t-i  <initialize cache when starting up>\n");
	printf("\t\t-m  [max size of cache file (in pages) ]\n");
	printf(
	"\t\t-n  <insecure mode - directory object signatures not checked>.\n");
	printf("\t\t-s  [initial size of cache file (in pages)]\n");
	printf("\t\t-v  <verbose - sends events to syslog>\n");
	exit(1);
}

main(int argc, char **argv)
{
	int c;
	int n;
	int pid;
	int fd;
	char *p;
	int init_cache = 0;
	char *options = 0;
	char *uaddr;
	nis_error err;

	/*
	 * Make sure any files we create without explicit permissions
	 * aren't group or world writeable.
	 */
	(void) umask(0022);

	openlog("nis_cachemgr", LOG_CONS, LOG_DAEMON);

	while ((c = getopt(argc, argv, "ivd:a:ns:m:o:")) != EOF) {
		switch (c) {
		    case 'd':
			__nis_debuglevel = atoi(optarg);
			break;

		    case 'i':
			init_cache = 1;
			break;

		    case 'm':
		    case 'n':
		    case 's':
			/* obsolete */
			break;

		    case 'v':
			mgr_verbose = 1;
			break;

		    case 'o':
			options = optarg;
			break;

		    default:
			usage(argv[0]);
			break;
		}
	}

	/*
	 *  We pass options to the cache library through the
	 *  environment variable NIS_OPTIONS.  If options are
	 *  passed on the command line, then we put them into
	 *  the environment.
	 */
	if (options) {
		n = strlen("NIS_OPTIONS=") + strlen(options) + 1;
		p = (char *)malloc(n);
		if (p == NULL) {
			fprintf(stderr, "out of memory\n");
			exit(1);
		}
		strcpy(p, "NIS_OPTIONS=");
		strcat(p, options);
		putenv(p);
	}

	if (init_cache) {
		unlink(PRIVATE_CACHE_FILE);
		unlink(TMP_CACHE_FILE);
	}

	err = __nis_CacheMgrInit_discard(init_cache);
	if (err != NIS_SUCCESS) {
		nis_perror(err, "can't initialize cache");
		exit(1);
	}

	if (!__nis_debuglevel) {
		pid = fork();
		if (pid == -1) {
			perror("fork");
			exit(1);
		} else if (pid != 0) {
			exit(0);
		}
		close(0);
		close(1);
		close(2);
		fd = open("/dev/console", 2);
		if (fd == -1) {
			perror("/dev/console");
			exit(1);
		}
		dup2(fd, 1);
		dup2(fd, 2);
		setsid();
	}

	sigset(SIGHUP, hangup_handler);
	sigset(SIGALRM, cache_purge_handler);
	sigset(SIGPIPE, SIG_IGN);
	sigset(SIGTERM, cache_cleanup_handler);
	sigset(SIGINT, cache_cleanup_handler);
	sigset(SIGKILL, cache_cleanup_handler);
	sigset(SIGIOT, cache_cleanup_handler);
	sigset(SIGILL, cache_cleanup_handler);
	sigset(SIGQUIT, cache_cleanup_handler);
	sigset(SIGSYS, cache_cleanup_handler);
	sigset(SIGFPE, cache_cleanup_handler);

	uaddr = create_ti_server();
	__nis_CacheMgrUpdateUaddr(uaddr);
	free(uaddr);
	__nis_CacheMgrMarkUp();

	if (mgr_verbose)
		printf("running service, uaddr = %s\n", uaddr);
	do_timers();
	my_svc_run();
	syslog(LOG_ERR, "svc_run() returned");
	exit(1);
}


static
void
my_svc_run()
{
	int i;
	int npollfds = 0;
	pollfd_t *pollset = NULL;

	while (1) {
		if (got_sighup) {
			got_sighup = 0;
			do_config();
		} else if (alarm_rang) {
			alarm_rang = 0;
			do_timers();
		}

		if (npollfds != svc_max_pollfd) {
			pollset = realloc(pollset,
					sizeof (pollfd_t) * svc_max_pollfd);
			npollfds = svc_max_pollfd;
		}

		if (npollfds == 0)
			break;	/* None waiting, hence return */

		/*
		 * Get existing array of pollfd's, should really compress
		 * this but it shouldn't get very large (or sparse).
		 */
		(void) memcpy(pollset, svc_pollfd,
					sizeof (pollfd_t) * svc_max_pollfd);

		switch (i = poll(pollset, npollfds, -1)) {
		case -1:
			/*
			 * We ignore all errors, continuing with the assumption
			 * that it was set by the signal handlers (or any
			 * other outside event) and not caused by poll().
			 */
		case 0:
			continue;
		default:
			svc_getreq_poll(pollset, i);
		}
	}
}


static
void
do_config()
{
	ulong_t secs;

	secs = __nis_CacheMgrRefreshCache();
	alarm(secs);
}



static
void
do_timers()
{
	ulong_t secs;

	secs = __nis_CacheMgrTimers();
	alarm(secs);
}

static
char *
create_ti_server()
{
	SVCXPRT *transp;
	struct netconfig *nconf;
	char *uaddr;
	void *nc_handle;
	bool_t found_loopback = FALSE;

	/*
	 * we want to get a loopback transport to have clients talk to the
	 * cachemgr
	 * this provides security in the sense that only local folk can access
	 * the cachemgr. Also, it makes the messages inherently more reliable.
	 * we also want a CLTS transport so search in the netconfig database
	 * for this type of transport.
	 * This is an implicit protocol between the cachemgr and the clients
	 * and the clients make the same selection of trasport (in
	 * cache_getclnt.c) and use the uaddr that the cachemgr writes into
	 * the cache file. This uaddr would be valid only for this transport
	 * that both agree on implicitly.
	 * If the selection scheme is changed here it should also be changed
	 * in the client side (cache_getclnt.c)
	 */

	nc_handle = setnetconfig();
	if (nc_handle != (void *) NULL) {
		while (nconf = getnetconfig(nc_handle))
			if ((strcmp(nconf->nc_protofmly, NC_LOOPBACK) == 0) &&
		    (nconf->nc_semantics == NC_TPI_CLTS)) {
			found_loopback = TRUE;
			break;
		}
	}
	if (!found_loopback) {
		syslog(LOG_ERR,
		"Could not get loopback transport from netconfig database");
		exit(1);

	}
	/* create a new transport endpoint */
	transp = svc_tp_create(
			(void (*)(struct svc_req *, SVCXPRT *))cacheprog_2,
			CACHEPROG, CACHE_VER_2, nconf);
	if (!transp) {
		syslog(LOG_ERR,
	"create_ti_server: cannot create server handle on loopback transport");
		exit(1);
	}
	uaddr = taddr2uaddr(nconf, &transp->xp_ltaddr);
	endnetconfig(nc_handle);
	return (uaddr);
}

/*
 *  Service routines.
 */

void *
nis_cache_add_entry_2()
{
	if (mgr_verbose)
		printf("nis_cache_add_entry\n");
}

void *
nis_cache_remove_entry_2()
{
	if (mgr_verbose)
		printf("nis_cache_remove_entry\n");
}

void *
nis_cache_read_coldstart_2()
{
	if (mgr_verbose)
		printf("nis_cache_read_coldstart\n");
}

void *
nis_cache_refresh_entry_2()
{
	if (mgr_verbose)
		printf("nis_cache_refresh_entry\n");
}

nis_error *
nis_cache_bind_replica_2(char **argp, struct svc_req *rqstp)
{
	static nis_error err;

	if (mgr_verbose)
		printf("nis_cache_bind_replica_2(%s)\n", *argp);
	err = __nis_CacheMgrBindReplica(*argp);
	return (&err);
}

nis_error *
nis_cache_bind_master_2(char **argp, struct svc_req *rqstp)
{
	static nis_error err;

	if (mgr_verbose)
		printf("nis_cache_bind_master_2(%s)\n", *argp);
	err = __nis_CacheMgrBindMaster(*argp);
	return (&err);
}

nis_error *
nis_cache_bind_server_2(bind_server_arg *argp, struct svc_req *rqstp)
{
	static nis_error err;

	if (mgr_verbose)
		printf("nis_cache_bind_server_2(%s)\n", argp->srv->name);
	err = __nis_CacheMgrBindServer(argp->srv, argp->nsrv);
	return (&err);
}

refresh_res *
nis_cache_refresh_binding_2(nis_bound_directory *binding, struct svc_req *rqstp)
{
	static refresh_res res;

	if (mgr_verbose)
		printf("nis_cache_refresh_binding_2\n");
	res.changed = __nis_CacheMgrRefreshBinding(binding);
	res.ep.family = NULL;
	res.ep.proto = NULL;
	res.ep.uaddr = NULL;
	return (&res);
}

refresh_res *
nis_cache_refresh_address_2(nis_bound_endpoint *bep, struct svc_req *rqstp)
{
	static refresh_res res;

	if (mgr_verbose)
		printf("nis_cache_refresh_address_2\n");
	/* xdr_free old result */
	xdr_free(xdr_refresh_res, (char *)&res);
	res.changed = __nis_CacheMgrRefreshAddress(bep);
	res.ep.family = NULL;	/* not used */
	res.ep.proto = NULL;	/* not used */
	if (res.changed)
		res.ep.uaddr = strdup(bep->uaddr);
	else
		res.ep.uaddr = NULL;
	return (&res);
}

refresh_res *
nis_cache_refresh_callback_2(nis_bound_endpoint *bep, struct svc_req *rqstp)
{
	static refresh_res res;

	if (mgr_verbose)
		printf("nis_cache_refresh_callback_2\n");
	/* xdr_free old result */
	xdr_free(xdr_refresh_res, (char *)&res);
	res.changed = __nis_CacheMgrRefreshCallback(bep);
	if (res.changed) {
		res.ep.family = strdup(bep->cbep.family);
		res.ep.proto = strdup(bep->cbep.proto);
		res.ep.uaddr = strdup(bep->cbep.uaddr);
	} else {
		res.ep.family = NULL;
		res.ep.proto = NULL;
		res.ep.uaddr = NULL;
	}
	return (&res);
}
