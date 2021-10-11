/*
 * Copyright (c) 1984-1998,1999 Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rpcbind.c	1.35	99/09/27 SMI"

/*
 * rpcbind.c
 * Implements the program, version to address mapping for rpc.
 *
 */

#include <dlfcn.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <rpc/rpc.h>
#include <netconfig.h>
#include <netdir.h>
#include <sys/wait.h>
#include <sys/signal.h>
#ifdef PORTMAP
#include <netinet/in.h>
#endif
#include <sys/termios.h>
#include "rpcbind.h"
#include <sys/syslog.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/resource.h>

#ifdef PORTMAP
extern void pmap_service();
#endif
extern void rpcb_service_3();
extern void rpcb_service_4();
extern void read_warmstart();
extern void write_warmstart();
extern void *malloc();
extern char *strdup();
extern int Is_ipv6present(void);
#ifdef WAIT3
void reap();
#endif

#define	MAX_FILEDESC_LIMIT	1023

static void terminate();
static void detachfromtty();
static void parseargs();
static void rbllist_add();
static int init_transport();

/* Global variables */
#ifdef ND_DEBUG
int debugging = 1;	/* Tell me what's going on */
#else
int debugging = 0;	/* Tell me what's going on */
#endif
static int ipv6flag = 0;
int doabort = 0;	/* When debugging, do an abort on errors */
static int listen_backlog = 64;
rpcblist_ptr list_rbl;	/* A list of version 3/4 rpcbind services */
char *loopback_dg;	/* Datagram loopback transport, for set and unset */
char *loopback_vc;	/* COTS loopback transport, for set and unset */
char *loopback_vc_ord;	/* COTS_ORD loopback transport, for set and unset */

/* Local Variable */
static int warmstart = 0;	/* Grab a old copy of registrations */

#ifdef PORTMAP
PMAPLIST *list_pml;	/* A list of version 2 rpcbind services */
char *udptrans;		/* Name of UDP transport */
char *tcptrans;		/* Name of TCP transport */
char *udp_uaddr;	/* Universal UDP address */
char *tcp_uaddr;	/* Universal TCP address */
#endif
static char servname[] = "rpcbind";
static char superuser[] = "superuser";

int
main(int argc, char *argv[])
{
	struct netconfig *nconf;
	void *nc_handle;	/* Net config handle */
	struct rlimit rl;
	int maxrecsz = RPC_MAXDATASIZE;

	parseargs(argc, argv);

	getrlimit(RLIMIT_NOFILE, &rl);

	if (rl.rlim_cur < MAX_FILEDESC_LIMIT) {
		if (rl.rlim_max <= MAX_FILEDESC_LIMIT)
			rl.rlim_cur = rl.rlim_max;
		else
			rl.rlim_cur = MAX_FILEDESC_LIMIT;
		setrlimit(RLIMIT_NOFILE, &rl);
	}
	openlog("rpcbind", LOG_CONS, LOG_DAEMON);
	if (geteuid()) { /* This command allowed only to root */
		fprintf(stderr, "Sorry. You are not superuser\n");
		exit(1);
	}

	/*
	 * Enable non-blocking mode and maximum record size checks for
	 * connection oriented transports.
	 */
	if (!rpc_control(RPC_SVC_CONNMAXREC_SET, &maxrecsz)) {
		syslog(LOG_INFO, "unable to set RPC max record size");
	}

	nc_handle = setnetconfig(); 	/* open netconfig file */
	if (nc_handle == NULL) {
		syslog(LOG_ERR, "could not read /etc/netconfig");
		exit(1);
	}
	loopback_dg = "";
	loopback_vc = "";
	loopback_vc_ord = "";
#ifdef PORTMAP
	udptrans = "";
	tcptrans = "";
#endif

	{
		/*
		 * rpcbind is the first application to encounter the
		 * various netconfig files.  check_netconfig() verifies
		 * that they are set up correctly and complains loudly
		 * if not.
		 */
		int trouble;

		trouble = check_netconfig();
		if (trouble) {
			syslog(LOG_ERR,
	"%s: found %d errors with network configuration files. Exiting.",
				argv[0], trouble);
			fprintf(stderr,
	"%s: found %d errors with network configuration files. Exiting.\n",
				argv[0], trouble);
			exit(1);
		}
	}
	ipv6flag = Is_ipv6present();
	while (nconf = getnetconfig(nc_handle)) {
		if (nconf->nc_flag & NC_VISIBLE)
			init_transport(nconf);
	}
	endnetconfig(nc_handle);

	if ((loopback_dg[0] == NULL) && (loopback_vc[0] == NULL) &&
		(loopback_vc_ord[0] == NULL)) {
		syslog(LOG_ERR, "could not find loopback transports");
		exit(1);
	}

	/* catch the usual termination signals for graceful exit */
	(void) signal(SIGINT, terminate);
	(void) signal(SIGTERM, terminate);
	(void) signal(SIGQUIT, terminate);
	/* ignore others that could get sent */
	(void) signal(SIGHUP, SIG_IGN);
	(void) signal(SIGUSR1, SIG_IGN);
	(void) signal(SIGUSR2, SIG_IGN);
	if (warmstart) {
		read_warmstart();
	}
	if (debugging) {
		printf("rpcbind debugging enabled.");
		if (doabort) {
			printf("  Will abort on errors!\n");
		} else {
			printf("\n");
		}
	} else {
		detachfromtty();
	}
	my_svc_run();
	syslog(LOG_ERR, "svc_run returned unexpectedly");
	rpcbind_abort();
	/* NOTREACHED */
}

/*
 * Increments a counter each time a problem is found with the network
 * configuration information.
 */
int
check_netconfig()
{
	void	*nc;
	void	*dlcookie;
	int	busted = 0;
	int	i;
	int	lo_clts_found = 0, lo_cots_found = 0, lo_cotsord_found = 0;
	struct netconfig	*nconf, *np;
	struct stat	sb;
	struct nd_hostserv	nh;
	struct nd_addrlist	*na;
	extern int	errno;

	nc = setnetconfig();
	if (nc == (void *) 0) {
		if (debugging)
			fprintf(stderr,
				"setnetconfig() failed:  %s\n", nc_sperror());
		syslog(LOG_ALERT, "setnetconfig() failed:  %s", nc_sperror());
		return (1);
	}
	while (np = getnetconfig(nc)) {
		if ((np->nc_flag & NC_VISIBLE) == 0)
			continue;
		if (debugging)
			fprintf(stderr, "checking netid \"%s\"\n",
				np->nc_netid);
		if (strcmp(np->nc_protofmly, NC_LOOPBACK) == 0)
			switch (np->nc_semantics) {
			case NC_TPI_CLTS:
				lo_clts_found = 1;
				break;

			case NC_TPI_COTS:
				lo_cots_found = 1;
				break;

			case NC_TPI_COTS_ORD:
				lo_cotsord_found = 1;
				break;
			}
		if (stat(np->nc_device, &sb) == -1 && errno == ENOENT) {
			if (debugging)
				fprintf(stderr, "\tdevice %s does not exist\n",
					np->nc_device);
			syslog(LOG_ERR, "netid %s:  device %s does not exist",
				np->nc_netid, np->nc_device);
			busted++;
		} else
			if (debugging)
				fprintf(stderr, "\tdevice %s present\n",
					np->nc_device);
		for (i = 0; i < np->nc_nlookups; i++) {
			char	*libname = np->nc_lookups[i];

			if ((dlcookie = dlopen(libname, RTLD_LAZY)) ==
				(void *) NULL) {

				char *dlerrstr;

				dlerrstr = dlerror();
				if (debugging) {
					fprintf(stderr,
	"\tnetid %s: dlopen of name-to-address library %s failed\ndlerror: %s",
	np->nc_netid, libname, dlerrstr ? dlerrstr : "");
				}
				syslog(LOG_ERR,
	"netid %s:  dlopen of name-to-address library %s failed",
						np->nc_netid, libname);
				if (dlerrstr)
					syslog(LOG_ERR, "%s", dlerrstr);
				busted++;
			} else {
				if (debugging)
					fprintf(stderr,
	"\tdlopen of name-to-address library %s succeeded\n", libname);
				(void) dlclose(dlcookie);
			}
		}
		nconf = getnetconfigent(np->nc_netid);

		nh.h_host = HOST_SELF;
		nh.h_serv = "";
		if (netdir_getbyname(nconf, &nh, &na) != 0 || na->n_cnt == 0) {
			if (debugging)
				fprintf(stderr,
	"\tnetdir_getbyname for HOST_SELF failed\n");
			syslog(LOG_ALERT,
	"netid %s:  cannot find an address for HOST_SELF", np->nc_netid);
			busted++;
		} else
			if (debugging)
				fprintf(stderr,
	"\tnetdir_getbyname for HOST_SELF succeeded\n");

		nh.h_host = HOST_SELF_CONNECT;
		nh.h_serv = "";
		if (netdir_getbyname(nconf, &nh, &na) != 0 || na->n_cnt == 0) {
			syslog(LOG_ALERT,
"netid %s:  cannot find an address for HOST_SELF_CONNECT", np->nc_netid);
			if (debugging)
				fprintf(stderr,
	"\tnetdir_getbyname for HOST_SELF_CONNECT failed\n");
			busted++;
		} else
			if (debugging)
				fprintf(stderr,
	"\tnetdir_getbyname for HOST_SELF_CONNECT succeeded\n");
		nh.h_host = HOST_SELF;
		nh.h_serv = "rpcbind";
		if (netdir_getbyname(nconf, &nh, &na) != 0 || na->n_cnt == 0) {
			syslog(LOG_ALERT,
"netid %s: cannot find an address for host HOST_SELF, service \"rpcbind\"",
				np->nc_netid);
			if (debugging)
				fprintf(stderr,
	"\tnetdir_getbyname for HOST_SELF, service \"rpcbind\" failed\n");
			busted++;
		} else
			if (debugging)
				fprintf(stderr,
"\tnetdir_getbyname for host HOST_SELF, service \"rpcbind\" succeeded\n");

		nh.h_host = HOST_SELF_CONNECT;
		nh.h_serv = "rpcbind";
		if (netdir_getbyname(nconf, &nh, &na) != 0 || na->n_cnt == 0) {
			syslog(LOG_ALERT,
"netid %s: cannot find an address for host HOST_SELF_CONNECT, \
service \"rpcbind\"",
				np->nc_netid);
			if (debugging)
				fprintf(stderr,
"\tnetdir_getbyname for HOST_SELF_CONNECT, service \"rpcbind\" failed\n");
			busted++;
		} else
			if (debugging)
				fprintf(stderr,
"\tnetdir_getbyname for host HOST_SELF_CONNECT, \
service \"rpcbind\" succeeded\n");
		freenetconfigent(nconf);
	}
	endnetconfig(nc);

	if (lo_clts_found) {
		if (debugging)
			fprintf(stderr, "Found CLTS loopback transport\n");
	} else {
		syslog(LOG_ALERT, "no CLTS loopback transport found\n");
		if (debugging)
			fprintf(stderr, "no CLTS loopback transport found\n");
	}
	if (lo_cots_found) {
		if (debugging)
			fprintf(stderr, "Found COTS loopback transport\n");
	} else {
		syslog(LOG_ALERT, "no COTS loopback transport found\n");
		if (debugging)
			fprintf(stderr, "no COTS loopback transport found\n");
	}
	if (lo_cotsord_found) {
		if (debugging)
			fprintf(stderr, "Found COTS ORD loopback transport\n");
	} else {
		syslog(LOG_ALERT, "no COTS ORD loopback transport found\n");
		if (debugging)
			fprintf(stderr,
				"no COTS ORD loopback transport found\n");
	}

	return (busted);
}

/*
 * Adds the entry into the rpcbind database.
 * If PORTMAP, then for UDP and TCP, it adds the entries for version 2 also
 * Returns 0 if succeeds, else fails
 */
static int
init_transport(nconf)
	struct netconfig *nconf;	/* Transport provider info */
{
	int fd;
	struct t_bind *taddr, *baddr;
	SVCXPRT	*my_xprt;
	struct nd_addrlist *nas;
	struct nd_hostserv hs;
	int status;	/* bound checking ? */
	int sc_try = 2;
	static int msgprt = 0;

	static int setopt_reuseaddr(int);
	static int setup_callit(int);

	if ((nconf->nc_semantics != NC_TPI_CLTS) &&
		(nconf->nc_semantics != NC_TPI_COTS) &&
		(nconf->nc_semantics != NC_TPI_COTS_ORD))
		return (1);	/* not my type */

	if ((strcmp(nconf->nc_protofmly, NC_INET6) == 0) && !ipv6flag) {
		if (!msgprt)
			syslog(LOG_DEBUG,
"/etc/netconfig has IPv6 entries but IPv6 is not configured");
		msgprt++;
		return (1);
	}
#ifdef ND_DEBUG
	{
	int i;
	char **s;

	(void) fprintf(stderr, "%s: %d lookup routines :\n",
		nconf->nc_netid, nconf->nc_nlookups);
	for (i = 0, s = nconf->nc_lookups; i < nconf->nc_nlookups; i++, s++)
		fprintf(stderr, "[%d] - %s\n", i, *s);
	}
#endif

	if ((fd = t_open(nconf->nc_device, O_RDWR, NULL)) < 0) {
		syslog(LOG_ERR, "%s: cannot open connection: %s",
				nconf->nc_netid, t_errlist[t_errno]);
		return (1);
	}

	/*
	 * In case of loopback transports, negotiate for
	 * returning the uid of the caller. This should
	 * done before enabling the endpoint for service via
	 * t_bind() so that requests to rpcbind contain the uid.
	 */
	if (strcmp(nconf->nc_protofmly, NC_LOOPBACK) == 0) {
		if (__rpc_negotiate_uid(fd) != 0) {
			syslog(LOG_ERR,
			"Couldn't negotiate for uid with loopback tranport %s",
				nconf->nc_netid);
			t_close(fd);
			return (1);
		}
	}

	taddr = (struct t_bind *)t_alloc(fd, T_BIND, T_ADDR);
	baddr = (struct t_bind *)t_alloc(fd, T_BIND, T_ADDR);
	if ((baddr == NULL) || (taddr == NULL)) {
		syslog(LOG_ERR, "%s: cannot allocate netbuf: %s",
				nconf->nc_netid, t_errlist[t_errno]);
		exit(1);
	}

	/* Get rpcbind's address on this transport */
	hs.h_host = HOST_SELF;
	hs.h_serv = servname;
	if (netdir_getbyname(nconf, &hs, &nas))
		goto error;

	/* Copy the address */
	taddr->addr.len = nas->n_addrs->len;
	memcpy(taddr->addr.buf, nas->n_addrs->buf, (int)nas->n_addrs->len);
#ifdef ND_DEBUG
	{
	/* for debugging print out our universal address */
	char *uaddr;

	uaddr = taddr2uaddr(nconf, nas->n_addrs);
	(void) fprintf(stderr, "rpcbind : my address is %s\n", uaddr);
	(void) free(uaddr);
	}
#endif
	netdir_free((char *)nas, ND_ADDRLIST);

	if (nconf->nc_semantics == NC_TPI_CLTS)
		taddr->qlen = 0;
	else
		taddr->qlen = listen_backlog;

	if (strcmp(nconf->nc_proto, NC_TCP) == 0) {
		/*
		 * Sm: If we are running then set SO_REUSEADDR option
		 * so that we can bind to our preferred address even if
		 * previous connections are in FIN_WAIT state
		 */
#ifdef ND_DEBUG
		fprintf(stdout, "Setting SO_REUSEADDR.\n");
#endif
		if (setopt_reuseaddr(fd) == -1) {
			syslog(LOG_ERR, "Couldn't set SO_REUSEADDR option");
		}
	}

	if (t_bind(fd, taddr, baddr) != 0) {
		syslog(LOG_ERR, "%s: cannot bind: %s",
			nconf->nc_netid, t_errlist[t_errno]);
		goto error;
	}


	if (memcmp(taddr->addr.buf, baddr->addr.buf, (int)baddr->addr.len)) {
		syslog(LOG_ERR, "%s: address in use", nconf->nc_netid);
		goto error;
	}

	my_xprt = (SVCXPRT *)svc_tli_create(fd, nconf, baddr, 0, 0);
	if (my_xprt == (SVCXPRT *)NULL) {
		syslog(LOG_ERR, "%s: could not create service",
				nconf->nc_netid);
		goto error;
	}

	/* set up multicast address for RPC CALL_IT, IPv6 */

	if ((strcmp(nconf->nc_protofmly, NC_INET6) == 0) &&
	    (strcmp(nconf->nc_proto, NC_UDP) == 0)) {
		if (setup_callit(fd) < 0) {
			syslog(LOG_ERR, "Unable to join IPv6 multicast group \
for rpc broadcast %s", RPCB_MULTICAST_ADDR);
		}
	}

	if (strcmp(nconf->nc_proto, NC_TCP) == 0) {
			svc_control(my_xprt, SVCSET_KEEPALIVE, (void *) TRUE);
	}

#ifdef PORTMAP
	/*
	 * Register both the versions for tcp/ip and udp/ip
	 */
	if ((strcmp(nconf->nc_protofmly, NC_INET) == 0) &&
		((strcmp(nconf->nc_proto, NC_TCP) == 0) ||
		(strcmp(nconf->nc_proto, NC_UDP) == 0))) {
		PMAPLIST *pml;

		if (!svc_register(my_xprt, PMAPPROG, PMAPVERS,
			pmap_service, NULL)) {
			syslog(LOG_ERR, "could not register on %s",
					nconf->nc_netid);
			goto error;
		}
		pml = (PMAPLIST *)malloc((uint_t)sizeof (PMAPLIST));
		if (pml == (PMAPLIST *)NULL) {
			syslog(LOG_ERR, "no memory!");
			exit(1);
		}
		pml->pml_map.pm_prog = PMAPPROG;
		pml->pml_map.pm_vers = PMAPVERS;
		pml->pml_map.pm_port = PMAPPORT;
		if (strcmp(nconf->nc_proto, NC_TCP) == 0) {
			if (tcptrans[0]) {
				syslog(LOG_ERR,
				"cannot have more than one TCP transport");
				goto error;
			}
			tcptrans = strdup(nconf->nc_netid);
			pml->pml_map.pm_prot = IPPROTO_TCP;

			/* Let's snarf the universal address */
			/* "h1.h2.h3.h4.p1.p2" */
			tcp_uaddr = taddr2uaddr(nconf, &baddr->addr);
		} else {
			if (udptrans[0]) {
				syslog(LOG_ERR,
				"cannot have more than one UDP transport");
				goto error;
			}
			udptrans = strdup(nconf->nc_netid);
			pml->pml_map.pm_prot = IPPROTO_UDP;

			/* Let's snarf the universal address */
			/* "h1.h2.h3.h4.p1.p2" */
			udp_uaddr = taddr2uaddr(nconf, &baddr->addr);
		}
		pml->pml_next = list_pml;
		list_pml = pml;

		/* Add version 3 information */
		pml = (PMAPLIST *)malloc((uint_t)sizeof (PMAPLIST));
		if (pml == (PMAPLIST *)NULL) {
			syslog(LOG_ERR, "no memory!");
			exit(1);
		}
		pml->pml_map = list_pml->pml_map;
		pml->pml_map.pm_vers = RPCBVERS;
		pml->pml_next = list_pml;
		list_pml = pml;

		/* Add version 4 information */
		pml = (PMAPLIST *)malloc((uint_t)sizeof (PMAPLIST));
		if (pml == (PMAPLIST *)NULL) {
			syslog(LOG_ERR, "no memory!");
			exit(1);
		}
		pml->pml_map = list_pml->pml_map;
		pml->pml_map.pm_vers = RPCBVERS4;
		pml->pml_next = list_pml;
		list_pml = pml;

		/* Also add version 2 stuff to rpcbind list */
		rbllist_add(PMAPPROG, PMAPVERS, nconf, &baddr->addr);
	}
#endif

	/* version 3 registration */
	if (!svc_reg(my_xprt, RPCBPROG, RPCBVERS, rpcb_service_3, NULL)) {
		syslog(LOG_ERR, "could not register %s version 3",
				nconf->nc_netid);
		goto error;
	}
	rbllist_add(RPCBPROG, RPCBVERS, nconf, &baddr->addr);

	/* version 4 registration */
	if (!svc_reg(my_xprt, RPCBPROG, RPCBVERS4, rpcb_service_4, NULL)) {
		syslog(LOG_ERR, "could not register %s version 4",
				nconf->nc_netid);
		goto error;
	}
	rbllist_add(RPCBPROG, RPCBVERS4, nconf, &baddr->addr);

	if (strcmp(nconf->nc_protofmly, NC_LOOPBACK) == 0) {
		if (nconf->nc_semantics == NC_TPI_CLTS)
			loopback_dg = strdup(nconf->nc_netid);
		else if (nconf->nc_semantics == NC_TPI_COTS)
			loopback_vc = strdup(nconf->nc_netid);
		else if (nconf->nc_semantics == NC_TPI_COTS_ORD)
			loopback_vc_ord = strdup(nconf->nc_netid);
	}

	/* decide if bound checking works for this transport */
	status = add_bndlist(nconf, taddr, baddr);
#ifdef BIND_DEBUG
	if (status < 0) {
		fprintf(stderr, "Error in finding bind status for %s\n",
			nconf->nc_netid);
	} else if (status == 0) {
		fprintf(stderr, "check binding for %s\n",
			nconf->nc_netid);
	} else if (status > 0) {
		fprintf(stderr, "No check binding for %s\n",
			nconf->nc_netid);
	}
#endif
	/*
	 * rmtcall only supported on CLTS transports for now.
	 */
	if (nconf->nc_semantics == NC_TPI_CLTS) {
		status = create_rmtcall_fd(nconf);

#ifdef BIND_DEBUG
		if (status < 0) {
			fprintf(stderr, "Could not create rmtcall fd for %s\n",
				nconf->nc_netid);
		} else {
			fprintf(stderr, "rmtcall fd for %s is %d\n",
				nconf->nc_netid, status);
		}
#endif
	}
	(void) t_free((char *)taddr, T_BIND);
	(void) t_free((char *)baddr, T_BIND);
	return (0);
error:
	(void) t_free((char *)taddr, T_BIND);
	(void) t_free((char *)baddr, T_BIND);
	(void) t_close(fd);
	return (1);
}

static void
rbllist_add(prog, vers, nconf, addr)
	ulong_t prog;
	ulong_t vers;
	struct netconfig *nconf;
	struct netbuf *addr;
{
	rpcblist_ptr rbl;

	rbl = (rpcblist_ptr)malloc((uint_t)sizeof (rpcblist));
	if (rbl == (rpcblist_ptr)NULL) {
		syslog(LOG_ERR, "no memory!");
		exit(1);
	}

	rbl->rpcb_map.r_prog = prog;
	rbl->rpcb_map.r_vers = vers;
	rbl->rpcb_map.r_netid = strdup(nconf->nc_netid);
	rbl->rpcb_map.r_addr = taddr2uaddr(nconf, addr);
	rbl->rpcb_map.r_owner = strdup(superuser);
	rbl->rpcb_next = list_rbl;	/* Attach to global list */
	list_rbl = rbl;
}

/*
 * Catch the signal and die
 */
static void
terminate()
{
	syslog(LOG_ERR, "rpcbind terminating on signal.");
	write_warmstart();	/* Dump yourself */
	exit(2);
}

void
rpcbind_abort()
{
	write_warmstart();	/* Dump yourself */
	abort();
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
	switch (fork()) {
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

/* get command line options */
static void
parseargs(argc, argv)
	int argc;
	char *argv[];
{
	int c;
	int tmp;

	while ((c = getopt(argc, argv, "dwal:")) != EOF) {
		switch (c) {
		case 'd':
			debugging = 1;
			break;
		case 'a':
			doabort = 1;	/* when debugging, do an abort on */
			break;		/* errors; for rpcbind developers */
					/* only! */
		case 'w':
			warmstart = 1;
			break;

		case 'l':
			if (sscanf(optarg, "%d", &tmp)) {
				if (tmp > listen_backlog) {
					listen_backlog = tmp;
				}
			}
			break;
		default:	/* error */
			fprintf(stderr,
			"usage: rpcbind [-d] [-w] [-l listen_backlog]\n");
			exit(1);
		}
	}
	if (doabort && !debugging) {
	    fprintf(stderr,
		"-a (abort) specified without -d (debugging) -- ignored.\n");
	    doabort = 0;
	}
}

static int
setopt_reuseaddr(int fd)
{
	struct t_optmgmt req, resp;
	struct opthdr *opt;
	char reqbuf[128];
	int *ip;

	opt = (struct opthdr *)reqbuf;
	opt->level = SOL_SOCKET;
	opt->name = SO_REUSEADDR;
	opt->len = sizeof (int);

	ip = (int *)&reqbuf[sizeof (struct opthdr)];
	*ip = 1;

	req.flags = T_NEGOTIATE;
	req.opt.len = sizeof (struct opthdr) + opt->len;
	req.opt.buf = (char *)opt;

	resp.flags = 0;
	resp.opt.buf = reqbuf;
	resp.opt.maxlen = sizeof (reqbuf);

	if (t_optmgmt(fd, &req, &resp) < 0 || resp.flags != T_SUCCESS) {
		t_error("t_optmgmt");
		return (-1);
	}
	return (0);
}

static int
setup_callit(int fd)
{
	struct ipv6_mreq mreq;
	struct t_optmgmt req, resp;
	struct opthdr *opt;
	char reqbuf[ sizeof (struct ipv6_mreq) + 24];
	struct ipv6_mreq *pmreq;

	opt = (struct opthdr *)reqbuf;

	opt->level = IPPROTO_IPV6;
	opt->name = IPV6_ADD_MEMBERSHIP;
	opt->len = sizeof (struct ipv6_mreq);

	/* multicast address */
	(void) inet_pton(AF_INET6, RPCB_MULTICAST_ADDR,
		mreq.ipv6mr_multiaddr.s6_addr);
	mreq.ipv6mr_interface = 0;

	/* insert it into opt */
	pmreq = (struct ipv6_mreq *)&reqbuf[sizeof (struct opthdr)];
	memcpy(pmreq, &mreq, sizeof (struct ipv6_mreq));

	req.flags = T_NEGOTIATE;
	req.opt.len = sizeof (struct opthdr) + opt->len;
	req.opt.buf = (char *)opt;

	resp.flags = 0;
	resp.opt.buf = reqbuf;
	resp.opt.maxlen = sizeof (reqbuf);

	if (t_optmgmt(fd, &req, &resp) < 0 || resp.flags != T_SUCCESS) {
		t_error("t_optmgmt");
		return (-1);
	}
	return (0);
}
