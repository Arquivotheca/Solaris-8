/*
 * Copyright (c) 1986-1996,1998,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)clnt_generic.c	1.44	99/07/19 SMI"

#include "rpc_mt.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <rpc/rpc.h>
#include <rpc/trace.h>
#include <rpc/nettype.h>
#include <netdir.h>
#include <string.h>
#include <syslog.h>

extern __td_setnodelay(int);
extern bool_t __rpc_is_local_host(const char *);
extern bool_t __rpc_try_doors(char *, bool_t *);

#ifndef NETIDLEN
#define	NETIDLEN 32
#endif


/*
 * Generic client creation with version checking the value of
 * vers_out is set to the highest server supported value
 * vers_low <= vers_out <= vers_high  AND an error results
 * if this can not be done.
 *
 * It calls clnt_create_vers_timed() with a NULL value for the timeout
 * pointer, which indicates that the default timeout should be used.
 */
CLIENT *
clnt_create_vers(const char *hostname, rpcprog_t prog, rpcvers_t *vers_out,
	rpcvers_t vers_low, rpcvers_t vers_high, const char *nettype)
{
	return (clnt_create_vers_timed(hostname, prog, vers_out, vers_low,
				vers_high, nettype, NULL));
}

/*
 * This the routine has the same definition as clnt_create_vers(),
 * except it takes an additional timeout parameter - a pointer to
 * a timeval structure.  A NULL value for the pointer indicates
 * that the default timeout value should be used.
 */
CLIENT *
clnt_create_vers_timed(const char *hostname, rpcprog_t prog,
    rpcvers_t *vers_out, rpcvers_t vers_low, rpcvers_t vers_high,
    const char *nettype, const struct timeval *tp)
{
	CLIENT *clnt;
	struct timeval to;
	enum clnt_stat rpc_stat;
	struct rpc_err rpcerr;

	trace4(TR_clnt_create_vers_timed, 0, prog, vers_low, vers_high);
	clnt = clnt_create_timed(hostname, prog, vers_high, nettype, tp);
	if (clnt == NULL) {
		trace4(TR_clnt_create_vers_timed, 1, prog, vers_low, vers_high);
		return (NULL);
	}
	if (tp == NULL) {
		to.tv_sec = 10;
		to.tv_usec = 0;
	} else
		to = *tp;

	rpc_stat = clnt_call(clnt, NULLPROC, (xdrproc_t)xdr_void,
			(char *)NULL, (xdrproc_t)xdr_void, (char *)NULL, to);
	if (rpc_stat == RPC_SUCCESS) {
		*vers_out = vers_high;
		trace4(TR_clnt_create_vers_timed, 1, prog, vers_low, vers_high);
		return (clnt);
	}
	while (rpc_stat == RPC_PROGVERSMISMATCH && vers_high > vers_low) {
		unsigned int minvers, maxvers;

		clnt_geterr(clnt, &rpcerr);
		minvers = rpcerr.re_vers.low;
		maxvers = rpcerr.re_vers.high;
		if (maxvers < vers_high)
			vers_high = maxvers;
		else
			vers_high--;
		if (minvers > vers_low)
			vers_low = minvers;
		if (vers_low > vers_high) {
			goto error;
		}
		CLNT_CONTROL(clnt, CLSET_VERS, (char *)&vers_high);
		rpc_stat = clnt_call(clnt, NULLPROC, (xdrproc_t)xdr_void,
				(char *)NULL, (xdrproc_t)xdr_void,
				(char *)NULL, to);
		if (rpc_stat == RPC_SUCCESS) {
			*vers_out = vers_high;
			trace4(TR_clnt_create_vers_timed, 1, prog,
				vers_low, vers_high);
			return (clnt);
		}
	}
	clnt_geterr(clnt, &rpcerr);

error:
	rpc_createerr.cf_stat = rpc_stat;
	rpc_createerr.cf_error = rpcerr;
	clnt_destroy(clnt);
	trace4(TR_clnt_create_vers_timed, 1, prog, vers_low, vers_high);
	return (NULL);
}

/*
 * Top level client creation routine.
 * Generic client creation: takes (servers name, program-number, nettype) and
 * returns client handle. Default options are set, which the user can
 * change using the rpc equivalent of ioctl()'s.
 *
 * It tries for all the netids in that particular class of netid until
 * it succeeds.
 * XXX The error message in the case of failure will be the one
 * pertaining to the last create error.
 *
 * It calls clnt_create_timed() with the default timeout.
 */
CLIENT *
clnt_create(const char *hostname, rpcprog_t prog, rpcvers_t vers,
    const char *nettype)
{
	return (clnt_create_timed(hostname, prog, vers, nettype, NULL));
}

/*
 * This the routine has the same definition as clnt_create(),
 * except it takes an additional timeout parameter - a pointer to
 * a timeval structure.  A NULL value for the pointer indicates
 * that the default timeout value should be used.
 *
 * This function calls clnt_tp_create_timed().
 */
CLIENT *
clnt_create_timed(const char *hostname, rpcprog_t prog, rpcvers_t vers,
    const char *netclass, const struct timeval *tp)
{
	struct netconfig *nconf;
	CLIENT *clnt = NULL;
	void *handle;
	enum clnt_stat	save_cf_stat = RPC_SUCCESS;
	struct rpc_err	save_cf_error;
	char nettype_array[NETIDLEN];
	char *nettype = &nettype_array[0];
	bool_t try_others;

	trace3(TR_clnt_create, 0, prog, vers);

	if (netclass == NULL)
		nettype = NULL;
	else {
		size_t len = strlen(netclass);
		if (len >= sizeof (nettype_array)) {
			rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
			trace3(TR_clnt_create, 1, prog, vers);
			return ((CLIENT *)NULL);
		}
		strcpy(nettype, netclass);
	}

	/*
	 * Check to see if a rendezvous over doors should be attempted.
	 */
	if (__rpc_try_doors(nettype, &try_others)) {
		/*
		 * Make sure this is the local host.
		 */
		if (__rpc_is_local_host(hostname)) {
			if ((clnt = clnt_door_create(prog, vers, 0)) != NULL)
				return (clnt);
			else {
				if (rpc_createerr.cf_stat == RPC_SYSTEMERROR)
					return ((CLIENT *)NULL);
				save_cf_stat = rpc_createerr.cf_stat;
				save_cf_error = rpc_createerr.cf_error;
			}
		} else {
			save_cf_stat = rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		}
	}
	if (!try_others)
		return (NULL);

	if ((handle = __rpc_setconf((char *)nettype)) == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		trace3(TR_clnt_create, 1, prog, vers);
		return ((CLIENT *)NULL);
	}
	rpc_createerr.cf_stat = RPC_SUCCESS;
	while (clnt == (CLIENT *)NULL) {
		if ((nconf = __rpc_getconf(handle)) == NULL) {
			if (rpc_createerr.cf_stat == RPC_SUCCESS)
				rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
			break;
		}
		clnt = clnt_tp_create_timed(hostname, prog, vers, nconf, tp);
		if (clnt)
			break;
		else {
			/*
			 *	Since we didn't get a name-to-address
			 *	translation failure here, we remember
			 *	this particular error.  The object of
			 *	this is to enable us to return to the
			 *	caller a more-specific error than the
			 *	unhelpful ``Name to address translation
			 *	failed'' which might well occur if we
			 *	merely returned the last error (because
			 *	the local loopbacks are typically the
			 *	last ones in /etc/netconfig and the most
			 *	likely to be unable to translate a host
			 *	name).  We also check for a more
			 *	meaningful error than ``unknown host
			 *	name'' for the same reasons.
			 */
			if (rpc_createerr.cf_stat == RPC_SYSTEMERROR) {
				syslog(LOG_ERR, "clnt_create_timed: "
					"RPC_SYSTEMERROR.");
				break;
			}

			if (rpc_createerr.cf_stat != RPC_N2AXLATEFAILURE &&
			    rpc_createerr.cf_stat != RPC_UNKNOWNHOST) {
				save_cf_stat = rpc_createerr.cf_stat;
				save_cf_error = rpc_createerr.cf_error;
			}
		}
	}

	/*
	 *	Attempt to return an error more specific than ``Name to address
	 *	translation failed'' or ``unknown host name''
	 */
	if ((rpc_createerr.cf_stat == RPC_N2AXLATEFAILURE ||
				rpc_createerr.cf_stat == RPC_UNKNOWNHOST) &&
					(save_cf_stat != RPC_SUCCESS)) {
		rpc_createerr.cf_stat = save_cf_stat;
		rpc_createerr.cf_error = save_cf_error;
	}
	__rpc_endconf(handle);
	trace3(TR_clnt_create, 1, prog, vers);
	return (clnt);
}

/*
 * Generic client creation: takes (servers name, program-number, netconf) and
 * returns client handle. Default options are set, which the user can
 * change using the rpc equivalent of ioctl()'s : clnt_control()
 * It finds out the server address from rpcbind and calls clnt_tli_create().
 *
 * It calls clnt_tp_create_timed() with the default timeout.
 */
CLIENT *
clnt_tp_create(const char *hostname, rpcprog_t prog, rpcvers_t vers,
    const struct netconfig *nconf)
{
	return (clnt_tp_create_timed(hostname, prog, vers, nconf, NULL));
}

/*
 * This has the same definition as clnt_tp_create(), except it
 * takes an additional parameter - a pointer to a timeval structure.
 * A NULL value for the timeout pointer indicates that the default
 * value for the timeout should be used.
 */
CLIENT *
clnt_tp_create_timed(const char *hostname, rpcprog_t prog, rpcvers_t vers,
    const struct netconfig *nconf, const struct timeval *tp)
{
	struct netbuf *svcaddr;			/* servers address */
	CLIENT *cl = NULL;			/* client handle */
	extern struct netbuf *__rpcb_findaddr_timed(rpcprog_t, rpcvers_t,
	    struct netconfig *, char *, CLIENT **, struct timeval *);

	trace3(TR_clnt_tp_create, 0, prog, vers);
	if (nconf == (struct netconfig *)NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		trace3(TR_clnt_tp_create, 1, prog, vers);
		return ((CLIENT *)NULL);
	}

	/*
	 * Get the address of the server
	 */
	if ((svcaddr = __rpcb_findaddr_timed(prog, vers,
			(struct netconfig *)nconf, (char *)hostname,
			&cl, (struct timeval *)tp)) == NULL) {
		/* appropriate error number is set by rpcbind libraries */
		trace3(TR_clnt_tp_create, 1, prog, vers);
		return ((CLIENT *)NULL);
	}
	if (cl == (CLIENT *)NULL) {
		cl = clnt_tli_create(RPC_ANYFD, nconf, svcaddr,
					prog, vers, 0, 0);
	} else {
		/* Reuse the CLIENT handle and change the appropriate fields */
		if (CLNT_CONTROL(cl, CLSET_SVC_ADDR, (void *)svcaddr) == TRUE) {
			if (cl->cl_netid == NULL) {
				cl->cl_netid = strdup(nconf->nc_netid);
				if (cl->cl_netid == NULL) {
					netdir_free((char *)svcaddr, ND_ADDR);
					rpc_createerr.cf_stat = RPC_SYSTEMERROR;
					syslog(LOG_ERR,
						"clnt_tp_create_timed: "
						"strdup failed.");
					return ((CLIENT *)NULL);
				}
			}
			if (cl->cl_tp == NULL) {
				cl->cl_tp = strdup(nconf->nc_device);
				if (cl->cl_tp == NULL) {
					netdir_free((char *)svcaddr, ND_ADDR);
					if (cl->cl_netid)
						free(cl->cl_netid);
					rpc_createerr.cf_stat = RPC_SYSTEMERROR;
					syslog(LOG_ERR,
						"clnt_tp_create_timed: "
						"strdup failed.");
					return ((CLIENT *)NULL);
				}
			}
			(void) CLNT_CONTROL(cl, CLSET_PROG, (void *)&prog);
			(void) CLNT_CONTROL(cl, CLSET_VERS, (void *)&vers);
		} else {
			CLNT_DESTROY(cl);
			cl = clnt_tli_create(RPC_ANYFD, nconf, svcaddr,
					prog, vers, 0, 0);
		}
	}
	netdir_free((char *)svcaddr, ND_ADDR);
	trace3(TR_clnt_tp_create, 1, prog, vers);
	return (cl);
}

/*
 * Generic client creation:  returns client handle.
 * Default options are set, which the user can
 * change using the rpc equivalent of ioctl()'s : clnt_control().
 * If fd is RPC_ANYFD, it will be opened using nconf.
 * It will be bound if not so.
 * If sizes are 0; appropriate defaults will be chosen.
 */
CLIENT *
clnt_tli_create(int fd, const struct netconfig *nconf, struct netbuf *svcaddr,
    rpcprog_t prog, rpcvers_t vers, uint_t sendsz, uint_t recvsz)
{
	CLIENT *cl;			/* client handle */
	struct t_info tinfo;		/* transport info */
	bool_t madefd;			/* whether fd opened here */
	t_scalar_t servtype;
	int retval;
	extern int __rpc_minfd;

	trace5(TR_clnt_tli_create, 0, prog, vers, sendsz, recvsz);
	if (fd == RPC_ANYFD) {
		if (nconf == (struct netconfig *)NULL) {
			rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
			trace3(TR_clnt_tli_create, 1, prog, vers);
			return ((CLIENT *)NULL);
		}

		fd = t_open(nconf->nc_device, O_RDWR, NULL);
		if (fd == -1)
			goto err;
		if (fd < __rpc_minfd)
			fd = __rpc_raise_fd(fd);
		madefd = TRUE;
		if (t_bind(fd, (struct t_bind *)NULL,
			(struct t_bind *)NULL) == -1)
				goto err;
		switch (nconf->nc_semantics) {
		case NC_TPI_CLTS:
			servtype = T_CLTS;
			break;
		case NC_TPI_COTS:
			servtype = T_COTS;
			break;
		case NC_TPI_COTS_ORD:
			servtype = T_COTS_ORD;
			break;
		default:
			if (t_getinfo(fd, &tinfo) == -1)
				goto err;
			servtype = tinfo.servtype;
			break;
		}
	} else {
		int state;		/* Current state of provider */

		/*
		 * Sync the opened fd.
		 * Check whether bound or not, else bind it
		 */
		if (((state = t_sync(fd)) == -1) ||
		    ((state == T_UNBND) && (t_bind(fd, (struct t_bind *)NULL,
				(struct t_bind *)NULL) == -1)) ||
		    (t_getinfo(fd, &tinfo) == -1))
			goto err;
		servtype = tinfo.servtype;
		madefd = FALSE;
	}

	switch (servtype) {
	case T_COTS:
		cl = clnt_vc_create(fd, svcaddr, prog, vers, sendsz, recvsz);
		break;
	case T_COTS_ORD:
		if (nconf && ((strcmp(nconf->nc_protofmly, NC_INET) == 0) ||
		    (strcmp(nconf->nc_protofmly, NC_INET6) == 0))) {
			retval =  __td_setnodelay(fd);
			if (retval == -1)
				goto err;
		}
		cl = clnt_vc_create(fd, svcaddr, prog, vers, sendsz, recvsz);
		break;
	case T_CLTS:
		cl = clnt_dg_create(fd, svcaddr, prog, vers, sendsz, recvsz);
		break;
	default:
		goto err;
	}

	if (cl == (CLIENT *)NULL)
		goto err1; /* borrow errors from clnt_dg/vc creates */
	if (nconf) {
		cl->cl_netid = strdup(nconf->nc_netid);
		if (cl->cl_netid == NULL) {
			rpc_createerr.cf_stat = RPC_SYSTEMERROR;
			rpc_createerr.cf_error.re_errno = errno;
			rpc_createerr.cf_error.re_terrno = 0;
			syslog(LOG_ERR,
				"clnt_tli_create: strdup failed");
			goto err1;
		}
		cl->cl_tp = strdup(nconf->nc_device);
		if (cl->cl_tp == NULL) {
			if (cl->cl_netid)
				free(cl->cl_netid);
			rpc_createerr.cf_stat = RPC_SYSTEMERROR;
			rpc_createerr.cf_error.re_errno = errno;
			rpc_createerr.cf_error.re_terrno = 0;
			syslog(LOG_ERR,
				"clnt_tli_create: strdup failed");
			goto err1;
		}
	} else {
		struct netconfig *nc;

		if ((nc = __rpcfd_to_nconf(fd, servtype)) != NULL) {
			if (nc->nc_netid) {
				cl->cl_netid = strdup(nc->nc_netid);
				if (cl->cl_netid == NULL) {
					rpc_createerr.cf_stat = RPC_SYSTEMERROR;
					rpc_createerr.cf_error.re_errno = errno;
					rpc_createerr.cf_error.re_terrno = 0;
					syslog(LOG_ERR,
						"clnt_tli_create: "
						"strdup failed");
					goto err1;
				}
			}
			if (nc->nc_device) {
				cl->cl_tp = strdup(nc->nc_device);
				if (cl->cl_tp == NULL) {
					if (cl->cl_netid)
						free(cl->cl_netid);
					rpc_createerr.cf_stat = RPC_SYSTEMERROR;
					rpc_createerr.cf_error.re_errno = errno;
					rpc_createerr.cf_error.re_terrno = 0;
					syslog(LOG_ERR,
						"clnt_tli_create: "
						"strdup failed");
					goto err1;
				}
			}
			freenetconfigent(nc);
		}
		if (cl->cl_netid == NULL)
			cl->cl_netid = "";
		if (cl->cl_tp == NULL)
			cl->cl_tp = "";
	}
	if (madefd) {
		(void) CLNT_CONTROL(cl, CLSET_FD_CLOSE, (char *)NULL);
/*		(void) CLNT_CONTROL(cl, CLSET_POP_TIMOD, (char *)NULL);  */
	};

	trace3(TR_clnt_tli_create, 1, prog, vers);
	return (cl);

err:
	rpc_createerr.cf_stat = RPC_TLIERROR;
	rpc_createerr.cf_error.re_errno = errno;
	rpc_createerr.cf_error.re_terrno = t_errno;
err1:	if (madefd)
		(void) t_close(fd);
	trace3(TR_clnt_tli_create, 1, prog, vers);
	return ((CLIENT *)NULL);
}

/*
 *  To avoid conflicts with the "magic" file descriptors (0, 1, and 2),
 *  we try to not use them.  The __rpc_raise_fd() routine will dup
 *  a descriptor to a higher value.  If we fail to do it, we continue
 *  to use the old one (and hope for the best).
 */
int __rpc_minfd = 3;

int
__rpc_raise_fd(int fd)
{
	int nfd;

	if (fd >= __rpc_minfd)
		return (fd);

	if ((nfd = _fcntl(fd, F_DUPFD, __rpc_minfd)) == -1)
		return (fd);

	if (t_sync(nfd) == -1) {
		close(nfd);
		return (fd);
	}

	if (t_close(fd) == -1) {
		/* this is okay, we will syslog an error, then use the new fd */
		(void) syslog(LOG_ERR,
			"could not t_close() fd %d; mem & fd leak", fd);
	}

	return (nfd);
}
