/*
 *	autod_nfs.c
 *
 *	Copyright (c) 1988-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)autod_nfs.c 1.99	99/08/12 SMI"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <syslog.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/signal.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <sys/mount.h>
#include <sys/mntent.h>
#include <sys/mnttab.h>
#include <sys/fstyp.h>
#include <sys/fsid.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netconfig.h>
#include <netdir.h>
#include <errno.h>
#define	NFSCLIENT
#include <nfs/nfs.h>
#include <nfs/mount.h>
#include <rpcsvc/mount.h>
#include <rpc/nettype.h>
#include <locale.h>
#include <setjmp.h>
#include <sys/socket.h>
#include <thread.h>
#include <limits.h>
#include <nss_dbdefs.h>			/* for NSS_BUFLEN_HOSTS */
#include <nfs/nfs_sec.h>
#include "automount.h"
#include "replica.h"
#include "nfs_subr.h"
#include "webnfs.h"
#include <sys/sockio.h>
#include <net/if.h>
#include <assert.h>

extern char *nfs_get_qop_name();
extern AUTH *nfs_create_ah();
extern enum snego_stat nfs_sec_nego();

#define	MAXHOSTS	512

/* number of transports to try */
#define	MNT_PREF_LISTLEN	2
#define	FIRST_TRY		1
#define	SECOND_TRY		2

#define	MNTTYPE_CACHEFS "cachefs"

/*
 * The following definitions must be kept in sync
 * with those in lib/libnsl/rpc/clnt_dg.c
 */
#define	RPC_MAX_BACKOFF	30
#define	CLCR_GET_RPCB_TIMEOUT	1
#define	CLCR_SET_RPCB_TIMEOUT	2
#define	CLCR_SET_RPCB_RMTTIME	5
#define	CLCR_GET_RPCB_RMTTIME	6

/*
 * host cache states
 */
#define	NOHOST		0
#define	GOODHOST	1
#define	DEADHOST	2

#define	NFS_ARGS_EXTB_secdata(args, secdata) \
	{ (args).nfs_args_ext = NFS_ARGS_EXTB, \
	(args).nfs_ext_u.nfs_extB.secdata = secdata; }

struct cache_entry {
	struct	cache_entry *cache_next;
	char	*cache_host;
	time_t	cache_time;
	int	cache_state;
	rpcvers_t cache_reqvers;
	rpcvers_t cache_outvers;
};

struct mfs_snego_t {
	int sec_opt;
	bool_t snego_done;
	char *nfs_flavor;
	seconfig_t nfs_sec;
};
typedef struct mfs_snego_t mfs_snego_t;

static struct cache_entry *cache_head = NULL;
rwlock_t cache_lock;	/* protect the cache chain */

static enum nfsstat nfsmount(struct mapfs *, char *, char *, int, int);
static int is_nfs_port(char *);

static void netbuf_free(struct netbuf *);
static struct knetconfig *get_knconf(struct netconfig *);
static void free_knconf(struct knetconfig *);
static int get_pathconf(CLIENT *, char *, char *, struct pathcnf **, int);
static struct mapfs *enum_servers(struct mapent *, char *);
static struct mapfs *get_mysubnet_servers(struct mapfs *);
static int subnet_test(int af, struct sioc_addrreq *);
static	struct	netbuf *get_addr(char *, rpcprog_t, rpcvers_t,
	struct netconfig **, char *, ushort_t, struct t_info *);

static	struct	netbuf *get_pubfh(char *, rpcvers_t, mfs_snego_t *,
	struct netconfig **, char *, ushort_t, struct t_info *, caddr_t *,
	bool_t, char *);

enum type_of_stuff {
	SERVER_ADDR = 0,
	SERVER_PING = 1,
	SERVER_FH = 2
};

static	void *get_server_stuff(enum type_of_stuff, char *, rpcprog_t,
	rpcvers_t, mfs_snego_t *, struct netconfig **, char *, ushort_t,
	struct t_info *, caddr_t *, bool_t, char *, enum clnt_stat *);

static	void *get_the_stuff(enum type_of_stuff, char *, rpcprog_t,
	rpcvers_t, mfs_snego_t *, struct netconfig *, ushort_t, struct t_info *,
	caddr_t *, bool_t, char *, enum clnt_stat *);

struct mapfs *add_mfs(struct mapfs *, int, struct mapfs **, struct mapfs **);
void free_mfs(struct mapfs *);
static void dump_mfs(struct mapfs *, char *, int);
static char *dump_distance(struct mapfs *);
static void cache_free(struct cache_entry *);
static int cache_check(char *, rpcvers_t *);
static void cache_enter(char *, rpcvers_t, rpcvers_t, int);
static void destroy_auth_client_handle(CLIENT *cl);

#ifdef CACHE_DEBUG
static void trace_host_cache();
static void trace_portmap_cache();
#endif /* CACHE_DEBUG */

static int rpc_timeout = 20;

#ifdef CACHE_DEBUG
/*
 * host cache counters. These variables do not need to be protected
 * by mutex's. They have been added to measure the utility of the
 * goodhost/deadhost cache in the lazy hierarchical mounting scheme.
 */
static int host_cache_accesses = 0;
static int host_cache_lookups = 0;
static int deadhost_cache_hits = 0;
static int goodhost_cache_hits = 0;

/*
 * portmap cache counters. These variables do not need to be protected
 * by mutex's. They have been added to measure the utility of the portmap
 * cache in the lazy hierarchical mounting scheme.
 */
static int portmap_cache_accesses = 0;
static int portmap_cache_lookups = 0;
static int portmap_cache_hits = 0;
#endif /* CACHE_DEBUG */


int
mount_nfs(me, mntpnt, prevhost, overlay)
	struct mapent *me;
	char *mntpnt;
	char *prevhost;
	int overlay;
{
	struct mapfs *mfs, *mp;
	int err = -1;
	int cached;

	mfs = enum_servers(me, prevhost);
	if (mfs == NULL)
		return (ENOENT);

	/*
	 * Try loopback if we have something on localhost; if nothing
	 * works, we will fall back to NFS
	 */
	if (is_nfs_port(me->map_mntopts)) {
		for (mp = mfs; mp; mp = mp->mfs_next) {
			if (self_check(mp->mfs_host)) {
				err = loopbackmount(mp->mfs_dir,
					mntpnt, me->map_mntopts, overlay);
				if (err) {
					mp->mfs_ignore = 1;
				} else {
					break;
				}
			}
		}
	}
	if (err) {
		cached = strcmp(me->map_mounter, MNTTYPE_CACHEFS) == 0;
		err = nfsmount(mfs, mntpnt, me->map_mntopts,
				cached, overlay);
		if (err && trace > 1) {
			trace_prt(1, "	Couldn't mount %s:%s, err=%d\n",
				mfs->mfs_host, mfs->mfs_dir, err);
		}
	}
	free_mfs(mfs);
	return (err);
}


/*
 * Using the new ioctl SIOCTONLINK to determine if a host is on the same
 * subnet. Remove the old network, subnet check.
 */

static struct mapfs *
get_mysubnet_servers(struct mapfs *mfs_in)
{
	int s;
	struct mapfs *mfs, *p, *mfs_head = NULL, *mfs_tail = NULL;

	struct netconfig *nconf;
	NCONF_HANDLE *nc = NULL;
	struct nd_hostserv hs;
	struct nd_addrlist *retaddrs;
	struct netbuf *nb;
	struct sioc_addrreq areq;
	int res;
	int af;
	int i;
	int sa_size;

	hs.h_serv = "rpcbind";

	for (mfs = mfs_in; mfs; mfs = mfs->mfs_next) {
		nc = setnetconfig();

		while (nconf = getnetconfig(nc)) {

			/*
			 * Care about INET family only. proto_done flag
			 * indicates if we have already covered this
			 * protocol family. If so skip it
			 */
			if (((strcmp(nconf->nc_protofmly, NC_INET6) == 0) ||
				(strcmp(nconf->nc_protofmly, NC_INET) == 0)) &&
				(nconf->nc_semantics == NC_TPI_CLTS)) {
			} else
				continue;

			hs.h_host = mfs->mfs_host;

			if (netdir_getbyname(nconf, &hs, &retaddrs) != ND_OK)
				continue;

			/*
			 * For each host address see if it's on our
			 * local subnet.
			 */

			if (strcmp(nconf->nc_protofmly, NC_INET6) == 0)
				af = AF_INET6;
			else
				af = AF_INET;
			nb = retaddrs->n_addrs;
			for (i = 0; i < retaddrs->n_cnt; i++, nb++) {
				memcpy(&areq.sa_addr, nb->buf,
					sizeof (struct sockaddr_storage));
				if (res = subnet_test(af, &areq)) {
					p = add_mfs(mfs, DIST_MYNET,
						&mfs_head, &mfs_tail);
					if (!p) {
						netdir_free(retaddrs,
							ND_ADDRLIST);
						endnetconfig(nc);
						return (NULL);
					}
					break;
				}
			}  /* end of every host */
			if (trace > 2) {
				trace_prt(1, "get_mysubnet_servers: host=%s "
					"netid=%s res=%s\n", mfs->mfs_host,
					nconf->nc_netid, res == 1?"SUC":"FAIL");
			}

			netdir_free(retaddrs, ND_ADDRLIST);
		} /* end of while */

		endnetconfig(nc);

	} /* end of every map */

	return (mfs_head);

}

int
subnet_test(int af, struct sioc_addrreq *areq)
{
	int s;

	if ((s = socket(af, SOCK_DGRAM, 0)) < 0) {
		return (0);
	}

	areq->sa_res = -1;

	if (ioctl(s, SIOCTONLINK, (caddr_t)areq) < 0) {
		syslog(LOG_ERR, "subnet_test:SIOCTONLINK failed");
		return (0);
	}
	close(s);
	if (areq->sa_res == 1)
		return (1);
	else
		return (0);


}

/*
 * ping a bunch of hosts at once and sort by who responds first
 */
static struct mapfs *
sort_servers(struct mapfs *mfs_in, int timeout)
{
	struct mapfs *m1 = NULL;
	enum clnt_stat clnt_stat;

	if (!mfs_in)
		return (NULL);

	clnt_stat = nfs_cast(mfs_in, &m1, timeout);

	if (!m1) {
		char buff[2048] = {'\0'};

		for (m1 = mfs_in; m1; m1 = m1->mfs_next) {
			(void) strcat(buff, m1->mfs_host);
			if (m1->mfs_next)
				(void) strcat(buff, ",");
		}

		syslog(LOG_ERR, "servers %s not responding: %s",
			buff, clnt_sperrno(clnt_stat));
	}

	return (m1);
}

/*
 * Add a mapfs entry to the list described by *mfs_head and *mfs_tail,
 * provided it is not marked "ignored" and isn't a dupe of ones we've
 * already seen.
 */
struct mapfs *
add_mfs(struct mapfs *mfs, int distance, struct mapfs **mfs_head,
	struct mapfs **mfs_tail)
{
	struct mapfs *tmp, *new;
	void bcopy();

	for (tmp = *mfs_head; tmp; tmp = tmp->mfs_next)
		if ((strcmp(tmp->mfs_host, mfs->mfs_host) == 0 &&
		    strcmp(tmp->mfs_dir, mfs->mfs_dir) == 0) ||
			mfs->mfs_ignore)
			return (*mfs_head);
	new = (struct mapfs *)malloc(sizeof (struct mapfs));
	if (!new) {
		syslog(LOG_ERR, "Memory allocation failed: %m");
		return (NULL);
	}
	bcopy(mfs, new, sizeof (struct mapfs));
	new->mfs_next = NULL;
	if (distance)
		new->mfs_distance = distance;
	if (!*mfs_head)
		*mfs_tail = *mfs_head = new;
	else {
		(*mfs_tail)->mfs_next = new;
		*mfs_tail = new;
	}
	return (*mfs_head);
}

static void
dump_mfs(struct mapfs *mfs, char *message, int level)
{
	struct mapfs *m1;

	if (trace <= level)
		return;

	trace_prt(1, "%s", message);
	if (!mfs) {
		trace_prt(0, "mfs is null\n");
		return;
	}
	for (m1 = mfs; m1; m1 = m1->mfs_next)
		trace_prt(0, "%s[%s] ", m1->mfs_host, dump_distance(m1));
	trace_prt(0, "\n");
}

static char *
dump_distance(struct mapfs *mfs)
{
	switch (mfs->mfs_distance) {
	case 0:			return ("zero");
	case DIST_SELF:		return ("self");
	case DIST_MYSUB:	return ("mysub");
	case DIST_MYNET:	return ("mynet");
	case DIST_OTHER:	return ("other");
	default:		return ("other");
	}
}

/*
 * Walk linked list "raw", building a new list consisting of members
 * NOT found in list "filter", returning the result.
 */
static struct mapfs *
filter_mfs(struct mapfs *raw, struct mapfs *filter)
{
	struct mapfs *mfs, *p, *mfs_head = NULL, *mfs_tail = NULL;
	int skip;

	if (!raw)
		return (NULL);
	for (mfs = raw; mfs; mfs = mfs->mfs_next) {
		for (skip = 0, p = filter; p; p = p->mfs_next) {
			if (strcmp(p->mfs_host, mfs->mfs_host) == 0 &&
			    strcmp(p->mfs_dir, mfs->mfs_dir) == 0) {
				skip = 1;
				break;
			}
		}
		if (skip)
			continue;
		p = add_mfs(mfs, 0, &mfs_head, &mfs_tail);
		if (!p)
			return (NULL);
	}
	return (mfs_head);
}

/*
 * Walk a linked list of mapfs structs, freeing each member.
 */
void
free_mfs(struct mapfs *mfs)
{
	struct mapfs *tmp;

	while (mfs) {
		tmp = mfs->mfs_next;
		free(mfs);
		mfs = tmp;
	}
}

/*
 * New code for NFS client failover: we need to carry and sort
 * lists of server possibilities rather than return a single
 * entry.  It preserves previous behaviour of sorting first by
 * locality (loopback-or-preferred/subnet/net/other) and then
 * by ping times.  We'll short-circuit this process when we
 * have ENOUGH or more entries.
 */
static struct mapfs *
enum_servers(struct mapent *me, char *preferred)
{
	struct mapfs *p, *m1, *m2, *mfs_head = NULL, *mfs_tail = NULL;

	/*
	 * Short-circuit for simple cases.
	 */
	if (!me->map_fs->mfs_next) {
		p = add_mfs(me->map_fs, DIST_OTHER, &mfs_head, &mfs_tail);
		if (!p)
			return (NULL);
		return (mfs_head);
	}

	dump_mfs(me->map_fs, "	enum_servers: mapent: ", 2);

	/*
	 * get addresses & see if any are myself
	 * or were mounted from previously in a
	 * hierarchical mount.
	 */
	if (trace > 2)
		trace_prt(1, "	enum_servers: looking for pref/self\n");
	for (m1 = me->map_fs; m1; m1 = m1->mfs_next) {
		if (m1->mfs_ignore)
			continue;
		if (self_check(m1->mfs_host) ||
		    strcmp(m1->mfs_host, preferred) == 0) {
			p = add_mfs(m1, DIST_SELF, &mfs_head, &mfs_tail);
			if (!p)
				return (NULL);
		}
	}
	if (trace > 2 && m1)
		trace_prt(1, "	enum_servers: pref/self found, %s\n",
			m1->mfs_host);

	/*
	 * look for entries on this subnet
	 */
	dump_mfs(m1, "	enum_servers: input of get_mysubnet_servers: ", 2);
	m1 = get_mysubnet_servers(me->map_fs);
	dump_mfs(m1, "	enum_servers: output of get_mysubnet_servers: ", 3);
	if (m1 && m1->mfs_next) {
		m2 = sort_servers(m1, rpc_timeout / 2);
		dump_mfs(m2, "	enum_servers: output of sort_servers: ", 3);
		free_mfs(m1);
		m1 = m2;
	}

	for (m2 = m1; m2; m2 = m2->mfs_next) {
		p = add_mfs(m2, 0, &mfs_head, &mfs_tail);
		if (!p)
			return (NULL);
	}
	if (m1)
		free_mfs(m1);

	/*
	 * add the rest of the entries at the end
	 */
	m1 = filter_mfs(me->map_fs, mfs_head);
	dump_mfs(m1, "	enum_servers: etc: output of filter_mfs: ", 3);
	m2 = sort_servers(m1, rpc_timeout / 2);
	dump_mfs(m2, "	enum_servers: etc: output of sort_servers: ", 3);
	if (m1)
		free_mfs(m1);
	m1 = m2;
	for (m2 = m1; m2; m2 = m2->mfs_next) {
		p = add_mfs(m2, DIST_OTHER, &mfs_head, &mfs_tail);
		if (!p)
			return (NULL);
	}
	if (m1)
		free_mfs(m1);

done:
	dump_mfs(mfs_head, "  enum_servers: output: ", 1);
	return (mfs_head);
}

static enum nfsstat
nfsmount(mfs_in, mntpnt, opts, cached, overlay)
	struct mapfs *mfs_in;
	char *mntpnt, *opts;
	int cached, overlay;
{
	CLIENT *cl;
	char remname[MAXPATHLEN], *mnttabtext = NULL;
	char mopts[MAX_MNTOPT_STR];
	int mnttabcnt = 0;
	int loglevel;
	struct mnttab m;
	struct nfs_args *argp = NULL, *head = NULL, *tail = NULL,
		*prevhead, *prevtail;
	int flags;
	struct fhstatus fhs;
	struct timeval timeout;
	enum clnt_stat rpc_stat;
	enum nfsstat status;
	struct stat stbuf;
	struct netconfig *nconf;
	rpcvers_t vers, versmin; /* used to negotiate nfs version in pingnfs */
				/* and mount version with mountd */
	rpcvers_t outvers;	/* final version to be used during mount() */
	rpcvers_t nfsvers;	/* version in map options, 0 if not there */
	rpcvers_t mountversmax;	/* tracks the max mountvers during retries */

	/* used to negotiate nfs version using webnfs */
	rpcvers_t pubvers, pubversmin, pubversmax;
	int posix;
	struct nd_addrlist *retaddrs;
	struct mountres3 res3;
	nfs_fh3 fh3;
	char *fstype;
	int count, i;
	char scerror_msg[MAXMSGLEN];
	int *auths;
	int delay;
	int retries;
	char *nfs_proto = NULL;
	uint_t nfs_port = 0;
	char *p, *host, *dir;
	struct mapfs *mfs = NULL;
	int error, last_error = 0;
	int replicated;
	int entries = 0;
	int v2cnt = 0, v3cnt = 0;
	int v2near = 0, v3near = 0;
	int skipentry = 0;
	char *nfs_flavor;
	seconfig_t nfs_sec;
	int sec_opt, scerror;
	struct sec_data *secdata;
	int secflags;
	struct netbuf *syncaddr;
	bool_t	use_pubfh;
	ushort_t thisport;
	int got_val;
	mfs_snego_t mfssnego_init, mfssnego;

	dump_mfs(mfs_in, "  nfsmount: input: ", 2);
	replicated = (mfs_in->mfs_next != NULL);
	m.mnt_mntopts = opts;
	if (replicated && hasmntopt(&m, MNTOPT_SOFT)) {
		if (verbose)
			syslog(LOG_WARNING,
		    "mount on %s is soft and will not be replicated.", mntpnt);
		replicated = 0;
	}
	if (replicated && !hasmntopt(&m, MNTOPT_RO)) {
		if (verbose)
			syslog(LOG_WARNING,
		    "mount on %s is not read-only and will not be replicated.",
			    mntpnt);
		replicated = 0;
	}
	if (replicated && cached) {
		if (verbose)
			syslog(LOG_WARNING,
		    "mount on %s is cached and will not be replicated.",
			mntpnt);
		replicated = 0;
	}
	if (replicated)
		loglevel = LOG_WARNING;
	else
		loglevel = LOG_ERR;

	if (trace > 1) {
		if (replicated)
			trace_prt(1, "	nfsmount: replicated mount on %s %s:\n",
				mntpnt, opts);
		else
			trace_prt(1, "	nfsmount: standard mount on %s %s:\n",
				mntpnt, opts);
		for (mfs = mfs_in; mfs; mfs = mfs->mfs_next)
			trace_prt(1, "	  %s:%s\n",
				mfs->mfs_host, mfs->mfs_dir);
	}

	/*
	 * Make sure mountpoint is safe to mount on
	 */
	if (lstat(mntpnt, &stbuf) < 0) {
		syslog(LOG_ERR, "Couldn't stat %s: %m", mntpnt);
		return (NFSERR_NOENT);
	}

	/*
	 * Get protocol specified in options list, if any.
	 */
	if ((str_opt(&m, "proto", &nfs_proto)) == -1) {
		return (NFSERR_NOENT);
	}

	/*
	 * Get port specified in options list, if any.
	 */
	got_val = nopt(&m, MNTOPT_PORT, (int *)&nfs_port);
	if (!got_val)
		nfs_port = 0;	/* "unspecified" */
	if (nfs_port > USHRT_MAX) {
		syslog(LOG_ERR, "%s: invalid port number %d", mntpnt, nfs_port);
		return (NFSERR_NOENT);
	}

	/*
	 * Set mount(2) flags here, outside of the loop.
	 */
	flags = MS_OPTIONSTR;
	flags |= (hasmntopt(&m, MNTOPT_RO) == NULL) ? 0 : MS_RDONLY;
	flags |= (hasmntopt(&m, MNTOPT_NOSUID) == NULL) ? 0 : MS_NOSUID;
	flags |= overlay ? MS_OVERLAY : 0;
	if (mntpnt[strlen(mntpnt) - 1] != ' ')
		/* direct mount point without offsets */
		flags |= MS_OVERLAY;

	use_pubfh = (hasmntopt(&m, MNTOPT_PUBLIC) == NULL) ? FALSE : TRUE;

	(void) memset(&mfssnego_init, 0, sizeof (mfs_snego_t));
	if (hasmntopt(&m, MNTOPT_SECURE) != NULL) {
		if (++mfssnego_init.sec_opt > 1) {
			syslog(loglevel,
			    "conflicting security options");
			return (NFSERR_IO);
		}
		if (nfs_getseconfig_byname("dh", &mfssnego_init.nfs_sec)) {
			syslog(loglevel,
			    "error getting dh information from %s",
			    NFSSEC_CONF);
			return (NFSERR_IO);
		}
	}

	/*
	 * Have to workaround the fact that hasmntopt() returns true
	 * when comparing "secure" (in &m) with "sec".
	 */
	if (hasmntopt(&m, "sec=") != NULL) {
		if ((str_opt(&m, MNTOPT_SEC,
			&mfssnego_init.nfs_flavor)) == -1) {
			syslog(LOG_ERR, "nfsmount: no memory");
			return (NFSERR_IO);
		}
	}

	if (mfssnego_init.nfs_flavor) {
		if (++mfssnego_init.sec_opt > 1) {
			syslog(loglevel,
			    "conflicting security options");
			free(mfssnego_init.nfs_flavor);
			return (NFSERR_IO);
		}
		if (nfs_getseconfig_byname(mfssnego_init.nfs_flavor,
			&mfssnego_init.nfs_sec)) {
			syslog(loglevel,
			    "error getting %s information from %s",
			    mfssnego_init.nfs_flavor, NFSSEC_CONF);
			free(mfssnego_init.nfs_flavor);
			return (NFSERR_IO);
		}
		free(mfssnego_init.nfs_flavor);
	}

nextentry:
	skipentry = 0;

	got_val = nopt(&m, MNTOPT_VERS, (int *)&nfsvers);
	if (!got_val)
		nfsvers = 0;	/* "unspecified" */
	if (set_versrange(nfsvers, &vers, &versmin) != 0) {
		syslog(LOG_ERR, "Incorrect NFS version specified for %s",
			mntpnt);
		last_error = NFSERR_NOENT;
		goto ret;
	}

	if (nfsvers != 0) {
		pubversmax = pubversmin = nfsvers;
	} else {
		pubversmax = NFS_VERSMAX;
		pubversmin = NFS_VERSMIN;
	}

	/*
	 * Walk the whole list, pinging and collecting version
	 * info so that we can make sure the mount will be
	 * homogeneous with respect to version.
	 *
	 * If we have a version preference, this is easy; we'll
	 * just reject anything that doesn't match.
	 *
	 * If not, we want to try to provide the best compromise
	 * that considers proximity, preference for a higher version,
	 * sorted order, and number of replicas.  We will count
	 * the number of V2 and V3 replicas and also the number
	 * which are "near", i.e. the localhost or on the same
	 * subnet.
	 */
	for (mfs = mfs_in; mfs; mfs = mfs->mfs_next) {


		if (mfs->mfs_ignore)
			continue;

		host = mfs->mfs_host;
		(void) memcpy(&mfssnego, &mfssnego_init, sizeof (mfs_snego_t));

		if (use_pubfh == TRUE || mfs->mfs_flags & MFS_URL) {
			char *path;

			if (nfs_port != 0 && mfs->mfs_port != 0 &&
			    nfs_port != mfs->mfs_port) {

				syslog(LOG_ERR, "nfsmount: port (%u) in nfs URL"
					" not the same as port (%d) in port "
					"option\n", mfs->mfs_port, nfs_port);
				last_error = NFSERR_IO;
				goto out;

			} else if (nfs_port != 0)
				thisport = nfs_port;
			else
				thisport = mfs->mfs_port;

			dir = mfs->mfs_dir;

			if ((mfs->mfs_flags & MFS_URL) == 0) {
				path = malloc(strlen(dir) + 2);
				if (path == NULL) {
					syslog(LOG_ERR, "nfsmount: no memory");
					last_error = NFSERR_IO;
					goto out;
				}
				path[0] = (char)WNL_NATIVEPATH;
				(void) strcpy(&path[1], dir);
			} else {
				path = dir;
			}

			argp = (struct nfs_args *)
				malloc(sizeof (struct nfs_args));

			if (!argp) {
				if (path != dir)
					free(path);
				syslog(LOG_ERR, "nfsmount: no memory");
				last_error = NFSERR_IO;
				goto out;
			}
			(void) memset(argp, 0, sizeof (*argp));

			for (pubvers = pubversmax; pubvers >= pubversmin;
			    pubvers--) {

				nconf = NULL;
				argp->addr = get_pubfh(host, pubvers, &mfssnego,
				    &nconf, nfs_proto, thisport, NULL,
				    &argp->fh, TRUE, path);

				if (argp->addr != NULL)
					break;

				if (nconf != NULL)
					freenetconfigent(nconf);
			}

			if (path != dir)
				free(path);

			if (argp->addr != NULL) {

				argp->flags |= NFSMNT_LLOCK;

				mfs->mfs_args = argp;
				mfs->mfs_version = pubvers;
				mfs->mfs_nconf = nconf;
				mfs->mfs_flags |= MFS_FH_VIA_WEBNFS;

			} else {
				free(argp);

				/*
				 * If -public was specified, give up
				 * on this entry now.
				 */
				if (use_pubfh == TRUE) {
					syslog(loglevel,
					    "%s: no public file handle support",
					    host);
					last_error = NFSERR_NOENT;
					mfs->mfs_ignore = 1;
					continue;
				}

				/*
				 * Back off to a conventional mount.
				 *
				 * URL's can contain escape characters. Get
				 * rid of them.
				 */
				path = malloc(strlen(dir) + 2);

				if (path == NULL) {
					syslog(LOG_ERR, "nfsmount: no memory");
					last_error = NFSERR_IO;
					goto out;
				}

				strcpy(path, dir);
				URLparse(path);
				mfs->mfs_dir = path;
				mfs->mfs_flags |= MFS_ALLOC_DIR;
				mfs->mfs_flags &= ~MFS_URL;
			}
		}

		if ((mfs->mfs_flags & MFS_FH_VIA_WEBNFS) ==  0) {
			i = pingnfs(host, get_retry(opts) + 1, &vers, versmin,
				0, FALSE, NULL);
			if (i != RPC_SUCCESS) {
				syslog(loglevel, "server %s not responding",
					host);
				mfs->mfs_ignore = 1;
				last_error = NFSERR_NOENT;
				continue;
			}
			if (nfsvers != 0 && nfsvers != vers) {
				syslog(loglevel,
					"NFS version %d not supported by %s",
					nfsvers, host);
				mfs->mfs_ignore = 1;
				last_error = NFSERR_NOENT;
				continue;
			}
		}

		if (vers == NFS_V3)
			v3cnt++;
		else
			v2cnt++;

		/*
		 * It's not clear how useful this stuff is if
		 * we are using webnfs across the internet, but it
		 * can't hurt.
		 */
		if (mfs->mfs_distance &&
		    mfs->mfs_distance <= DIST_MYSUB) {
			if (vers == NFS_V3)
				v3near++;
			else
				v2near++;
		}

		/*
		 * If the mount is not replicated, we don't want to
		 * ping every entry, so we'll stop here.  This means
		 * that we may have to go back to "nextentry" above
		 * to consider another entry if there we can't get
		 * all the way to mount(2) with this one.
		 */
		if (!replicated)
			break;
	}

	if (nfsvers == 0) {
		/*
		 * Choose the NFS version.
		 * V3-capable servers are better, if we only have
		 * V2 nearby, we'd rather use them to avoid going
		 * through a router.  If we downgrade to NFS V2,
		 * we can use the V3 servers that also support V2.
		 */
		if (v3cnt >= v2cnt && (v3near || !v2near))
			nfsvers = NFS_V3;
		else
			nfsvers = NFS_VERSION;
		if (trace > 2)
			trace_prt(1,
			    "  nfsmount: v3=%d[%d],v2=%d[%d] => v%d.\n",
			    v3cnt, v3near, v2cnt, v2near, nfsvers);
	}

	/*
	 * Since we don't support different NFS versions in replicated
	 * mounts, set fstype now.
	 */
	switch (nfsvers) {
	case NFS_V3:
		fstype = MNTTYPE_NFS3;
		break;
	case NFS_VERSION:
		fstype = MNTTYPE_NFS;
		break;
	}

	if (use_pubfh == FALSE) {
		/*
		 * Now choose the proper mount protocol version
		 */
		if (nfsvers == NFS_V3) {
			mountversmax = MOUNTVERS3;
			versmin = MOUNTVERS3;
		} else {
			mountversmax = MOUNTVERS_POSIX;
			versmin = MOUNTVERS;
		}
	}

	/*
	 * Our goal here is to evaluate each of several possible
	 * replicas and try to come up with a list we can hand
	 * to mount(2).  If we don't have a valid "head" at the
	 * end of this process, it means we have rejected all
	 * potential server:/path tuples.  We will fail quietly
	 * in front of mount(2), and will have printed errors
	 * where we found them.
	 * XXX - do option work outside loop w careful design
	 * XXX - use macro for error condition free handling
	 */
	for (mfs = mfs_in; mfs; mfs = mfs->mfs_next) {

		/*
		 * Initialize retry and delay values on a per-server basis.
		 */
		retries = get_retry(opts);
		delay = INITDELAY;
retry:
		if (mfs->mfs_ignore)
			continue;

		/*
		 * If we don't have a fh yet, and if this is not a replicated
		 * mount, we haven't done a pingnfs() on the next entry,
		 * so we don't know if the next entry is up or if it
		 * supports an NFS version we like.  So if we had a problem
		 * with an entry, we need to go back and run through some new
		 * code.
		 */
		if ((mfs->mfs_flags & MFS_FH_VIA_WEBNFS) == 0 &&
		    !replicated && skipentry)
			goto nextentry;

		vers = mountversmax;
		host = mfs->mfs_host;
		dir = mfs->mfs_dir;
		(void) sprintf(remname, "%s:%s", host, dir);
		if (trace > 4 && replicated)
			trace_prt(1, "	nfsmount: examining %s\n", remname);

		/*
		 * If it's cached we need to get cachefs to mount it.
		 */
		if (cached) {
			char *copts = opts;

			/*
			 * If we started with a URL we need to turn on
			 * -o public if not on already
			 */
			if (use_pubfh == FALSE &&
			    (mfs->mfs_flags & MFS_FH_VIA_WEBNFS)) {

				copts = malloc(strlen(opts) +
					strlen(",public")+1);

				if (copts == NULL) {
					syslog(LOG_ERR, "nfsmount: no memory");
					last_error = NFSERR_IO;
					goto out;
				}

				strcpy(copts, opts);

				if (strlen(copts) != 0)
					strcat(copts, ",");

				strcat(copts, "public");
			}

			last_error = mount_generic(remname, MNTTYPE_CACHEFS,
				copts, mntpnt, overlay);

			if (copts != opts)
				free(copts);

			if (last_error) {
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
			goto out;
		}

		if (mfs->mfs_args == NULL) {

			/*
			 * Allocate nfs_args structure
			 */
			argp = (struct nfs_args *)
				malloc(sizeof (struct nfs_args));

			if (!argp) {
				syslog(LOG_ERR, "nfsmount: no memory");
				last_error = NFSERR_IO;
				goto out;
			}

			(void) memset(argp, 0, sizeof (*argp));

		} else {
			argp = mfs->mfs_args;
			mfs->mfs_args = NULL;

			/*
			 * Skip entry if we already have file handle but the
			 * NFS version is wrong.
			 */
			if ((mfs->mfs_flags & MFS_FH_VIA_WEBNFS) &&
			    mfs->mfs_version != nfsvers) {

				free(argp);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
		}

		prevhead = head;
		prevtail = tail;
		if (!head)
			head = tail = argp;
		else
			tail = tail->nfs_ext_u.nfs_extB.next = argp;

		if ((mfs->mfs_flags & MFS_FH_VIA_WEBNFS) == 0) {
		    timeout.tv_usec = 0;
		    timeout.tv_sec = rpc_timeout;
		    rpc_stat = RPC_TIMEDOUT;

		    /* Create the client handle. */

		    if (trace > 1) {
			trace_prt(1, "  nfsmount: Get mount version: request "
			    "vers=%d min=%d\n", vers, versmin);
		    }

		    while ((cl = clnt_create_vers(host, MOUNTPROG, &outvers,
			versmin, vers, "udp")) == NULL) {
			if (trace > 4) {
			    trace_prt(1,
			    "  nfsmount: Can't get mount version: rpcerr=%d\n",
			    rpc_createerr.cf_stat);
			}
			if (rpc_createerr.cf_stat == RPC_UNKNOWNHOST ||
			    rpc_createerr.cf_stat == RPC_TIMEDOUT)
				break;

			/*
			 * backoff and return lower version to retry the ping.
			 * XXX we should be more careful and handle
			 * RPC_PROGVERSMISMATCH here, because that error
			 * is handled in clnt_create_vers(). It's not done to
			 * stay in sync with the nfs mount command.
			 */
			vers--;
			if (vers < versmin)
				break;
			if (trace > 4) {
			    trace_prt(1, "  nfsmount: Try version=%d\n", vers);
			}
		    }

		    if (cl == NULL) {
			free(argp);
			head = prevhead;
			tail = prevtail;
			if (tail)
				tail->nfs_ext_u.nfs_extB.next = NULL;
			last_error = NFSERR_NOENT;

			if (rpc_createerr.cf_stat != RPC_UNKNOWNHOST &&
			    rpc_createerr.cf_stat != RPC_PROGVERSMISMATCH &&
			    retries-- > 0) {
				DELAY(delay)
				goto retry;
			}

			syslog(loglevel, "%s %s", host,
				clnt_spcreateerror("server not responding"));
			skipentry = 1;
			mfs->mfs_ignore = 1;
			continue;
		    }
		    if (trace > 1) {
			trace_prt(1, "	nfsmount: mount version=%d\n", outvers);
		    }

		    if (__clnt_bindresvport(cl) < 0) {
			free(argp);
			head = prevhead;
			tail = prevtail;
			if (tail)
				tail->nfs_ext_u.nfs_extB.next = NULL;
			last_error = NFSERR_NOENT;

			if (retries-- > 0) {
				destroy_auth_client_handle(cl);
				DELAY(delay);
				goto retry;
			}

			syslog(loglevel, "mount %s: %s", host,
				"Couldn't bind to reserved port");
			destroy_auth_client_handle(cl);
			skipentry = 1;
			mfs->mfs_ignore = 1;
			continue;
		    }

		    if ((cl->cl_auth = authsys_create_default()) == NULL) {
			free(argp);
			head = prevhead;
			tail = prevtail;
			if (tail)
				tail->nfs_ext_u.nfs_extB.next = NULL;
			last_error = NFSERR_NOENT;

			if (retries-- > 0) {
				destroy_auth_client_handle(cl);
				DELAY(delay);
				goto retry;
			}

			syslog(loglevel, "mount %s: %s", host,
				"Failed creating default auth handle");
			destroy_auth_client_handle(cl);
			skipentry = 1;
			mfs->mfs_ignore = 1;
			continue;
		    }
		} else
		    cl = NULL;

		/*
		 * set security options
		 */
		sec_opt = 0;
		(void) memset(&nfs_sec, 0, sizeof (nfs_sec));
		if (hasmntopt(&m, MNTOPT_SECURE) != NULL) {
			if (++sec_opt > 1) {
				syslog(loglevel,
				    "conflicting security options for %s",
				    remname);
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_extB.next = NULL;
				last_error = NFSERR_IO;
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
			if (nfs_getseconfig_byname("dh", &nfs_sec)) {
				syslog(loglevel,
				    "error getting dh information from %s",
				    NFSSEC_CONF);
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_extB.next = NULL;
				last_error = NFSERR_IO;
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
		}

		nfs_flavor = NULL;
		/*
		 * Have to workaround the fact that hasmntopt() returns true
		 * when comparing "secure" (in &m) with "sec".
		 */
		if (hasmntopt(&m, "sec=") != NULL) {
			if ((str_opt(&m, MNTOPT_SEC, &nfs_flavor)) == -1) {
				syslog(LOG_ERR, "nfsmount: no memory");
				last_error = NFSERR_IO;
				destroy_auth_client_handle(cl);
				goto out;
			}
		}

		if (nfs_flavor) {
			if (++sec_opt > 1) {
				syslog(loglevel,
				    "conflicting security options for %s",
				    remname);
				free(nfs_flavor);
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_extB.next = NULL;
				last_error = NFSERR_IO;
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
			if (nfs_getseconfig_byname(nfs_flavor, &nfs_sec)) {
				syslog(loglevel,
				    "error getting %s information from %s",
				    nfs_flavor, NFSSEC_CONF);
				free(nfs_flavor);
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_extB.next = NULL;
				last_error = NFSERR_IO;
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
			free(nfs_flavor);
		}

		posix = (hasmntopt(&m, MNTOPT_POSIX) != NULL) ? 1 : 0;

		if ((mfs->mfs_flags & MFS_FH_VIA_WEBNFS) == 0) {
		    bool_t give_up_on_mnt;
		    bool_t got_mnt_error;
		/*
		 * If we started with a URL, if first byte of path is not "/",
		 * then the mount will likely fail, so we should try again
		 * with a prepended "/".
		 */
		    if (mfs->mfs_flags & MFS_ALLOC_DIR && *dir != '/')
			give_up_on_mnt = FALSE;
		    else
			give_up_on_mnt = TRUE;

		    got_mnt_error = FALSE;

try_mnt_slash:
		    if (got_mnt_error == TRUE) {
			int i, l;

			give_up_on_mnt = TRUE;
			l = strlen(dir);

			/*
			 * Insert a "/" to front of mfs_dir.
			 */
			for (i = l; i > 0; i--)
				dir[i] = dir[i-1];

			dir[0] = '/';
		    }

		    /* Get fhandle of remote path from server's mountd */

		    switch (outvers) {
		    case MOUNTVERS:
			if (posix) {
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_extB.next = NULL;
				last_error = NFSERR_NOENT;
				syslog(loglevel, "can't get posix info for %s",
					host);
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
		    /* FALLTHRU */
		    case MOUNTVERS_POSIX:
			if (nfsvers == NFS_V3) {
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_extB.next = NULL;
				last_error = NFSERR_NOENT;
				syslog(loglevel,
					"%s doesn't support NFS Version 3",
					host);
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
			rpc_stat = clnt_call(cl, MOUNTPROC_MNT,
				xdr_dirpath, (caddr_t)&dir,
				xdr_fhstatus, (caddr_t)&fhs, timeout);
			if (rpc_stat != RPC_SUCCESS) {

				if (give_up_on_mnt == FALSE) {
					got_mnt_error = TRUE;
					goto try_mnt_slash;
				}

				/*
				 * Given the way "clnt_sperror" works, the "%s"
				 * immediately following the "not responding"
				 * is correct.
				 */
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_extB.next = NULL;
				last_error = NFSERR_NOENT;

				if (retries-- > 0) {
					destroy_auth_client_handle(cl);
					DELAY(delay);
					goto retry;
				}

				if (trace > 3) {
				    trace_prt(1,
					"  nfsmount: mount RPC failed for %s\n",
					host);
				}
				syslog(loglevel, "%s server not responding%s",
				    host, clnt_sperror(cl, ""));
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
			if ((errno = fhs.fhs_status) != MNT_OK)  {

				if (give_up_on_mnt == FALSE) {
					got_mnt_error = TRUE;
					goto try_mnt_slash;
				}

				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_extB.next = NULL;
				if (errno == EACCES) {
					status = NFSERR_ACCES;
				} else {
					syslog(loglevel, "%s: %m", host);
					status = NFSERR_IO;
				}
				if (trace > 3) {
				    trace_prt(1, "  nfsmount: mount RPC gave"
					" %d for %s:%s\n",
					errno, host, dir);
				}
				last_error = status;
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
			argp->fh = malloc((sizeof (fhandle)));
			if (!argp->fh) {
				syslog(LOG_ERR, "nfsmount: no memory");
				last_error = NFSERR_IO;
				destroy_auth_client_handle(cl);
				goto out;
			}
			(void) memcpy(argp->fh, &fhs.fhstatus_u.fhs_fhandle,
				sizeof (fhandle));
			break;
		    case MOUNTVERS3:
			posix = 0;
			(void) memset((char *)&res3, '\0', sizeof (res3));
			rpc_stat = clnt_call(cl, MOUNTPROC_MNT,
				xdr_dirpath, (caddr_t)&dir,
				xdr_mountres3, (caddr_t)&res3, timeout);
			if (rpc_stat != RPC_SUCCESS) {

				if (give_up_on_mnt == FALSE) {
					got_mnt_error = TRUE;
					goto try_mnt_slash;
				}

				/*
				 * Given the way "clnt_sperror" works, the "%s"
				 * immediately following the "not responding"
				 * is correct.
				 */
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_extB.next = NULL;
				last_error = NFSERR_NOENT;

				if (retries-- > 0) {
					destroy_auth_client_handle(cl);
					DELAY(delay);
					goto retry;
				}

				if (trace > 3) {
				    trace_prt(1,
					"  nfsmount: mount RPC failed for %s\n",
					host);
				}
				syslog(loglevel, "%s server not responding%s",
				    remname, clnt_sperror(cl, ""));
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
			if ((errno = res3.fhs_status) != MNT_OK)  {

				if (give_up_on_mnt == FALSE) {
					got_mnt_error = TRUE;
					goto try_mnt_slash;
				}

				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_extB.next = NULL;
				if (errno == EACCES) {
					status = NFSERR_ACCES;
				} else {
					syslog(loglevel, "%s: %m", remname);
					status = NFSERR_IO;
				}
				if (trace > 3) {
				    trace_prt(1, "  nfsmount: mount RPC gave"
					" %d for %s:%s\n",
					errno, host, dir);
				}
				last_error = status;
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}

			/*
			 *  Negotiate the security flavor for nfs_mount
			 */
			auths =
		    res3.mountres3_u.mountinfo.auth_flavors.auth_flavors_val;
			count =
		    res3.mountres3_u.mountinfo.auth_flavors.auth_flavors_len;

			if (sec_opt) {
				for (i = 0; i < count; i++)
					if (auths[i] == nfs_sec.sc_nfsnum) {
						break;
					}
				if (i >= count) {
					syslog(LOG_ERR,
				    "%s: does not support security \"%s\"\n",
					    remname, nfs_sec.sc_name);
					free(argp);
					head = prevhead;
					tail = prevtail;
					if (tail)
				tail->nfs_ext_u.nfs_extB.next = NULL;
					last_error = NFSERR_IO;
					destroy_auth_client_handle(cl);
					skipentry = 1;
					mfs->mfs_ignore = 1;
					continue;
				}
			} else {
				if (count > 0) {
					for (i = 0; i < count; i++) {
					    if (!(scerror =
				nfs_getseconfig_bynumber(auths[i], &nfs_sec))) {
						sec_opt++;
						break;
					    }
					}
					if (i >= count) {
						if (nfs_syslog_scerr(scerror,
								scerror_msg)
							!= -1) {
							syslog(LOG_ERR,
			"%s cannot be mounted because it is shared with "
			"security flavor %d which %s",
							remname,
							auths[i-1],
							scerror_msg);
						}
						free(argp);
						head = prevhead;
						tail = prevtail;
						if (tail)
					tail->nfs_ext_u.nfs_extB.next = NULL;
						last_error = NFSERR_IO;
						destroy_auth_client_handle(cl);
						skipentry = 1;
						mfs->mfs_ignore = 1;
						continue;
					}
				}
			}

			fh3.fh3_length =
			    res3.mountres3_u.mountinfo.fhandle.fhandle3_len;
			(void) memcpy(fh3.fh3_u.data,
			    res3.mountres3_u.mountinfo.fhandle.fhandle3_val,
			    fh3.fh3_length);
			argp->fh = malloc(sizeof (nfs_fh3));
			if (!argp->fh) {
				syslog(LOG_ERR, "nfsmount: no memory");
				last_error = NFSERR_IO;
				destroy_auth_client_handle(cl);
				goto out;
			}
			(void) memcpy(argp->fh, &fh3, sizeof (nfs_fh3));
			break;
		    default:
			free(argp);
			head = prevhead;
			tail = prevtail;
			if (tail)
				tail->nfs_ext_u.nfs_extB.next = NULL;
			last_error = NFSERR_NOENT;
			syslog(loglevel, "unknown MOUNT version %ld on %s",
			    vers, remname);
			destroy_auth_client_handle(cl);
			skipentry = 1;
			mfs->mfs_ignore = 1;
			continue;
		    } /* switch */
		}

		if (trace > 4)
			trace_prt(1, "	nfsmount: have %s filehandle for %s\n",
			    fstype, remname);

		argp->flags |= NFSMNT_NEWARGS;
		argp->flags |= NFSMNT_INT;	/* default is "intr" */
		argp->hostname = host;
		argp->flags |= NFSMNT_HOSTNAME;

		if ((mfs->mfs_flags & MFS_FH_VIA_WEBNFS) == 0) {
			nconf = NULL;

			if (nfs_port != 0)
				thisport = nfs_port;
			else
				thisport = mfs->mfs_port;

			argp->addr = get_addr(host, NFS_PROGRAM, nfsvers,
					&nconf, nfs_proto, thisport, NULL);

			if (argp->addr == NULL) {
				free(argp->fh);
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_extB.next = NULL;
				last_error = NFSERR_NOENT;

				if (retries-- > 0) {
					destroy_auth_client_handle(cl);
					DELAY(delay);
					goto retry;
				}

				syslog(loglevel, "%s: no NFS service", host);
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
			if (trace > 4)
				trace_prt(1,
				    "\tnfsmount: have net address for %s\n",
				    remname);

		} else {
			nconf = mfs->mfs_nconf;
			mfs->mfs_nconf = NULL;
		}

		argp->flags |= NFSMNT_KNCONF;
		argp->knconf = get_knconf(nconf);
		if (argp->knconf == NULL) {
			netbuf_free(argp->addr);
			freenetconfigent(nconf);
			free(argp->fh);
			free(argp);
			head = prevhead;
			tail = prevtail;
			if (tail)
				tail->nfs_ext_u.nfs_extB.next = NULL;
			last_error = NFSERR_NOSPC;
			destroy_auth_client_handle(cl);
			skipentry = 1;
			mfs->mfs_ignore = 1;
			continue;
		}
		if (trace > 4)
			trace_prt(1,
			    "\tnfsmount: have net config for %s\n",
			    remname);

		if (hasmntopt(&m, MNTOPT_SOFT) != NULL) {
			argp->flags |= NFSMNT_SOFT;
		}
		if (hasmntopt(&m, MNTOPT_NOINTR) != NULL) {
			argp->flags &= ~(NFSMNT_INT);
		}
		if (hasmntopt(&m, MNTOPT_NOAC) != NULL) {
			argp->flags |= NFSMNT_NOAC;
		}
		if (hasmntopt(&m, MNTOPT_NOCTO) != NULL) {
			argp->flags |= NFSMNT_NOCTO;
		}
		if (hasmntopt(&m, MNTOPT_FORCEDIRECTIO) != NULL) {
			argp->flags |= NFSMNT_DIRECTIO;
		}
		if (hasmntopt(&m, MNTOPT_NOFORCEDIRECTIO) != NULL) {
			argp->flags &= ~(NFSMNT_DIRECTIO);
		}

		/*
		 * Set up security data for argp->nfs_ext_u.nfs_extB.secdata.
		 */
		if (mfssnego.snego_done) {
			memcpy(&nfs_sec, &mfssnego.nfs_sec,
				sizeof (seconfig_t));
		} else if (!sec_opt) {
			/*
			 * Get default security mode.
			 */
			if (nfs_getseconfig_default(&nfs_sec)) {
				syslog(loglevel,
				    "error getting default security entry\n");
				free_knconf(argp->knconf);
				netbuf_free(argp->addr);
				freenetconfigent(nconf);
				free(argp->fh);
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_extB.next = NULL;
				last_error = NFSERR_NOSPC;
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
		}

		/*
		 * For AUTH_DES
		 * get the network address for the time service on
		 * the server.	If an RPC based time service is
		 * not available then try the IP time service.
		 *
		 * Eventurally, we want to move this code to nfs_clnt_secdata()
		 * when autod_nfs.c and mount.c can share the same
		 * get_the_addr/get_the_stuff routine.
		 */
		secflags = 0;
		syncaddr = NULL;
		retaddrs = NULL;
		if (nfs_sec.sc_rpcnum == AUTH_DES) {
			/*
			 * If not using the public fh, we can try talking
			 * RPCBIND. Otherwise, assume that firewalls prevent
			 * us from doing that.
			 */
		    if ((mfs->mfs_flags & MFS_FH_VIA_WEBNFS) == 0)
			syncaddr = get_the_stuff(SERVER_ADDR, host, RPCBPROG,
				RPCBVERS, NULL, nconf, 0, NULL, NULL, FALSE,
				NULL, NULL);
		    else
			syncaddr = NULL;

		    if (syncaddr) {
			secflags |= AUTH_F_RPCTIMESYNC;
		    } else {
			struct nd_hostserv hs;

			hs.h_host = host;
			hs.h_serv = "rpcbind";
			if (netdir_getbyname(nconf, &hs, &retaddrs) != ND_OK) {
				syslog(loglevel,
				    "%s: secure: no time service\n", host);
				free_knconf(argp->knconf);
				netbuf_free(argp->addr);
				freenetconfigent(nconf);
				free(argp->fh);
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_extB.next = NULL;
				last_error = NFSERR_IO;
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
			syncaddr = retaddrs->n_addrs;

			/* LINTED pointer alignment */
			if (strcmp(nconf->nc_protofmly, NC_INET) == 0)
				((struct sockaddr_in *)syncaddr->buf)->sin_port
					= htons((ushort_t)IPPORT_TIMESERVER);

			else if (strcmp(nconf->nc_protofmly, NC_INET6) == NULL)
				((struct sockaddr_in6 *)
				    syncaddr->buf)->sin6_port
					= htons((ushort_t)IPPORT_TIMESERVER);

		}
		} /* if AUTH_DES */

		if (!(secdata = nfs_clnt_secdata(&nfs_sec, host, argp->knconf,
					syncaddr, secflags))) {
			syslog(LOG_ERR,
				"errors constructing security related data\n");
			if (secflags & AUTH_F_RPCTIMESYNC)
				netbuf_free(syncaddr);
			else if (retaddrs)
				netdir_free(retaddrs, ND_ADDRLIST);
			free_knconf(argp->knconf);
			netbuf_free(argp->addr);
			freenetconfigent(nconf);
			free(argp->fh);
			free(argp);
			head = prevhead;
			tail = prevtail;
			if (tail)
				tail->nfs_ext_u.nfs_extB.next = NULL;
			last_error = NFSERR_IO;
			destroy_auth_client_handle(cl);
			skipentry = 1;
			mfs->mfs_ignore = 1;
			continue;
		}
		NFS_ARGS_EXTB_secdata(*argp, secdata);
		/* end of security stuff */

		if (trace > 4)
			trace_prt(1,
			    "  nfsmount: have secure info for %s\n", remname);

		if (hasmntopt(&m, MNTOPT_GRPID) != NULL) {
			argp->flags |= NFSMNT_GRPID;
		}
		if (nopt(&m, MNTOPT_RSIZE, &argp->rsize)) {
			argp->flags |= NFSMNT_RSIZE;
		}
		if (nopt(&m, MNTOPT_WSIZE, &argp->wsize)) {
			argp->flags |= NFSMNT_WSIZE;
		}
		if (nopt(&m, MNTOPT_TIMEO, &argp->timeo)) {
			argp->flags |= NFSMNT_TIMEO;
		}
		if (nopt(&m, MNTOPT_RETRANS, &argp->retrans)) {
			argp->flags |= NFSMNT_RETRANS;
		}
		if (nopt(&m, MNTOPT_ACTIMEO, &argp->acregmax)) {
			argp->flags |= NFSMNT_ACREGMAX;
			argp->flags |= NFSMNT_ACDIRMAX;
			argp->flags |= NFSMNT_ACDIRMIN;
			argp->flags |= NFSMNT_ACREGMIN;
			argp->acdirmin = argp->acregmin = argp->acdirmax
				= argp->acregmax;
		} else {
			if (nopt(&m, MNTOPT_ACREGMIN, &argp->acregmin)) {
				argp->flags |= NFSMNT_ACREGMIN;
			}
			if (nopt(&m, MNTOPT_ACREGMAX, &argp->acregmax)) {
				argp->flags |= NFSMNT_ACREGMAX;
			}
			if (nopt(&m, MNTOPT_ACDIRMIN, &argp->acdirmin)) {
				argp->flags |= NFSMNT_ACDIRMIN;
			}
			if (nopt(&m, MNTOPT_ACDIRMAX, &argp->acdirmax)) {
				argp->flags |= NFSMNT_ACDIRMAX;
			}
		}

		if (posix) {
			argp->pathconf = NULL;
			if (error = get_pathconf(cl, dir, remname,
			    &argp->pathconf, retries)) {
				if (secflags & AUTH_F_RPCTIMESYNC)
					netbuf_free(syncaddr);
				else if (retaddrs)
					netdir_free(retaddrs, ND_ADDRLIST);
				free_knconf(argp->knconf);
				netbuf_free(argp->addr);
				freenetconfigent(nconf);
				nfs_free_secdata(
					argp->nfs_ext_u.nfs_extB.secdata);
				free(argp->fh);
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_extB.next = NULL;
				last_error = NFSERR_IO;

				if (error == RET_RETRY && retries-- > 0) {
					destroy_auth_client_handle(cl);
					DELAY(delay);
					goto retry;
				}

				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
			argp->flags |= NFSMNT_POSIX;
			if (trace > 4)
				trace_prt(1,
				    "  nfsmount: have pathconf for %s\n",
				    remname);
		}

		/*
		 * free loop-specific data structures
		 */
		destroy_auth_client_handle(cl);
		freenetconfigent(nconf);
		if (secflags & AUTH_F_RPCTIMESYNC)
			netbuf_free(syncaddr);
		else if (retaddrs)
			netdir_free(retaddrs, ND_ADDRLIST);

		/*
		 * Decide whether to use remote host's lockd or local locking.
		 * If we are using the public fh, we've already turned
		 * LLOCK on.
		 */
		if (hasmntopt(&m, MNTOPT_LLOCK))
			argp->flags |= NFSMNT_LLOCK;
		if (!(argp->flags & NFSMNT_LLOCK) && nfsvers == NFS_VERSION &&
			remote_lock(host, argp->fh)) {
			syslog(loglevel, "No network locking on %s : "
			"contact admin to install server change", host);
			argp->flags |= NFSMNT_LLOCK;
		}

		/*
		 * Build a string for /etc/mnttab.
		 * If possible, coalesce strings with same 'dir' info.
		 */
		if ((mfs->mfs_flags & MFS_URL) == 0) {
			char *tmp;

			if (mnttabcnt) {
				p = strrchr(mnttabtext, (int)':');
				if (!p || strcmp(p+1, dir) != 0) {
					mnttabcnt += strlen(remname) + 2;
				} else {
					*p = '\0';
					mnttabcnt += strlen(host) + 2;
				}
				if ((tmp = realloc(mnttabtext,
				    mnttabcnt)) != NULL) {
					mnttabtext = tmp;
					strcat(mnttabtext, ",");
				} else {
					free(mnttabtext);
					mnttabtext = NULL;
				}
			} else {
				mnttabcnt = strlen(remname) + 1;
				if ((mnttabtext = malloc(mnttabcnt)) != NULL)
					mnttabtext[0] = '\0';
			}

			if (mnttabtext != NULL)
				strcat(mnttabtext, remname);

		} else {
			char *tmp;
			int more_cnt = 0;
			char sport[16];

			more_cnt += strlen("nfs://");
			more_cnt += strlen(mfs->mfs_host);

			if (mfs->mfs_port != 0) {
				(void) sprintf(sport, ":%u", mfs->mfs_port);
			} else
				sport[0] = '\0';

			more_cnt += strlen(sport);
			more_cnt += 1; /* "/" */
			more_cnt += strlen(mfs->mfs_dir);

			if (mnttabcnt) {
				more_cnt += 1; /* "," */
				mnttabcnt += more_cnt;

				if ((tmp = realloc(mnttabtext,
				    mnttabcnt)) != NULL) {
					mnttabtext = tmp;
					strcat(mnttabtext, ",");
				} else {
					free(mnttabtext);
					mnttabtext = NULL;
				}
			} else {
				mnttabcnt = more_cnt + 1;
				if ((mnttabtext = malloc(mnttabcnt)) != NULL)
					mnttabtext[0] = '\0';
			}

			if (mnttabtext != NULL) {
				strcat(mnttabtext, "nfs://");
				strcat(mnttabtext, mfs->mfs_host);
				strcat(mnttabtext, sport);
				strcat(mnttabtext, "/");
				strcat(mnttabtext, mfs->mfs_dir);
			}
		}

		if (!mnttabtext) {
			syslog(LOG_ERR, "nfsmount: no memory");
			last_error = NFSERR_IO;
			goto out;
		}

		/*
		 * At least one entry, can call mount(2).
		 */
		entries++;

		/*
		 * If replication was defeated, don't do more work
		 */
		if (!replicated)
			break;
	}


	/*
	 * Did we get through all possibilities without success?
	 */
	if (!entries)
		goto out;

	/*
	 * Whew; do the mount, at last.
	 */
	if (trace > 1) {
		trace_prt(1, "	mount %s %s (%s)\n", mnttabtext, mntpnt, opts);
	}

	strcpy(mopts, opts);
	if (mount(mnttabtext, mntpnt, flags | MS_DATA, fstype,
			head, sizeof (*head), mopts, MAX_MNTOPT_STR) < 0) {
		if (trace > 1)
			trace_prt(1, "	Mount of %s on %s: %d",
			    mnttabtext, mntpnt, errno);
		if (errno != EBUSY || verbose)
			syslog(LOG_ERR,
				"Mount of %s on %s: %m", mnttabtext, mntpnt);
		last_error = NFSERR_IO;
		goto out;
	}

	last_error = NFS_OK;
	if (stat(mntpnt, &stbuf) == 0) {
		if (trace > 1) {
			trace_prt(1, "	mount %s dev=%x rdev=%x OK\n",
				mnttabtext, stbuf.st_dev, stbuf.st_rdev);
		}
	} else {
		if (trace > 1) {
			trace_prt(1, "	mount %s OK\n", mnttabtext);
			trace_prt(1, "	stat of %s failed\n", mntpnt);
		}
	}

out:
	argp = head;
	while (argp) {
		if (argp->pathconf)
			free(argp->pathconf);
		free_knconf(argp->knconf);
		netbuf_free(argp->addr);
		nfs_free_secdata(argp->nfs_ext_u.nfs_extB.secdata);
		free(argp->fh);
		head = argp;
		argp = argp->nfs_ext_u.nfs_extB.next;
		free(head);
	}
ret:
	if (nfs_proto)
		free(nfs_proto);
	if (mnttabtext)
		free(mnttabtext);

	for (mfs = mfs_in; mfs; mfs = mfs->mfs_next) {

		if (mfs->mfs_flags & MFS_ALLOC_DIR) {
			free(mfs->mfs_dir);
			mfs->mfs_dir = NULL;
			mfs->mfs_flags &= ~MFS_ALLOC_DIR;
		}

		if (mfs->mfs_args != NULL) {
			free(mfs->mfs_args);
			mfs->mfs_args = NULL;
		}

		if (mfs->mfs_nconf != NULL) {
			freenetconfigent(mfs->mfs_nconf);
			mfs->mfs_nconf = NULL;
		}
	}

	return (last_error);
}

/*
 * get_pathconf(cl, path, fsname, pcnf, cretries)
 * ugliness that requires that ppathcnf and pathcnf stay consistent
 * cretries is a copy of retries used to determine when to syslog
 * on retry situations.
 */
static int
get_pathconf(CLIENT *cl, char *path, char *fsname, struct pathcnf **pcnf,
	int cretries)
{
	struct ppathcnf *p = NULL;
	enum clnt_stat rpc_stat;
	struct timeval timeout;

	p = (struct ppathcnf *)malloc(sizeof (struct ppathcnf));
	if (p == NULL) {
		syslog(LOG_ERR, "get_pathconf: Out of memory");
		return (RET_ERR);
	}
	memset((caddr_t)p, 0, sizeof (struct ppathcnf));

	timeout.tv_sec = 10;
	timeout.tv_usec = 0;
	rpc_stat = clnt_call(cl, MOUNTPROC_PATHCONF,
	    xdr_dirpath, (caddr_t)&path, xdr_ppathcnf, (caddr_t)p, timeout);
	if (rpc_stat != RPC_SUCCESS) {
		if (cretries-- <= 0) {
			syslog(LOG_ERR,
			    "get_pathconf: %s: server not responding: %s",
			    fsname, clnt_sperror(cl, ""));
		}
		free(p);
		return (RET_RETRY);
	}
	if (_PC_ISSET(_PC_ERROR, p->pc_mask)) {
		syslog(LOG_ERR, "get_pathconf: no info for %s", fsname);
		free(p);
		return (RET_ERR);
	}
	*pcnf = (struct pathcnf *)p;
	return (RET_OK);
}

static struct knetconfig *
get_knconf(nconf)
	struct netconfig *nconf;
{
	struct stat stbuf;
	struct knetconfig *k;

	if (stat(nconf->nc_device, &stbuf) < 0) {
		syslog(LOG_ERR, "get_knconf: stat %s: %m", nconf->nc_device);
		return (NULL);
	}
	k = (struct knetconfig *)malloc(sizeof (*k));
	if (k == NULL)
		goto nomem;
	k->knc_semantics = nconf->nc_semantics;
	k->knc_protofmly = strdup(nconf->nc_protofmly);
	if (k->knc_protofmly == NULL)
		goto nomem;
	k->knc_proto = strdup(nconf->nc_proto);
	if (k->knc_proto == NULL)
		goto nomem;
	k->knc_rdev = stbuf.st_rdev;

	return (k);

nomem:
	syslog(LOG_ERR, "get_knconf: no memory");
	free_knconf(k);
	return (NULL);
}

static void
free_knconf(k)
	struct knetconfig *k;
{
	if (k == NULL)
		return;
	if (k->knc_protofmly)
		free(k->knc_protofmly);
	if (k->knc_proto)
		free(k->knc_proto);
	free(k);
}

static void
netbuf_free(nb)
	struct netbuf *nb;
{
	if (nb == NULL)
		return;
	if (nb->buf)
		free(nb->buf);
	free(nb);
}

#define	SMALL_HOSTNAME		20
#define	SMALL_PROTONAME		10
#define	SMALL_PROTOFMLYNAME		10

struct portmap_cache {
	int cache_prog;
	int cache_vers;
	time_t cache_time;
	char cache_small_hosts[SMALL_HOSTNAME + 1];
	char *cache_hostname;
	char *cache_proto;
	char *cache_protofmly;
	char cache_small_protofmly[SMALL_PROTOFMLYNAME + 1];
	char cache_small_proto[SMALL_PROTONAME + 1];
	struct netbuf cache_srv_addr;
	struct portmap_cache *cache_prev, *cache_next;
};

rwlock_t portmap_cache_lock;
static int portmap_cache_valid_time = 30;
struct portmap_cache *portmap_cache_head, *portmap_cache_tail;

/*
 * Returns 1 if the entry is found in the cache, 0 otherwise.
 */
static int
portmap_cache_lookup(hostname, prog, vers, nconf, addrp)
	char *hostname;
	rpcprog_t prog;
	rpcvers_t vers;
	struct netconfig *nconf;
	struct netbuf *addrp;
{
	struct	portmap_cache *cachep, *prev, *next = NULL, *cp;
	int	retval = 0;

	timenow = time(NULL);

	(void) rw_rdlock(&portmap_cache_lock);

	/*
	 * Increment the portmap cache counters for # accesses and lookups
	 * Use a smaller factor (100 vs 1000 for the host cache) since
	 * initial analysis shows this cache is looked up 10% that of the
	 * host cache.
	 */
#ifdef CACHE_DEBUG
	portmap_cache_accesses++;
	portmap_cache_lookups++;
	if ((portmap_cache_lookups%100) == 0)
		trace_portmap_cache();
#endif /* CACHE_DEBUG */

	for (cachep = portmap_cache_head; cachep;
		cachep = cachep->cache_next) {
		if (timenow > cachep->cache_time) {
			/*
			 * We stumbled across an entry in the cache which
			 * has timed out. Free up all the entries that
			 * were added before it, which will positionally
			 * be after this entry. And adjust neighboring
			 * pointers.
			 * When we drop the lock and re-acquire it, we
			 * need to start from the beginning.
			 */
			(void) rw_unlock(&portmap_cache_lock);
			(void) rw_wrlock(&portmap_cache_lock);
			for (cp = portmap_cache_head;
				cp && (cp->cache_time >= timenow);
				cp = cp->cache_next)
				;
			if (cp == NULL)
				goto done;
			/*
			 * Adjust the link of the predecessor.
			 * Make the tail point to the new last entry.
			 */
			prev = cp->cache_prev;
			if (prev == NULL) {
				portmap_cache_head = NULL;
				portmap_cache_tail = NULL;
			} else {
				prev->cache_next = NULL;
				portmap_cache_tail = prev;
			}
			for (; cp; cp = next) {
				if (cp->cache_hostname != NULL &&
				    cp->cache_hostname !=
				    cp->cache_small_hosts)
					free(cp->cache_hostname);
				if (cp->cache_proto != NULL &&
				    cp->cache_proto !=
				    cp->cache_small_proto)
					free(cp->cache_proto);
				if (cp->cache_srv_addr.buf != NULL)
					free(cp->cache_srv_addr.buf);
				next = cp->cache_next;
				free(cp);
			}
			goto done;
		}
		if (cachep->cache_hostname == NULL ||
		    prog != cachep->cache_prog || vers != cachep->cache_vers ||
		    strcmp(nconf->nc_proto, cachep->cache_proto) != 0 ||
		    strcmp(nconf->nc_protofmly, cachep->cache_protofmly) != 0 ||
		    strcmp(hostname, cachep->cache_hostname) != 0)
			continue;
		/*
		 * Cache Hit.
		 */
#ifdef CACHE_DEBUG
		portmap_cache_hits++;	/* up portmap cache hit counter */
#endif /* CACHE_DEBUG */
		addrp->len = cachep->cache_srv_addr.len;
		memcpy(addrp->buf, cachep->cache_srv_addr.buf, addrp->len);
		retval = 1;
		break;
	}
done:
	(void) rw_unlock(&portmap_cache_lock);
	return (retval);
}

static void
portmap_cache_enter(hostname, prog, vers, nconf, addrp)
	char *hostname;
	rpcprog_t prog;
	rpcvers_t vers;
	struct netconfig *nconf;
	struct netbuf *addrp;
{
	struct portmap_cache *cachep;
	int protofmlylen;
	int protolen, hostnamelen;

	timenow = time(NULL);

	cachep = malloc(sizeof (struct portmap_cache));
	if (cachep == NULL)
		return;
	memset((char *)cachep, 0, sizeof (*cachep));

	hostnamelen = strlen(hostname);
	if (hostnamelen <= SMALL_HOSTNAME)
		cachep->cache_hostname = cachep->cache_small_hosts;
	else {
		cachep->cache_hostname = malloc(hostnamelen + 1);
		if (cachep->cache_hostname == NULL)
			goto nomem;
	}
	strcpy(cachep->cache_hostname, hostname);
	protolen = strlen(nconf->nc_proto);
	if (protolen <= SMALL_PROTONAME)
		cachep->cache_proto = cachep->cache_small_proto;
	else {
		cachep->cache_proto = malloc(protolen + 1);
		if (cachep->cache_proto == NULL)
			goto nomem;
	}
	protofmlylen = strlen(nconf->nc_protofmly);
	if (protofmlylen <= SMALL_PROTOFMLYNAME)
		cachep->cache_protofmly = cachep->cache_small_protofmly;
	else {
		cachep->cache_protofmly = malloc(protofmlylen + 1);
		if (cachep->cache_protofmly == NULL)
			goto nomem;
	}

	strcpy(cachep->cache_proto, nconf->nc_proto);
	cachep->cache_prog = prog;
	cachep->cache_vers = vers;
	cachep->cache_time = timenow + portmap_cache_valid_time;
	cachep->cache_srv_addr.len = addrp->len;
	cachep->cache_srv_addr.buf = malloc(addrp->len);
	if (cachep->cache_srv_addr.buf == NULL)
		goto nomem;
	memcpy(cachep->cache_srv_addr.buf, addrp->buf, addrp->maxlen);
	cachep->cache_prev = NULL;
	(void) rw_wrlock(&portmap_cache_lock);
	/*
	 * There's a window in which we could have multiple threads making
	 * the same cache entry. This can be avoided by walking the cache
	 * once again here to check and see if there are duplicate entries
	 * (after grabbing the write lock). This isn't fatal and I'm not
	 * going to bother with this.
	 */
#ifdef CACHE_DEBUG
	portmap_cache_accesses++;	/* up portmap cache access counter */
#endif /* CACHE_DEBUG */
	cachep->cache_next = portmap_cache_head;
	if (portmap_cache_head != NULL)
		portmap_cache_head->cache_prev = cachep;
	portmap_cache_head = cachep;
	(void) rw_unlock(&portmap_cache_lock);
	return;

nomem:
	syslog(LOG_ERR, "portmap_cache_enter: Memory allocation failed");
	if (cachep->cache_srv_addr.buf)
		free(cachep->cache_srv_addr.buf);
	if (cachep->cache_proto && protolen > SMALL_PROTONAME)
		free(cachep->cache_proto);
	if (cachep->cache_hostname && hostnamelen > SMALL_HOSTNAME)
		free(cachep->cache_hostname);
	if (cachep->cache_protofmly && protofmlylen > SMALL_PROTOFMLYNAME)
		free(cachep->cache_protofmly);
	if (cachep)
		free(cachep);
	cachep = NULL;
}

static int
get_cached_srv_addr(char *hostname, rpcprog_t prog, rpcvers_t vers,
	struct netconfig *nconf, struct netbuf *addrp)
{
	if (portmap_cache_lookup(hostname, prog, vers, nconf, addrp))
		return (1);
	if (rpcb_getaddr(prog, vers, nconf, addrp, hostname) == 0)
		return (0);
	portmap_cache_enter(hostname, prog, vers, nconf, addrp);
	return (1);
}

/*
 * Get the network address on "hostname" for program "prog"
 * with version "vers" by using the nconf configuration data
 * passed in.
 *
 * If the address of a netconfig pointer is null then
 * information is not sufficient and no netbuf will be returned.
 *
 * tinfo argument is for matching the get_the_addr() defined in
 * ../nfs/mount/mount.c
 */
static void *
get_the_stuff(
	enum type_of_stuff type_of_stuff,
	char *hostname,
	rpcprog_t prog,
	rpcprog_t vers,
	mfs_snego_t *mfssnego,
	struct netconfig *nconf,
	ushort_t port,
	struct t_info *tinfo,
	caddr_t *fhp,
	bool_t direct_to_server,
	char *fspath,
	enum clnt_stat *cstat)

{
	struct netbuf *nb = NULL;
	struct t_bind *tbind = NULL;
	int fd = -1;
	enum clnt_stat cs = RPC_TIMEDOUT;
	CLIENT *cl = NULL;
	struct timeval tv;
	AUTH *ah = NULL;
	AUTH *new_ah = NULL;
	struct snego_t snego;

	if (nconf == NULL) {
		goto done;
	}

	if ((fd = t_open(nconf->nc_device, O_RDWR, tinfo)) < 0) {
		goto done;
	}

	/* LINTED pointer alignment */
	if ((tbind = (struct t_bind *)t_alloc(fd, T_BIND, T_ADDR))
		    == NULL) {
			goto done;
	}

	if (direct_to_server == TRUE) {
		struct nd_hostserv hs;
		struct nd_addrlist *retaddrs;
		hs.h_host = hostname;

		if (trace > 1)
			trace_prt(1, "	get_the_stuff: %s call "
				"direct to server %s\n",
				type_of_stuff == SERVER_FH ? "pub fh" :
				type_of_stuff == SERVER_ADDR ? "get address" :
				type_of_stuff == SERVER_PING ? "ping" :
				"unknown", hostname);
		if (port == 0)
			hs.h_serv = "nfs";
		else
			hs.h_serv = NULL;

		if (netdir_getbyname(nconf, &hs, &retaddrs) != ND_OK) {
			goto done;
		}
		memcpy(tbind->addr.buf, retaddrs->n_addrs->buf,
			retaddrs->n_addrs->len);
		tbind->addr.len = retaddrs->n_addrs->len;
		netdir_free((void *)retaddrs, ND_ADDRLIST);
		if (port) {
			/* LINTED pointer alignment */

			if (strcmp(nconf->nc_protofmly, NC_INET) == NULL)
				((struct sockaddr_in *)
				tbind->addr.buf)->sin_port =
					htons((ushort_t)port);
			else if (strcmp(nconf->nc_protofmly, NC_INET6) == NULL)
				((struct sockaddr_in6 *)
				tbind->addr.buf)->sin6_port =
					htons((ushort_t)port);
		}

		if (type_of_stuff == SERVER_FH) {
			if (netdir_options(nconf, ND_SET_RESERVEDPORT, fd,
				NULL) == -1)
				if (trace > 1)
					trace_prt(1, "\tget_the_stuff: "
						"ND_SET_RESERVEDPORT(%s) "
						"failed\n", hostname);
		}

		cl = clnt_tli_create(fd, nconf, &tbind->addr, prog,
			vers, 0, 0);

		if (trace > 1)
			trace_prt(1, "	get_the_stuff: clnt_tli_create(%s) "
				"returned %p\n", hostname, cl);
		if (cl == NULL)
			goto done;

		switch (type_of_stuff) {
		case SERVER_FH:
		    {
		    enum snego_stat sec;

		    ah = authsys_create_default();
			if (ah != NULL)
			    cl->cl_auth = ah;

		    if (!mfssnego->snego_done) {
			/*
			 * negotiate sec flavor.
			 */
			snego.cnt = 0;
			if ((sec = nfs_sec_nego(vers, cl, fspath, &snego)) ==
				SNEGO_SUCCESS) {
			    int jj;

			/*
			 * check if server supports the one
			 * specified in the sec= option.
			 */
			    if (mfssnego->sec_opt) {
				for (jj = 0; jj < snego.cnt; jj++) {
				    if (snego.array[jj] ==
					mfssnego->nfs_sec.sc_nfsnum) {
					mfssnego->snego_done = TRUE;
					break;
				    }
				}
			    }

			/*
			 * find a common sec flavor
			 */
			    if (!mfssnego->snego_done) {
				for (jj = 0; jj < snego.cnt; jj++) {
				    if (!nfs_getseconfig_bynumber(
					snego.array[jj], &mfssnego->nfs_sec)) {
					mfssnego->snego_done = TRUE;
					break;
				    }
				}
			    }
			    if (!mfssnego->snego_done)
				return (NULL);

			/*
			 * Now that the flavor has been
			 * negotiated, get the fh.
			 *
			 * First, create an auth handle using the negotiated
			 * sec flavor in the next lookup to
			 * fetch the filehandle.
			 */
			    new_ah = nfs_create_ah(cl, hostname,
					&mfssnego->nfs_sec);
			    if (new_ah == NULL)
				goto done;
			    cl->cl_auth = new_ah;
			} else if (sec == SNEGO_ARRAY_TOO_SMALL ||
			    sec == SNEGO_FAILURE) {
			    goto done;
			}
			/*
			 * Note that if sec == SNEGO_DEF_VALID
			 * the default sec flavor is acceptable.
			 * Use it to get the filehandle.
			 */
		    }
		    }

		    if (vers == NFS_VERSION) {
			    wnl_diropargs arg;
			    wnl_diropres *res;

			    memset((char *)&arg.dir, 0, sizeof (wnl_fh));
			    arg.name = fspath;
			    res = wnlproc_lookup_2(&arg, cl);

			    if (res == NULL || res->status != NFS_OK)
				    goto done;
			    *fhp = malloc(sizeof (wnl_fh));

			    if (*fhp == NULL) {
				    syslog(LOG_ERR, "no memory\n");
				    goto done;
			    }

			    memcpy((char *)*fhp,
			    (char *)&res->wnl_diropres_u.wnl_diropres.file,
				sizeof (wnl_fh));
			    cs = RPC_SUCCESS;
		    } else {
			    WNL_LOOKUP3args arg;
			    WNL_LOOKUP3res *res;
			    nfs_fh3 *fh3p;

			    memset((char *)&arg.what.dir, 0, sizeof (wnl_fh3));
			    arg.what.name = fspath;
			    res = wnlproc3_lookup_3(&arg, cl);

			    if (res == NULL || res->status != NFS3_OK)
				    goto done;

			    fh3p = (nfs_fh3 *)malloc(sizeof (*fh3p));

			    if (fh3p == NULL) {
				    syslog(LOG_ERR, "no memory\n");
				    CLNT_FREERES(cl, xdr_WNL_LOOKUP3res,
					(char *)res);
				    goto done;
			    }

			    fh3p->fh3_length = res->
				WNL_LOOKUP3res_u.res_ok.object.data.data_len;
			    memcpy(fh3p->fh3_u.data, res->
				WNL_LOOKUP3res_u.res_ok.object.data.data_val,
				fh3p->fh3_length);

			    *fhp = (caddr_t)fh3p;

			    CLNT_FREERES(cl, xdr_WNL_LOOKUP3res, (char *)res);
			    cs = RPC_SUCCESS;
		    }
		    break;
		case SERVER_ADDR:
		case SERVER_PING:
			tv.tv_sec = 10;
			tv.tv_usec = 0;
			cs = clnt_call(cl, NULLPROC, xdr_void, 0,
				xdr_void, 0, tv);
			if (trace > 1)
				trace_prt(1,
					"get_the_stuff: clnt_call(%s) "
					"returned %s\n",
				hostname,
					cs == RPC_SUCCESS ? "success" :
					"failure");

			if (cs != RPC_SUCCESS)
				goto done;
			break;
		}

	} else if (type_of_stuff != SERVER_FH) {

		if (type_of_stuff == SERVER_ADDR) {
			if (get_cached_srv_addr(hostname, prog, vers, nconf,
			    &tbind->addr) == 0)
				goto done;
		}

		if (port) {
			/* LINTED pointer alignment */
			if (strcmp(nconf->nc_protofmly, NC_INET) == NULL)
				((struct sockaddr_in *)
				tbind->addr.buf)->sin_port =
					htons((ushort_t)port);
			else if (strcmp(nconf->nc_protofmly, NC_INET6) == NULL)
				((struct sockaddr_in6 *)
				tbind->addr.buf)->sin6_port =
					htons((ushort_t)port);
			cl = clnt_tli_create(fd, nconf, &tbind->addr,
				prog, vers, 0, 0);
			if (cl == NULL)
				goto done;
			tv.tv_sec = 10;
			tv.tv_usec = 0;
			cs = clnt_call(cl, NULLPROC, xdr_void, 0, xdr_void,
				0, tv);
			if (cs != RPC_SUCCESS)
				goto done;
		}

	} else {
		/* can't happen */
		goto done;
	}

	if (type_of_stuff != SERVER_PING) {

		cs = RPC_SYSTEMERROR;

		/*
		 * Make a copy of the netbuf to return
		 */
		nb = (struct netbuf *)malloc(sizeof (struct netbuf));
		if (nb == NULL) {
			syslog(LOG_ERR, "no memory\n");
			goto done;
		}
		*nb = tbind->addr;
		nb->buf = (char *)malloc(nb->maxlen);
		if (nb->buf == NULL) {
			syslog(LOG_ERR, "no memory\n");
			free(nb);
			nb = NULL;
			goto done;
		}
		(void) memcpy(nb->buf, tbind->addr.buf, tbind->addr.len);

		cs = RPC_SUCCESS;
	}

done:
	if (cl != NULL) {
		if (ah != NULL) {
			AUTH_DESTROY(cl->cl_auth);
			cl->cl_auth = NULL;
		}
		clnt_destroy(cl);
	}

	if (tbind) {
		t_free((char *)tbind, T_BIND);
		tbind = NULL;
	}

	if (fd >= 0)
		(void) t_close(fd);

	if (cstat != NULL)
		*cstat = cs;

	return (nb);
}

/*
 * Get a network address on "hostname" for program "prog"
 * with version "vers".  If the port number is specified (non zero)
 * then try for a TCP/UDP transport and set the port number of the
 * resulting IP address.
 *
 * If the address of a netconfig pointer was passed and
 * if it's not null, use it as the netconfig otherwise
 * assign the address of the netconfig that was used to
 * establish contact with the service.
 *
 * tinfo argument is for matching the get_addr() defined in
 * ../nfs/mount/mount.c
 */

static struct netbuf *
get_addr(char *hostname, rpcprog_t prog, rpcvers_t vers,
	struct netconfig **nconfp, char *proto, ushort_t port,
	struct t_info *tinfo)

{
	enum clnt_stat cstat;

	return (get_server_stuff(SERVER_ADDR, hostname, prog, vers, NULL,
		nconfp, proto, port, tinfo, NULL, FALSE, NULL, &cstat));
}

static struct netbuf *
get_pubfh(char *hostname, rpcvers_t vers, mfs_snego_t *mfssnego,
	struct netconfig **nconfp, char *proto, ushort_t port,
	struct t_info *tinfo, caddr_t *fhp, bool_t get_pubfh, char *fspath)
{
	enum clnt_stat cstat;

	return (get_server_stuff(SERVER_FH, hostname, NFS_PROGRAM, vers,
		mfssnego, nconfp, proto, port, tinfo, fhp, get_pubfh, fspath,
		&cstat));
}

static enum clnt_stat
get_ping(char *hostname, rpcprog_t prog, rpcvers_t vers,
	struct netconfig **nconfp, ushort_t port, bool_t direct_to_server)
{
	enum clnt_stat cstat;

	(void) get_server_stuff(SERVER_PING, hostname, prog, vers, NULL, nconfp,
		NULL, port, NULL, NULL, direct_to_server, NULL, &cstat);

	return (cstat);
}

static void *
get_server_stuff(
	enum type_of_stuff type_of_stuff,
	char *hostname,
	rpcprog_t prog,
	rpcvers_t vers,
	mfs_snego_t *mfssnego,
	struct netconfig **nconfp,
	char *proto,
	ushort_t port,			/* may be zero */
	struct t_info *tinfo,
	caddr_t *fhp,
	bool_t direct_to_server,
	char *fspath,
	enum clnt_stat *cstatp)
{
	struct netbuf *nb = NULL;
	struct netconfig *nconf = NULL;
	NCONF_HANDLE *nc = NULL;
	int nthtry = FIRST_TRY;

	if (nconfp && *nconfp)
		return (get_the_stuff(type_of_stuff, hostname, prog, vers,
			mfssnego, *nconfp, port, tinfo, fhp, direct_to_server,
			fspath, cstatp));


	/*
	 * No nconf passed in.
	 *
	 * Try to get a nconf from /etc/netconfig.
	 * First choice is COTS, second is CLTS unless proto
	 * is specified.  When we retry, we reset the
	 * netconfig list, so that we search the whole list
	 * for the next choice.
	 */
	if ((nc = setnetpath()) == NULL)
		goto done;

	/*
	 * If proto is specified, then only search for the match,
	 * otherwise try COTS first, if failed, then try CLTS.
	 */
	if (proto) {

		while (nconf = getnetpath(nc)) {
			if (strcmp(nconf->nc_proto, proto))
				continue;
			/*
			 * If the port number is specified then TCP/UDP
			 * is needed. Otherwise any cots/clts will do.
			 */
			if (port)  {
				if ((strcmp(nconf->nc_protofmly, NC_INET) &&
				    strcmp(nconf->nc_protofmly, NC_INET6)) ||
				    (strcmp(nconf->nc_proto, NC_TCP) &&
				    strcmp(nconf->nc_proto, NC_UDP)))
					continue;
			}

			nb = get_the_stuff(type_of_stuff, hostname, prog, vers,
				mfssnego, nconf, port, tinfo, fhp,
				direct_to_server, fspath, cstatp);

			if (*cstatp == RPC_SUCCESS)
				break;

			assert(nb == NULL);

		} /* end of while */

		if (nconf == NULL)
			goto done;

	} else {
retry:
		while (nconf = getnetpath(nc)) {
			if (nconf->nc_flag & NC_VISIBLE) {
			    if (nthtry == FIRST_TRY) {
				if ((nconf->nc_semantics == NC_TPI_COTS_ORD) ||
					(nconf->nc_semantics == NC_TPI_COTS)) {
				    if (port == 0)
					break;
				    if ((strcmp(nconf->nc_protofmly,
					NC_INET) == 0 ||
					strcmp(nconf->nc_protofmly,
					NC_INET6) == 0) &&
					(strcmp(nconf->nc_proto, NC_TCP) == 0))
					break;
				}
			    }
			    if (nthtry == SECOND_TRY) {
				if (nconf->nc_semantics == NC_TPI_CLTS) {
				    if (port == 0)
					break;
				    if ((strcmp(nconf->nc_protofmly,
					NC_INET) == 0 ||
					strcmp(nconf->nc_protofmly,
					NC_INET6) == 0) &&
					(strcmp(nconf->nc_proto, NC_UDP) == 0))
					break;
				}
			    }
			}
		    } /* while */
		    if (nconf == NULL) {
			if (++nthtry <= MNT_PREF_LISTLEN) {
				endnetpath(nc);
				if ((nc = setnetpath()) == NULL)
					goto done;
				goto retry;
			} else
				goto done;
		    } else {
			nb = get_the_stuff(type_of_stuff, hostname, prog, vers,
			    mfssnego, nconf, port, tinfo, fhp, direct_to_server,
			    fspath, cstatp);
			if (*cstatp != RPC_SUCCESS)
				/*
				 * Continue the same search path in the
				 * netconfig db until no more matched nconf
				 * (nconf == NULL).
				 */
				goto retry;
		    }
	} /* if !proto */

	/*
	 * Got nconf and nb.  Now dup the netconfig structure (nconf)
	 * and return it thru nconfp.
	 */
	*nconfp = getnetconfigent(nconf->nc_netid);
	if (*nconfp == NULL) {
		syslog(LOG_ERR, "no memory\n");
		free(nb);
		nb = NULL;
	}
done:
	if (nc)
		endnetpath(nc);
	return (nb);
}

/*
 * Sends a null call to the remote host's (NFS program, versp). versp
 * may be "NULL" in which case NFS_V3 is used.
 * Upon return, versp contains the maximum version supported iff versp!= NULL.
 */
enum clnt_stat
pingnfs(
	char *hostpart,
	int attempts,
	rpcvers_t *versp,
	rpcvers_t versmin,
	ushort_t port,			/* may be zeor */
	bool_t usepub,
	char *path)
{
	CLIENT *cl = NULL;
	struct timeval rpc_to_new = {15, 0};
	static struct timeval rpc_rtrans_new = {-1, -1};
	enum clnt_stat clnt_stat;
	int i, j;
	rpcvers_t versmax;	/* maximum version to try against server */
	rpcvers_t outvers;	/* version supported by host on last call */
	rpcvers_t vers_to_try;	/* to try different versions against host */
	char *hostname = hostpart;

	if (path != NULL && strcmp(hostname, "nfs") == 0 &&
	    strncmp(path, "//", 2) == 0) {
		char *sport;

		hostname = strdup(path+2);

		if (hostname == NULL)
			return (RPC_SYSTEMERROR);

		path = strchr(hostname, '/');

		/*
		 * This cannot happen. If it does, give up
		 * on the ping as this is obviously a corrupt
		 * entry.
		 */
		if (path == NULL) {
			free(hostname);
			return (RPC_SUCCESS);
		}

		/*
		 * Probable end point of host string.
		 */
		*path = '\0';

		sport = strchr(hostname, ':');

		if (sport != NULL && sport < path) {

			/*
			 * Actual end point of host string.
			 */
			*sport = '\0';
			port = htons((ushort_t)atoi(sport+1));
		}

		usepub = TRUE;
	}

	switch (cache_check(hostname, versp)) {
	case GOODHOST:
		if (hostname != hostpart)
			free(hostname);
		return (RPC_SUCCESS);
	case DEADHOST:
		if (hostname != hostpart)
			free(hostname);
		return (RPC_TIMEDOUT);
	case NOHOST:
	default:
		break;
	}

	/*
	 * XXX The retransmission time rpcbrmttime is a global defined
	 * in the rpc library (rpcb_clnt.c). We use (and like) the default
	 * value of 15 sec in the rpc library. The code below is to protect
	 * us in case it changes. This need not be done under a lock since
	 * any # of threads entering this function will get the same
	 * retransmission value.
	 */
	if (rpc_rtrans_new.tv_sec == -1 && rpc_rtrans_new.tv_usec == -1) {
		__rpc_control(CLCR_GET_RPCB_RMTTIME, (char *)&rpc_rtrans_new);
		if (rpc_rtrans_new.tv_sec != 15 && rpc_rtrans_new.tv_sec != 0)
			if (trace > 1)
				trace_prt(1, "RPC library rttimer changed\n");
	}

	/*
	 * XXX Manipulate the total timeout to get the number of
	 * desired retransmissions. This code is heavily dependant on
	 * the RPC backoff mechanism in clnt_dg_call (clnt_dg.c).
	 */
	for (i = 0, j = rpc_rtrans_new.tv_sec; i < attempts-1; i++) {
		if (j < RPC_MAX_BACKOFF)
			j *= 2;
		else
			j = RPC_MAX_BACKOFF;
		rpc_to_new.tv_sec += j;
	}

	if (versp != NULL) {
		versmax = *versp;
		/* use versmin passed in */
	} else {
		versmax = NFS_V3;
		versmin = NFS_VERSMIN;
	}
	vers_to_try = versmax;

	/*
	 * check the host's version within the timeout
	 */
	if (trace > 1)
		trace_prt(1, "	ping: %s timeout=%ld request vers=%d min=%d\n",
				hostname, rpc_to_new.tv_sec, versmax, versmin);

	if (usepub == FALSE) {
	    do {
		if ((cl = clnt_create_vers_timed(hostname, NFS_PROGRAM,
		    &outvers, versmin, vers_to_try, "datagram_v", &rpc_to_new))
		    != NULL)
		    break;
		if (trace > 4) {
		    trace_prt(1,
		"  pingnfs: Can't ping via \"datagram_v\"%s: RPC error=%d\n",
			hostname, rpc_createerr.cf_stat);
		}
		if (rpc_createerr.cf_stat == RPC_UNKNOWNHOST ||
		    rpc_createerr.cf_stat == RPC_TIMEDOUT)
		    break;
		if (rpc_createerr.cf_stat == RPC_PROGNOTREGISTERED) {
		    if (trace > 4) {
			trace_prt(1,
		    "  pingnfs: Trying ping via \"circuit_v\"\n");
		    }
		    if ((cl = clnt_create_vers_timed(hostname, NFS_PROGRAM,
			&outvers, versmin, vers_to_try,
			"circuit_v", &rpc_to_new)) != NULL)
			break;
		    if (trace > 4) {
			trace_prt(1,
		"  pingnfs: Can't ping via \"circuit_v\" %s: RPC error=%d\n",
			    hostname, rpc_createerr.cf_stat);
		    }
		}

		/*
		 * backoff and return lower version to retry the ping.
		 * XXX we should be more careful and handle
		 * RPC_PROGVERSMISMATCH here, because that error is handled
		 * in clnt_create_vers(). It's not done to stay in sync
		 * with the nfs mount command.
		 */
		    vers_to_try--;
		    if (vers_to_try < versmin)
			    break;
		    if (versp != NULL) {	/* recheck the cache */
			    *versp = vers_to_try;
			    if (trace > 4) {
				trace_prt(1,
				    "  pingnfs: check cache: vers=%d\n",
				    *versp);
			    }
			    switch (cache_check(hostname, versp)) {
			    case GOODHOST:
				    if (hostname != hostpart)
					    free(hostname);
				    return (RPC_SUCCESS);
			    case DEADHOST:
				    if (hostname != hostpart)
					    free(hostname);
				    return (RPC_TIMEDOUT);
			    case NOHOST:
			    default:
				    break;
			    }
		    }
		    if (trace > 4) {
			trace_prt(1, "  pingnfs: Try version=%d\n",
				vers_to_try);
		    }
	    } while (cl == NULL);

	    if (cl == NULL) {
		    if (verbose)
			    syslog(LOG_ERR, "pingnfs: %s%s",
				    hostname, clnt_spcreateerror(""));
		    clnt_stat = RPC_TIMEDOUT;
	    } else {
		    clnt_destroy(cl);
		    clnt_stat = RPC_SUCCESS;
	    }

	} else {
		struct netconfig *nconf;

		for (vers_to_try = versmax; vers_to_try >= versmin;
		    vers_to_try--) {

			nconf = NULL;

			if (trace > 4) {
				trace_prt(1, "  pingnfs: Try version=%d "
					"using get_ping()\n", vers_to_try);
			}

			clnt_stat = get_ping(hostname, NFS_PROGRAM,
				vers_to_try, &nconf, port, TRUE);

			if (nconf != NULL)
				freenetconfigent(nconf);

			if (clnt_stat == RPC_SUCCESS) {
				outvers = vers_to_try;
				break;
			}
		}
	}

	if (trace > 1)
		clnt_stat == RPC_SUCCESS ?
			trace_prt(1, "	pingnfs OK: nfs version=%d\n", outvers):
			trace_prt(1, "	pingnfs FAIL: can't get nfs version\n");

	if (clnt_stat == RPC_SUCCESS) {
		cache_enter(hostname, versmax, outvers, GOODHOST);
		if (versp != NULL)
			*versp = outvers;
	} else
		cache_enter(hostname, versmax, versmax, DEADHOST);

	if (hostpart != hostname)
		free(hostname);

	return (clnt_stat);
}

#define	MNTTYPE_LOFS	"lofs"

int
loopbackmount(fsname, dir, mntopts, overlay)
	char *fsname;		/* Directory being mounted */
	char *dir;		/* Directory being mounted on */
	char *mntopts;
	int overlay;
{
	struct mnttab mnt;
	int flags = 0;
	char fstype[] = MNTTYPE_LOFS;
	int dirlen;
	struct stat st;

	dirlen = strlen(dir);
	if (dir[dirlen-1] == ' ')
		dirlen--;

	if (dirlen == strlen(fsname) &&
		strncmp(fsname, dir, dirlen) == 0) {
		syslog(LOG_ERR,
			"Mount of %s on %s would result in deadlock, aborted\n",
			fsname, dir);
		return (RET_ERR);
	}
	mnt.mnt_mntopts = mntopts;
	if (hasmntopt(&mnt, MNTOPT_RO) != NULL)
		flags |= MS_RDONLY;

	if (overlay)
		flags |= MS_OVERLAY;

	if (trace > 1)
		trace_prt(1,
			"  loopbackmount: fsname=%s, dir=%s, flags=%d\n",
			fsname, dir, flags);

	if (mount(fsname, dir, flags | MS_DATA, fstype, 0, 0) < 0) {
		syslog(LOG_ERR, "Mount of %s on %s: %m", fsname, dir);
		return (RET_ERR);
	}

	if (stat(dir, &st) == 0) {
		if (trace > 1) {
			trace_prt(1,
			    "  loopbackmount of %s on %s dev=%x rdev=%x OK\n",
			    fsname, dir, st.st_dev, st.st_rdev);
		}
	} else {
		if (trace > 1) {
			trace_prt(1,
			    "  loopbackmount of %s on %s OK\n", fsname, dir);
			trace_prt(1, "	stat of %s failed\n", dir);
		}
	}

	return (0);
}

/*
 * Look for the value of a numeric option of the form foo=x.  If found, set
 * *valp to the value and return non-zero.  If not found or the option is
 * malformed, return zero.
 */

int
nopt(mnt, opt, valp)
	struct mnttab *mnt;
	char *opt;
	int *valp;			/* OUT */
{
	char *equal;
	char *str;

	/*
	 * We should never get a null pointer, but if we do, it's better to
	 * ignore the option than to dump core.
	 */

	if (valp == NULL) {
		syslog(LOG_DEBUG, "null pointer for %s option", opt);
		return (0);
	}

	if (str = hasmntopt(mnt, opt)) {
		if (equal = strchr(str, '=')) {
			*valp = atoi(&equal[1]);
			return (1);
		} else {
			syslog(LOG_ERR, "Bad numeric option '%s'", str);
		}
	}
	return (0);
}

nfsunmount(mnt)
	struct mnttab *mnt;
{
	struct timeval timeout;
	CLIENT *cl;
	enum clnt_stat rpc_stat;
	char *host, *path;
	struct replica *list;
	int i, count = 0;

	if (trace > 1)
		trace_prt(1, "	nfsunmount: umount %s\n", mnt->mnt_mountp);

	if (umount(mnt->mnt_mountp) < 0) {
		if (trace > 1)
			trace_prt(1, "	nfsunmount: umount %s FAILED\n",
				mnt->mnt_mountp);
		if (errno)
			return (errno);
	}

	/*
	 * If mounted with -o public, then no need to contact server
	 * because mount protocol was not used.
	 */
	if (hasmntopt(mnt, MNTOPT_PUBLIC) != NULL) {
		return (0);
	}

	/*
	 * The rest of this code is advisory to the server.
	 * If it fails return success anyway.
	 */

	list = parse_replica(mnt->mnt_special, &count);
	if (!list) {
		if (count >= 0)
			syslog(LOG_ERR,
			    "Memory allocation failed: %m");
		return (ENOMEM);
	}

	for (i = 0; i < count; i++) {

		host = list[i].host;
		path = list[i].path;

		/*
		 * Skip file systems mounted using WebNFS, because mount
		 * protocol was not used.
		 */
		if (strcmp(host, "nfs") == 0 && strncmp(path, "//", 2) == 0)
			continue;

		cl = clnt_create(host, MOUNTPROG, MOUNTVERS, "datagram_v");
		if (cl == NULL)
			break;
		if (__clnt_bindresvport(cl) < 0) {
			if (verbose)
				syslog(LOG_ERR, "umount %s:%s: %s",
					host, path,
					"Couldn't bind to reserved port");
			destroy_auth_client_handle(cl);
			continue;
		}
		if ((cl->cl_auth = authsys_create_default()) == NULL) {
			if (verbose)
				syslog(LOG_ERR, "umount %s:%s: %s",
					host, path,
					"Failed creating default auth handle");
			destroy_auth_client_handle(cl);
			continue;
		}
		timeout.tv_usec = 0;
		timeout.tv_sec = 5;
		rpc_stat = clnt_call(cl, MOUNTPROC_UMNT, xdr_dirpath,
			    (caddr_t)&path, xdr_void, (char *)NULL, timeout);
		if (verbose && rpc_stat != RPC_SUCCESS)
			syslog(LOG_ERR, "%s: %s",
				host, clnt_sperror(cl, "unmount"));
		destroy_auth_client_handle(cl);
	}

	free_replica(list, count);

	if (trace > 1)
		trace_prt(1, "	nfsunmount: umount %s OK\n", mnt->mnt_mountp);

done:
	return (0);
}

/*
 * Put a new entry in the cache chain by prepending it to the front.
 * If there isn't enough memory then just give up.
 */
static void
cache_enter(host, reqvers, outvers, state)
	char *host;
	rpcvers_t reqvers;
	rpcvers_t outvers;
	int state;
{
	struct cache_entry *entry;
	int cache_time = 30;	/* sec */

	timenow = time(NULL);

	entry = (struct cache_entry *)malloc(sizeof (struct cache_entry));
	if (entry == NULL)
		return;
	(void) memset((caddr_t)entry, 0, sizeof (struct cache_entry));
	entry->cache_host = strdup(host);
	if (entry->cache_host == NULL) {
		cache_free(entry);
		return;
	}
	entry->cache_reqvers = reqvers;
	entry->cache_outvers = outvers;
	entry->cache_state = state;
	entry->cache_time = timenow + cache_time;
	(void) rw_wrlock(&cache_lock);
#ifdef CACHE_DEBUG
	host_cache_accesses++;		/* up host cache access counter */
#endif /* CACHE DEBUG */
	entry->cache_next = cache_head;
	cache_head = entry;
	(void) rw_unlock(&cache_lock);
}

static int
cache_check(host, versp)
	char *host;
	rpcvers_t *versp;
{
	int state = NOHOST;
	struct cache_entry *ce, *prev;

	timenow = time(NULL);

	(void) rw_rdlock(&cache_lock);

#ifdef CACHE_DEBUG
	/* Increment the lookup and access counters for the host cache */
	host_cache_accesses++;
	host_cache_lookups++;
	if ((host_cache_lookups%1000) == 0)
		trace_host_cache();
#endif /* CACHE DEBUG */

	for (ce = cache_head; ce; ce = ce->cache_next) {
		if (timenow > ce->cache_time) {
			(void) rw_unlock(&cache_lock);
			(void) rw_wrlock(&cache_lock);
			for (prev = NULL, ce = cache_head; ce;
				prev = ce, ce = ce->cache_next) {
				if (timenow > ce->cache_time) {
					cache_free(ce);
					if (prev)
						prev->cache_next = NULL;
					else
						cache_head = NULL;
					break;
				}
			}
			(void) rw_unlock(&cache_lock);
			return (state);
		}
		if (strcmp(host, ce->cache_host) != 0)
			continue;
		if (versp == NULL ||
			(versp != NULL && *versp == ce->cache_reqvers) ||
			(versp != NULL && *versp == ce->cache_outvers)) {
				if (versp != NULL)
					*versp = ce->cache_outvers;
				state = ce->cache_state;

				/* increment the host cache hit counters */
#ifdef CACHE_DEBUG
				if (state == GOODHOST)
					goodhost_cache_hits++;
				if (state == DEADHOST)
					deadhost_cache_hits++;
#endif /* CACHE_DEBUG */
				(void) rw_unlock(&cache_lock);
				return (state);
		}
	}
	(void) rw_unlock(&cache_lock);
	return (state);
}

/*
 * Free a cache entry and all entries
 * further down the chain since they
 * will also be expired.
 */
static void
cache_free(entry)
	struct cache_entry *entry;
{
	struct cache_entry *ce, *next = NULL;

	for (ce = entry; ce; ce = next) {
		if (ce->cache_host)
			free(ce->cache_host);
		next = ce->cache_next;
		free(ce);
	}
}

/*
 * Returns 1, if port option is NFS_PORT or
 *	nfsd is running on the port given
 * Returns 0, if both port is not NFS_PORT and nfsd is not
 *	running on the port.
 */

static int
is_nfs_port(char *opts)
{
	struct mnttab m;
	uint_t nfs_port = 0;
	struct servent sv;
	char buf[256];
	int got_port;

	m.mnt_mntopts = opts;

	/*
	 * Get port specified in options list, if any.
	 */
	got_port = nopt(&m, MNTOPT_PORT, (int *)&nfs_port);

	/*
	 * if no port specified or it is same as NFS_PORT return nfs
	 * To use any other daemon the port number should be different
	 */
	if (!got_port || nfs_port == NFS_PORT)
		return (1);
	/*
	 * If daemon is nfsd, return nfs
	 */
	if (getservbyport_r(nfs_port, NULL, &sv, buf, 256) == &sv &&
		strcmp(sv.s_name, "nfsd") == 0)
		return (1);

	/*
	 * daemon is not nfs
	 */
	return (0);
}


/*
 * destroy_auth_client_handle(cl)
 * destroys the created client handle
 */
static void
destroy_auth_client_handle(CLIENT *cl)
{
	if (cl) {
		if (cl->cl_auth) {
			AUTH_DESTROY(cl->cl_auth);
			cl->cl_auth = NULL;
		}
		clnt_destroy(cl);
	}
}


/*
 * Attempt to figure out which version of NFS to use in pingnfs().
 * If the version number was specified (i.e., non-zero), then use it.
 * Otherwise, default to NFS Version 3 with a fallback to NFS Version
 * 2. Return 0 on success and -1 on error.
 */
int
set_versrange(rpcvers_t nfsvers, rpcvers_t *vers, rpcvers_t *versmin)
{
	switch (nfsvers) {
	case 0:
		*vers = NFS_V3;
		*versmin = NFS_VERSMIN;		/* version 2 */
		break;
	case NFS_V3:
		*vers = NFS_V3;
		*versmin = NFS_V3;
		break;
	case NFS_VERSION:
		*vers = NFS_VERSION;		/* version 2 */
		*versmin = NFS_VERSMIN;		/* version 2 */
		break;
	default:
		return (-1);
	}
	return (0);
}

#ifdef CACHE_DEBUG
/*
 * trace_portmap_cache()
 * traces the portmap cache values at desired points
 */
static void
trace_portmap_cache()
{
	syslog(LOG_ERR, "portmap_cache: accesses=%d lookups=%d hits=%d\n",
		portmap_cache_accesses, portmap_cache_lookups,
		portmap_cache_hits);
}

/*
 * trace_host_cache()
 * traces the host cache values at desired points
 */
static void
trace_host_cache()
{
	syslog(LOG_ERR,
		"host_cache: accesses=%d lookups=%d deadhits=%d goodhits=%d\n",
		host_cache_accesses, host_cache_lookups, deadhost_cache_hits,
		goodhost_cache_hits);
}
#endif /* CACHE_DEBUG */
