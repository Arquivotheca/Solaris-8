/*
 * 	(c) 1991-1994,1999  Sun Microsystems, Inc
 *	All rights reserved.
 *
 * lib/libnsl/nss/netdir_inet.c
 *
 * This is where we have chosen to combine every useful bit of code for
 * all the Solaris frontends to lookup hosts, services, and netdir information
 * for inet family (udp, tcp) transports. gethostbyYY(), getservbyYY(), and
 * netdir_getbyYY() are all implemented on top of this code. Similarly,
 * netdir_options, taddr2uaddr, and uaddr2taddr for inet transports also
 * find a home here.
 *
 * If the netconfig structure supplied has NO nametoaddr libs (i.e. a "-"
 * in /etc/netconfig), this code calls the name service switch, and
 * therefore, /etc/nsswitch.conf is effectively the only place that
 * dictates hosts/serv lookup policy.
 * If an administrator chooses to bypass the name service switch by
 * specifying third party supplied nametoaddr libs in /etc/netconfig, this
 * implementation does NOT call the name service switch, it merely loops
 * through the nametoaddr libs. In this case, if this code was called
 * from gethost/servbyYY() we marshal the inet specific struct into
 * transport independent netbuf or hostserv, and unmarshal the resulting
 * nd_addrlist or hostservlist back into hostent and servent, as the case
 * may be.
 *
 * Goes without saying that most of the future bugs in gethost/servbyYY
 * and netdir_getbyYY are lurking somewhere here.
 */

#pragma ident	"@(#)netdir_inet.c	1.43	99/12/05 SMI"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stropts.h>
#include <sys/types.h>
#include <sys/byteorder.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <errno.h>
#include <fcntl.h>
#include <thread.h>
#include <synch.h>
#include <sys/utsname.h>
#include <netdb.h>
#include <netconfig.h>
#include <netdir.h>
#include <tiuser.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <nss_dbdefs.h>
#include <nss_netdir.h>
#include <rpc/trace.h>
#include <syslog.h>

#define	GETHOSTBUF(host_buf)					\
	NSS_XbyY_ALLOC(&host_buf, sizeof (struct hostent), NSS_BUFLEN_HOSTS)
#define	GETSERVBUF(serv_buf)					\
	NSS_XbyY_ALLOC(&serv_buf, sizeof (struct servent), NSS_BUFLEN_SERVICES)

#ifdef PIC
#define	DOOR_GETHOSTBYNAME_R	_door_gethostbyname_r
#define	DOOR_GETHOSTBYADDR_R	_door_gethostbyaddr_r
#define	DOOR_GETIPNODEBYNAME_R	_door_getipnodebyname_r
#define	DOOR_GETIPNODEBYADDR_R	_door_getipnodebyaddr_r
#else
#define	DOOR_GETHOSTBYNAME_R	_switch_gethostbyname_r
#define	DOOR_GETHOSTBYADDR_R	_switch_gethostbyaddr_r
#define	DOOR_GETIPNODEBYNAME_R	_switch_getipnodebyname_r
#define	DOOR_GETIPNODEBYADDR_R	_switch_getipnodebyaddr_r
#endif PIC

/*
 * constant values of addresses for HOST_SELF_BIND, HOST_SELF_CONNECT
 * and localhost.
 *
 * The following variables are static to the extent that they should
 * not be visible outside of this file. Watch out for sa_con which
 * is initialized programmatically to a const at only ONE place (we
 * can't initialize a union's non-first member); it should not
 * be modified elsewhere so as to keep things re-entrant.
 */
static char *localaddr[] = {"\000\000\000\000", NULL};
static char *connectaddr[] = {"\177\000\000\001", NULL};
static char *localaddr6[] =
{"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000", NULL};
static char *connectaddr6[] =
{"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\001", NULL};

/* IPv4 nd_addrlist */
mutex_t	nd_addr_lock = DEFAULTMUTEX;
static struct sockaddr_in sa_con;
static struct netbuf nd_conbuf = {sizeof (sa_con),\
    sizeof (sa_con), (char *)&sa_con};
static struct nd_addrlist nd_conaddrlist = {1, &nd_conbuf};

/* IPv6 nd_addrlist */
mutex_t	nd6_addr_lock = DEFAULTMUTEX;
static struct sockaddr_in6 sa6_con;
static struct netbuf nd6_conbuf = {sizeof (sa6_con),\
	sizeof (sa6_con), (char *)&sa6_con};
static struct nd_addrlist nd6_conaddrlist = {1, &nd6_conbuf};

#define	LOCALHOST "localhost"

int str2hostent(const char *, int, void *, char *, int);
int str2hostent6(const char *, int, void *, char *, int);
int str2servent(const char *, int, void *, char *, int);

static int __herrno2netdir(int h_errnop);
static struct ifinfo *get_local_info(void);
static int islocal();
static int getbroadcastnets(struct netconfig *, struct in_addr **);
struct hostent *_switch_gethostbyname_r(const char *,
			struct hostent *, char *, int, int *);
struct hostent *_switch_gethostbyaddr_r(const char *, int, int,
			struct hostent *, char *, int, int *);
struct hostent *_switch_getipnodebyname_r(const char *,
			struct hostent *, char *, int, int *);
struct hostent *_switch_getipnodebyaddr_r(const char *, int, int,
			struct hostent *, char *, int, int *);
struct hostent *_switch_hostname2addr_r(const char *,		/* XXX remove */
			struct hostent *, char *, int, int *);	/* XXX remove */
struct hostent *_door_gethostbyname_r(const char *,
			struct hostent *, char *, int, int *);
struct hostent *_door_gethostbyaddr_r(const char *, int, int,
			struct hostent *, char *, int, int *);
struct hostent *_door_getipnodebyname_r(const char *,
			struct hostent *, char *, int, int *);
struct hostent *_door_getipnodebyaddr_r(const char *, int, int,
			struct hostent *, char *, int, int *);
struct servent *_switch_getservbyname_r(const char *,
			const char *, struct servent *, char *, int);
struct servent *_switch_getservbyport_r(int,
			const char *, struct servent *, char *, int);


int ndaddr2hent(int, const char *nam, struct nd_addrlist *nlist,
    struct hostent *result, char *buffer, int buflen);
int hsents2ndhostservs(struct hostent *he, struct servent *se,
	ushort_t port, struct nd_hostservlist **hslist);
int hent2ndaddr(int af, char **haddrlist, int *servp,
    struct nd_addrlist **nd_alist);
int order_haddrlist(char **haddrlist, struct sockaddr_in **res_salist);
int order_haddrlist_af(sa_family_t af, char **haddrlist, void *res_salist);
int ndaddr2srent(const char *name, const char *proto, ushort_t port,
    struct servent *result, char *buffer, int buflen);
int ndhostserv2hent(struct netbuf *nbuf, struct nd_hostservlist *addrs,
    struct hostent *result, char *buffer, int buflen);
int ndhostserv2srent(int port, const char *proto, struct nd_hostservlist *addrs,
    struct servent *result, char *buffer, int buflen);
static int nd2herrno(int nerr);

static struct in_addr _inet_makeaddr(in_addr_t net, in_addr_t host);

extern void _nss_initf_hosts(nss_db_params_t *);
extern void _nss_initf_ipnodes(nss_db_params_t *);

/*
 * Begin: PART I
 * Top Level Interfaces that gethost/serv/netdir funnel through.
 *
 */

/*
 * gethost/servbyname always call this function; if they call
 * with nametoaddr libs in nconf, we call netdir_getbyname
 * implementation: __classic_netdir_getbyname, otherwise nsswitch.
 *
 * netdir_getbyname calls this only if nametoaddr libs are NOT
 * specified for inet transports; i.e. it's supposed to follow
 * the name service switch.
 */
int
_get_hostserv_inetnetdir_byname(nconf, args, res)
	struct	netconfig *nconf;
	struct	nss_netdirbyname_in *args;
	union	nss_netdirbyname_out *res;
{
	int	server_port;
	int *servp = &server_port;
	char	**haddrlist;
	uint32_t dotnameaddr;
	char	*dotnamelist[2];
	struct in_addr	*inaddrs = NULL;
	struct in6_addr	v6nameaddr;
	char	**baddrlist = NULL;

	if (nconf == 0) {
		_nderror = ND_BADARG;
		return (ND_BADARG);
	}

	/*
	 * 1. gethostbyname()/netdir_getbyname() special cases:
	 */
	switch (args->op_t) {

		case NSS_HOST:
		/*
		 * Worth the performance gain -- assuming a lot of inet apps
		 * actively use "localhost".
		 */
		if (strcmp(args->arg.nss.host.name, LOCALHOST) == 0) {

			mutex_lock(&nd_addr_lock);
			IN_SET_LOOPBACK_ADDR(&sa_con);
			_nderror = ndaddr2hent(AF_INET, args->arg.nss.host.name,
			    &nd_conaddrlist, res->nss.host.hent,
			    args->arg.nss.host.buf,
			    args->arg.nss.host.buflen);
			mutex_unlock(&nd_addr_lock);
			if (_nderror != ND_OK)
				*(res->nss.host.herrno_p) =
				    nd2herrno(_nderror);
			return (_nderror);
		}
		/*
		 * If the caller passed in a dot separated IP notation to
		 * gethostbyname, return that back as the address.
		 * The nd_addr_lock mutex was added to be truely re-entrant.
		 */
		if ((int)(dotnameaddr = inet_addr(args->arg.nss.host.name)) !=
		    -1) {
			mutex_lock(&nd_addr_lock);
			(void) memset((char *)&sa_con, 0, sizeof (sa_con));
			sa_con.sin_family = AF_INET;
			sa_con.sin_addr.s_addr = dotnameaddr;
			_nderror = ndaddr2hent(AF_INET, args->arg.nss.host.name,
			    &nd_conaddrlist, res->nss.host.hent,
			    args->arg.nss.host.buf,
			    args->arg.nss.host.buflen);
			mutex_unlock(&nd_addr_lock);
			if (_nderror != ND_OK)
				*(res->nss.host.herrno_p) =
				    nd2herrno(_nderror);
			return (_nderror);
		}
		break;

		case NSS_HOST6:
		/*
		 * Handle case of literal address string.
		 */
		if (strchr(args->arg.nss.host.name, ':') != NULL &&
		    (inet_pton(AF_INET6, args->arg.nss.host.name,
		    &v6nameaddr) != 0)) {
			int	ret;

			mutex_lock(&nd6_addr_lock);
			(void) memset((char *)&sa6_con, 0, sizeof (sa6_con));
			sa6_con.sin6_family = AF_INET6;
			memcpy((char *)&(sa6_con.sin6_addr.s6_addr),
			    &v6nameaddr, sizeof (struct in6_addr));
			ret = ndaddr2hent(AF_INET6,
			    args->arg.nss.host.name,
			    &nd6_conaddrlist, res->nss.host.hent,
			    args->arg.nss.host.buf,
			    args->arg.nss.host.buflen);
			mutex_unlock(&nd_addr_lock);
			if (ret != ND_OK)
				*(res->nss.host.herrno_p) = nd2herrno(ret);
			else
				res->nss.host.hent->h_aliases = NULL;
			return (ret);
		}
		break;

		case NETDIR_BY:
			if (args->arg.nd_hs == 0) {
				_nderror = ND_BADARG;
				return (ND_BADARG);
			}
			/*
			 * If servname is NULL, return 0 as the port number
			 * If servname is rpcbind, return 111 as the port number
			 * If servname is a number, return it back as the port
			 * number.
			 */
			if (args->arg.nd_hs->h_serv == 0) {
				*servp = htons(0);
			} else if (strcmp(args->arg.nd_hs->h_serv, "rpcbind")
									== 0) {
				*servp = htons(111);
			} else if (strspn(args->arg.nd_hs->h_serv, "0123456789")
				    == strlen(args->arg.nd_hs->h_serv)) {
				*servp = htons(atoi(args->arg.nd_hs->h_serv));
			} else {
				/* i.e. need to call a name service on this */
				servp = NULL;
			}

			/*
			 * If the hostname is HOST_SELF_BIND, we return 0.0.0.0
			 * so the  binding can be contacted through all
			 * interfaces. If the hostname is HOST_SELF_CONNECT,
			 * we return 127.0.0.1 so the address can be connected
			 * to locally. If the hostname is HOST_ANY, we return
			 * no addresses because IP doesn't know how to specify
			 * a service without a host. And finally if we specify
			 * HOST_BROADCAST then we ask a tli fd to tell us what
			 * the broadcast addresses are for any udp
			 * interfaces on this machine.
			 */
			if (args->arg.nd_hs->h_host == 0) {
				_nderror = ND_NOHOST;
				return (ND_NOHOST);
			} else if ((strcmp(args->arg.nd_hs->h_host,
			    HOST_SELF_BIND) == 0)) {
				haddrlist = localaddr;
			} else if ((strcmp(args->arg.nd_hs->h_host,
					    HOST_SELF_CONNECT) == 0)) {
				haddrlist = connectaddr;
			} else if ((strcmp(args->arg.nd_hs->h_host,
					    LOCALHOST) == 0)) {
				haddrlist = connectaddr;
			} else if ((int)(dotnameaddr =
				    inet_addr(args->arg.nd_hs->h_host)) != -1) {
				/*
				 * If the caller passed in a dot separated IP
				 * notation to netdir_getbyname, convert that
				 * back into address.
				 */

				dotnamelist[0] = (char *)&dotnameaddr;
				dotnamelist[1] = NULL;
				haddrlist = dotnamelist;
			} else if ((strcmp(args->arg.nd_hs->h_host,
					    HOST_BROADCAST) == 0)) {
				/*
				 * Now that inaddrs and baddrlist are
				 * dynamically allocated, care must be
				 * taken in freeing up the
				 * memory at each 'return()' point.
				 *
				 * Early return protection (using
				 * FREE_return()) is needed only in NETDIR_BY
				 * cases because dynamic allocation is used
				 * when args->op_t == NETDIR_BY.
				 *
				 * Early return protection is not needed in
				 * haddrlist==0 conditionals because dynamic
				 * allocation guarantees haddrlist!=0.
				 *
				 * Early return protection is not needed in most
				 * servp!=0 conditionals because this is handled
				 * (and returned) first.
				 */
#define	FREE_return(ret) \
				{ \
				    if (inaddrs) \
					    free(inaddrs); \
					    if (baddrlist) \
						    free(baddrlist); \
						    _nderror = ret; \
						    return (ret); \
				}
				int i, bnets;

				bnets = getbroadcastnets(nconf, &inaddrs);
				if (bnets == 0) {
					_nderror = ND_NOHOST;
					return (ND_NOHOST);
				}
				baddrlist =
				    (char **)malloc((bnets+1)*sizeof (char *));
				if (baddrlist == NULL)
					FREE_return(ND_NOMEM);
				for (i = 0; i < bnets; i++)
					baddrlist[i] = (char *)&inaddrs[i];
				baddrlist[i] = NULL;
				haddrlist = baddrlist;
			} else {
				/* i.e. need to call a name service on this */
				haddrlist = 0;
			}

			if (haddrlist && servp) {
				int ret;
				/*
				 * Convert h_addr_list into ordered nd_addrlist.
				 * malloc's will be done, freed using
				 * netdir_free.
				 */
				ret = hent2ndaddr(AF_INET, haddrlist, servp,
					    res->nd_alist);
				FREE_return(ret)
				}
			break;


		case NETDIR_BY6:
			if (args->arg.nd_hs == 0) {
				_nderror = ND_BADARG;
				return (ND_BADARG);
			}
			/*
			 * If servname is NULL, return 0 as the port number.
			 * If servname is rpcbind, return 111 as the port number
			 * If servname is a number, return it back as the port
			 * number.
			 */
			if (args->arg.nd_hs->h_serv == 0) {
				*servp = htons(0);
			} else if (strcmp(args->arg.nd_hs->h_serv,
				    "rpcbind") == 0) {
				*servp = htons(111);
			} else if (strspn(args->arg.nd_hs->h_serv, "0123456789")
				    == strlen(args->arg.nd_hs->h_serv)) {
				*servp = htons(atoi(args->arg.nd_hs->h_serv));
			} else {
				/* i.e. need to call a name service on this */
				servp = NULL;
			}

			/*
			 * If the hostname is HOST_SELF_BIND, we return ipv6
			 * localaddress so the binding can be contacted through
			 * all interfaces.
			 * If the hostname is HOST_SELF_CONNECT, we return
			 * ipv6 loopback address so the address can be connected
			 * to locally.
			 * If the hostname is HOST_ANY, we return no addresses
			 * because IP doesn't know how to specify a service
			 * without a host.
			 * And finally if we specify HOST_BROADCAST then we
			 * disallow since IPV6 does not have any
			 * broadcast concept.
			 */
			if (args->arg.nd_hs->h_host == 0) {
				return (ND_NOHOST);
			} else if ((strcmp(args->arg.nd_hs->h_host,
					    HOST_SELF_BIND) == 0)) {
				haddrlist = localaddr6;
			} else if ((strcmp(args->arg.nd_hs->h_host,
					    HOST_SELF_CONNECT) == 0)) {
				haddrlist = connectaddr6;
			} else if ((strcmp(args->arg.nd_hs->h_host,
					    LOCALHOST) == 0)) {
				haddrlist = connectaddr6;
			} else if (strchr(args->arg.nd_hs->h_host, ':')
						    != NULL) {

			/*
			 * If the caller passed in a dot separated IP notation
			 * to netdir_getbyname, convert that back into address.
			 */

				if ((inet_pton(AF_INET6,
					    args->arg.nd_hs->h_host,
					    &v6nameaddr)) != 0) {
					dotnamelist[0] = (char *)&v6nameaddr;
					dotnamelist[1] = NULL;
					haddrlist = dotnamelist;
				}
				else
					/* not sure what to return */
					return (ND_NOHOST);

			} else if ((strcmp(args->arg.nd_hs->h_host,
				    HOST_BROADCAST) == 0)) {
				/*
				 * Don't support broadcast in
				 * IPV6
				 */
				return (ND_NOHOST);
			} else {
				/* i.e. need to call a name service on this */
				haddrlist = 0;
			}

			if (haddrlist && servp) {
				int ret;
				/*
				 * Convert h_addr_list into ordered nd_addrlist.
				 * malloc's will be done, freed
				 * using netdir_free.
				 */
				ret = hent2ndaddr(AF_INET6, haddrlist,
				    servp, res->nd_alist);
				FREE_return(ret)
				}
			break;


	}

	/*
	 * 2. Most common scenario. This is the way we ship /etc/netconfig.
	 *    Emphasis on improving performance in the "if" part.
	 */
	if (nconf->nc_nlookups == 0) {
		struct hostent	*he;
		struct servent	*se;
		int	ret;
		nss_XbyY_buf_t	*ndbuf4switch = 0;

	switch (args->op_t) {

		case NSS_HOST:
		he = DOOR_GETHOSTBYNAME_R(args->arg.nss.host.name,
		    res->nss.host.hent, args->arg.nss.host.buf,
		    args->arg.nss.host.buflen,
		    res->nss.host.herrno_p);
		if (he == 0) {
			_nderror = ND_NOHOST;
			return (ND_NOHOST);
		} else {
			/*
			 * Order host addresses, in place, if need be.
			 */
			char	**t;
			int	num;

			haddrlist = res->nss.host.hent->h_addr_list;
			for (num = 0, t = haddrlist; *t; t++, num++);
			if (num == 1) {
				_nderror = ND_OK;
				return (ND_OK);
			}

			_nderror = order_haddrlist(haddrlist, NULL);
			return (_nderror);
		}

		case NSS_HOST6:

		he = DOOR_GETIPNODEBYNAME_R(args->arg.nss.host.name,
		    res->nss.host.hent, args->arg.nss.host.buf,
		    args->arg.nss.host.buflen,
		    res->nss.host.herrno_p);

		if (he == 0) {
			trace1(TR__get_hostserv_inetnetdir_byname, 12);
			return (ND_NOHOST);
		} else {
			/*
			 * Order host addresses, in place, if need be.
			 */
			_nderror = order_haddrlist_af(
				res->nss.host.hent->h_addrtype,
				res->nss.host.hent->h_addr_list, 0);
			trace1(TR__get_hostserv_inetnetdir_byname, 13);
			return (_nderror);
		}

		case NSS_SERV:

		se = _switch_getservbyname_r(args->arg.nss.serv.name,
		    args->arg.nss.serv.proto,
		    res->nss.serv, args->arg.nss.serv.buf,
		    args->arg.nss.serv.buflen);

		_nderror = ND_OK;
		if (se == 0)
			_nderror = ND_NOSERV;
		return (_nderror);

		case NETDIR_BY:

		if (servp == 0) {
			char	*proto =
	    (strcmp(nconf->nc_proto, NC_TCP) == 0) ? NC_TCP : NC_UDP;

			/*
			 * We go through all this for just one port number,
			 * which is most often constant. How about linking in
			 * an indexed database of well-known ports in the name
			 * of performance ?
			 */
			GETSERVBUF(ndbuf4switch);
			if (ndbuf4switch == 0)
				FREE_return(ND_NOMEM);
			se = _switch_getservbyname_r(args->arg.nd_hs->h_serv,
				proto, ndbuf4switch->result,
				ndbuf4switch->buffer, ndbuf4switch->buflen);
			if (!se) {
				NSS_XbyY_FREE(&ndbuf4switch);
				FREE_return(ND_NOSERV)
			}
			server_port = se->s_port;
			NSS_XbyY_FREE(&ndbuf4switch);
		}

		if (haddrlist == 0) {
			int	h_errnop;

			GETHOSTBUF(ndbuf4switch);
			if (ndbuf4switch == 0) {
				_nderror = ND_NOMEM;
				return (ND_NOMEM);
			}
			he = DOOR_GETHOSTBYNAME_R(args->arg.nd_hs->h_host,
				    ndbuf4switch->result, ndbuf4switch->buffer,
				    ndbuf4switch->buflen, &h_errnop);
			if (!he) {
				NSS_XbyY_FREE(&ndbuf4switch);
				return (_nderror = __herrno2netdir(h_errnop));
			}

			/*
			 * Convert h_addr_list into ordered nd_addrlist.
			 * malloc's will be done, freed using netdir_free.
			 */
			ret = hent2ndaddr(AF_INET,
		    ((struct hostent *)(ndbuf4switch->result))->h_addr_list,
			    &server_port, res->nd_alist);

				_nderror = ret;
				NSS_XbyY_FREE(&ndbuf4switch);
				return (ret);
		} else {
			int ret;
			/*
			 * Convert h_addr_list into ordered nd_addrlist.
			 * malloc's will be done, freed using netdir_free.
			 */
			ret = hent2ndaddr(AF_INET, haddrlist,
				    &server_port, res->nd_alist);
			FREE_return(ret)
		}


		case NETDIR_BY6:

			if (servp == 0) {
				char	*proto =
	(strcmp(nconf->nc_proto, NC_TCP) == 0) ? NC_TCP : NC_UDP;

				/*
				 * We go through all this for just
				 * one port number,
				 * which is most often constant.
				 * How about linking in
				 * an indexed database of well-known
				 * ports in the name
				 * of performance ?
				 */
				GETSERVBUF(ndbuf4switch);
				if (ndbuf4switch == 0)
					FREE_return(ND_NOMEM);
				se = _switch_getservbyname_r(
					    args->arg.nd_hs->h_serv,
				    proto, ndbuf4switch->result,
				    ndbuf4switch->buffer, ndbuf4switch->buflen);
				if (!se) {
					NSS_XbyY_FREE(&ndbuf4switch);
					FREE_return(ND_NOSERV)
				}
				server_port = se->s_port;
				NSS_XbyY_FREE(&ndbuf4switch);
			}

			if (haddrlist == 0) {
				int	h_errnop;

				GETHOSTBUF(ndbuf4switch);
				if (ndbuf4switch == 0) {
					_nderror = ND_NOMEM;
					return (ND_NOMEM);
				}
				he = DOOR_GETIPNODEBYNAME_R(
				    args->arg.nd_hs->h_host,
				    ndbuf4switch->result, ndbuf4switch->buffer,
				    ndbuf4switch->buflen, &h_errnop);
				if (!he) {
					NSS_XbyY_FREE(&ndbuf4switch);
					return (_nderror =
					    __herrno2netdir(h_errnop));
				}
				/*
				 * Convert h_addr_list into ordered nd_addrlist.
				 * malloc's will be done,
				 * freed using netdir_free.
				 */
				ret = hent2ndaddr(AF_INET6,
		    ((struct hostent *)(ndbuf4switch->result))->h_addr_list,
				    &server_port, res->nd_alist);
				_nderror = ret;
				NSS_XbyY_FREE(&ndbuf4switch);
				return (ret);
			} else {
				int ret;
				/*
				 * Convert h_addr_list into ordered nd_addrlist.
				 * malloc's will be done,
				 * freed using netdir_free.
				 */
				ret = hent2ndaddr(AF_INET6, haddrlist,
					    &server_port, res->nd_alist);
				FREE_return(ret)
			}

		default:
		_nderror = ND_BADARG;
		return (ND_BADARG); /* should never happen */
	}

	} else {
		/* haddrlist is no longer used, so clean up */
		if (inaddrs)
			free(inaddrs);
		if (baddrlist)
			free(baddrlist);
	}

	/*
	 * 3. We come this far only if nametoaddr libs are specified for
	 *    inet transports and we are called by gethost/servbyname only.
	 */
	switch (args->op_t) {
		struct	nd_hostserv service;
		struct	nd_addrlist *addrs;
		int ret;

		case NSS_HOST:

		service.h_host = (char *)args->arg.nss.host.name;
		service.h_serv = NULL;
		if ((_nderror = __classic_netdir_getbyname(nconf,
			    &service, &addrs)) != ND_OK) {
			*(res->nss.host.herrno_p) = nd2herrno(_nderror);
			return (_nderror);
		}
		/*
		 * convert addresses back into sockaddr for gethostbyname.
		 */
		ret = ndaddr2hent(AF_INET, service.h_host, addrs,
		    res->nss.host.hent, args->arg.nss.host.buf,
		    args->arg.nss.host.buflen);
		if (ret != ND_OK)
			*(res->nss.host.herrno_p) = nd2herrno(ret);
		netdir_free((char *)addrs, ND_ADDRLIST);
		_nderror = ret;
		return (ret);

		case NSS_SERV:

		if (args->arg.nss.serv.proto == NULL) {
			/*
			 * A similar HACK showed up in Solaris 2.3.
			 * The caller wild-carded proto -- i.e. will
			 * accept a match using tcp or udp for the port
			 * number. Since we have no hope of getting
			 * directly to a name service switch backend
			 * from here that understands this semantics,
			 * we try calling the netdir interfaces first
			 * with "tcp" and then "udp".
			 */
			args->arg.nss.serv.proto = "tcp";
			_nderror = _get_hostserv_inetnetdir_byname(nconf, args,
			    res);
			if (_nderror != ND_OK) {
				args->arg.nss.serv.proto = "udp";
				_nderror =
				    _get_hostserv_inetnetdir_byname(nconf,
				    args, res);
			}
			return (_nderror);
		}

		/*
		 * Third-parties should optimize their nametoaddr
		 * libraries for the HOST_SELF case.
		 */
		service.h_host = HOST_SELF;
		service.h_serv = (char *)args->arg.nss.serv.name;
		if ((_nderror = __classic_netdir_getbyname(nconf,
			    &service, &addrs)) != ND_OK) {
			return (_nderror);
		}
		/*
		 * convert addresses back into servent for getservbyname.
		 */
		_nderror = ndaddr2srent(service.h_serv,
		    args->arg.nss.serv.proto,
		    ((struct sockaddr_in *)addrs->n_addrs->buf)->sin_port,
		    res->nss.serv,
		    args->arg.nss.serv.buf, args->arg.nss.serv.buflen);
		netdir_free((char *)addrs, ND_ADDRLIST);
		return (_nderror);

		default:
		_nderror = ND_BADARG;
		return (ND_BADARG); /* should never happen */
	}
}

/*
 * gethostbyaddr/servbyport always call this function; if they call
 * with nametoaddr libs in nconf, we call netdir_getbyaddr
 * implementation __classic_netdir_getbyaddr, otherwise nsswitch.
 *
 * netdir_getbyaddr calls this only if nametoaddr libs are NOT
 * specified for inet transports; i.e. it's supposed to follow
 * the name service switch.
 */
int
_get_hostserv_inetnetdir_byaddr(nconf, args, res)
	struct	netconfig *nconf;
	struct	nss_netdirbyaddr_in *args;
	union	nss_netdirbyaddr_out *res;
{

	if (nconf == 0) {
		_nderror = ND_BADARG;
		return (_nderror);
	}

	/*
	 * 1. gethostbyaddr()/netdir_getbyaddr() special cases:
	 */
	switch (args->op_t) {

		case NSS_HOST:
		/*
		 * Worth the performance gain: assuming a lot of inet apps
		 * actively use "127.0.0.1".
		 */
		if (*(uint32_t *)(args->arg.nss.host.addr) ==
					htonl(INADDR_LOOPBACK)) {
			IN_SET_LOOPBACK_ADDR(&sa_con);
			_nderror = ndaddr2hent(AF_INET, LOCALHOST,
			    &nd_conaddrlist, res->nss.host.hent,
			    args->arg.nss.host.buf,
			    args->arg.nss.host.buflen);
			if (_nderror != ND_OK)
				*(res->nss.host.herrno_p) =
				    nd2herrno(_nderror);
			return (_nderror);
		}
		break;

		case NETDIR_BY:
		case NETDIR_BY_NOSRV:
		case NETDIR_BY6:
		case NETDIR_BY_NOSRV6:
		if (args->arg.nd_nbuf == 0) {
			_nderror = ND_BADARG;
			return (_nderror);
		}
		break;

	}

	/*
	 * 2. Most common scenario. This is the way we ship /etc/netconfig.
	 *    Emphasis on improving performance in the "if" part.
	 */
	if (nconf->nc_nlookups == 0) {
		struct hostent	*he;
		struct servent	*se = NULL;
		nss_XbyY_buf_t	*ndbuf4host = 0;
		nss_XbyY_buf_t	*ndbuf4serv = 0;
		char	*proto =
		    (strcmp(nconf->nc_proto, NC_TCP) == 0) ? NC_TCP : NC_UDP;
		struct	sockaddr_in *sa;
		struct sockaddr_in6 *sin6;
		int	h_errnop;

	switch (args->op_t) {

		case NSS_HOST:

		he = DOOR_GETHOSTBYADDR_R(args->arg.nss.host.addr,
		    args->arg.nss.host.len, args->arg.nss.host.type,
		    res->nss.host.hent, args->arg.nss.host.buf,
		    args->arg.nss.host.buflen,
		    res->nss.host.herrno_p);
		if (he == 0)
			_nderror = ND_NOHOST;
		else
			_nderror = ND_OK;
		return (_nderror);


		case NSS_HOST6:
		he = DOOR_GETIPNODEBYADDR_R(args->arg.nss.host.addr,
		    args->arg.nss.host.len, args->arg.nss.host.type,
		    res->nss.host.hent, args->arg.nss.host.buf,
		    args->arg.nss.host.buflen,
		    res->nss.host.herrno_p);

		if (he == 0)
			return (ND_NOHOST);
		return (ND_OK);


		case NSS_SERV:

		se = _switch_getservbyport_r(args->arg.nss.serv.port,
		    args->arg.nss.serv.proto,
		    res->nss.serv, args->arg.nss.serv.buf,
		    args->arg.nss.serv.buflen);

		if (se == 0)
			_nderror = ND_NOSERV;
		else
			_nderror = ND_OK;
		return (_nderror);

		case NETDIR_BY:
		case NETDIR_BY_NOSRV:

		GETSERVBUF(ndbuf4serv);
		if (ndbuf4serv == 0) {
			_nderror = ND_NOMEM;
			return (_nderror);
		}
		sa = (struct sockaddr_in *)(args->arg.nd_nbuf->buf);

		/*
		 * if NETDIR_BY_NOSRV or port == 0 skip the service
		 * lookup.
		 */
		if (args->op_t != NETDIR_BY_NOSRV && sa->sin_port != 0) {
			se = _switch_getservbyport_r(sa->sin_port, proto,
			    ndbuf4serv->result, ndbuf4serv->buffer,
				    ndbuf4serv->buflen);
			if (!se) {
				NSS_XbyY_FREE(&ndbuf4serv);
				/*
				 * We can live with this - i.e. the address
				 * does not
				 * belong to a well known service. The caller
				 * traditionally accepts a stringified port
				 * number
				 * as the service name. The state of se is used
				 * ahead to indicate the same.
				 * However, we do not tolerate this nonsense
				 * when we cannot get a host name. See below.
				 */
			}
		}

		GETHOSTBUF(ndbuf4host);
		if (ndbuf4host == 0) {
			_nderror = ND_NOMEM;
			return (_nderror);
		}
		he = DOOR_GETHOSTBYADDR_R((char *)&(sa->sin_addr.s_addr),
		    4, sa->sin_family, ndbuf4host->result, ndbuf4host->buffer,
		    ndbuf4host->buflen, &h_errnop);
		if (!he) {
			NSS_XbyY_FREE(&ndbuf4host);
			if (ndbuf4serv)
			    NSS_XbyY_FREE(&ndbuf4serv);
			_nderror = __herrno2netdir(h_errnop);
			return (_nderror);
		}
		/*
		 * Convert host names and service names into hostserv
		 * pairs. malloc's will be done, freed using netdir_free.
		 */
		h_errnop = hsents2ndhostservs(he, se,
		    sa->sin_port, res->nd_hslist);

		NSS_XbyY_FREE(&ndbuf4host);
		if (ndbuf4serv)
		    NSS_XbyY_FREE(&ndbuf4serv);
		_nderror = __herrno2netdir(h_errnop);
		return (_nderror);

		case NETDIR_BY6:
		case NETDIR_BY_NOSRV6:

		GETSERVBUF(ndbuf4serv);
		if (ndbuf4serv == 0) {
			_nderror = ND_NOMEM;
			return (ND_NOMEM);
		}
		sin6 = (struct sockaddr_in6 *)(args->arg.nd_nbuf->buf);

		/*
		 * if NETDIR_BY_NOSRV6 or port == 0 skip the service
		 * lookup.
		 */
		if (args->op_t != NETDIR_BY_NOSRV6 && sin6->sin6_port == 0) {
			se = _switch_getservbyport_r(sin6->sin6_port, proto,
			    ndbuf4serv->result, ndbuf4serv->buffer,
				    ndbuf4serv->buflen);
			if (!se) {
				NSS_XbyY_FREE(&ndbuf4serv);
				/*
				 * We can live with this - i.e. the address does
				 * not * belong to a well known service. The
				 * caller traditionally accepts a stringified
				 * port number
				 * as the service name. The state of se is used
				 * ahead to indicate the same.
				 * However, we do not tolerate this nonsense
				 * when we cannot get a host name. See below.
				 */
			}
		}

		GETHOSTBUF(ndbuf4host);
		if (ndbuf4host == 0) {
			_nderror = ND_NOMEM;
			return (_nderror);
		}
		he = DOOR_GETIPNODEBYADDR_R((char *)&(sin6->sin6_addr),
		    16, sin6->sin6_family, ndbuf4host->result,
			    ndbuf4host->buffer,
			    ndbuf4host->buflen, &h_errnop);
		if (!he) {
			NSS_XbyY_FREE(&ndbuf4host);
			if (ndbuf4serv)
			    NSS_XbyY_FREE(&ndbuf4serv);
			_nderror = __herrno2netdir(h_errnop);
			return (_nderror);
		}
		/*
		 * Convert host names and service names into hostserv
		 * pairs. malloc's will be done, freed using netdir_free.
		 */
		h_errnop = hsents2ndhostservs(he, se,
		    sin6->sin6_port, res->nd_hslist);

		NSS_XbyY_FREE(&ndbuf4host);
		if (ndbuf4serv)
		    NSS_XbyY_FREE(&ndbuf4serv);
		return (_nderror);

		default:
		_nderror = ND_BADARG;
		return (_nderror); /* should never happen */
	}

	}
	/*
	 * 3. We come this far only if nametoaddr libs are specified for
	 *    inet transports and we are called by gethost/servbyname only.
	 */
	switch (args->op_t) {
		struct	netbuf nbuf;
		struct	nd_hostservlist *addrs;
		struct	sockaddr_in sa;

		case NSS_HOST:

		sa.sin_addr.s_addr = *(uint32_t *)args->arg.nss.host.addr;
		sa.sin_family = AF_INET;
		/* Hopefully, third-parties get this optimization */
		sa.sin_port = 0;
		nbuf.buf = (char *)&sa;
		nbuf.len = nbuf.maxlen = sizeof (sa);
		if ((_nderror = __classic_netdir_getbyaddr(nconf,
			    &addrs, &nbuf)) != 0) {
			*(res->nss.host.herrno_p) = nd2herrno(_nderror);
			return (_nderror);
		}
		/*
		 * convert the host-serv pairs into h_aliases and hent.
		 */
		_nderror = ndhostserv2hent(&nbuf, addrs, res->nss.host.hent,
		    args->arg.nss.host.buf, args->arg.nss.host.buflen);
		if (_nderror != ND_OK)
			*(res->nss.host.herrno_p) = nd2herrno(_nderror);
		netdir_free((char *)addrs, ND_HOSTSERVLIST);
		return (_nderror);

		case NSS_SERV:

		if (args->arg.nss.serv.proto == NULL) {
			/*
			 * A similar HACK showed up in Solaris 2.3.
			 * The caller wild-carded proto -- i.e. will
			 * accept a match on tcp or udp for the port
			 * number. Since we have no hope of getting
			 * directly to a name service switch backend
			 * from here that understands this semantics,
			 * we try calling the netdir interfaces first
			 * with "tcp" and then "udp".
			 */
			args->arg.nss.serv.proto = "tcp";
			_nderror = _get_hostserv_inetnetdir_byaddr(nconf, args,
			    res);
			if (_nderror != ND_OK) {
				args->arg.nss.serv.proto = "udp";
				_nderror =
				    _get_hostserv_inetnetdir_byaddr(nconf,
					args, res);
			}
			return (_nderror);
		}

		/*
		 * Third-party nametoaddr_libs should be optimized for
		 * this case. It also gives a special semantics twist to
		 * netdir_getbyaddr. Only for the INADDR_ANY case, it gives
		 * higher priority to service lookups (over host lookups).
		 * If service lookup fails, the backend returns ND_NOSERV to
		 * facilitate lookup in the "next" naming service.
		 * BugId: 1075403.
		 */
		sa.sin_addr.s_addr = INADDR_ANY;
		sa.sin_family = AF_INET;
		sa.sin_port = (ushort_t)args->arg.nss.serv.port;
		sa.sin_zero[0] = '\0';
		nbuf.buf = (char *)&sa;
		nbuf.len = nbuf.maxlen = sizeof (sa);
		if ((_nderror = __classic_netdir_getbyaddr(nconf,
			    &addrs, &nbuf)) != ND_OK) {
			return (_nderror);
		}
		/*
		 * convert the host-serv pairs into s_aliases and servent.
		 */
		_nderror = ndhostserv2srent(args->arg.nss.serv.port,
		    args->arg.nss.serv.proto, addrs, res->nss.serv,
		    args->arg.nss.serv.buf, args->arg.nss.serv.buflen);
		netdir_free((char *)addrs, ND_HOSTSERVLIST);
		return (_nderror);

		default:
		_nderror = ND_BADARG;
		return (_nderror); /* should never happen */
	}
}

/*
 * Part II: Name Service Switch interfacing routines.
 */

static DEFINE_NSS_DB_ROOT(db_root_hosts);
static DEFINE_NSS_DB_ROOT(db_root_ipnodes);
static DEFINE_NSS_DB_ROOT(db_root_services);


/*
 * There is a copy of __nss2herrno() in nsswitch/files/gethostent.c.
 * It is there because /etc/lib/nss_files.so.1 cannot call
 * routines in libnsl.  Care should be taken to keep the two copies
 * in sync.
 */
int
__nss2herrno(nsstat)
	nss_status_t nsstat;
{
	switch (nsstat) {
	case NSS_SUCCESS:
		/* no macro-defined success code for h_errno */
		return (0);
	case NSS_NOTFOUND:
		return (HOST_NOT_FOUND);
	case NSS_TRYAGAIN:
		return (TRY_AGAIN);
	case NSS_UNAVAIL:
		return (NO_RECOVERY);
	}
	/* NOTREACHED */
}

nss_status_t
_herrno2nss(int h_errno)
{
	switch (h_errno) {
	case 0:
		return (NSS_SUCCESS);
	case TRY_AGAIN:
		return (NSS_TRYAGAIN);
	case NO_RECOVERY:
	case NETDB_INTERNAL:
		return (NSS_UNAVAIL);
	case HOST_NOT_FOUND:
	case NO_DATA:
	default:
		return (NSS_NOTFOUND);
	}
}

static int
__herrno2netdir(int h_errnop)
{
	switch (h_errnop) {
		case 0:
			return (ND_OK);
		case HOST_NOT_FOUND:
			return (ND_NOHOST);
		case TRY_AGAIN:
			return (ND_TRY_AGAIN);
		case NO_RECOVERY:
		case NETDB_INTERNAL:
			return (ND_NO_RECOVERY);
		case NO_DATA:
			return (ND_NO_DATA);
		default:
			return (ND_NOHOST);
	}
}

/*
 * The _switch_getXXbyYY_r() routines should be static.  They used to
 * be exported in SunOS 5.3, and in fact publicised as work-around
 * interfaces for getting CNAME/aliases, and therefore, we preserve
 * their signatures here. Just in case.
 */

struct hostent *
_switch_gethostbyname_r(name, result, buffer, buflen, h_errnop)
	const char	*name;
	struct hostent	*result;
	char		*buffer;
	int		buflen;
	int		*h_errnop;
{
	nss_XbyY_args_t arg;
	nss_status_t	res;

	trace2(TR__switch_gethostbyname_r, 0, buflen);
	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2hostent);
	arg.key.name	= name;
	arg.stayopen	= 0;
	res = nss_search(&db_root_hosts, _nss_initf_hosts,
	    NSS_DBOP_HOSTS_BYNAME, &arg);
	arg.status = res;
	*h_errnop = arg.h_errno;
	trace2(TR__switch_gethostbyname_r, 1, buflen);
	return (struct hostent *)NSS_XbyY_FINI(&arg);
}

struct hostent *
_switch_getipnodebyname_r(name, result, buffer, buflen, h_errnop)
	const char	*name;
	struct hostent	*result;
	char		*buffer;
	int		buflen;
	int		*h_errnop;
{
	nss_XbyY_args_t arg;
	nss_status_t	res;

	trace2(TR__switch_getipnodebyname_r, 0, buflen);
	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2hostent6);
	arg.key.name	= name;
	arg.stayopen	= 0;
	res = nss_search(&db_root_ipnodes, _nss_initf_ipnodes,
	    NSS_DBOP_IPNODES_BYNAME, &arg);
	arg.status = res;
	*h_errnop = arg.h_errno;
	trace2(TR__switch_getipnodebyname_r, 1, buflen);
	return (struct hostent *)NSS_XbyY_FINI(&arg);
}

struct hostent *
_switch_gethostbyaddr_r(addr, len, type, result, buffer, buflen, h_errnop)
	const char	*addr;
	int		len;
	int		type;
	struct hostent	*result;
	char		*buffer;
	int		buflen;
	int		*h_errnop;
{
	nss_XbyY_args_t arg;
	nss_status_t	res;

	trace3(TR__switch_gethostbyaddr_r, 0, len, buflen);
	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2hostent);
	arg.key.hostaddr.addr	= addr;
	arg.key.hostaddr.len	= len;
	arg.key.hostaddr.type	= type;
	arg.stayopen		= 0;
	res = nss_search(&db_root_hosts, _nss_initf_hosts,
	    NSS_DBOP_HOSTS_BYADDR, &arg);
	arg.status = res;
	*h_errnop = arg.h_errno;
	trace3(TR__switch_gethostbyaddr_r, 1, len, buflen);
	return (struct hostent *)NSS_XbyY_FINI(&arg);
}

struct hostent *
_switch_getipnodebyaddr_r(addr, len, type, result, buffer, buflen, h_errnop)
	const char	*addr;
	int		len;
	int		type;
	struct hostent	*result;
	char		*buffer;
	int		buflen;
	int		*h_errnop;
{
	nss_XbyY_args_t arg;
	nss_status_t	res;

	trace3(TR__switch_getipnodebyaddr_r, 0, len, buflen);
	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2hostent6);
	arg.key.hostaddr.addr	= addr;
	arg.key.hostaddr.len	= len;
	arg.key.hostaddr.type	= type;
	arg.stayopen		= 0;
	res = nss_search(&db_root_ipnodes, _nss_initf_ipnodes,
	    NSS_DBOP_IPNODES_BYADDR, &arg);
	arg.status = res;
	*h_errnop = arg.h_errno;
	trace3(TR__switch_getipnodebyaddr_r, 1, len, buflen);
	return (struct hostent *)NSS_XbyY_FINI(&arg);
}

static void
_nss_initf_services(p)
	nss_db_params_t	*p;
{
	/* === need tracepoints */
	p->name	= NSS_DBNAM_SERVICES;
	p->default_config = NSS_DEFCONF_SERVICES;
}

struct servent *
_switch_getservbyname_r(name, proto, result, buffer, buflen)
	const char	*name;
	const char	*proto;
	struct servent	*result;
	char		*buffer;
	int		buflen;
{
	nss_XbyY_args_t arg;
	nss_status_t	res;

	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2servent);
	arg.key.serv.serv.name	= name;
	arg.key.serv.proto	= proto;
	arg.stayopen		= 0;
	res = nss_search(&db_root_services, _nss_initf_services,
	    NSS_DBOP_SERVICES_BYNAME, &arg);
	arg.status = res;
	return ((struct servent *)NSS_XbyY_FINI(&arg));
}

struct servent *
_switch_getservbyport_r(port, proto, result, buffer, buflen)
	int		port;
	const char	*proto;
	struct servent	*result;
	char		*buffer;
	int		buflen;
{
	nss_XbyY_args_t arg;
	nss_status_t	res;

	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2servent);
	arg.key.serv.serv.port	= port;
	arg.key.serv.proto	= proto;
	arg.stayopen		= 0;
	res = nss_search(&db_root_services, _nss_initf_services,
	    NSS_DBOP_SERVICES_BYPORT, &arg);
	arg.status = res;
	return ((struct servent *)NSS_XbyY_FINI(&arg));
}


/*
 * Return values: 0 = success, 1 = parse error, 2 = erange ...
 * The structure pointer passed in is a structure in the caller's space
 * wherein the field pointers would be set to areas in the buffer if
 * need be. instring and buffer should be separate areas.
 *
 * Defined here because we need it and we (libnsl) cannot have a dependency
 * on libsocket (however, libsocket always depends on libnsl).
 */
int
str2servent(instr, lenstr, ent, buffer, buflen)
	const char	*instr;
	int		lenstr;
	void	*ent; /* really (struct servnet *) */
	char	*buffer;
	int	buflen;
{
	struct servent	*serv	= (struct servent *)ent;
	const char	*p, *fieldstart, *limit, *namestart;
	ssize_t		fieldlen, namelen = 0;
	char		numbuf[12];
	char		*numend;

	if ((instr >= buffer && (buffer + buflen) > instr) ||
	    (buffer >= instr && (instr + lenstr) > buffer)) {
		return (NSS_STR_PARSE_PARSE);
	}

	p = instr;
	limit = p + lenstr;

	while (p < limit && isspace(*p)) {
		p++;
	}
	namestart = p;
	while (p < limit && !isspace(*p)) {
		p++;		/* Skip over the canonical name */
	}
	namelen = p - namestart;

	if (buflen <= namelen) { /* not enough buffer */
		return (NSS_STR_PARSE_ERANGE);
	}
	(void) memcpy(buffer, namestart, namelen);
	buffer[namelen] = '\0';
	serv->s_name = buffer;

	while (p < limit && isspace(*p)) {
		p++;
	}

	fieldstart = p;
	do {
		if (p > limit || isspace(*p)) {
			/* Syntax error -- no port/proto */
			return (NSS_STR_PARSE_PARSE);
		}
	}
	while (*p++ != '/');
	fieldlen = p - fieldstart - 1;
	if (fieldlen == 0 || fieldlen >= sizeof (numbuf)) {
		/* Syntax error -- supposed number is empty or too long */
		return (NSS_STR_PARSE_PARSE);
	}
	(void) memcpy(numbuf, fieldstart, fieldlen);
	numbuf[fieldlen] = '\0';
	serv->s_port = htons((int)strtol(numbuf, &numend, 10));
	if (*numend != '\0') {
		/* Syntax error -- port number isn't a number */
		return (NSS_STR_PARSE_PARSE);
	}

	fieldstart = p;
	while (p < limit && !isspace(*p)) {
		p++;		/* Scan the protocol name */
	}
	fieldlen = p - fieldstart + 1;		/* Include '\0' this time */
	if (fieldlen > buflen - namelen - 1) {
		return (NSS_STR_PARSE_ERANGE);
	}
	serv->s_proto = buffer + namelen + 1;
	(void) memcpy(serv->s_proto, fieldstart, fieldlen - 1);
	serv->s_proto[fieldlen - 1] = '\0';

	while (p < limit && isspace(*p)) {
		p++;
	}
	/*
	 * Although nss_files_XY_all calls us with # stripped,
	 * we should be able to deal with it here in order to
	 * be more useful.
	 */
	if (p >= limit || *p == '#') { /* no aliases, no problem */
		char **ptr;

		ptr = (char **)ROUND_UP(buffer + namelen + 1 + fieldlen,
		    sizeof (char *));
		if ((char *)ptr >= buffer + buflen) {
			/* hope they don't try to peek in */
			serv->s_aliases = 0;
			return (NSS_STR_PARSE_ERANGE);
		} else {
			*ptr = 0;
			serv->s_aliases = ptr;
			return (NSS_STR_PARSE_SUCCESS);
		}
	}
	serv->s_aliases = _nss_netdb_aliases(p, (int)(lenstr - (p - instr)),
	    buffer + namelen + 1 + fieldlen,
	    (int)(buflen - namelen - 1 - fieldlen));
	return (NSS_STR_PARSE_SUCCESS);
}

/*
 * Part III: All `n sundry routines that are useful only in this
 * module. In the interest of keeping this source file shorter,
 * we would create them a new module only if the linker allowed
 * "library-static" functions.
 *
 * Routines to order addresses based on local interfaces and netmasks,
 * to get and check reserved ports, and to get broadcast nets.
 */

union __v4v6addr {
	struct in6_addr	in6;
	struct in_addr	in4;
};

struct __ifaddr {
	sa_family_t		af;
	union __v4v6addr	addr;
	union __v4v6addr	mask;
};

struct ifinfo {
	int		count;
	struct __ifaddr	*addresses;
};

typedef enum {IF_ADDR, IF_MASK}	__ifaddr_type;
static int	__inet_ifassign(sa_family_t, struct __ifaddr *, __ifaddr_type,
				void *);
int		__inet_address_is_local_af(void *, sa_family_t, void *);

#define	ifaf(index)	(localinfo->addresses[index].af)
#define	ifaddr4(index)	(localinfo->addresses[index].addr.in4)
#define	ifaddr6(index)	(localinfo->addresses[index].addr.in6)
#define	ifmask4(index)	(localinfo->addresses[index].mask.in4)
#define	ifmask6(index)	(localinfo->addresses[index].mask.in6)
#define	ifinfosize(n)	(sizeof (struct ifinfo) + (n)*sizeof (struct __ifaddr))

#define	lifraddrp(lifr)	((lifr.lifr_addr.ss_family == AF_INET6) ? \
	(void *)&((struct sockaddr_in6 *)&lifr.lifr_addr)->sin6_addr : \
	(void *)&((struct sockaddr_in *)&lifr.lifr_addr)->sin_addr)

#define	ifassign(lifr, index, type) \
			__inet_ifassign(lifr.lifr_addr.ss_family, \
				&localinfo->addresses[index], type, \
				lifraddrp(lifr))

#define	IFINFOTIMEOUT	300

/*
 * Move any local addresses toward the beginning of haddrlist,
 * and make sure IPv6 addresses (except mapped ones) precede
 * IPv4 (mapped) addresses. The order within the subclasses
 * (local IPv6, non-local IPv6, local IPv4, non-local IPv4) is
 * preserved.
 *
 * Order haddrlist in place if res_salist is NULL, otherwise malloc it
 * under res_salist.  Caller is responsible for freeing this storage.
 *
 * The interface list is retrieved no more often than every
 * IFINFOTIMEOUT seconds. Access to the interface list is protected
 * by an RW lock.
 */
int
order_haddrlist_af(sa_family_t af, char **haddrlist, void *res_salist)
{
	static struct		ifinfo *localinfo = 0;
	static struct timeval	then = {0, 0};
	struct timeval		now;
	static rwlock_t		localinfo_lock = DEFAULTRWLOCK;
	int	num;
	char	**t;
	int	inplace = (res_salist == 0);
	int	*sortclass;
	int	classcount[4];
	int	i;

	for (num = 0, t = haddrlist; *t; num++, t++);

	/*
	 * Trivial cases.
	 */
	if (num == 0)
		return (ND_OK);
	else if (num == 1) {
		if (inplace)
			return (ND_OK);
		else {
			void	*sa, *a;
			int	len, salen;

			if (af == AF_INET6) {
				salen = sizeof (struct sockaddr_in6);
				len = sizeof (struct in6_addr);
			} else {
				salen = sizeof (struct sockaddr_in);
				len = sizeof (struct in_addr);
			}
			if ((sa = calloc(1, salen)) == 0)
				return (ND_NOMEM);
			if (af == AF_INET6)
				a = &(((struct sockaddr_in6 *)sa)->sin6_addr);
			else
				a = &(((struct sockaddr_in *)sa)->sin_addr);
			memcpy(a, haddrlist[0], len);
			*((struct sockaddr_in6 **)res_salist) = sa;
			return (ND_OK);
		}
	}

	/*
	 * Get a read lock, and check if the interface information
	 * is too old.
	 */
	(void) rw_rdlock(&localinfo_lock);
	(void) gettimeofday(&now, 0);
	if ((now.tv_sec - then.tv_sec) > IFINFOTIMEOUT) {
		/* Need to update I/F info. Upgrade to write lock. */
		(void) rw_unlock(&localinfo_lock);
		(void) rw_wrlock(&localinfo_lock);
		/*
		 * Another thread might have updated already, so
		 * re-check the timeout.
		 */
		if ((now.tv_sec - then.tv_sec) > IFINFOTIMEOUT) {
			if (localinfo != 0)
				free(localinfo);
			if ((localinfo = get_local_info()) == 0) {
				(void) rw_unlock(&localinfo_lock);
				return (ND_NOMEM);
			}
			then = now;
		}
		/* Downgrade to read lock */
		(void) rw_unlock(&localinfo_lock);
		(void) rw_rdlock(&localinfo_lock);
		/*
		 * Another thread may have updated the I/F info,
		 * so verify that the 'localinfo' pointer still
		 * is non-NULL.
		 */
		if (localinfo == 0) {
			(void) rw_unlock(&localinfo_lock);
			return (ND_NOMEM);
		}
	}

	/*
	 * Classify addesses:
	 *	0	Local IPv6
	 *	1	Non-local IPv6
	 *	2	Local IPv4 (including mapped IPv6)
	 *	3	Non-local IPv4 (including mapped IPv6)
	 *
	 * We also maintain the classcount array to keep track of the
	 * number of addresses in each class.
	 */
	if ((sortclass = malloc(num * sizeof (sortclass[0]))) == 0) {
		(void) rw_unlock(&localinfo_lock);
		return (ND_NOMEM);
	}
	memset(classcount, 0, sizeof (classcount));
	for (i = 0, t = haddrlist; i < num; t++, i++) {
		if (af == AF_INET6 &&
			!IN6_IS_ADDR_V4MAPPED((struct in6_addr *)*t))
			sortclass[i] = 0;
		else
			sortclass[i] = 2;
		if (!__inet_address_is_local_af(localinfo, af, *t))
			sortclass[i]++;
		classcount[sortclass[i]]++;
	}

	/* Don't need the interface list anymore in this call */
	(void) rw_unlock(&localinfo_lock);

	if (inplace) {
		char	**classnext[4];
		int	rc;

		if ((t = malloc(num * sizeof (t[0]))) == 0) {
			free(sortclass);
			return (ND_NOMEM);
		}


		/*
		 * Each element in the classnext array points to
		 * the next element for that class in the sorted
		 * address list. 'rc' is the running count of
		 * elements as we sum the class sub-totals.
		 */
		for (rc = 0, i = 0; i < 4; i++) {
			classnext[i] = &t[rc];
			rc += classcount[i];
		}

		/* Now for the actual rearrangement of the addresses */
		for (i = 0; i < num; i++) {
			*(classnext[sortclass[i]]) = haddrlist[i];
			classnext[sortclass[i]]++;
		}

		/* Copy the sorted list to haddrlist */
		(void) memcpy(haddrlist, t, num * sizeof (t[0]));
		free(t);
	} else {
		void			*sa;
		struct sockaddr_in	*sa4;
		struct sockaddr_in6	*sa6;
		int			salen, len;
		void			*classnext[4];
		int			rc;

		if (af == AF_INET6) {
			salen = sizeof (struct sockaddr_in6);
			len = sizeof (struct in6_addr);
		} else {
			salen = sizeof (struct sockaddr_in);
			len = sizeof (struct in_addr);
		}

		if ((sa = calloc(num, salen)) == 0) {
			free(sortclass);
			return (ND_NOMEM);
		}

		/* Compute the start of each address class in sa? */
		if (af == AF_INET6) {
			*((struct sockaddr_in6 **)res_salist) = sa6 = sa;
			for (rc = 0, i = 0; i < 4; i++) {
				classnext[i] = &sa6[rc];
				rc += classcount[i];
			}
		} else {
			*((struct sockaddr_in **)res_salist) = sa4 = sa;
			for (rc = 0, i = 0; i < 4; i++) {
				classnext[i] = &sa4[rc];
				rc += classcount[i];
			}
		}

		/* Rearrange */
		for (i = 0; i < num; i++) {
			void	*addr;
			if (af == AF_INET6) {
				sa6 = classnext[sortclass[i]];
				addr = &sa6->sin6_addr;
				classnext[sortclass[i]] = ++sa6;
			} else {
				sa4 = classnext[sortclass[i]];
				addr = &sa4->sin_addr;
				classnext[sortclass[i]] = ++sa4;
			}
			memcpy(addr, haddrlist[i], len);
		}
	}

	free(sortclass);

	return (ND_OK);
}


int
order_haddrlist(char **haddrlist, struct sockaddr_in **res_salist)
{
	return (order_haddrlist_af(AF_INET, haddrlist, res_salist));
}

/*
 * Given an haddrlist and a port number, mallocs and populates
 * a new nd_addrlist with ordered addresses.
 */
int
hent2ndaddr(af, haddrlist, servp, nd_alist)
	int af;
	char	**haddrlist;
	int	*servp;
	struct	nd_addrlist **nd_alist;
{
	struct	nd_addrlist *result;
	int		num, ret, i;
	char    **t;
	struct	netbuf *na;
	struct	sockaddr_in *sa;
	struct	sockaddr_in6 *sin6;
	void	*sabuf;

	result = (struct nd_addrlist *)malloc(sizeof (struct nd_addrlist));
	if (result == 0)
		return (ND_NOMEM);

	/* Address count */
	for (num = 0, t = haddrlist; *t; t++, num++);

	result->n_cnt = num;
	result->n_addrs = (struct netbuf *)calloc(num, sizeof (struct netbuf));
	if (result->n_addrs == 0) {
		free(result);
		return (ND_NOMEM);
	}

	ret = order_haddrlist_af((sa_family_t)af, haddrlist, &sabuf);
	if (ret != 0) {
		free(result->n_addrs);
		free(result);
		return (ret);
	}

	na = result->n_addrs;
	if (af == AF_INET) {
		sa = sabuf;
		for (i = 0; i < num; i++, na++, sa++) {
			na->len = na->maxlen = sizeof (struct sockaddr_in);
			na->buf = (char *)sa;
			sa->sin_family = AF_INET;
			sa->sin_port = *servp;
		}
	} else if (af == AF_INET6) {
		int arg_processed = 0;
		sin6 = sabuf;
		/* Skip leading mapped addresses */
		for (i = 0; i < num; i++, sin6++) {
			if (!IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
				break;
		}
		if (i > 0) {
			/*
			 * Make sure that first element in sabuf isn't
			 * a mapped address.
			 */
			num = num - i;
			if (num > 0)
				memmove(sabuf, sin6, num *
				    sizeof (struct sockaddr_in6));
			sin6 = sabuf;
		}
		for (i = 0; i < num; i++, na++, sin6++) {

			/*
			 * Skip all the mapped V4 address
			 */
			if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
				continue;
			}
			na->len = na->maxlen = sizeof (struct sockaddr_in6);
			na->buf = (char *)sin6;
			sin6->sin6_family = AF_INET6;
			sin6->sin6_port = *servp;
			arg_processed++;
		}
		/*
		 * No real IPV6 address has been processed. This can happen
		 * when V4 server address appears as mapped V6 address.
		 * We don't use it.
		 */
		if (arg_processed == 0) {
			free(sabuf);
			free(result->n_addrs);
			free(result);
			return (ND_NOHOST);
		}
		result->n_cnt = arg_processed;
	}
	*(nd_alist) = result;
	return (ND_OK);
}

/*
 * Given a hostent and a servent, mallocs and populates
 * a new nd_hostservlist with host and service names.
 *
 * We could be passed in a NULL servent, in which case stringify port.
 */
int
hsents2ndhostservs(struct hostent *he, struct servent *se,
    ushort_t port, struct nd_hostservlist **hslist)
{
	struct	nd_hostservlist *result;
	struct	nd_hostserv *hs;
	int	hosts, servs, i, j;
	char	**hn, **sn;

	if ((result = (struct nd_hostservlist *)
		    malloc(sizeof (struct nd_hostservlist))) == 0)
		return (ND_NOMEM);

	/*
	 * We initialize the counters to 1 rather than zero because
	 * we have to count the "official" name as well as the aliases.
	 */
	for (hn = he->h_aliases, hosts = 1; hn && *hn; hn++, hosts++);
	if (se)
		for (sn = se->s_aliases, servs = 1; sn && *sn; sn++, servs++);
	else
		servs = 1;

	if ((hs = (struct nd_hostserv *)calloc(hosts * servs,
			sizeof (struct nd_hostserv))) == 0)
		return (ND_NOMEM);

	result->h_cnt	= servs * hosts;
	result->h_hostservs = hs;

	for (i = 0, hn = he->h_aliases; i < hosts; i++) {
		sn = se ? se->s_aliases : NULL;

		for (j = 0; j < servs; j++) {
			if (i == 0)
				hs->h_host = strdup(he->h_name);
			else
				hs->h_host = strdup(*hn);
			if (j == 0) {
				if (se)
					hs->h_serv = strdup(se->s_name);
				else {
					/* Convert to a number string */
					char stmp[16];

					(void) sprintf(stmp, "%d", port);
					hs->h_serv = strdup(stmp);
				}
			} else
				hs->h_serv = strdup(*sn++);

			if ((hs->h_host == 0) || (hs->h_serv == 0)) {
				free((void *)result->h_hostservs);
				free((void *)result);
				return (ND_NOMEM);
			}
			hs++;
		}
		if (i)
			hn++;
	}
	*(hslist) = result;
	return (ND_OK);
}

/*
 * Process results from nd_addrlist ( returned by netdir_getbyname)
 * into a hostent using buf.
 * *** ASSUMES that nd_addrlist->n_addrs->buf contains IP addresses in
 * sockaddr_in's ***
 */
int
ndaddr2hent(af, nam, addrs, result, buffer, buflen)
	int	af;
	const	char *nam;
	struct	nd_addrlist *addrs;
	struct	hostent *result;
	char	*buffer;
	int	buflen;
{
	int	i, count;
	struct	in_addr *addrp;
	struct	in6_addr *addr6p;
	char	**addrvec;
	struct	netbuf *na;
	size_t	len;

	result->h_name		= buffer;
	result->h_addrtype	= af;
	result->h_length	= (af == AF_INET) ? sizeof (*addrp):
						    sizeof (*addr6p);

	/*
	 * Build addrlist at start of buffer (after name);  store the
	 * addresses themselves at the end of the buffer.
	 */
	len = strlen(nam) + 1;
	addrvec = (char **)ROUND_UP(buffer + len, sizeof (*addrvec));
	result->h_addr_list 	= addrvec;

	if (af == AF_INET) {
		addrp = (struct in_addr *)ROUND_DOWN(buffer + buflen,
		    sizeof (*addrp));

		count = addrs->n_cnt;
		if ((char *)(&addrvec[count + 1]) > (char *)(&addrp[-count]))
			return (ND_NOMEM);

		(void) memcpy(buffer, nam, len);

		for (na = addrs->n_addrs, i = 0;  i < count;  na++, i++) {
			--addrp;
			(void) memcpy(addrp,
			    &((struct sockaddr_in *)na->buf)->sin_addr,
			    sizeof (*addrp));
			*addrvec++ = (char *)addrp;
		}
	} else {
		addr6p = (struct in6_addr *)ROUND_DOWN(buffer + buflen,
			sizeof (*addr6p));

		count = addrs->n_cnt;
		if ((char *)(&addrvec[count + 1]) > (char *)(&addr6p[-count]))
			return (ND_NOMEM);

		(void) memcpy(buffer, nam, len);

		for (na = addrs->n_addrs, i = 0;  i < count;  na++, i++) {
			--addr6p;
			(void) memcpy(addr6p,
			    &((struct sockaddr_in6 *)na->buf)->sin6_addr,
			    sizeof (*addr6p));
			*addrvec++ = (char *)addr6p;
		}
	}
	*addrvec = 0;
	result->h_aliases = addrvec;

	return (ND_OK);
}

/*
 * Process results from nd_addrlist ( returned by netdir_getbyname)
 * into a servent using buf.
 */
int
ndaddr2srent(const char *name, const char *proto, ushort_t port,
    struct servent *result, char *buffer, int buflen)
{
	size_t	i;
	char	*bufend = (buffer + buflen);

	result->s_port = (int)port;

	result->s_aliases =
	    (char **)ROUND_UP(buffer, sizeof (char *));
	result->s_aliases[0] = NULL;
	buffer = (char *)&result->s_aliases[1];
	result->s_name = buffer;
	i = strlen(name) + 1;
	if ((buffer + i) > bufend)
		return (ND_NOMEM);
	(void) memcpy(buffer, name, i);
	buffer += i;

	result->s_proto	= buffer;
	i = strlen(proto) + 1;
	if ((buffer + i) > bufend)
		return (ND_NOMEM);
	(void) memcpy(buffer, proto, i);
	buffer += i;

	return (ND_OK);
}

/*
 * Process results from nd_hostservlist ( returned by netdir_getbyaddr)
 * into a hostent using buf.
 * *** ASSUMES that nd_buf->buf is a sockaddr_in ***
 */
int
ndhostserv2hent(nbuf, addrs, result, buffer, buflen)
	struct	netbuf *nbuf;
	struct	nd_hostservlist *addrs;
	struct	hostent *result;
	char	*buffer;
	int	buflen;
{
	int	i, count;
	char	*aliasp;
	char	**aliasvec;
	struct	sockaddr_in *sa;
	struct	nd_hostserv *hs;
	const	char *la;
	size_t	length;

	/* First, give the lonely address a specious home in h_addr_list. */
	aliasp   = (char  *)ROUND_UP(buffer, sizeof (sa->sin_addr));
	sa = (struct sockaddr_in *)nbuf->buf;
	(void) memcpy(aliasp, (char *)&(sa->sin_addr), sizeof (sa->sin_addr));
	aliasvec = (char **)ROUND_UP(aliasp + sizeof (sa->sin_addr),
		sizeof (*aliasvec));
	result->h_addr_list = aliasvec;
	*aliasvec++ = aliasp;
	*aliasvec++ = 0;

	/*
	 * Build h_aliases at start of buffer (after addr and h_addr_list);
	 * store the aliase strings at the end of the buffer (before h_name).
	 */

	aliasp = buffer + buflen;

	result->h_aliases	= aliasvec;

	hs = addrs->h_hostservs;
	if (! hs)
		return (ND_NOHOST);

	length = strlen(hs->h_host) + 1;
	aliasp -= length;
	if ((char *)(&aliasvec[1]) > aliasp)
		return (ND_NOMEM);
	(void) memcpy(aliasp, hs->h_host, length);

	result->h_name		= aliasp;
	result->h_addrtype	= AF_INET;
	result->h_length	= sizeof (sa->sin_addr);

	/*
	 * Assumption: the netdir nametoaddr_libs
	 * sort the vector of (host, serv) pairs in such a way that
	 * all pairs with the same host name are contiguous.
	 */
	la = hs->h_host;
	count = addrs->h_cnt;
	for (i = 0;  i < count;  i++, hs++)
		if (strcmp(la, hs->h_host) != 0) {
			size_t len = strlen(hs->h_host) + 1;

			aliasp -= len;
			if ((char *)(&aliasvec[2]) > aliasp)
				return (ND_NOMEM);
			(void) memcpy(aliasp, hs->h_host, len);
			*aliasvec++ = aliasp;
			la = hs->h_host;
		}
	*aliasvec = 0;

	return (ND_OK);
}

/*
 * Process results from nd_hostservlist ( returned by netdir_getbyaddr)
 * into a servent using buf.
 */
int
ndhostserv2srent(port, proto, addrs, result, buffer, buflen)
	int	port;
	const	char *proto;
	struct	nd_hostservlist *addrs;
	struct	servent *result;
	char	*buffer;
	int	buflen;
{
	int	i, count;
	char	*aliasp;
	char	**aliasvec;
	struct	nd_hostserv *hs;
	const	char *host_cname;
	size_t	leni, lenj;

	result->s_port = port;
	/*
	 * Build s_aliases at start of buffer;
	 * store proto and aliases at the end of the buffer (before h_name).
	 */

	aliasp = buffer + buflen;
	aliasvec = (char **)ROUND_UP(buffer, sizeof (char *));

	result->s_aliases	= aliasvec;

	hs = addrs->h_hostservs;
	if (! hs)
		return (ND_NOHOST);
	host_cname = hs->h_host;

	leni = strlen(proto) + 1;
	lenj = strlen(hs->h_serv) + 1;
	if ((char *)(&aliasvec[2]) > (aliasp - leni - lenj))
		return (ND_NOMEM);

	aliasp -= leni;
	(void) memcpy(aliasp, proto, leni);
	result->s_proto = aliasp;

	aliasp -= lenj;
	(void) memcpy(aliasp, hs->h_serv, lenj);
	result->s_name = aliasp;

	/*
	 * Assumption: the netdir nametoaddr_libs
	 * do a host aliases first and serv aliases next
	 * enumeration for creating the list of hostserv
	 * structures.
	 */
	count = addrs->h_cnt;
	for (i = 0;
	    i < count && hs->h_serv && strcmp(hs->h_host, host_cname) == 0;
	    i++, hs++) {
		size_t len = strlen(hs->h_serv) + 1;

		aliasp -= len;
		if ((char *)(&aliasvec[2]) > aliasp)
			return (ND_NOMEM);
		(void) memcpy(aliasp, hs->h_serv, len);
		*aliasvec++ = aliasp;
	}
	*aliasvec = NULL;

	return (ND_OK);
}


static int
nd2herrno(nerr)
int nerr;
{
	trace1(TR_nd2herrno, 0);
	switch (nerr) {
	case ND_OK:
		trace1(TR_nd2herrno, 1);
		return (0);
	case ND_TRY_AGAIN:
		trace1(TR_nd2herrno, 1);
		return (TRY_AGAIN);
	case ND_NO_RECOVERY:
	case ND_BADARG:
	case ND_NOMEM:
		trace1(TR_nd2herrno, 1);
		return (NO_RECOVERY);
	case ND_NO_DATA:
		trace1(TR_nd2herrno, 1);
		return (NO_DATA);
	case ND_NOHOST:
	case ND_NOSERV:
		trace1(TR_nd2herrno, 1);
		return (HOST_NOT_FOUND);
	default:
		trace1(TR_nd2herrno, 1);
		return (NO_RECOVERY);
	}
}

#define	MAXIFS 32
#define	UDP4	"/dev/udp"
#define	UDP6	"/dev/udp6"

static struct ifinfo *
get_local_info()
{
	int numifs;
	char	*buf;
	struct lifconf lifc;
	struct lifreq lifreq, *lifr;
	struct lifnum lifn;
	int fd, fd4, fd6;
	struct ifinfo *localinfo;
	int n;

	fd4 = open(UDP4, O_RDONLY);
	fd6 = open(UDP6, O_RDONLY);

	if (fd4 < 0 && fd6 < 0) {
		(void) syslog(LOG_ERR,
	    "n2a get_local_info: open to get interface configuration: %m");
		_nderror = ND_OPEN;
		(void) close(fd4);
		(void) close(fd6);
		return ((struct ifinfo *)NULL);
	}

	lifn.lifn_family = AF_UNSPEC;
	lifn.lifn_flags = 0;
	if (ioctl(fd = (fd6 >= 0)?fd6:fd4, SIOCGLIFNUM, (char *)&lifn) < 0) {
		perror("ioctl (get number of interfaces)");
		numifs = MAXIFS;
	} else {
		numifs = lifn.lifn_count;
	}

	buf = (char *)malloc(numifs * sizeof (lifreq));
	if (buf == NULL) {
		(void) syslog(LOG_ERR, "n2a get_local_info: malloc failed: %m");
		(void) close(fd4);
		(void) close(fd6);
		_nderror = ND_NOMEM;
		return ((struct ifinfo *)NULL);
	}
	lifc.lifc_family = AF_UNSPEC;
	lifc.lifc_flags = 0;
	lifc.lifc_len = numifs * sizeof (lifreq);
	lifc.lifc_buf = buf;
	if (ioctl(fd, SIOCGLIFCONF, (char *)&lifc) < 0) {
		(void) syslog(LOG_ERR,
	    "n2a get_local_info: ioctl (get interface configuration): %m");
		(void) close(fd4);
		(void) close(fd6);
		free(buf);
		_nderror = ND_SYSTEM;
		return ((struct ifinfo *)NULL);
	}
	lifr = (struct lifreq *)buf;
	numifs = lifc.lifc_len/sizeof (lifreq);
	localinfo = (struct ifinfo *)malloc(ifinfosize(numifs));
	if (localinfo == NULL) {
		(void) syslog(LOG_ERR, "n2a get_local_info: malloc failed: %m");
		(void) close(fd4);
		(void) close(fd6);
		free(buf);
		_nderror = ND_SYSTEM;
		return ((struct ifinfo *)NULL);
	}

	localinfo->addresses = (struct __ifaddr *)
				((char *)localinfo + sizeof (struct ifinfo));

	for (localinfo->count = 0, n = numifs; n > 0; n--, lifr++) {
		uint_t lifrflags;

		lifreq = *lifr;

		/* Squirrel away the address */
		if (ifassign(lifreq, localinfo->count, IF_ADDR) == 0)
			continue;

		fd = (ifaf(localinfo->count) == AF_INET6) ? fd6 : fd4;
		if (fd < 0)
			continue;

		if (ioctl(fd, SIOCGLIFFLAGS, (char *)&lifreq) < 0) {
			(void) syslog(LOG_ERR,
		    "n2a get_local_info: ioctl (get interface flags): %m");
			continue;
		}
		lifrflags = lifreq.lifr_flags;
		if (((lifrflags & IFF_UP) == 0))
			continue;

		if (ioctl(fd, SIOCGLIFNETMASK, (char *)&lifreq) < 0) {
			(void) syslog(LOG_ERR,
	    "n2a get_local_info: ioctl (get interface netmask): %m");
			continue;
		}

		if (ifassign(lifreq, localinfo->count, IF_MASK) == 0)
			continue;

		localinfo->count++;
	}

	free(buf);
	(void) close(fd4);
	(void) close(fd6);
	return (localinfo);
}

static int
__inet_ifassign(sa_family_t af, struct __ifaddr *ifa, __ifaddr_type type,
		void *addr) {

	switch (type) {
	case IF_ADDR:
		ifa->af = af;
		if (af == AF_INET6) {
			ifa->addr.in6 = *(struct in6_addr *)addr;
		} else {
			ifa->addr.in4 = *(struct in_addr *)addr;
		}
		break;
	case IF_MASK:
		if (ifa->af == af) {
			if (af == AF_INET6) {
				ifa->mask.in6 = *(struct in6_addr *)addr;
			} else {
				ifa->mask.in4 = *(struct in_addr *)addr;
			}
		} else {
			return (0);
		}
		break;
	default:
		return (0);
	}

	return (1);
}

static int
islocal(localinfo, addr)
struct ifinfo *localinfo;
struct in_addr addr;
{
	int i;

	if (!localinfo)
	    return (0);

	for (i = 0; i < localinfo->count; i++) {
		if (ifaf(i) == AF_INET &&
				((addr.s_addr & ifmask4(i).s_addr) ==
				(ifaddr4(i).s_addr & ifmask4(i).s_addr)))
			return (1);
	}
	return (0);
}

/*
 *  Some higher-level routines for determining if an address is
 *  on a local network.
 *
 *      __inet_get_local_interfaces() - get an opaque handle with
 *          with a list of local interfaces
 *      __inet_address_is_local() - return 1 if an address is
 *          on a local network; 0 otherwise
 *      __inet_free_local_interfaces() - free handle that was
 *          returned by __inet_get_local_interfaces()
 *
 *  A typical calling sequence is:
 *
 *      p = __inet_get_local_interfaces();
 *      if (__inet_address_is_local(p, inaddr)) {
 *          ...
 *      }
 *      __inet_free_local_interfaces(p);
 */

/*
 *  Return an opaque pointer to a list of configured interfaces.
 */
void *
__inet_get_local_interfaces()
{
	struct ifinfo *lp;

	lp = get_local_info();
	return ((void *)lp);
}

/*
 *  Free memory allocated by inet_local_interfaces().
 */
void
__inet_free_local_interfaces(void *p)
{
	free(p);
}

/*
 *  Determine if an address is on a local network.
 *
 *  Might have made sense to use SIOCTONLINK, except that it doesn't
 *  handle matching on IPv4 network addresses.
 */
int
__inet_address_is_local_af(void *p, sa_family_t af, void *addr) {

	struct ifinfo	*localinfo = (struct ifinfo *)p;
	int		i, a;
	struct in_addr	v4addr;

	if (localinfo == 0)
		return (0);

	if (af == AF_INET6 && IN6_IS_ADDR_V4MAPPED((struct in6_addr *)addr)) {
		IN6_V4MAPPED_TO_INADDR((struct in6_addr *)addr, &v4addr);
		af = AF_INET;
		addr = (void *)&v4addr;
	}

	for (i = 0; i < localinfo->count; i++) {
		if (ifaf(i) == af) {
			if (af == AF_INET6) {
				struct in6_addr *a6 = (struct in6_addr *)addr;
				for (a = 0; a < sizeof (a6->s6_addr); a++) {
					if ((a6->s6_addr[a] &
						ifmask6(i).s6_addr[a]) !=
						(ifaddr6(i).s6_addr[a] &
						ifmask6(i).s6_addr[a]))
						break;
				}
				if (a >= sizeof (a6->s6_addr))
					return (1);
			} else {
				if ((((struct in_addr *)addr)->s_addr &
						ifmask4(i).s_addr) ==
					(ifaddr4(i).s_addr &
						ifmask4(i).s_addr))
					return (1);
			}
		}
	}

	return (0);
}

int
__inet_address_is_local(void *p, struct in_addr addr)
{
	return (__inet_address_is_local_af(p, AF_INET, &addr));
}

int
__inet_uaddr_is_local(void *p, struct netconfig *nc, char *uaddr)
{
	struct netbuf		*taddr;
	sa_family_t		af;
	int			ret;

	taddr = uaddr2taddr(nc, uaddr);
	if (taddr == 0)
		return (0);

	af = ((struct sockaddr *)taddr->buf)->sa_family;

	ret = __inet_address_is_local_af(p, af,
		(af == AF_INET6) ?
		(void *)&((struct sockaddr_in6 *)taddr->buf)->sin6_addr :
		(void *)&((struct sockaddr_in *)taddr->buf)->sin_addr);

	netdir_free(taddr, ND_ADDR);
	return (ret);
}


int
__inet_address_count(void *p)
{
	struct ifinfo *lp = (struct ifinfo *)p;

	if (lp != 0) {
		return (lp->count);
	} else {
		return (0);
	}
}

uint32_t
__inet_get_addr(void *p, int n)
{
	struct ifinfo *localinfo = (struct ifinfo *)p;

	if (localinfo == 0 || n >= localinfo->count || ifaf(n) != AF_INET)
		return (0);

	return (ifaddr4(n).s_addr);
}

uint32_t
__inet_get_network(void *p, int n)
{
	struct ifinfo *localinfo = (struct ifinfo *)p;

	if (localinfo == 0 || n >= localinfo->count || ifaf(n) != AF_INET)
		return (0);

	return (ifaddr4(n).s_addr & ifmask4(n).s_addr);
}

char *
__inet_get_uaddr(void *p, struct netconfig *nc, int n)
{
	struct ifinfo *localinfo = (struct ifinfo *)p;
	char *uaddr;
	struct sockaddr_in sin4;
	struct sockaddr_in6 sin6;
	struct sockaddr *sin;
	struct netbuf nb;

	if (localinfo == 0 || nc == 0 || n >= localinfo->count)
		return (0);

	if (ifaf(n) == AF_INET6) {
		if (strcmp(NC_INET6, nc->nc_protofmly) != 0)
			return (0);
		memset(&sin6, 0, sizeof (sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_addr = ifaddr6(n);
		nb.buf = (char *)&sin6;
		nb.len = sizeof (sin6);
	} else {
		if (strcmp(NC_INET, nc->nc_protofmly) != 0)
			return (0);
		memset(&sin4, 0, sizeof (sin4));
		sin4.sin_family = AF_INET;
		sin4.sin_addr = ifaddr4(n);
		nb.buf = (char *)&sin4;
		nb.len = sizeof (sin4);
	}

	nb.maxlen = nb.len;

	uaddr = taddr2uaddr(nc, &nb);
	return (uaddr);
}

char *
__inet_get_networka(void *p, int n)
{
	struct ifinfo	*localinfo = (struct ifinfo *)p;

	if (localinfo == 0 || n >= localinfo->count)
		return (0);

	if (ifaf(n) == AF_INET6) {
		char		buf[INET6_ADDRSTRLEN];
		struct in6_addr	in6;
		int		i;

		for (i = 0; i < sizeof (in6.s6_addr); i++) {
			in6.s6_addr[i] = ifaddr6(n).s6_addr[i] &
					ifmask6(n).s6_addr[i];
		}
		return (strdup(inet_ntop(AF_INET6, &in6, buf, sizeof (buf))));
	} else {
		struct in_addr	in4;

		in4.s_addr = ifaddr4(n).s_addr & ifmask4(n).s_addr;
		return (strdup(inet_ntoa(in4)));
	}
}

static
int
in_list(struct in_addr *addrs, int n, struct in_addr a)
{
	int i;

	for (i = 0; i < n; i++) {
		if (addrs[i].s_addr == a.s_addr)
			return (1);
	}
	return (0);
}

static int
getbroadcastnets(tp, addrs)
	struct netconfig *tp;
	struct in_addr **addrs;
{
	struct ifconf ifc;
	struct ifreq ifreq, *ifr;
	struct sockaddr_in *sin;
	struct in_addr a;
	int fd;
	int n, i, numifs;
	char *buf;
	int	use_loopback = 0;

	_nderror = ND_SYSTEM;
	fd = open(tp->nc_device, O_RDONLY);
	if (fd < 0) {
		(void) syslog(LOG_ERR,
	    "broadcast: open to get interface configuration: %m");
		return (0);
	}
#ifdef SIOCGIFNUM
	if (ioctl(fd, SIOCGIFNUM, (char *)&numifs) < 0) {
		perror("ioctl (get number of interfaces)");
		numifs = MAXIFS;
	}
#else
	numifs = MAXIFS;
#endif
	buf = (char *)malloc(numifs * sizeof (struct ifreq));
	if (buf == NULL) {
		(void) syslog(LOG_ERR, "broadcast: malloc failed: %m");
		(void) close(fd);
		return (0);
	}
	*addrs = (struct in_addr *)malloc(numifs * sizeof (struct in_addr));
	if (*addrs == NULL) {
		(void) syslog(LOG_ERR, "broadcast: malloc failed: %m");
		free(buf);
		(void) close(fd);
		return (0);
	}
	ifc.ifc_len = numifs * (int)sizeof (struct ifreq);
	ifc.ifc_buf = buf;
	/*
	 * Ideally, this ioctl should also tell me, how many bytes were
	 * finally allocated, but it doesnt.
	 */
	if (ioctl(fd, SIOCGIFCONF, (char *)&ifc) < 0) {
		(void) syslog(LOG_ERR,
	    "broadcast: ioctl (get interface configuration): %m");
		free(buf);
		free(*addrs);
		(void) close(fd);
		return (0);
	}

retry:
	ifr = (struct ifreq *)buf;
	for (i = 0, n = ifc.ifc_len / (int)sizeof (struct ifreq);
		n > 0; n--, ifr++) {
		ifreq = *ifr;
		if (ioctl(fd, SIOCGIFFLAGS, (char *)&ifreq) < 0) {
			(void) syslog(LOG_ERR,
		    "broadcast: ioctl (get interface flags): %m");
			continue;
		}
		if (!(ifreq.ifr_flags & IFF_UP) ||
		    (ifr->ifr_addr.sa_family != AF_INET))
			continue;
		if (ifreq.ifr_flags & IFF_BROADCAST) {
			sin = (struct sockaddr_in *)&ifr->ifr_addr;
			if (ioctl(fd, SIOCGIFBRDADDR, (char *)&ifreq) < 0) {
				/* May not work with other implementation */
				a = _inet_makeaddr(
				    inet_netof(sin->sin_addr),
				    INADDR_ANY);
				if (!in_list(*addrs, i, a))
					(*addrs)[i++] = a;
			} else {
				a = ((struct sockaddr_in *)
				    &ifreq.ifr_addr)->sin_addr;
				if (!in_list(*addrs, i, a))
					(*addrs)[i++] = a;
			}
			continue;
		}
		if (use_loopback && (ifreq.ifr_flags & IFF_LOOPBACK)) {
			sin = (struct sockaddr_in *)&ifr->ifr_addr;
			a = sin->sin_addr;
			if (!in_list(*addrs, i, a))
				(*addrs)[i++] = a;
			continue;
		}
		if (ifreq.ifr_flags & IFF_POINTOPOINT) {
			if (ioctl(fd, SIOCGIFDSTADDR, (char *)&ifreq) < 0)
				continue;
			a = ((struct sockaddr_in *)
			    &ifreq.ifr_addr)->sin_addr;
			if (!in_list(*addrs, i, a))
				(*addrs)[i++] = a;
			continue;
		}
	}
	if (i == 0 && !use_loopback) {
		use_loopback = 1;
		goto retry;
	}
	free(buf);
	(void) close(fd);
	if (i)
		_nderror = ND_OK;
	else
		free(*addrs);
	return (i);
}

/*
 * This is lifted straigt from libsocket/inet/inet_mkaddr.c.
 * Copied here to avoid our dependency on libsocket. More important
 * to make sure partially static app that use libnsl, but not
 * libsocket, don't get screwed up.
 * If you understand the above paragraph, try to get rid of
 * this copy of inet_makeaddr; if you don;t, leave it alone.
 *
 * Formulate an Internet address from network + host.  Used in
 * building addresses stored in the ifnet structure.
 */
static struct in_addr
_inet_makeaddr(in_addr_t net, in_addr_t host)
{
	in_addr_t addr;

	if (net < 128)
		addr = (net << IN_CLASSA_NSHIFT) | (host & IN_CLASSA_HOST);
	else if (net < 65536)
		addr = (net << IN_CLASSB_NSHIFT) | (host & IN_CLASSB_HOST);
	else if (net < 16777216L)
		addr = (net << IN_CLASSC_NSHIFT) | (host & IN_CLASSC_HOST);
	else
		addr = net | host;
	addr = htonl(addr);
	return (*(struct in_addr *)&addr);
}
