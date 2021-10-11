/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)nis_main.c	1.76	99/05/19 SMI"

/*
 * Ported from
 *	"@(#)nis_main.c 1.44 91/03/18 Copyr 1990 Sun Micro";
 *
 * nis_main.c
 *
 * This is the main() module for the NIS+ service. It is compiled separately
 * so that the service can parse certain options and initialize the database
 * before starting up.
 */

#include <stdio.h>
#include <stdlib.h>
#include <rpc/rpc.h>
#include <syslog.h>
#include <signal.h>
#include <ucontext.h>
#include <time.h>
#include <string.h>
#include <wait.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <rpcsvc/nis.h>
#include <rpcsvc/nis_db.h>
#include <dirent.h>
#include "nis_svc.h"
#include "nis_proc.h"
#include "log.h"
#include <stropts.h>
#include <poll.h>
#include <limits.h>
#include <rpcsvc/nis_dhext.h>

#define	YPPROG	100004L
#define	YPVERS	2L
#define	YPVERS_ORIG 1L

/* Max interval between checking number of open fd:s */
#define	FD_CHECK_DEF	(5 * 60)	/* Five minutes */

/*
 * Purge since now minus delta depending on number of open fd:s, in seconds.
 *
 * The array values were chosen to do the following:
 *
 *    Behave like the old code (7200 seconds purge delta) for a very small
 *    number of open fd:s
 *
 *    Give reasonably smooth purge behavior for realistic fd growth rates,
 *    as observed at customer site.
 *
 *    Become aggressive about purging at high numbers of open fd:s, to
 *    reduce the risk of running out of fd:s.
 */
static int	purge_deltas[] = {
    7200, 6000, 4800, 3600, 2400, 1800, 1500, 1200,
    900,  600,  300,  150,  100,   75,   70,   60
};

#ifdef MEM_DEBUG
extern void init_malloc();
extern void xdump();
#endif

FILE	*cons = NULL;
extern int (*__clear_directory_ptr)(nis_name);
extern int clear_directory(nis_name);
extern nis_name invalid_directory;

/* Defined in nis_log_common.c */
extern int need_checkpoint; /* The log is "dirty"    */
extern pid_t master_pid;	/* Master PROCESS id */

int _rpcsvcdirty;
int children		= 0;	/* Current forked children 		*/
int max_children 	= 128;  /* Very large number 			*/
int secure_level 	= 2;	/* Security level 2 = max supported  	*/
int force_checkpoint	= 0;	/* Set when we wish to force a c.p. op	*/
int checkpoint_all	= 0;	/* Set when we wish to cp entire db	*/
int readonly		= 0;	/* When true the service is "read only" */
int readonly_pid	= 0;	/* Our child who is watching out for us */
int hup_received	= 0;	/* To tell us when to exit 		*/
int auth_verbose	= 0;	/* Messages on auth info		*/
int resolv_pid		= 0;	/* Resolv server pid 			*/
CLIENT *resolv_client	= NULL; /* Resolv client handle			*/
char *resolv_tp	= "ticots";	/* Resolv netid used on resolv_setup()  */
struct timeval start_time;	/* Time service started running.	*/
extern unsigned long __maxloglen; /* maximum transaction log size */
unsigned int heap_start;	/* sbrk(0) at start of time */
uint_t	next_refresh;		/* next time to re-load dot file */


#ifdef DEBUG
int debug		= 1;
#else
int debug		= 0;
#endif

#define	MAXRAGS	1024
cleanup	*looseends = NULL,	/* Things that need cleaning up		*/
	rags[MAXRAGS],		/* pointers to them			*/
	*free_rags = NULL;	/* Available structs			*/
struct upd_log	*nis_log;	/* Pointer to the mmap'd log.   	*/

extern int optind, opterr;
extern char *optarg;

extern nis_object* get_root_object();
extern cp_result *do_checkpoint();

extern void nis_prog_svc();
extern void ypprog_svc();
extern void ypprog_1();

/* routines for update timestamp cache */
extern void init_updatetime();

/*
 * Global state variables, these variables contain information about the
 * state of the server.
 */
int verbose = 0;		/* Verbose mode, LOG_INFO messages are 	*/
				/* generated for most functions.	*/
int root_server = 0;		/* TRUE if this server is a root server	*/
int static_root = 0;		/* TRUE if the network is partitioned.	*/
int emulate_yp	= FALSE;	/* NIS compat mode or not		*/
int resolv_flag	= 0;		/* Resolv on no host match		*/
NIS_HASH_TABLE ping_list;	/* List of directory names that need to */
				/* be notified of updates		*/
int ping_pid	= 0;		/* Pinger pid for pinger process	*/
NIS_HASH_TABLE upd_list;	/* List of directory names that have    */
				/* pending updates 			*/
ulong_t cp_time = 0;		/* Time of last checkpoint		*/
NIS_HASH_TABLE checkpoint_list;	/* List of directory names that have    */
				/* pending checkpoint 			*/


extern int errno; 		/* for the reap() function below 	*/

static void
reap()
{
	int		lerr;
	siginfo_t	si;
	idtype_t	idtype;
	id_t		pid;
	int		options;
	int		ret;

	if (cons)
		fprintf(cons,
		"reap[%d]: starting to reap child process...\n",
		master_pid);
	syslog(LOG_INFO,
		"reap[%d]: starting to reap child process...\n",
		master_pid);

	lerr = errno;	/* Save it */
	(void) memset(&si, '\0', sizeof (si));
	ret = 0;

	/*
	 * Set our waitid options as follows:
	 *
	 * if( we are the parent and we have a readonly child )
	 *	wait for the readonly child to exit, blocking
	 *	until it does.
	 * else
	 *	wait for all children, but don't hang.
	 */
	if (!readonly && (readonly_pid != 0)) {
		idtype = P_PID;
		pid = readonly_pid;
		options = WEXITED;
	} else {
		idtype = P_ALL;
		pid = 0;
		options = WEXITED | WNOHANG;
	}

	/* Now wait for the child processes to exit. */
	while ((ret = waitid(idtype, pid, &si, options)) == 0) {
		/*
		 * Make sure this is the child we were waiting for.
		 */
		if (idtype == P_PID) {
			/*
			 * The process that died should be the readonly
			 * child. If not, then something went badly wrong,
			 * but we go on waiting for it to die anyway.
			 */
			if (si.si_pid != readonly_pid) {
				syslog(LOG_ERR,
				"reap[%d]: unexpected child ended: pid %d\n",
					master_pid, si.si_pid);
				nis_delete_callback_pid(si.si_pid);
				(void) memset(&si, '\0', sizeof (si));
				continue;
			}

			/*
			 * The readonly child died, so clean up and return.
			 */
			readonly_pid = 0;
			children--;

			if (cons)
				fprintf(cons,
				"reap[%d]: readonly child ended: pid %d\n",
					master_pid, si.si_pid);
			syslog(LOG_INFO,
			"reap[%d]: readonly child ended: pid %d\n",
				master_pid, si.si_pid);
			break;
		} else {
			/*
			 * We were waiting for any child process to die, so
			 * see if a child process actually exited.
			 */
			if (si.si_pid != 0) {
				/*
				 * A child callback process died, so clean up
				 * and return.
				 */
				if (cons)
					fprintf(cons,
				"reap[%d]: child process ended: pid %d\n",
					master_pid, si.si_pid);
				syslog(LOG_INFO,
				"reap[%d]: child process ended: pid %d\n",
				master_pid, si.si_pid);
				nis_delete_callback_pid(si.si_pid);
				children--;
			}
			break;
		}
	}

	if (ret != 0) {
		syslog(LOG_ERR,
			"reap[%d]: waitid() failed: errno = %d - %m\n",
			master_pid, errno);
	}

	errno = lerr;
}

static void
timetodie(proc)
int	proc;
{
	hup_received = 1;
}

void
check_updaters()
{
	ping_item	*pp, *nxt;
	int		pid;
	ulong_t		lu, curtime = ~0UL;
	struct sigaction sigactn;
	int		pending_updates = 0;
	struct timeval	tp;


	/*
	 * Replication management :
	 * 	Part 1. Check to see that we need to resync,
	 *		haven't spawned too many children/threads,
	 *		and haven't already got a readonly listener.
	 *		If any are true, return to main loop.
	 */
	if ((children >= max_children) || (upd_list.first == NULL) ||
	    (readonly != 0) || (readonly_pid != 0)) {
		return;
	}

	if (CHILDPROC) {
		syslog(LOG_INFO,
			"check_updaters: Called from readonly child, ignored.");
		return;
	}


	/* Check if any item on the list is due for update */
	if (gettimeofday(&tp, 0) != -1) {
		curtime = tp.tv_sec;
		for (pp = (ping_item *)(upd_list.first); pp != 0; pp = nxt) {
			nxt = (ping_item *)(pp->item.nxt_item);
			if (pp->utime <= curtime) {
				pending_updates = 1;
				break;
			}
		}
		if (!pending_updates) {
			syslog(LOG_INFO, "check_updaters: no pending updates");
			return;
		}
	}

	/*
	 * Part 2. Start up a read only listener process to handle
	 *	   lookup requests while we're updating the real databases.
	 */
	pid = fork();
	if (pid == 0) {
		/* readonly child */
		if (cons)
			fprintf(cons,
			    "readonly process spawned in check_updaters()\n");
		if (verbose)
			syslog(LOG_INFO, "rpc.nisd: readonly process spawned.");
		readonly = 1;
		/* these are the number of children we can spawn for callback */
		max_children = max_children - children;
		if (max_children <= 0)
			max_children = 1;
		children = 0;
		(void) sigemptyset(&sigactn.sa_mask);
		sigactn.sa_handler = timetodie;
		sigactn.sa_flags = 0;
		sigaction(SIGHUP, &sigactn, NULL);
		/* now in child and in readonly mode */
		return;
	} else {
		/* parent process */
		if (pid == -1) {
			syslog(LOG_ERR,
			"check_updaters: Unable to fork readonly process.");
			return;
		}
	}

	/*
	 * Part 3. The parent process begins to resync each directory on the
	 *	   update list.
	 */
	readonly_pid = pid;
	children++;

	if (cons)
		fprintf(cons, "check_updaters : starting resync\n");
	if (verbose) {
		syslog(LOG_INFO, "check_updaters: forked readonly child pid %d",
			pid);
		syslog(LOG_INFO, "check_updaters: Starting resync.");
	}
	pp = (ping_item *)(upd_list.first);
	for (pp = (ping_item *)(upd_list.first); pp; pp = nxt) {
		nxt = (ping_item *)(pp->item.nxt_item);
		lu = last_update(pp->item.name);
		if (cons)
			fprintf(cons, "check_updaters : update %s\n",
				pp->item.name);
		if ((pp->mtime > lu) && (pp->utime <= curtime)) {
			if (replica_update(pp)) {
				(void) nis_remove_item(pp->item.name,
							&upd_list);
				XFREE(pp->item.name);
				if (pp->obj)
					nis_destroy_object(pp->obj);
				XFREE(pp);
			} else {
				/*
				 * No use continuously retrying, so backoff
				 * exponentially.
				 */
				if (curtime != ~0UL)
					pp->utime = curtime + pp->delta;
				pp->delta *= 2;
				if (pp->delta > MAX_UPD_LIST_TIME_INCR) {
					pp->delta = MAX_UPD_LIST_TIME_INCR;
					if (cons)
						fprintf(cons,
				"check_updaters: unable to resync %s\n",
							pp->item.name);
					syslog(LOG_WARNING,
				"check_updaters: unable to resync %s",
						pp->item.name);
				}
			}
		} else if (pp->mtime <= lu) {
			(void) nis_remove_item(pp->item.name, &upd_list);
			XFREE(pp->item.name);
			if (pp->obj)
				nis_destroy_object(pp->obj);
			XFREE(pp);
		}
	}

}

void
check_pingers()
{
	ping_item	*pp, *nxt;
	struct timeval	ctime;

	if (ping_list.first == NULL)
		return;

	gettimeofday(&ctime, 0);
	if (cons)
		fprintf(cons, "check_pingers : \n");
	for (pp = (ping_item *)ping_list.first; pp; pp = nxt) {
		/* save next pointer in case we remove it */
		nxt = (ping_item *)(pp->item.nxt_item);
		if ((pp->mtime + DIR_IDLE_TIME) > ctime.tv_sec)
			continue;

#ifdef PING_FORK
		if (children >= max_children)
			break;
#endif
		if (verbose && cons)
			fprintf(cons, "check_pingers: ping %s\n",
				pp->item.name);


/*
	A successful return from ping_replicas() means that
	a ping request has been sent to the replicas. It does
	not ensures that replica has indeed received the ping
	request.
*/
#ifdef PING_FORK
		if (ping_replicas(pp)) {
			children++;
			if (pp->obj)
				nis_destroy_object(pp->obj);
			nis_remove_item(pp->item.name, &ping_list);
			XFREE(pp->item.name);
			XFREE(pp);
		}
#else
		nis_remove_item(pp->item.name, &ping_list);
		if (!ping_replicas(pp))
			nis_insert_item((NIS_HASH_ITEM *)pp, &ping_list);
#endif

	}
}

#include <netconfig.h>

extern int	__rpc_bindresvport_ipv6(int, struct sockaddr *, int *, int,
					char *);

/*
 * A modified version of svc_tp_create().  The difference is that
 * nis_svc_tp_create() will try to bind to a privilege port if the
 * the server is to emulate YP and it's using the INET[6] protocol family.
 *
 * The high level interface to svc_tli_create().
 * It tries to create a server for "nconf" and registers the service
 * with the rpcbind. It calls svc_tli_create();
 */
static SVCXPRT *
nis_svc_tp_create(dispatch, prognum, versnum, nconf)
	void (*dispatch)();	/* Dispatch function */
	ulong_t prognum;	/* Program number */
	ulong_t versnum;	/* Version number */
	struct netconfig *nconf; /* Netconfig structure for the network */
{
	SVCXPRT *xprt;
	int	fd;
	struct t_info tinfo;

	if (nconf == (struct netconfig *)NULL) {
		(void) syslog(LOG_ERR,
	"nis_svc_tp_create: invalid netconfig structure for prog %d vers %d",
				prognum, versnum);
		return ((SVCXPRT *)NULL);
	}
	fd = RPC_ANYFD;
	if ((emulate_yp) && (strcmp(nconf->nc_protofmly, NC_INET) == 0 ||
				strcmp(nconf->nc_protofmly, NC_INET6) == 0)) {
		fd = t_open(nconf->nc_device, O_RDWR, &tinfo);
		if (fd == -1) {
			(void) syslog(LOG_ERR,
			"nis_svc_tp_create: could not open connection for %s",
					nconf->nc_netid);
			return ((SVCXPRT *)NULL);
		}
		if (__rpc_bindresvport_ipv6(fd, (struct sockaddr *)NULL,
						(int *)NULL, 8,
						nconf->nc_protofmly) == -1) {
			(void) t_close(fd);
			fd = RPC_ANYFD;
		}
	}
	xprt = svc_tli_create(fd, nconf, (struct t_bind *)NULL, 0, 0);
	if (xprt == (SVCXPRT *)NULL) {
		return ((SVCXPRT *)NULL);
	}
	(void) rpcb_unset(prognum, versnum, nconf);
	if (svc_reg(xprt, prognum, versnum, dispatch, nconf) == FALSE) {
		(void) syslog(LOG_ERR,
		"nis_svc_tp_create: Could not register prog %d vers %d on %s",
				prognum, versnum, nconf->nc_netid);
		SVC_DESTROY(xprt);
		return ((SVCXPRT *)NULL);
	}
	return (xprt);
}

/*
 * Modified version of 'svc_create' that maintains list of handles.
 * The only difference between this and svc_create is that
 * 1.	nis_svc_create maintains the list of handles created so that they
 *	can be reused later for re-registeration.
 *	This is required for nis_put_offline.
 * 2.	nis_svc_create uses the public netconfig interfaces, instead of
 *	 private rpc interfaces.
 */
struct xlist {
	SVCXPRT *xprt;		/* Server handle */
	struct xlist *next;	/* Next item */
};

/* A link list of all the handles */
static struct xlist *nis_xprtlist = (struct xlist *)NULL;

static int
nis_svc_create(dispatch, prognum, versnum, target_nc_flag)
	void (*dispatch)();	/* Dispatch function */
	ulong_t prognum;	/* Program number */
	ulong_t versnum;	/* Version number */
	unsigned target_nc_flag;  /* value of netconfig flag */
{
	struct xlist *l;
	int num = 0;
	SVCXPRT *xprt;
	struct netconfig *nconf;
	NCONF_HANDLE *handle;

	if ((handle = setnetconfig()) == NULL) {
		(void) syslog(LOG_ERR,
			"nis_svc_create: could not get netconfig information");
		return (0);
	}
	while (nconf = getnetconfig(handle)) {
		if (!(nconf->nc_flag & target_nc_flag))
			continue;
		for (l = nis_xprtlist; l; l = l->next) {
			if (strcmp(l->xprt->xp_netid, nconf->nc_netid) == 0) {
				/* Found an old one, use it */
				(void) rpcb_unset(prognum, versnum, nconf);
				if (svc_reg(l->xprt, prognum, versnum,
					dispatch, nconf) == FALSE)
					(void) syslog(LOG_ERR,
		"nis_svc_create: could not register prog %d vers %d on %s",
					prognum, versnum, nconf->nc_netid);
				else
					num++;
				break;
			}
		}
		if (l == (struct xlist *)NULL) {
			/* It was not found. Now create a new one */
			xprt = nis_svc_tp_create(dispatch, prognum,
							versnum, nconf);
			if (xprt) {
				l = (struct xlist *)malloc(sizeof (*l));
				if (l == (struct xlist *)NULL) {
					(void) syslog(LOG_ERR,
						"nis_svc_create: no memory");
					return (0);
				}
				l->xprt = xprt;
				l->next = nis_xprtlist;
				nis_xprtlist = l;
				num++;
			}
		}
	}
	endnetconfig(handle);
	/*
	 * In case of num == 0; the error messages are generated by the
	 * underlying layers; and hence not needed here.
	 */
	return (num);
}

/* Modified version of nis_svc_create that only does registration */

static
nis_svc_reg(dispatch, prognum, versnum, target_nc_flag)
	void (*dispatch)();	/* Dispatch function */
	ulong_t prognum;	/* Program number */
	ulong_t versnum;	/* Version number */
	unsigned target_nc_flag;  /* value of netconfig flag */
{
	struct xlist *l;
	int num = 0;
	struct netconfig *nconf;
	NCONF_HANDLE *handle;

	if ((handle = setnetconfig()) == NULL) {
		(void) syslog(LOG_ERR,
			"nis_svc_reg: could not get netconfig information");
		return (0);
	}
	while (nconf = getnetconfig(handle)) {
		if (!(nconf->nc_flag & target_nc_flag))
			continue;
		for (l = nis_xprtlist; l; l = l->next) {
			if (strcmp(l->xprt->xp_netid, nconf->nc_netid) == 0) {
				/* Found an old one, use it */
				if (svc_reg(l->xprt, prognum, versnum,
					dispatch, nconf) == FALSE)
					(void) syslog(LOG_ERR,
		"nis_svc_reg: could not register prog %d vers %d on %s",
					prognum, versnum, nconf->nc_netid);
				else
					num++;
				break;
			}
		}
	}
	endnetconfig(handle);
	return (num);
}

/*
 * Routines that puts the server online/offline.
 * Currently, the operation affects all directories served by the
 * server; 'dirname' is ignored.
 *
 * The fix for bug 1091692 should take dirname into account.
 */
void
nis_put_offline(nis_name dirname)
{
	rpcb_unset(NIS_PROG, NIS_VERSION, NULL);
	if (emulate_yp) {
		rpcb_unset(YPPROG, YPVERS, NULL);
		rpcb_unset(YPPROG, YPVERS_ORIG, NULL);
	}
	/* Kill the read-only child and wait for it to exit */
	if (readonly_pid) {
		if (cons)
			fprintf(cons,
		"Putting service offline. Killing read only child: pid #%d",
			readonly_pid);
		syslog(LOG_INFO,
		"Putting service offline. Killing read only child: pid #%d",
			readonly_pid);

		kill(readonly_pid, SIGHUP);
		reap();
	}

	if (debug) {
		fprintf(stderr, ".. rpc deregistration complete.\n");
	}
}

void
nis_put_online(nis_name dirname)
{
	int i;

	i = nis_svc_reg(nis_prog_svc, NIS_PROG, NIS_VERSION, NC_VISIBLE);
	if (! i)
		exit(1);
	else if (verbose)
		syslog(LOG_INFO, "NIS+ registered on %d transports", i);
	if (emulate_yp) {
		i = nis_svc_reg(ypprog_svc, YPPROG, YPVERS, NC_VISIBLE);
		if (! i)
			exit(1);
		else if (verbose)
			syslog(LOG_INFO, "NIS registered on %d transports", i);

		i = nis_svc_reg(ypprog_1, YPPROG, YPVERS_ORIG, NC_VISIBLE);
		if (! i)
			exit(1);
		else if (verbose)
			syslog(LOG_INFO,
				"Registered %d YPVERS_ORIG handles.", i);
	}
	if (debug) {
		fprintf(stderr, ".. rpc registration complete.\n");
	}
}

static void
update_cache_data(nis_object* root_obj)
{
	/* make sure the cache data is accurate */
	writeColdStartFile(&(root_obj->DI_data));
	__nis_CacheRestart();
}

static void
print_options()
{
	fprintf(stderr, "Options supported by this version :\n");
	fprintf(stderr, "\th - print this help message.\n");
	fprintf(stderr, "\tC - open diagnostic channel on /dev/console\n");
	fprintf(stderr, "\tF - force checkpoint at startup time\n");
	fprintf(stderr, "\tA - authentication verbose messages\n");
	fprintf(stderr, "\tL [n] - Max load (n) of child processes\n");
	fprintf(stderr, "\tf - force registration even if program # in use\n");
	fprintf(stderr, "\tv - enable verbose mode\n");
	fprintf(stderr, "\tY - emulate NIS (YP) service\n");
	fprintf(stderr, "\tB - emulate NIS (YP) dns resolver service\n");
	fprintf(stderr, "\tt netid - use netid as transport for resolver\n");
	fprintf(stderr, "\td [dictionary] - user defined dictionary\n");
	fprintf(stderr, "\tS [n] - Security level (n) 0,1, or 2\n");
	fprintf(stderr, "\tD - debug mode (don't fork)\n");
	fprintf(stderr, "\tc - checkpoint time in seconds (ignored)\n");
	fprintf(stderr, "\tT n - Size of transaction log in megabytes\n");
	exit(0);
}

static long
fd_open_count(void) {
	DIR *dirp;
	struct dirent *dentp;
	int count = 0;
	char name[1024];
	struct rlimit rl;

	sprintf(name, "/proc/%ld/fd", getpid());
	if ((dirp = opendir(name)) == NULL) {
		if (errno == EMFILE || errno == ENFILE) {
			if (getrlimit(RLIMIT_NOFILE, &rl) == -1)
				return (-1);
			return (rl.rlim_cur);
		}
		return (-1);
	}
	while ((dentp = readdir(dirp)) != NULL) {
		if (isdigit(dentp->d_name[0]))
			count++;
	}
	closedir(dirp);
	/* subtract one for closedir */
	count -= 1;
	return (count);
}

/*
 * Loop thru mech list from security conf file and set
 * the RPC GSS service name(s).  Stop processing list if
 * the classic AUTH_DES compat entry is encountered.
 */
static void
set_rpc_gss_svc_names()
{
	mechanism_t **mechs;

	if (mechs = __nis_get_mechanisms(FALSE)) {
		int		slen;
		mechanism_t	**mpp;
		char		svc_name[NIS_MAXNAMELEN+1];
		char		*lh = nis_local_host();

		if (! lh) {
			syslog(LOG_ERR,
		"can't set RPC GSS service name:  can't get local host name");
			__nis_release_mechanisms(mechs);
			return;
		}

		/* '@' + NUL = 2 */
		if (strlen(lh) + strlen(NIS_SVCNAME_NISD) + 2 >
							sizeof (svc_name)) {
			syslog(LOG_ERR,
		"can't set RPC GSS service name:  svc_name bufsize too small");
			__nis_release_mechanisms(mechs);
			return;
		}
		/* service names are of the form svc@server.dom */
		(void) sprintf(svc_name, "%s@%s", NIS_SVCNAME_NISD, lh);
		/* remove trailing '.' */
		slen = strlen(svc_name);
		if (svc_name[slen - 1] == '.')
			svc_name[slen - 1] = '\0';

		for (mpp = mechs; *mpp; mpp++) {
			mechanism_t *mp = *mpp;

			if (AUTH_DES_COMPAT_CHK(mp))
				break;

			if (! VALID_MECH_ENTRY(mp)) {
				syslog(LOG_ERR,
					"%s: invalid mechanism entry name '%s'",
					NIS_SEC_CF_PATHNAME,
					mp->mechname ? mp->mechname : "NULL");
				continue;
			}

			if (rpc_gss_set_svc_name(svc_name, mp->mechname,
							0, NIS_PROG,
							NIS_VERSION)) {
				if (verbose)
					syslog(LOG_INFO,
				"RPC GSS service name for mech '%s' set",
						mp->mechname);
			} else {
				if (secure_level > 1) {
					rpc_gss_error_t	err;

					rpc_gss_get_error(&err);
					syslog(LOG_ERR,
"can't set RPC GSS svc name '%s' for mech '%s': RPC GSS err = %d, sys err = %d",
						svc_name, mp->mechname,
						err.rpc_gss_error,
						err.system_error);
				} else {
					if (verbose)
						syslog(LOG_INFO,
				"can't set RPC GSS service name for mech '%s'",
								mp->mechname);
				}
			}
		}
		__nis_release_mechanisms(mechs);
		return;
	}
}

void
main(argc, argv)
	int	argc;
	char	*argv[];
{
	int			status = 0, i, c;
	nis_object		*rootobj;
	char			buf[80];
	char			logname[80];
	struct stat 		s;
	char			*dict = NULL;
	struct timeval 		cp_time;
	struct timeval 		now;
	long			last_purge = 0;
	int			pid;
	long 			dtbsize;
	int			force = 0, mb;
	struct rlimit		rl;
	int			minfd;
	int			open_console = FALSE;
	struct sigaction	sigactn;
	bool_t			massage_dict;
	sigset_t		new_mask;
	int			sig_recvd;
	int nfds = 0;
	int pollret = 0;
	struct pollfd 		*svc_pollset = NULL;
	long			fd_check = 0;
	long			fd_check_interval = FD_CHECK_DEF;
	long			purge_delta    = purge_deltas[
			(sizeof (purge_deltas)/sizeof (purge_deltas[0])) - 1];
	long			prev_since;
	long			cur_open_fd;

	/*
	 * increase the internal RPC server cache size to 1024.
	 * If it fails to increase, then just use the default (=128).
	 * NOTE: __rpc_control() is a private interface from RPC.
	 */
#define	CLCR_SET_CRED_CACHE_SZ	7
extern bool_t __rpc_control();

	int cache_size = 1024;
	int connmaxrec = RPC_MAXDATASIZE;

	/*
	 *  We cannot use the shared directory cache yet (possible
	 *  deadlock), so we start up the local cache.
	 */
	(void) __nis_CacheLocalInit(&next_refresh);

	if (!__rpc_control(CLCR_SET_CRED_CACHE_SZ, &cache_size))
		syslog(LOG_ERR, "rpc.nisd: cannot set credential cache size");

	/*
	 * Set non-blocking mode, and establish maximum record size,
	 * for connection oriented RPC transports.
	 */
	if (!rpc_control(RPC_SVC_CONNMAXREC_SET, &connmaxrec)) {
		syslog(LOG_INFO, "unable to set maximum RPC record size");
	}

	/*
	 * __clear_directory_ptr is a global defined in libnsl.
	 * We need to set this here so that clear_directory() be called from
	 * within nis_dump. This is part of the servers serving stale data
	 * bug fix. See 1179965.
	 */
	__clear_directory_ptr = &clear_directory;

	/*
	 *  Make sure that files created by stdio do not have
	 *  extra permission.  We allow group read, but we don't
	 *  allow world to read or write.  We disallow write for
	 *  obvious reasons, but also disallow read so that
	 *  tables can't be read by world (thus bypassing the
	 *  NIS+ access controls.
	 */
	(void) umask(027);

	/*
	 * Process the command line arguments
	 */
	opterr = 0;
	chdir("/var/nis");
	while ((c = getopt(argc, argv, "hCFDAL:fvYBS:rd:T:t:")) != -1) {
		switch (c) {
			case 'h' : /* internal help screen */
				print_options();
				break;
			case 'T' :
				mb = atoi(optarg);
				if ((mb < 4) || (mb > 129)) {
					fprintf(stderr, "Illegal log size.\n");
					exit(1);
				}
				__maxloglen = mb * 1024 * 1024;
				break;

			case 'C' :
				open_console = TRUE;
				break;
			case 'F' :
				force_checkpoint = TRUE;
				need_checkpoint = TRUE;
				break;
			case 'A' :
				auth_verbose++;
				break;
			case 'Y' :
				emulate_yp = TRUE;
				break;
			case 'B' :
				if (!emulate_yp) {
					fprintf(stderr, "Need -Y first.\n");
					exit(1);
				}
				resolv_flag = TRUE;
				break;
			case 't' :
				if (!resolv_flag) {
					fprintf(stderr, "Need -Y -B first.\n");
					exit(1);
				}
				resolv_tp = optarg;
				break;
			case 'v' :
				verbose = 1;
				break;
			case 'd' :
				dict = optarg;
				break;
			case 'S' :
				secure_level = atoi(optarg);
				break;
			case 'r' :
				/* obsolete option */
				root_server = -1;
				fprintf(stderr,
"The -r option is obsolete and no longer necessary for root servers.\n");
				break;
			case 'f' :
				force = TRUE;
				break;
			case 'L' :
				max_children = atoi(optarg);
				if (max_children <= 0) {
					fprintf(stderr, "Illegal load value\n");
					exit(1);
				}
				break;

			case 'D' :
				debug = 1;
				break;
			case '?' :
				fprintf(stderr,
	"usage: rpc.nisd [ -ACDFhlv ] [ -Y [ -B [ -t netid ]]]\n");
				fprintf(stderr,
	"\t[ -d dictionary ] [ -L load ] [ -S level ]\n");
				exit(1);
		}
	}

	heap_start = (unsigned int) sbrk(0); /* before any allocs */

	if (! debug)  {
		switch (fork()) {
		case -1:
			fprintf(stderr, "Couldn't fork a process exiting.\n");
			exit(1);
		case 0:
			break;
		default:
			exit(0);
		}

		closelog();
		getrlimit(RLIMIT_NOFILE, &rl);
		for (i = 0; i < rl.rlim_max; i++)
			(void) close(i);

		(void) open("/dev/null", O_RDONLY);
		(void) open("/dev/null", O_WRONLY);
		(void) dup(1);
		pid = setsid();
		openlog("nisd", LOG_PID+LOG_NOWAIT, LOG_DAEMON);
	}
	/*
	 *  The data base functions use stdio.  The stdio routines
	 *  can only handle file descriptors up to 255.  To make
	 *  more of these low-numbered descriptors available, we
	 *  bump up the file descriptor limit, and tell the RPC
	 *  library (our other main source of file descriptors) to
	 *  try to use descriptors numbered 256 and above.
	 */
	getrlimit(RLIMIT_NOFILE, &rl);
	rl.rlim_cur = 1024;
	rl.rlim_max = 1024;
	setrlimit(RLIMIT_NOFILE, &rl);
	minfd = 256;
	rpc_control(__RPC_CLNT_MINFD_SET, (char *)&minfd);

	/*
	 * get the maximum number of file descriptors for poll
	 */
	getrlimit(RLIMIT_NOFILE, &rl);
	dtbsize = rl.rlim_cur;
#ifdef MEM_DEBUG
	init_xmalloc();
#endif
	if (open_console)
		cons = fopen("/dev/console", "w");
	openlog("nisd", LOG_PID+LOG_NOWAIT, LOG_DAEMON);
	syslog(LOG_INFO, "NIS+ service started.");
	gettimeofday(&start_time, 0);
	master_pid = getpid();
	if (verbose)
		syslog(LOG_INFO, "verbose mode set.");

	/* Clear this out (for portability we can't assume bss == 0) */
	memset((char *)&checkpoint_list, 0, sizeof (checkpoint_list));
	memset((char *)&upd_list, 0, sizeof (upd_list));
	memset((char *)rags, 0, sizeof (cleanup) * MAXRAGS);
	for (i = 0; i < MAXRAGS; i++) {
		rags[i].next = free_rags;
		free_rags = &(rags[i]);
	}

	if (nis_local_directory() == NULL) {
		if (debug)
			fprintf(stderr, "NIS+ Directory not set. Exiting.\n");
		else
			syslog(LOG_ERR, "NIS+ Directory not set. Exiting.");
		exit(1);
	}

	if (debug) {
		fprintf(stderr, "NIS+ Server startup.\n");
	}

	/*
	** Fix for bug #1248972 - Block SIGCHLD in the parent
	** thread, so all subsequent threads will inherit the
	** same signal mask - i.e. block SIGCHLD.
	*/
	(void) sigemptyset(&new_mask);
	(void) sigaddset(&new_mask, SIGCHLD);
	(void) thr_sigsetmask(SIG_BLOCK, &new_mask, NULL);

	if (debug) {
		fprintf(stderr, "Database initialization ...\n");
	}

	if (resolv_flag)
		setup_resolv(&resolv_flag, &resolv_pid,
					&resolv_client, resolv_tp, 0);

	massage_dict = FALSE;
	if (!dict) {
		if (stat(nis_old_data(NULL), &s) == -1) { /* No old */
			if (stat(nis_data(NULL), &s) == -1) { /* No New */
				if (errno == ENOENT) {
					strcpy(buf, nis_data(NULL));
					if (mkdir(buf, 0700)) {
						perror("rpc.nisd");
						syslog(LOG_ERR,
			"rpc.nisd: Unable to create NIS+ directory %s", buf);
						exit(1);
					}
				} else {
					perror("rpc.nisd");
					syslog(LOG_ERR,
			"rpc.nisd: unable to stat NIS+ directory %s.", buf);
					exit(1);
				}
			}
			strcpy(buf, nis_data(NULL));
		} else if (stat(nis_data(NULL), &s) != -1) { /* Old and New */
			/*
			 * Handle the case for a host called data:
			 * 	- ONly the transaction log needs to be
			 *		renamed.
			 *	- the dict has already been massaged and
			 *		named correctly.
			 */
			if (strcmp(NIS_DIR,
					nis_leaf_of(nis_local_host())) == 0) {
				char	oldstr[NIS_MAXNAMELEN];
				char	newstr[NIS_MAXNAMELEN];

				sprintf(oldstr, "%s.log", nis_old_data(NULL));
				strcpy(newstr, LOG_FILE);
				if (rename(oldstr, newstr) == -1) {
					syslog(LOG_ERR,
				"Unable to rename NIS+ transaction log.");
					exit(1);
				}
				strcpy(buf, nis_data(NULL));
				dict = buf;
				/* No need to massage dict */
			} else {
				syslog(LOG_ERR,
				"Old and new dir structures cannot coexist.");
				exit(1);
			}
		} else { /* Old, No New => massage dict. */
			massage_dict = TRUE;
			strcpy(buf, nis_old_data(NULL));
		}
		strcat(buf, ".dict");
		dict = buf;
	}
	if (debug)
		fprintf(stderr, "Dictionary is %s\n", buf);
	status = db_initialize(dict);
	if (status == 0) {
		if (debug)
			fprintf(stderr, "Unable to initialize %s\n", buf);
		else
			syslog(LOG_ERR, "Unable to initialize %s", buf);
		exit(1);
	}

	/*
	 * Now, rename the `hostname` directory if necessary
	 * and massage the dictionary file. This must be done
	 * _after_ the dictionary has been initialiazed. Remember,
	 * the dictionary would have been initialized with dict name
	 * based on the old structure.
	 *
	*/
	if (massage_dict) {
		char	oldbuf[NIS_MAXNAMELEN], newbuf[NIS_MAXNAMELEN];
		char	oldstr[NIS_MAXNAMELEN];
		char	newstr[NIS_MAXNAMELEN];
		char	newdict[NIS_MAXNAMELEN];


		/* Massage the dictionary file */
		sprintf(oldbuf, "/%s/", nis_leaf_of(nis_local_host()));
		__make_legal(oldbuf);
		sprintf(newbuf, "/%s/", NIS_DIR);
		sprintf(newdict, "%s.dict", nis_data(NULL));
		if (db_massage_dict(newdict, oldbuf, newbuf) != DB_SUCCESS) {
			syslog(LOG_ERR,
				"Unable to change database dictionary.");
			exit(1);
		}


		/*
		 * Now, rename the old structure. This includes the following:
		 * 	- directory containing the tables.
		 * We don't worry about the dictionary file and its log
		 * since db_massage_dict() will take care of that for us.
		 * We also don't worry about the dictionary  log file, since
		 * db_massage_dict() will checkpoint before it makes any
		 * changes.
		 *
		 * However, we do need to change the NIS+ transaction log.
		 */
		strcpy(oldstr, nis_old_data(NULL));
		strcpy(newstr, nis_data(NULL));
		if (rename(oldstr, newstr) == -1) {
			syslog(LOG_ERR,
				"Unable to rename directory structure.");
			exit(1);
		}
		sprintf(oldstr, "%s.log", nis_old_data(NULL));
		strcpy(newstr, LOG_FILE);
		if (rename(oldstr, newstr) == -1) {
			syslog(LOG_ERR,
				"Unable to rename NIS+ transaction log.");
			exit(1);
		}
		/* Now, reinitialize the dictionary */
		status = db_initialize(newdict);
		if (status == 0) {
			if (debug)
				fprintf(stderr,
				"Unable to REinitialize %s\n", newdict);
			else
				syslog(LOG_ERR, "Unable to initialize %s",
								newdict);
			exit(1);
		}
	}
	rootobj = get_root_object();
	if (rootobj)
		root_server = 1;
	else if (root_server == -1) {
		/* if -r option is specified in the command line */
		root_server = 0;
		fprintf(stderr,
		"No root object present; running as non-root server.\n");
	}

	if (root_server) {
		update_cache_data(rootobj);  /* must do after detach */
		if (verbose)
			syslog(LOG_INFO, "Service running as root server.");
		if (we_serve(&(rootobj->DI_data), MASTER_ONLY) &&
		    !verify_table_exists(__nis_rpc_domain()))
			exit(1);
		nis_destroy_object(rootobj); /* free; not needed anymore */
	}

	if (debug) {
		fprintf(stderr, "... database initialization complete.\n");
		fprintf(stderr, "Transaction log initialization ...\n");
	}

	if (! status)
		syslog(LOG_ERR, "WARNING: Dictionary not initialized!");

	sprintf(logname, "%s", LOG_FILE);
	if (map_log(logname, FNISD)) {
		if (debug)
			fprintf(stderr, "Transaction log corrupt. Exiting.\n");
		else
			syslog(LOG_ERR, "Transaction log corrupt. Exiting.");
		exit(1);
	}

	/* initialize the timestamp cache table */
	init_updatetime();

	if (debug) {
		fprintf(stderr, "... transaction log initialized.\n");
	}

	/* Initialize in-core list of directories served by this server */
	(void) nis_server_control(SERVING_LIST, DIR_INITLIST, NULL);

	/* rpc registration */
	if (debug) {
		fprintf(stderr, "RPC program registration ...\n");
	}

	rpcb_unset(NIS_PROG, NIS_VERSION, NULL);
	if (emulate_yp)
	{
		rpcb_unset(YPPROG, YPVERS, NULL);
		rpcb_unset(YPPROG, YPVERS_ORIG, NULL);
	}
	i = nis_svc_create(nis_prog_svc, NIS_PROG, NIS_VERSION, NC_VISIBLE);
	if (! i)
		exit(1);
	else if (verbose)
		syslog(LOG_INFO, "NIS+ service listening on %d transports.", i);
	if (emulate_yp) {
		i = nis_svc_create(ypprog_svc, YPPROG, YPVERS, NC_VISIBLE);
		if (! i)
			exit(1);
		else if (verbose)
			syslog(LOG_INFO,
				"NIS service listening on %d transports.", i);
		i = nis_svc_create(ypprog_1, YPPROG, YPVERS_ORIG, NC_VISIBLE);
		if (! i)
			exit(1);
		else if (verbose)
			syslog(LOG_INFO, "Created %d YPVERS_ORIG handles.", i);
	}

	set_rpc_gss_svc_names();

	gettimeofday(&now, NULL);
	last_purge = prev_since = now.tv_sec;
	__svc_nisplus_enable_timestamps();

	if (debug) {
		fprintf(stderr, "... RPC registration complete.\n");
		fprintf(stderr, "Service starting.\n");
	}

	/*
	 * If we crashed during update, directory_invalid will contain
	 * the name of the invalidated directory; otherwise, it will
	 * be NULL.  (map_log sets this)
	 */
	if (invalid_directory) {
		nis_object id[1], *invdir = id;
		struct ticks t[1];
		ping_item dummy_ping[1];
		int drastic_measures = 0;

		syslog(LOG_WARNING,
		"directory %s corrupted during update; attempting recovery",
			invalid_directory);
		clear_directory(invalid_directory);
		/* forge a ping item; fill just enought for replica_update */
		if (__directory_object(invalid_directory, t, 0, &invdir) !=
		    NIS_SUCCESS) {
			syslog(LOG_WARNING,
		"recovery for %s failed; couldn't get directory object",
				invalid_directory);
			drastic_measures = 1;
		} else {
			dummy_ping->item.name = invalid_directory;
			dummy_ping->mtime = 0;
			dummy_ping->obj = invdir;
			if (!replica_update(dummy_ping)) {
				syslog(LOG_WARNING,
				"recovery for %s failed; couldn't resync",
					invalid_directory);
				/*
				 * replica_update will also have invalidated
				 * this directory, but just to be sure...
				 */
				drastic_measures = 1;
			}
		}

		if (drastic_measures) {
			syslog(LOG_WARNING,
			"Forcing resync by setting update time to 0 for %s",
				invalid_directory);
			syslog(LOG_WARNING,
				"You may need to restore from backup");
			make_stamp(invalid_directory, 0);
		} else
			syslog(LOG_WARNING,
			"Recovery for %s completed", invalid_directory);
	}
	/*
	 * An unrolled version of svc_run to allow us to checkpoint when
	 * we aren't busy.
	 */
	if (cons)
		fprintf(cons, "rpc.nisd : Ready to roll...\n");

	cp_time.tv_usec = 0;
	for (;;) {

		/*
		 * Check open fd:s if more than 'fd_check_interval' seconds
		 * since last time.
		 *
		 * XXX: The somewhat complicated code below sets the
		 *	purge_delta depending on the number of open fd:s,
		 *	using linear interpolation in the purge_deltas[]
		 *	array.
		 */
		gettimeofday(&now, NULL);
		if (now.tv_sec - fd_check >= fd_check_interval) {
			long	index;
			int	deltas = (sizeof (purge_deltas)/
					sizeof (purge_deltas[0]));
			cur_open_fd = fd_open_count();
			fd_check = now.tv_sec;
			index = (cur_open_fd-1) * deltas / dtbsize;
			if (index < 0) {
				purge_delta = purge_deltas[0];
			} else if (index < deltas-1) {
				/* Interpolate */
				purge_delta = purge_deltas[index] +
			((cur_open_fd - index*(dtbsize/deltas)) *
			(purge_deltas[index+1]-purge_deltas[index])) /
				(dtbsize/deltas);
			} else {
				purge_delta = purge_deltas[deltas-1];
			}
			fd_check_interval = purge_delta;
			if (fd_check_interval > FD_CHECK_DEF)
				fd_check_interval = FD_CHECK_DEF;
			/*
			 * Run a purge on virtual circuits unused since now
			 * minus purge_delta seconds, if
			 *
			 *   The new purge cutoff would be more recent than
			 *   the one used in the last purge, and
			 *
			 *	We have a fairly large number of fd:s open
			 *	(more than 128, if deltas=16 and dtbsize=1024),
			 *
			 *	or
			 *
			 *	More than 'purge_delta' seconds have passed
			 *	since the last purge.
			 */
			if (now.tv_sec - purge_delta > prev_since &&
		(index > 1 || now.tv_sec - last_purge >= purge_delta)) {
				__svc_nisplus_purge_since(
					now.tv_sec-purge_delta);
				prev_since = now.tv_sec - purge_delta;
				last_purge = now.tv_sec;
			}
		}

		gettimeofday(&now, NULL);
		if (now.tv_sec > next_refresh) {
			next_refresh = __nis_serverRefreshCache();
			syslog(LOG_DEBUG, "nis_main: next_refresh %u",
					next_refresh);
		}

		/*
		 * Clean up our child processes. A child may be either
		 * the readonly process, or a callback.
		 */
		if (children > 0) {
			if (readonly_pid) {
				if (cons)
					fprintf(cons,
					"killing read only child: pid #%d",
					readonly_pid);
				syslog(LOG_INFO,
					"killing read only child: pid #%d",
					readonly_pid);

				kill(readonly_pid, SIGHUP);
			}

			reap();

		}

		if (readonly)
			cp_time.tv_sec = 10;
		else
			cp_time.tv_sec = DIR_IDLE_TIME;

		/*
		 * Check whether there is any server fd on which
		 * we may have to wait.
		 */
		if (nfds != svc_max_pollfd) {
			svc_pollset = realloc(svc_pollset,
					sizeof (pollfd_t) * svc_max_pollfd);
			nfds = svc_max_pollfd;
		}
		if (nfds == 0)
			break; /* None waiting, thus return */

		(void) memcpy(svc_pollset, svc_pollfd,
					sizeof (pollfd_t) * svc_max_pollfd);
		switch ((pollret =
				poll(svc_pollset, nfds,
				__rpc_timeval_to_msec(&cp_time)))) {
			case -1:
				/*
				 * We ignore all other errors except EFAULT. For
				 * all other errors, we just continue with the
				 * assumption that it was set by the signal
				 * handlers (or any other outside event) and
				 * not caused by poll().
				 */
				if (errno != EFAULT) {
					continue;
				}
				(void) syslog(LOG_ERR, "poll(2) failed: %s",
							strerror(errno));
				break;
			case 0:
				/*
				 * Timeout, let's do a checkpoint
				 */
				if (readonly && hup_received &&
							    (children == 0)) {
					if (cons)
						fprintf(cons,
						"readonly listener exiting.\n");
					if (verbose)
					    syslog(LOG_INFO,
						"readonly listener exiting.");
					exit(0);
				}
				/* standby mode conserve resources */
				(void) db_standby(0);
				break;
			default:
				svc_getreq_poll(svc_pollset, pollret);
				/*
				 * a weak attempt at fixing those memory leaks
				 */
				if (looseends) {
					do_cleanup(looseends);
					looseends = NULL;
				}
				if (readonly && hup_received &&
							    (children == 0)) {
					if (cons)
						fprintf(cons,
						"readonly listener exiting.\n");
					if (verbose)
					    syslog(LOG_INFO,
						"readonly listener exiting.");
					exit(0);
				}
				break;
		}
		if (! readonly) {
			check_updaters();
			check_pingers();

			/* the force_checkpoint flag is set by -F or ping -C */
			/* honor max_children to limit number of forks	*/
			/* limit readonly children to one	*/

			/* NOTE: need to check readonly flag again so that */
			/* the readonly child created in check_updaters() */
			/* does not try doing checkpointing.	*/

			if ((force_checkpoint) && (children < max_children) &&
				(! readonly) && (readonly_pid == 0)) {
				/*
				 * go readonly.
				 */
				pid = fork();
				if (pid == 0) {
					/* readonly child */
					if (verbose)
						syslog(LOG_INFO,
					"rpc.nisd: Readonly listener spawned.");
					readonly = 1;
					max_children = max_children - children;
					if (max_children <= 0)
						max_children = 1;
					children = 0;
					(void) sigemptyset(&sigactn.sa_mask);
					sigactn.sa_handler = timetodie;
					sigactn.sa_flags = 0;
					sigaction(SIGHUP, &sigactn, NULL);
					/* now in read only mode */
					continue;
				} else {
					/* parent process */
					if (pid == -1) {
						syslog(LOG_ERR,
			"rpc.nisd: unable to fork readonly listener process.");
						continue;
					}
					if (verbose)
						syslog(LOG_INFO,
			"checkpointing: forked readonly child pid %d", pid);
					readonly_pid = pid;
					children++;
				}

				force_checkpoint = FALSE;
				/*
				 * Give the local databases a chance to
				 * checkpoint their data.
				 * XXX If nis_checkpoint_svc maintained
				 * a list of directories to be
				 * checkpointed, we should call
				 * do_checkpoint_dir on items on
				 * that list here, rather than
				 * calling do_checkpoint(NULL).
				 */
				if (verbose)
					syslog(LOG_INFO,
						"Service Checkpoint...");
				checkpoint_db();
				if (checkpoint_log()) {
					if (verbose)
						syslog(LOG_INFO,
						"checkpoint succeeded.");
					need_checkpoint = FALSE;
				} else if (verbose)
					syslog(LOG_INFO, "failed.");
			}
		}
	}
}

int
__rpcsec_gss_is_server()
{
	return (1);
}
