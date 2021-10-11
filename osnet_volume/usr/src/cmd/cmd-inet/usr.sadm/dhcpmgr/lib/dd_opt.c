/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)dd_opt.c	1.10	99/05/17 SMI"

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <string.h>
#include <malloc.h>
#include <libintl.h>
#include <stdio.h>
#include <netinet/dhcp.h>
#include <rpcsvc/nis.h>
#include <netdb.h>
#include <errno.h>
#include <sys/sockio.h>
#include <dirent.h>
#include <procfs.h>
#include <netdir.h>
#include <arpa/inet.h>
#include <rpcsvc/ypclnt.h>

#include "dd_misc.h"
#include "dd_opt.h"

#define	RDISC_FNAME	"in.rdisc"

/*
 * Free an allocated dhcp option structure.
 */
void
dd_freeopt(struct dhcp_option *opt)
{
	int i;

	if (opt->error_code == 0) {
		switch (opt->u.ret.datatype) {
		case ASCII_OPTION:
			for (i = 0; i < opt->u.ret.count; ++i) {
				free(opt->u.ret.data.strings[i]);
			}
			free(opt->u.ret.data.strings);
			break;
		case BOOLEAN_OPTION:
			break;
		case IP_OPTION:
			for (i = 0; i < opt->u.ret.count; ++i) {
				free(opt->u.ret.data.addrs[i]);
			}
			free(opt->u.ret.data.addrs);
			break;
		case NUMBER_OPTION:
			free(opt->u.ret.data.numbers);
			break;
		case OCTET_OPTION:
			for (i = 0; i < opt->u.ret.count; ++i) {
				free(opt->u.ret.data.octets[i]);
			}
			free(opt->u.ret.data.octets);
			break;
		default:
			return;
		}
	}
	free(opt);
}

/*
 * Allocate an option structure.
 */

static struct dhcp_option *
newopt(enum option_type ot, ushort_t count)
{
	struct dhcp_option *opt;

	opt = malloc(sizeof (struct dhcp_option));
	if ((opt != NULL) && (ot != ERROR_OPTION)) {
		opt->error_code = 0;
		opt->u.ret.datatype = ot;
		switch (ot) {
		case ASCII_OPTION:
			opt->u.ret.data.strings =
			    calloc(count, sizeof (char *));
			if (opt->u.ret.data.strings == NULL) {
				free(opt);
				opt = NULL;
			} else {
				opt->u.ret.count = count;
			}
			break;
		case BOOLEAN_OPTION:
			opt->u.ret.count = count;
			break;
		case IP_OPTION:
			opt->u.ret.data.addrs = calloc(count,
			    sizeof (struct in_addr *));
			if (opt->u.ret.data.addrs == NULL) {
				free(opt);
				opt = NULL;
			} else {
				opt->u.ret.count = count;
			}
			break;
		case NUMBER_OPTION:
			opt->u.ret.data.numbers = calloc(count,
			    sizeof (int64_t));
			if (opt->u.ret.data.numbers == NULL) {
				free(opt);
				opt = NULL;
			} else {
				opt->u.ret.count = count;
			}
			break;
		case OCTET_OPTION:
			opt->u.ret.data.octets = calloc(count,
			    sizeof (uchar_t *));
			if (opt->u.ret.data.octets == NULL) {
				free(opt);
				opt = NULL;
			} else {
				opt->u.ret.count = count;
			}
			break;
		default:
			free(opt);
			opt = NULL;
		}
	}
	return (opt);
}

/*
 * Return an out of memory error
 */
static struct dhcp_option *
malloc_failure()
{
	struct dhcp_option *opt;

	opt = newopt(ERROR_OPTION, 0);
	if (opt != NULL) {
		opt->error_code = ENOMEM;
		opt->u.msg = strerror(ENOMEM);
	}
	return (opt);
}

/*
 * Construct list of default routers.
 */
/*ARGSUSED*/
static struct dhcp_option *
get_default_routers(const char *arg)
{
	struct dhcp_option *opt;
	FILE *fp;
	char rbuff[BUFSIZ];
	struct in_addr **addrs = NULL;
	struct in_addr **tmpaddrs;
	int addrcnt = 0;
	char *cp;
	int i;

	/*
	 * Method here is completely bogus; read output from netstat and
	 * grab lines with destination of 'default'.  Look at the netstat
	 * code if you think there's a better way...
	 */
	if ((fp = popen("netstat -r -n -f inet", "r")) == NULL) {
		opt = newopt(ERROR_OPTION, 0);
		if (opt != NULL) {
			opt->error_code = errno;
			opt->u.msg = strerror(errno);
		}
		return (opt);
	}

	while (fgets(rbuff, BUFSIZ, fp) != NULL) {
		cp = strtok(rbuff, " \t");
		if (cp == NULL)
			continue;
		if (strcmp(cp, "default") == 0) {
			/* got one, add to list */
			tmpaddrs = realloc(addrs,
			    (addrcnt+1) * sizeof (struct in_addr *));
			if (tmpaddrs == NULL) {
				opt = newopt(ERROR_OPTION, 0);
				if (opt != NULL) {
					opt->error_code = errno;
					opt->u.msg = strerror(errno);
				}
				for (i = addrcnt - 1; i >= 0; --i) {
					free(addrs[i]);
				}
				free(addrs);
				(void) pclose(fp);
				return (opt);
			}
			addrs = tmpaddrs;
			addrs[addrcnt] = malloc(sizeof (struct in_addr));
			if (addrs[addrcnt] == NULL) {
				opt = newopt(ERROR_OPTION, 0);
				if (opt != NULL) {
					opt->error_code = errno;
					opt->u.msg = strerror(errno);
				}
				for (i = addrcnt - 1; i >= 0; --i) {
					free(addrs[i]);
				}
				free(addrs);
				(void) pclose(fp);
				return (opt);
			}

			cp = strtok(NULL, " \t");
			addrs[addrcnt]->s_addr = inet_addr(cp);
			/* LINTED - comparison */
			if (addrs[addrcnt]->s_addr == -1) {
				/* inet_addr didn't like it */
				opt = newopt(ERROR_OPTION, 0);
				if (opt != NULL) {
					opt->error_code = EINVAL;
					opt->u.msg = strerror(EINVAL);
				}
				while (--addrcnt >= 0)
					free(addrs[addrcnt]);
				free(addrs);
				(void) pclose(fp);
				return (opt);
			}
			++addrcnt;
		}
	}
	(void) pclose(fp);
	/*
	 * Return all the routers we found.
	 */
	if (addrcnt != 0) {
		opt = newopt(IP_OPTION, addrcnt);
		if (opt == NULL) {
			for (i = 0; i < addrcnt; ++i) {
				free(addrs[i]);
				free(addrs);
			}
			return (opt);
		}
		for (i = 0; i < addrcnt; ++i) {
			opt->u.ret.data.addrs[i] = addrs[i];
		}
		free(addrs);
	} else {
		opt = newopt(ERROR_OPTION, 0);
		if (opt != NULL) {
			opt->error_code = 1;
			opt->u.msg = gettext("No default router found");
		}
	}
	return (opt);
}

/*ARGSUSED*/
static struct dhcp_option *
get_dns_domain(const char *arg)
{
	struct dhcp_option *opt;

	res_init();
	opt = newopt(ASCII_OPTION, 1);
	if (opt != NULL) {
		opt->u.ret.data.strings[0] = strdup(_res.defdname);
	}
	if (opt->u.ret.data.strings[0] == NULL) {
		dd_freeopt(opt);
		opt = malloc_failure();
	}
	return (opt);
}

/*ARGSUSED*/
static struct dhcp_option *
get_dns_servers(const char *arg)
{
	struct dhcp_option *opt;
	int i;

	/* Initialize resolver library and then pick servers out of structure */
	res_init();
	/*
	 * If only one & it's loopback address, we ignore as this
	 * really means that DNS is not configured per res_init(4).
	 */
	if (_res.nscount == 1 &&
	    _res.nsaddr_list[0].sin_addr.s_addr == ntohl(INADDR_LOOPBACK)) {
		opt = newopt(IP_OPTION, 0);
	} else {
		/* Just copy the data into our return structure */
		opt = newopt(IP_OPTION, _res.nscount);
		if (opt != NULL) {
			for (i = 0; i < _res.nscount; ++i) {
				opt->u.ret.data.addrs[i] = malloc(
				    sizeof (struct in_addr));
				if (opt->u.ret.data.addrs[i] == NULL) {
					dd_freeopt(opt);
					return (malloc_failure());
				}
				*opt->u.ret.data.addrs[i] =
				    _res.nsaddr_list[i].sin_addr;
			}
		}
	}
	return (opt);
}

/* Get parameters related to a specific interface */
static struct dhcp_option *
get_if_param(int code, const char *arg)
{
	int s;
	struct ifconf ifc;
	int num_ifs;
	int i;
	struct ifreq *ifr;
	struct dhcp_option *opt;
#define	MY_TRUE	1
#define	MY_FALSE	0
	int found = MY_FALSE;
	struct sockaddr_in *sin;

	/*
	 * Open socket, needed for doing the ioctls.  Then get number of
	 * interfaces so we know how much memory to allocate, then get
	 * all the interface configurations.
	 */
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (ioctl(s, SIOCGIFNUM, &num_ifs) < 0) {
		opt = newopt(ERROR_OPTION, 0);
		if (opt != NULL) {
			opt->error_code = errno;
			opt->u.msg = strerror(errno);
		}
		return (opt);
	}
	ifc.ifc_len = num_ifs * sizeof (struct ifreq);
	ifc.ifc_buf = malloc(ifc.ifc_len);
	if (ifc.ifc_buf == NULL) {
		return (malloc_failure());
	}
	if (ioctl(s, SIOCGIFCONF, &ifc) < 0) {
		opt = newopt(ERROR_OPTION, 0);
		if (opt != NULL) {
			opt->error_code = errno;
			opt->u.msg = strerror(errno);
		}
		free(ifc.ifc_buf);
		(void) close(s);
		return (opt);
	}

	/*
	 * Find the interface which matches the one requested, and then
	 * return the parameter requested.
	 */
	for (i = 0, ifr = ifc.ifc_req; i < num_ifs; ++i, ++ifr) {
		if (strcmp(ifr->ifr_name, arg) != 0) {
			continue;
		}
		found = MY_TRUE;
		switch (code) {
		case CD_SUBNETMASK:
			if (ioctl(s, SIOCGIFNETMASK, ifr) < 0) {
				opt = newopt(ERROR_OPTION, 0);
				if (opt != NULL) {
					opt->error_code = errno;
					opt->u.msg = strerror(errno);
				}
				free(ifc.ifc_buf);
				(void) close(s);
				return (opt);
			}
			opt = newopt(IP_OPTION, 1);
			if (opt == NULL) {
				free(ifc.ifc_buf);
				(void) close(s);
				return (malloc_failure());
			}
			opt->u.ret.data.addrs[0] =
			    malloc(sizeof (struct in_addr));
			if (opt->u.ret.data.addrs[0] == NULL) {
				free(ifc.ifc_buf);
				(void) close(s);
				return (malloc_failure());
			}
			/*LINTED - alignment*/
			sin = (struct sockaddr_in *)&ifr->ifr_addr;
			*opt->u.ret.data.addrs[0] = sin->sin_addr;
			break;
		case CD_MTU:
			if (ioctl(s, SIOCGIFMTU, ifr) < 0) {
				opt = newopt(ERROR_OPTION, 0);
				if (opt != NULL) {
					opt->error_code = errno;
					opt->u.msg = strerror(errno);
				}
				free(ifc.ifc_buf);
				(void) close(s);
				return (opt);
			}
			opt = newopt(NUMBER_OPTION, 1);
			if (opt == NULL) {
				free(ifc.ifc_buf);
				(void) close(s);
				return (malloc_failure());
			}
			opt->u.ret.data.numbers[0] = ifr->ifr_metric;
			break;
		case CD_BROADCASTADDR:
			if (ioctl(s, SIOCGIFBRDADDR, ifr) < 0) {
				opt = newopt(ERROR_OPTION, 0);
				if (opt != NULL) {
					opt->error_code = errno;
					opt->u.msg = strerror(errno);
				}
				free(ifc.ifc_buf);
				(void) close(s);
				return (opt);
			}
			opt = newopt(IP_OPTION, 1);
			if (opt == NULL) {
				free(ifc.ifc_buf);
				(void) close(s);
				return (malloc_failure());
			}
			opt->u.ret.data.addrs[0] =
			    malloc(sizeof (struct in_addr));
			if (opt->u.ret.data.addrs[0] == NULL) {
				free(ifc.ifc_buf);
				(void) close(s);
				return (malloc_failure());
			}
			/*LINTED - alignment*/
			sin = (struct sockaddr_in *)&ifr->ifr_addr;
			*opt->u.ret.data.addrs[0] = sin->sin_addr;
			break;
		default:
			opt = newopt(ERROR_OPTION, 0);
			opt->error_code = 1;
			opt->u.msg = gettext("Bad option code in get_if_param");
		}
		break;
	}
	free(ifc.ifc_buf);
	(void) close(s);
	if (found == MY_FALSE) {
		opt = newopt(ERROR_OPTION, 0);
		opt->error_code = 1;
		opt->u.msg = gettext("No such interface");
	}
	return (opt);
}

/*
 * See if we are using router discovery on this system.  Method is to
 * read procfs and find out if the in.rdisc daemon is running.
 */
/*ARGSUSED*/
static struct dhcp_option *
get_router_discovery(const char *arg)
{
	struct dhcp_option *opt;

	opt = newopt(NUMBER_OPTION, 1);
	if (opt == NULL) {
		return (malloc_failure());
	}
	if (dd_getpid(RDISC_FNAME) != -1) {
		opt->u.ret.data.numbers[0] = 1;
	} else {
		opt->u.ret.data.numbers[0] = 0;
	}
	return (opt);
}

/*ARGSUSED*/
static struct dhcp_option *
get_nis_domain(const char *arg)
{
	struct dhcp_option *opt;
	char *d;
	int err;

	err = yp_get_default_domain(&d);
	if (err != 0) {
		opt = newopt(ERROR_OPTION, 0);
		if (opt != NULL) {
			opt->error_code = err;
			opt->u.msg = gettext("Error in yp_get_default_domain");
		}
	} else {
		opt = newopt(ASCII_OPTION, 1);
		if (opt == NULL) {
			return (malloc_failure());
		}
		opt->u.ret.data.strings[0] = strdup(d);
		if (opt->u.ret.data.strings[0] == NULL) {
			dd_freeopt(opt);
			return (malloc_failure());
		}
	}
	return (opt);
}

/*
 * Provide a default for the NISserv option.  We can only reliably
 * find out the master (as that's the only API) so that's what we provide.
 */
/*ARGSUSED*/
static struct dhcp_option *
get_nis_servers(const char *arg)
{
	struct dhcp_option *opt;
	int err;
	char *d;
	char *m;
	struct hostent *hent;

	/*
	 * Get the default domain name, ask for master of hosts table,
	 * look master up in hosts table to get address.
	 */
	err = yp_get_default_domain(&d);
	if (err != 0) {
		opt = newopt(ERROR_OPTION, 0);
		if (opt != NULL) {
			opt->error_code = err;
			opt->u.msg = gettext("Error in yp_get_default_domain");
		}
	} else if ((err = yp_master(d, "hosts.byname", &m)) != 0) {
		free(m);
		opt = newopt(ERROR_OPTION, 0);
		if (opt != NULL) {
			opt->error_code = err;
			opt->u.msg = gettext("Error in yp_master");
		}
	} else if ((hent = gethostbyname(m)) == NULL) {
		free(m);
		opt = newopt(ERROR_OPTION, 0);
		if (opt != NULL) {
			opt->error_code = h_errno;
			opt->u.msg = gettext("Error in gethostbyname()");
		}
	} else {
		free(m);
		opt = newopt(IP_OPTION, 1);
		if (opt == NULL) {
			return (malloc_failure());
		}
		opt->u.ret.data.addrs[0] = malloc(sizeof (struct in_addr));
		if (opt->u.ret.data.addrs[0] == NULL) {
			dd_freeopt(opt);
			return (malloc_failure());
		}
		/*LINTED - alignment*/
		*opt->u.ret.data.addrs[0] = *(struct in_addr *)hent->h_addr;
	}
	return (opt);
}

/*ARGSUSED*/
static struct dhcp_option *
get_nisplus_domain(const char *arg)
{
	struct dhcp_option *opt;

	opt = newopt(ASCII_OPTION, 1);
	if (opt != NULL) {
		opt->u.ret.data.strings[0] = strdup(nis_local_directory());
		if (opt->u.ret.data.strings[0] == NULL) {
			dd_freeopt(opt);
			return (malloc_failure());
		}
	}
	return (opt);
}

/*ARGSUSED*/
static struct dhcp_option *
get_nisplus_servers(const char *arg)
{
	struct dhcp_option *opt;
	nis_result *nres;
	nis_object *nobj;
	int i;
	struct netconfig *nc;
	struct netbuf *nb;
	int cnt = 0;
	struct sockaddr_in *sin;
	struct in_addr **tmpaddrs;

	nres = nis_lookup(nis_local_directory(), FOLLOW_LINKS);
	if ((NIS_RES_STATUS(nres) != NIS_SUCCESS) &&
	    (NIS_RES_STATUS(nres) != NIS_S_SUCCESS)) {
		opt = newopt(ERROR_OPTION, 0);
		if (opt != NULL) {
			opt->error_code = NIS_RES_STATUS(nres);
			opt->u.msg = nis_sperrno(NIS_RES_STATUS(nres));
		}
		nis_freeresult(nres);
		return (opt);
	}
	nobj = NIS_RES_OBJECT(nres);
	if (nobj->zo_data.zo_type != NIS_DIRECTORY_OBJ) {
		opt = newopt(ERROR_OPTION, 0);
		if (opt != NULL) {
			opt->error_code = 1;
			opt->u.msg = gettext("Not a directory object");
		}
		nis_freeresult(nres);
		return (opt);
	}
	opt = newopt(IP_OPTION, nobj->DI_data.do_servers.do_servers_len);
	if (opt == NULL) {
		nis_freeresult(nres);
		return (malloc_failure());
	}
	for (i = 0; i < nobj->DI_data.do_servers.do_servers_len; ++i) {
		/* Make sure it's an internet-type address */
		if (strcmp("inet",
		    nobj->DI_data.do_servers.do_servers_val[i].
		    ep.ep_val[0].family) == 0) {
			/* Find a translation service */
			nc = getnetconfigent(nobj->DI_data.do_servers.
			    do_servers_val[i].ep.ep_val[0].proto);
			if (nc == NULL) {
				continue;
			}
			/* Translate address to normal in_addr structure */
			nb = uaddr2taddr(nc, nobj->DI_data.do_servers.
			    do_servers_val[i].ep.ep_val[0].uaddr);
			freenetconfigent(nc);
			if (nb == NULL) {
				continue;
			}
			opt->u.ret.data.addrs[cnt] = malloc(
			    sizeof (struct in_addr));
			if (opt->u.ret.data.addrs[cnt] == NULL) {
				dd_freeopt(opt);
				nis_freeresult(nres);
				netdir_free(nb, ND_ADDR);
				return (malloc_failure());
			}
			/*LINTED - alignment*/
			sin = (struct sockaddr_in *)nb->buf;
			*opt->u.ret.data.addrs[cnt] = sin->sin_addr;
			++cnt;
			netdir_free(nb, ND_ADDR);
		}
	}

	nis_freeresult(nres);
	/*
	 * Adjust size of returned block to the actual data size, as
	 * the above loop may not have filled in all of the elements
	 * which were allocated.
	 */
	if (cnt != opt->u.ret.count) {
		tmpaddrs = realloc(opt->u.ret.data.addrs,
		    cnt * sizeof (struct in_addr *));
		if (tmpaddrs == NULL) {
			dd_freeopt(opt);
			return (malloc_failure());
		}
		opt->u.ret.data.addrs = tmpaddrs;
		opt->u.ret.count = cnt;
	}
	return (opt);
}

/*
 * Retrieve the default value for a specified DHCP option.  Option code is
 * from the lst in dhcp.h, arg is an option-specific string argument, and
 * context is a presently unused parameter intended to allow this mechanism
 * to extend to vendor options in the future.  For now, only standard options
 * are supported.
 */

/*ARGSUSED*/
struct dhcp_option *
dd_getopt(ushort_t code, const char *arg, const char *context)
{
	struct dhcp_option *opt;

	switch (code) {
	case CD_SUBNETMASK:
	case CD_MTU:
	case CD_BROADCASTADDR:
		return (get_if_param(code, arg));
	case CD_ROUTER:
		return (get_default_routers(arg));
	case CD_DNSSERV:
		return (get_dns_servers(arg));
	case CD_DNSDOMAIN:
		return (get_dns_domain(arg));
	case CD_ROUTER_DISCVRY_ON:
		return (get_router_discovery(arg));
	case CD_NIS_DOMAIN:
		return (get_nis_domain(arg));
	case CD_NIS_SERV:
		return (get_nis_servers(arg));
	case CD_NISPLUS_DMAIN:
		return (get_nisplus_domain(arg));
	case CD_NISPLUS_SERVS:
		return (get_nisplus_servers(arg));
	default:
		opt = newopt(ERROR_OPTION, 0);
		if (opt != NULL) {
			opt->error_code = 1;
			opt->u.msg = gettext("Unimplemented option requested");
		}
		return (opt);
	}
}
